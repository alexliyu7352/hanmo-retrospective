#include "resident/domains/military/military_supply_lookup.hpp"
#include "native_family_initial_sources.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/domains/military/field_battle/field_contact.hpp"
#include "resident/domains/military/field_battle/field_install.hpp"
#include "resident/domains/military/field_battle/field_validation.hpp"
#include "resident/domains/tasks/personal_acceptances/task_personal_acceptance_world.hpp"
#include "resident/domains/tasks/personal_acceptances/task_personal_acceptance_boundary.hpp"
#include "resident/domains/tasks/personal_cancellations/task_personal_cancellation_world.hpp"
#include "resident/domains/tasks/personal_cancellations/task_personal_cancellation_boundary.hpp"
#include "political_subordinate_appointment_director_world.hpp"
#include "resident/domains/tasks/personal_reports/task_personal_report_world.hpp"
#include "resident/domains/tasks/personal_reports/task_personal_report_boundary.hpp"
#include "resident/domains/tasks/personal_messages/task_personal_message_world.hpp"
#include "resident/domains/tasks/personal_messages/task_personal_message_boundary.hpp"
#include "resident/model/procurement_timing.hpp"
#include "resident/domains/tasks/message_returns/task_message_return_world.hpp"
#include "resident/domains/tasks/message_returns/task_message_return_boundary.hpp"
#include "resident/domains/tasks/shop_order_contacts/task_shop_order_contact_world.hpp"
#include "resident/domains/interaction/visit_messages/visit_message_receipt_world.hpp"
#include "resident/domains/interaction/visit_messages/visit_message_world.hpp"
#include "resident/domains/interaction/personal_dialogues/known_personal_dialogue_world.hpp"
#include "military_siege_engineering_integration.hpp"
#include "resident/domains/interaction/visit_appointments/visit_appointment_world.hpp"
#include "resident/domains/interaction/born_dialogues/born_personal_dialogue_world.hpp"
#include "resident/domains/interaction/visit_replies/visit_reply_world.hpp"
#include "resident/domains/interaction/contact_inquiries/contact_inquiry_world.hpp"
#include "military_supply_attrition_integration.hpp"
#include "resident/domains/interaction/appointments/personal_appointment_world.hpp"
#include "military_recovery_integration.hpp"
#include "naval_navigation_integration.hpp"
#include "naval_gunnery_integration.hpp"
#include "military_commanded_damage_integration.hpp"
#include "military_command_resources_integration.hpp"
#include "resident/domains/military/military_tactic_entry_source.hpp"
#include "resident/domains/military/military_command_resources_source.hpp"
#include "resident/domains/military/military_commanded_damage_source.hpp"
#include "resident/domains/military/naval_navigation_history.hpp"
#include "resident/domains/tasks/message_notices/task_message_notice_world.hpp"
// 按已冻结17阶段推进同一个候选，再执行advance原尾；任何能力失败都返回给唯一事务丢弃。
#include "political_takeover_execution_world.hpp"
#include "world_bundle.hpp"
#include "private_shop_integration.hpp"
#include "private_shop_daily_rent_integration.hpp"
#include "resident/domains/military/naval_battle_history.hpp"
#include "resident/domains/military/naval_validation.hpp"
#include "naval_integration.hpp"
#include "resident/domains/military/naval_repair.hpp"
#include "resident/domains/military/naval_repair_history.hpp"
#include "resident/domains/military/naval_battle_phase.hpp"
#include "resident/domains/military/naval_journey_termination.hpp"
#include "resident/domains/military/military_journey_termination.hpp"
#include "resident/domains/military/naval_contact_install.hpp"
#include "resident/domains/local_economy/wages/cohort_wage_need_adapter.hpp"
#include "resident/domains/local_economy/fiscal/fiscal_need_adapter.hpp"
#include "siege_integration.hpp"
#include "dynamic_integration.hpp"
#include "governance_task_integration.hpp"
#include "envoy_task_integration.hpp"
#include "political_fiscal_civil_integration.hpp"
#include "occupation_fiscal_integration.hpp"
#include "hanmo/resident/phase_contract.hpp"
#include "resident/domains/military/military_phases.hpp"
#include "resident/domains/military/military_schedule.hpp"
#include "resident/domains/military/military_contact.hpp"
#include "resident/domains/military/battle_world.hpp"
#include "resident/domains/military/siege_world.hpp"
#include "resident/domains/military/military_lifecycle.hpp"
#include "resident/domains/military/military_conditions.hpp"
#include "resident/domains/military/training_rules.hpp"
#include "resident/domains/interaction/interaction_phases.hpp"
#include <set>
#include <limits>
#include <chrono>
namespace hanmo::resident {
namespace {
using PhaseClock = std::chrono::steady_clock;
Integer usec(PhaseClock::time_point start) {
  return std::chrono::duration_cast<std::chrono::microseconds>(
             PhaseClock::now() - start)
      .count();
}
void timed(WorldBundle &w, const StableId &name, Integer value) {
  for (auto &entry : w.timing)
    if (entry.first == name) {
      entry.second += value;
      return;
    }
  w.timing.emplace_back(name, value);
}
const std::array<const char *, 17> phase_names = {"preflight",
                                                  "military_supply",
                                                  "siege_step",
                                                  "military_battle",
                                                  "battle_actor_changes",
                                                  "siege_battles_finished",
                                                  "estate_production",
                                                  "contact_detect",
                                                  "actions_step",
                                                  "contact_install",
                                                  "social_step",
                                                  "siege_results",
                                                  "campaign_step",
                                                  "release_invalid_commands",
                                                  "clock_commit",
                                                  "military_return_complete",
                                                  "family_complete"};
Status phase_error(Status r, Phase phase) {
  if (!r.ok && r.error.phase.empty())
    r.error.phase = std::to_string(static_cast<int>(phase));
  return r;
}
} // namespace
// 原NPC失败可继续下一项，但不得把失败项写入保留；共同写集由A savepoint维护。
Status try_npc_command(WorldBundle &w, const StableId &actor,
                       const CommandPayload &command, bool &applied) {
  {
    auto allowed = task_personal_acceptance_npc_command_allowed(command.kind);
    if (!allowed.ok)
      return allowed;
  }
  {
    auto allowed = task_personal_cancellation_npc_command_allowed(command.kind);
    if (!allowed.ok)
      return allowed;
  }
  {
    auto allowed = task_personal_report_npc_command_allowed(command.kind);
    if (!allowed.ok)
      return allowed;
  }
  {
    auto allowed = task_personal_message_npc_command_allowed(command.kind);
    if (!allowed.ok)
      return allowed;
  }
  {
    auto allowed = task_message_return_npc_command_allowed(
        w, command.kind, std::get_if<TaskCommand>(&command.data));
    if (!allowed.ok)
      return allowed;
  }
  if (w.catalog->task_shop_order_contacts_enabled &&
      (command.kind == "task_shop_order_deliver" ||
       (std::get_if<TaskShopOrderCommand>(&command.data) &&
        std::get<TaskShopOrderCommand>(command.data).kind ==
            TaskShopOrderCommand::Kind::deliver)))
    return common_failure("TaskShopOrderContactOuterResultRequired");
  if (w.catalog->visit_message_receipts_enabled &&
      actor == w.common.state().metadata.player_actor_id &&
      (std::holds_alternative<VisitMessageCommand>(command.data) ||
       visit_message_command_kind(command.kind)))
    return common_failure("VisitMessageNpcActorRequired");
  applied = false;
  if (std::holds_alternative<OpenKnownPersonalDialogue>(command.data) ||
      command.kind == "open_known_personal_dialogue")
    return common_failure("KnownPersonalDialoguePlayerRequired");
  applied = false;
  if (std::holds_alternative<PoliticalHeadHandoverCommand>(command.data) ||
      is_political_head_handover_command(command.kind))
    return common_failure("PoliticalHeadHandoverPlayerCommandRequired");
  applied = false;
  if (std::holds_alternative<PoliticalSuccessorRoundCommand>(command.data) ||
      is_political_successor_round_command(command.kind))
    return common_failure("PoliticalSuccessorRoundPlayerCommandRequired");
  applied = false;
  if (std::holds_alternative<PoliticalTermHandoverCommand>(command.data) ||
      is_political_term_handover_command(command.kind))
    return common_failure("PoliticalTermHandoverPlayerCommandRequired");
  applied = false;
  if (std::holds_alternative<PoliticalOfficeReceptionCommand>(command.data) ||
      is_political_office_reception_command(command.kind))
    return common_failure("PoliticalOfficeReceptionPlayerCommandRequired");
  if (std::holds_alternative<PoliticalOfficeResignationCommand>(command.data) ||
      (is_political_office_resignation_command(command.kind) &&
       !std::holds_alternative<AuthoredOfficeResignationCommand>(command.data)))
    return common_failure("PoliticalOfficeResignationPlayerCommandRequired");
  if (std::holds_alternative<PoliticalSubordinateAppointmentCommand>(
          command.data) ||
      (is_political_subordinate_appointment_command(command.kind) &&
       !std::holds_alternative<AuthoredSubordinateAppointmentCommand>(
           command.data)))
    return common_failure(
        "PoliticalSubordinateAppointmentPlayerCommandRequired");
  if (w.catalog->political_dismissal_sources &&
      (command.kind == "political_dismiss" ||
       (std::get_if<PoliticsCommand>(&command.data) &&
        std::holds_alternative<PoliticalDismissCommand>(
            *std::get_if<PoliticsCommand>(&command.data)))))
    return common_failure("PoliticalDismissalPlayerOnly");
  if (std::holds_alternative<PoliticalGovernanceAgencyCommand>(command.data) ||
      (is_political_governance_agency_command(command.kind) &&
       !std::holds_alternative<AuthoredGovernanceAgencyCommand>(command.data)))
    return common_failure("PoliticalGovernanceAgencyPlayerCommandRequired");
  applied = false;
  if (std::holds_alternative<PoliticalSuccessionCommand>(command.data) ||
      is_political_succession_command(command.kind))
    return common_failure("PoliticalSuccessionPlayerCommandRequired");
  applied = false;
  if (std::holds_alternative<VisitAppointmentCommand>(command.data) ||
      std::holds_alternative<VisitAppointmentMeetingCommand>(command.data) ||
      is_visit_appointment_command(command.kind) ||
      is_visit_appointment_meeting_command(command.kind))
    return common_failure("VisitAppointmentPlayerCommandRequired");
  applied = false;
  if (std::holds_alternative<GrowthTacticLessonCommand>(command.data) ||
      command.kind.rfind("tactic_lesson_", 0) == 0)
    return common_failure("TacticLessonPlayerCommandRequired");
  // 控制许可只能本人提交并取得永久实际结果，NPC事务不得代决。
  if (std::holds_alternative<VoluntaryPathCommand>(command.data) ||
      command.kind.rfind("family_voluntary_path_", 0) == 0 ||
      (w.family_voluntary_paths_enabled() &&
       (command.kind == "family_agree_succession" ||
        command.kind == "family_cancel_succession" ||
        command.kind == "family_continue_as" ||
        command.kind == "family_retire")))
    return common_failure("VoluntaryPathPlayerCommandRequired");
  if (std::holds_alternative<CollateralControlCommand>(command.data) ||
      command.kind.rfind("family_collateral_", 0) == 0)
    return common_failure("CollateralControlPlayerCommandRequired");
  if (std::holds_alternative<BiologicalControlCommand>(command.data) ||
      command.kind == "family_biological_accept" ||
      command.kind == "family_biological_revoke")
    return common_failure("BiologicalControlPlayerCommandRequired");
  applied = false;
  if (std::holds_alternative<OpenBornPersonalDialogue>(command.data) ||
      command.kind == "open_born_personal_dialogue")
    return common_failure("BornPersonalDialoguePlayerRequired");
  applied = false;
  if (std::holds_alternative<PrivateShopDailyRentCommand>(command.data) ||
      private_shop_daily_rent_command_kind(command.kind))
    return common_failure("PrivateShopDailyRentPlayerCommandRequired");
  if (std::holds_alternative<GovernanceInvitationReplyCommand>(command.data) ||
      is_governance_invitation_reply_command(command.kind))
    return common_failure("GovernanceInvitationPlayerCommandRequired");
  if (std::holds_alternative<VisitReplyCommand>(command.data) ||
      visit_reply_command_kind(command.kind))
    return common_failure("VisitReplyPlayerCommandRequired");
  if (std::holds_alternative<ContactInquiryRequest>(command.data) ||
      command.kind == contact_inquiry_action_kind)
    return common_failure("ContactInquiryPlayerCommandRequired");
  if ((w.family_disciple_legacy_enabled() ||
       w.family_biological_legacy_enabled() ||
       w.family_collateral_legacy_enabled()) &&
      (std::holds_alternative<FamilyLegacyCommand>(command.data) ||
       command.kind.rfind("family_legacy_", 0) == 0))
    return common_failure("DiscipleControlDirectDecisionRequired");
  if (std::holds_alternative<PoliticalContinueGovernance>(command.data) ||
      command.kind == political_governance_continue_command)
    return common_failure("GovernanceContinuationPlayerCommandRequired");
  if (std::holds_alternative<ApprenticeshipCommand>(command.data) ||
      command.kind.rfind("family_apprenticeship_", 0) == 0)
    return common_failure("ApprenticeshipDirectDecisionRequired");
  auto started = PhaseClock::now();
  auto preview = prepare_command(w, actor, command);
  timed(w, "npc_preview", usec(started));
  if (!preview.ok)
    return preview.error.kind == ErrorKind::rule
               ? common_success()
               : Status{false, {}, preview.error};
  auto mark = w.common.begin_savepoint();
  if (!mark.ok)
    return {false, {}, mark.error};
  // payload只有发生该域命令才备份，不克隆全世界或所有演员/资产。
  started = PhaseClock::now();
  const bool estate_command =
      std::holds_alternative<PrivateShopLiquidationCommand>(command.data) ||
      std::holds_alternative<TaskShopOrderEstateCancel>(command.data);
  const bool shop_command =
      estate_command ||
      std::holds_alternative<PrivateShopCommand>(command.data) ||
      std::holds_alternative<TaskShopOrderCommand>(command.data);
  auto shop_liquidations =
      estate_command
          ? std::optional<
                PrivateShopLiquidationState>{w.private_shop_liquidations}
          : std::nullopt;
  auto shops = shop_command ? std::optional<PrivateShopState>{w.private_shops}
                            : std::nullopt;
  auto daily_rent =
      shop_command && w.private_shop_daily_rent_enabled()
          ? std::optional<PrivateShopDailyRentState>{w.private_shop_daily_rent}
          : std::nullopt;
  auto shop_results =
      w.private_shop_command_results_enabled()
          ? std::optional<
                PrivateShopCommandResultsState>{w.private_shop_command_results}
          : std::nullopt;
  auto shop_law =
      std::holds_alternative<PoliticalShopLicensingCommand>(command.data)
          ? std::optional<
                PoliticalShopLicensingState>{w.political_shop_licensing}
          : std::nullopt;
  auto registry =
      std::holds_alternative<PoliticalAcceptShopRegistry>(command.data)
          ? std::optional<
                PoliticalShopRegistrySuccessionState>{w.political_shop_registry_successions}
          : std::nullopt;
  auto market =
      w.growth_market_history_enabled()
          ? std::optional<GrowthMarketHistoryState>{w.growth_market_history}
          : std::nullopt;
  auto market_capture = w.growth_market_capture;
  auto tactic_lessons =
      w.growth_tactic_lessons_enabled()
          ? std::optional<GrowthTacticLessonState>{w.growth_tactic_lessons}
          : std::nullopt;
  auto politics = std::holds_alternative<PoliticsCommand>(command.data)
                      ? std::optional<PoliticsState>{w.politics}
                      : std::nullopt;
  const bool authored_resignation =
      std::holds_alternative<AuthoredOfficeResignationCommand>(command.data);
  const bool authored_subordinate =
      std::holds_alternative<AuthoredSubordinateAppointmentCommand>(
          command.data);
  auto subordinate_appointments = authored_subordinate
                                      ? w.political_subordinate_appointments
                                      : std::nullopt;
  auto office_resignations =
      authored_resignation ? w.political_office_resignations : std::nullopt;
  auto organization = (politics || authored_resignation || authored_subordinate)
                          ? std::optional<OrganizationState>{w.organization}
                          : std::nullopt;
  auto fiscal = std::holds_alternative<PoliticalSetTaxCommand>(command.data)
                    ? std::optional<PoliticalFiscalState>{w.political_fiscal}
                    : std::nullopt;
  auto treaty_passage =
      std::holds_alternative<TreatyPassageCommand>(command.data)
          ? std::optional<TreatyPassageState>{w.treaty_passage}
          : std::nullopt;
  auto political_military_appointments =
      std::holds_alternative<PoliticalMilitaryAppointmentCommand>(command.data)
          ? std::optional<
                PoliticalMilitaryAppointmentState>{w.political_military_appointments}
          : std::nullopt;
  auto political_field_appointments =
      std::holds_alternative<PoliticalFieldAppointmentCommand>(command.data)
          ? std::optional<
                PoliticalFieldAppointmentState>{w.political_field_appointments}
          : std::nullopt;
  auto occupation_fiscal =
      std::holds_alternative<PoliticalSetOccupationTaxCommand>(command.data)
          ? std::optional<OccupationFiscalState>{w.occupation_fiscal}
          : std::nullopt;
  const bool naval_repair_command =
      std::holds_alternative<NavalRepairCommand>(command.data);
  const bool naval_battle_command =
      std::holds_alternative<NavalBattleCommand>(command.data);
  const auto *military_payload = std::get_if<MilitaryCommand>(&command.data);
  const bool field_command =
      military_payload &&
      std::holds_alternative<MilitaryFieldCommandAssign>(*military_payload);
  const bool military_command =
      naval_battle_command ||
      std::holds_alternative<MilitaryCommand>(command.data) ||
      std::holds_alternative<SiegeIntegrationCommand>(command.data) ||
      std::holds_alternative<SiegeIntegrationApply>(command.data);
  const bool takeover_command =
      std::holds_alternative<PoliticalTakeoverDecisionCommand>(command.data);
  auto supply_ledger =
      std::holds_alternative<IssueEconomicSupplyPolicyCommand>(command.data)
          ? std::optional<EconomicSupplyPolicyState>{w.economic_supply_policies}
          : std::nullopt;
  auto supply_adoptions =
      takeover_command
          ? std::optional<
                PoliticalSupplyAdoptionState>{w.political_supply_adoptions}
          : std::nullopt;
  auto takeover_ledger =
      (takeover_command || military_command)
          ? std::optional<PoliticalTakeoverState>{w.political_takeover}
          : std::nullopt;
  auto takeover_economy =
      (takeover_command || military_command ||
       std::holds_alternative<TaskCommand>(command.data) ||
       std::holds_alternative<GovernanceCommand>(command.data) ||
       std::holds_alternative<ConfigureLocalProductionCommand>(command.data))
          ? std::optional<
                EconomicTakeoverExecutionState>{w.economic_takeover_execution}
          : std::nullopt;
  if (military_command) {
    politics = w.politics;
    organization = w.organization;
  }
  const bool travel_command =
      std::holds_alternative<TravelCommandPayload>(command.data) ||
      military_command;
  auto travel =
      travel_command ? std::optional<TravelState>{w.travel} : std::nullopt;
  const bool population_command =
      std::holds_alternative<PopulationMilitaryCommand>(command.data) ||
      std::holds_alternative<SiegeIntegrationApply>(command.data);
  auto military = (military_command || population_command)
                      ? std::optional<MilitaryState>{w.military}
                      : std::nullopt;
  auto tactic_learning =
      (military_command && w.military_tactic_learning_enabled())
          ? std::optional<
                MilitaryTacticLearningState>{w.military_tactic_learning}
          : std::nullopt;
  auto naval = ((w.naval_navigation_enabled() && travel_command) ||
                naval_repair_command || field_command || naval_battle_command ||
                std::holds_alternative<NavalCommand>(command.data))
                   ? std::optional<NavalState>{w.naval}
                   : std::nullopt;
  auto population =
      (naval_battle_command || population_command)
          ? std::optional<PopulationMilitaryState>{w.population_military}
          : std::nullopt;
  auto household_management =
      std::holds_alternative<ConfigureHouseholdProvisionCommand>(command.data)
          ? std::optional<
                HouseholdProvisionManagementState>{w.household_provision_management}
          : std::nullopt;
  auto economy =
      (shop_command ||
       std::holds_alternative<EstateEmploymentCommand>(command.data) ||
       naval_battle_command || population_command ||
       std::holds_alternative<ConfigureLocalProductionCommand>(command.data))
          ? std::optional<LocalEconomyState>{w.local_economy}
          : std::nullopt;
  auto personal = std::holds_alternative<PersonalCommand>(command.data)
                      ? std::optional<PersonalState>{w.personal}
                      : std::nullopt;
  auto growth = (w.personal_recovery_enabled() || shop_command ||
                 w.growth_market_history_enabled() || naval_battle_command ||
                 std::holds_alternative<GrowthCommand>(command.data))
                    ? std::optional<GrowthState>{w.growth}
                    : std::nullopt;
  auto personal_recovery =
      w.personal_recovery_enabled()
          ? std::optional<GrowthRecoveryState>{w.personal_recovery}
          : std::nullopt;
  auto family = (naval_battle_command ||
                 std::holds_alternative<FamilyCommand>(command.data))
                    ? std::optional<FamilyState>{w.family}
                    : std::nullopt;
  // 具名私产声明与控制许可只写各自独立账，公共NPC入口仍完整保留其回滚写集。
  auto employment =
      std::holds_alternative<EstateEmploymentCommand>(command.data)
          ? std::optional<EstateEmploymentState>{w.estate_employment}
          : std::nullopt;
  auto care_relief =
      std::holds_alternative<ChildCareReliefCommand>(command.data)
          ? std::optional<ChildCareReliefState>{w.child_care_relief}
          : std::nullopt;
  auto birth = std::holds_alternative<FamilyBirthCommand>(command.data)
                   ? std::optional<FamilyBirthState>{w.family_birth}
                   : std::nullopt;
  auto marriage = std::holds_alternative<FamilyMarriageCommand>(command.data)
                      ? std::optional<FamilyMarriageState>{w.family_marriage}
                      : std::nullopt;
  auto inherited =
      std::holds_alternative<FamilyInheritanceCommand>(command.data)
          ? std::optional<FamilyInheritanceState>{w.family_inheritance}
          : std::nullopt;
  auto bequest = std::holds_alternative<FamilyBequestCommand>(command.data)
                     ? std::optional<FamilyBequestState>{w.family_bequest}
                     : std::nullopt;
  auto collateral =
      std::holds_alternative<CollateralControlCommand>(command.data)
          ? std::optional<CollateralControlState>{w.family_collateral_legacy}
          : std::nullopt;
  auto legacy =
      (std::holds_alternative<FamilyLegacyCommand>(command.data) ||
       std::holds_alternative<BiologicalControlCommand>(command.data) ||
       std::holds_alternative<CollateralControlCommand>(command.data))
          ? std::optional<FamilyLegacyState>{w.family_legacy}
          : std::nullopt;
  auto estate =
      (std::holds_alternative<EstateEmploymentCommand>(command.data) ||
       std::holds_alternative<EstateCommand>(command.data) ||
       std::holds_alternative<FamilyCommand>(command.data) || military_command)
          ? std::optional<EstateState>{w.estate}
          : std::nullopt;
  auto social =
      (shop_command || std::holds_alternative<TaskCommand>(command.data) ||
       std::holds_alternative<GrowthCommand>(command.data) ||
       std::holds_alternative<FamilyCommand>(command.data))
          ? std::optional<SocialState>{w.social}
          : std::nullopt;
  // 组织发令、私人垫款与个人路遇结果均可写任务责任，必须包含同一回滚写集。
  const bool task_write =
      std::holds_alternative<TaskPersonalAcceptance>(command.data) ||
      std::holds_alternative<TaskPersonalCancel>(command.data) ||
      std::holds_alternative<TaskPersonalReport>(command.data) ||
      std::holds_alternative<TaskPersonalMessageDeliver>(command.data) ||
      std::holds_alternative<TaskMessageReturnCollect>(command.data) ||
      std::holds_alternative<TaskExtensionCommand>(command.data) ||
      shop_command || takeover_command ||
      std::holds_alternative<TaskPersonalAdvanceCommand>(command.data) ||
      std::holds_alternative<TaskCommand>(command.data) ||
      std::holds_alternative<OrganizationContractCommand>(command.data) ||
      std::holds_alternative<PersonalCommand>(command.data);
  const bool authored_agency =
      std::holds_alternative<AuthoredGovernanceAgencyCommand>(command.data);
  auto governance_agencies =
      authored_agency ? w.political_governance_agencies : std::nullopt;
  const bool governance_command =
      authored_agency ||
      std::holds_alternative<GovernanceCommand>(command.data);
  // 外交递交与原任务同一子事务，晚失败必须恢复全部领域责任。
  const bool diplomacy_command =
      std::holds_alternative<DiplomacyCommand>(command.data);
  auto diplomacy = diplomacy_command
                       ? std::optional<DiplomacyState>{w.diplomacy}
                       : std::nullopt;
  auto envoy_tasks = diplomacy_command
                         ? std::optional<EnvoyTaskState>{w.envoy_tasks}
                         : std::nullopt;
  auto governance =
      governance_command
          ? std::optional<PoliticsGovernanceState>{w.politics_governance}
          : std::nullopt;
  auto governance_tasks =
      governance_command
          ? std::optional<GovernanceTaskState>{w.governance_tasks}
          : std::nullopt;
  auto relief = governance_command
                    ? std::optional<PublicReliefState>{w.public_relief}
                    : std::nullopt;
  auto tasks = (w.economic_supply_policies_enabled() || naval_battle_command ||
                task_write || governance_command || diplomacy_command ||
                military_command)
                   ? std::optional<TaskState>{w.tasks}
                   : std::nullopt;
  auto affairs = (w.economic_supply_policies_enabled() || task_write ||
                  military_command) &&
                         w.dynamic_enabled()
                     ? std::optional<AffairsState>{w.affairs}
                     : std::nullopt;
  auto interaction =
      (w.personal_recovery_enabled() ||
       std::holds_alternative<PersonalAppointmentCommand>(command.data) ||
       std::holds_alternative<OpenKnownPersonalDialogue>(command.data) ||
       std::holds_alternative<VisitAppointmentCommand>(command.data) ||
       std::holds_alternative<VisitAppointmentMeetingCommand>(command.data) ||
       std::holds_alternative<AuthoredVisitAppointment>(command.data) ||
       std::holds_alternative<OpenBornPersonalDialogue>(command.data) ||
       std::holds_alternative<AuthoredVisitReply>(command.data) ||
       std::holds_alternative<VisitReplyCommand>(command.data) ||
       std::holds_alternative<ContactInquiryRequest>(command.data) ||
       shop_command ||
       std::holds_alternative<PoliticalShopLicensingCommand>(command.data) ||
       std::holds_alternative<ChildCareReliefCommand>(command.data) ||
       std::holds_alternative<VisitMessageCommand>(command.data) ||
       naval_repair_command ||
       std::holds_alternative<FamilyBirthCommand>(command.data) ||
       naval_battle_command || command.kind == "move_local" ||
       std::holds_alternative<InteractionCommand>(command.data) ||
       std::holds_alternative<PersonalCommand>(command.data))
          ? std::optional<InteractionState>{w.interaction}
          : std::nullopt;
  timed(w, "npc_domain_backup", usec(started));
  started = PhaseClock::now();
  auto result = apply_command(w, actor, *preview.value->operation);
  timed(w, "npc_apply", usec(started));
  if (result.ok && (w.growth_market_capture.open_scopes != 0 ||
                    w.growth_market_capture.unbound_business)) {
    result.ok = false;
    result.error =
        common_failure("GrowthMarketUnboundBusiness", {}, ErrorKind::internal)
            .error;
  }
  if (!result.ok) {
    auto rollback = w.common.rollback_savepoint(mark.value);
    if (!rollback.ok)
      return rollback;
    if (travel)
      w.travel = std::move(*travel);
    if (military) {
      // 回滚替换会销毁原候选行；必须在任何旧地址失效之前撤销索引。
      invalidate_military_supply_lookup(w.military);
      w.military = std::move(*military);
    }
    if (tactic_learning)
      w.military_tactic_learning = std::move(*tactic_learning);
    if (naval)
      w.naval = std::move(*naval);
    if (interaction)
      w.interaction = std::move(*interaction);
    if (tasks)
      w.tasks = std::move(*tasks);
    if (diplomacy)
      w.diplomacy = std::move(*diplomacy);
    if (envoy_tasks)
      w.envoy_tasks = std::move(*envoy_tasks);
    if (office_resignations)
      w.political_office_resignations = std::move(*office_resignations);
    if (subordinate_appointments)
      w.political_subordinate_appointments =
          std::move(*subordinate_appointments);
    if (governance_agencies)
      w.political_governance_agencies = std::move(*governance_agencies);
    if (governance)
      w.politics_governance = std::move(*governance);
    if (governance_tasks)
      w.governance_tasks = std::move(*governance_tasks);
    if (relief)
      w.public_relief = std::move(*relief);
    if (affairs)
      w.affairs = std::move(*affairs);
    if (estate)
      w.estate = std::move(*estate);
    if (family)
      w.family = std::move(*family);
    if (bequest)
      w.family_bequest = std::move(*bequest);
    if (inherited)
      w.family_inheritance = std::move(*inherited);
    if (marriage)
      w.family_marriage = std::move(*marriage);
    if (birth)
      w.family_birth = std::move(*birth);
    if (care_relief)
      w.child_care_relief = std::move(*care_relief);
    if (employment)
      w.estate_employment = std::move(*employment);
    if (legacy)
      w.family_legacy = std::move(*legacy);
    if (collateral)
      w.family_collateral_legacy = std::move(*collateral);
    if (growth)
      w.growth = std::move(*growth);
    if (personal_recovery)
      w.personal_recovery = std::move(*personal_recovery);
    if (personal)
      w.personal = std::move(*personal);
    if (social)
      w.social = std::move(*social);
    if (population)
      w.population_military = std::move(*population);
    if (economy)
      w.local_economy = std::move(*economy);
    if (household_management)
      w.household_provision_management = std::move(*household_management);
    if (politics)
      w.politics = std::move(*politics);
    if (organization)
      w.organization = std::move(*organization);
    if (fiscal)
      w.political_fiscal = std::move(*fiscal);
    if (occupation_fiscal)
      w.occupation_fiscal = std::move(*occupation_fiscal);
    if (treaty_passage)
      w.treaty_passage = std::move(*treaty_passage);
    if (political_military_appointments)
      w.political_military_appointments =
          std::move(*political_military_appointments);
    if (political_field_appointments)
      w.political_field_appointments = std::move(*political_field_appointments);
    if (supply_ledger)
      w.economic_supply_policies = std::move(*supply_ledger);
    if (supply_adoptions)
      w.political_supply_adoptions = std::move(*supply_adoptions);
    if (takeover_ledger)
      w.political_takeover = std::move(*takeover_ledger);
    if (takeover_economy)
      w.economic_takeover_execution = std::move(*takeover_economy);
    if (shops)
      w.private_shops = std::move(*shops);
    if (daily_rent)
      w.private_shop_daily_rent = std::move(*daily_rent);
    if (shop_results)
      w.private_shop_command_results = std::move(*shop_results);
    if (shop_liquidations)
      w.private_shop_liquidations = std::move(*shop_liquidations);
    if (shop_law)
      w.political_shop_licensing = std::move(*shop_law);
    if (registry)
      w.political_shop_registry_successions = std::move(*registry);
    if (market)
      w.growth_market_history = std::move(*market);
    w.growth_market_capture = market_capture;
    if (tactic_lessons)
      w.growth_tactic_lessons = std::move(*tactic_lessons);
    return result.error.kind == ErrorKind::rule
               ? common_success()
               : Status{false, {}, result.error};
  }
  auto committed = w.common.commit_savepoint(mark.value);
  if (!committed.ok)
    return committed;
  applied = true;
  return common_success();
}
namespace {
StableId military_kind(const MilitaryCommand &c) {
  if (std::holds_alternative<MilitaryTacticPracticeClaim>(c))
    return "military_tactic_practice";
  if (std::holds_alternative<MilitaryFieldCommandAssign>(c))
    return "military_field_command_assign";
  if (std::holds_alternative<MilitaryOfficerAssign>(c))
    return "military_officer_assign";
  if (std::holds_alternative<OfficeAccept>(c))
    return "office_accept";
  if (std::holds_alternative<CampaignAccept>(c))
    return "campaign_accept";
  if (std::holds_alternative<ArmyForm>(c))
    return "army_form";
  if (auto *p = std::get_if<CampaignManage>(&c))
    return p->kind;
  if (auto *p = std::get_if<ArmyLogistics>(&c))
    return p->kind;
  if (std::holds_alternative<SiegeStart>(c))
    return "siege_start";
  if (std::holds_alternative<SiegeClose>(c))
    return "siege_close";
  return "campaign_depart";
}
StableId travel_kind(const TravelCommandPayload &c) {
  if (std::holds_alternative<MoveLocalCommand>(c))
    return "move_local";
  return "travel";
}
} // namespace
Outcome<AdvanceDetails> advance_world(WorldBundle &w,
                                      const std::vector<StableId> &hold) {
  // Time2调用原业务primitive时由compiled
  // phase拥有；独立legacy整步入口不能再调这些writer。
  if (w.catalog->exact_time)
    return {false,
            {},
            common_failure("LegacyAdvanceDisabledForExactTime", {},
                           ErrorKind::unsupported_capability)
                .error};
  const auto recovery_closure_prefix =
      w.personal_recovery_enabled() ? w.personal_recovery.closures.order()
                                    : std::vector<StableId>{};
  const bool field_losses =
      military_field_commander_profile_in_use(w.military, *w.catalog->military);
  const auto naval_fact_prefix = (w.naval_combat_enabled() || field_losses)
                                     ? w.common.state().facts.order()
                                     : std::vector<StableId>{};
  const auto from_tick = w.common.tick();
  if (from_tick == std::numeric_limits<Integer>::max())
    return {false, {}, common_failure("WorldTickOverflow").error};
  const auto to_tick = from_tick + 1;
  auto old_places = snapshot_actor_places(w.common);
  DynamicStepInput dynamic_input;
  LocalEconomyEffects fiscal_effects, economy_effects;
  AdvanceDetails result;
  ContactBatch contacts;
  MilitaryContactProbe military_contacts;
  std::vector<FieldRoadContact> field_contacts;
  std::vector<StableId> field_held_armies;
  NavalContactProbe naval_contacts;
  SiegeWorldStep sieges;
  MilitaryBattleStep battles;
  std::vector<StableId> effective_hold = hold, completed_receipts;
  std::vector<MilitaryJourneyRequest> journeys, siege_journeys;
  std::vector<MilitaryReturnCompletion> military_returns;
  std::vector<TerrainExecution> military_executions, navigation_executions;
  std::vector<StableId> family_completed;
  std::set<StableId> contacted;
  auto ia = w.services.interaction_access();
  auto ea = w.services.estate_access();
  auto ma = w.services.military_access(
      to_tick, w.dynamic_enabled() ? &dynamic_input : nullptr);
  ma.interaction_access = &ia;
  for (auto phase : phase_order) {
    const auto phase_start = PhaseClock::now();
    Status r = common_success();
    switch (phase) {
    case Phase::preflight: {
      r = validate_field_battle_layout(
          w.catalog->identity, w.catalog->field_battle.get(),
          w.field_battle ? &*w.field_battle : nullptr);
      if (!r.ok)
        break;
      if (w.field_battle) {
        auto gate = read_field_world_advance_gate(
            *w.field_battle, w.common.state().metadata.player_actor_id,
            from_tick);
        if (!gate.ok) {
          r = {false, {}, gate.error};
          break;
        }
        if (gate.value.reason != FieldWorldAdvanceReason::allowed) {
          r = common_failure(gate.value.reason ==
                                     FieldWorldAdvanceReason::player_input
                                 ? "FieldBattlePlayerInputRequired"
                                 : "FieldBattleLocalSettlementRequired",
                             gate.value.battle_id);
          break;
        }
      }
      if (w.catalog->field_battle && !w.military.battles.empty()) {
        r = common_failure("FieldBattleLegacyStateRejected",
                           "military/battles");
        break;
      }
      // 死亡后停在稳定边界等本人选择或保存，不能让自动推进越过接续决定。
      if (w.family_life_enabled()) {
        const auto *player =
            w.common.actor(w.common.state().metadata.player_actor_id);
        if (player && !player->alive) {
          r = common_failure("PlayerDeathContinuationRequired");
          break;
        }
      }
      if (personal_pauses_world(w.personal)) {
        r = common_failure("BattlePending");
        break;
      }
      r = validate_dynamic_provision_owners(w, to_tick);
      if (!r.ok)
        break;
      if (w.naval_combat_enabled()) {
        r = preflight_naval_battles(w.common, w.naval, *w.catalog->naval,
                                    to_tick);
        if (!r.ok)
          break;
      }
      r = military_preflight(w.common, w.military, *w.catalog->military, ma,
                             to_tick);
      if (!r.ok)
        break;
      auto add_hold = [&](const std::vector<StableId> &ids) {
        for (const auto &id : ids)
          if (std::find(effective_hold.begin(), effective_hold.end(), id) ==
              effective_hold.end())
            effective_hold.push_back(id);
      };
      if (w.field_battle) {
        auto actors = read_field_battle_held_actors(w.common, w.military,
                                                    *w.field_battle);
        if (!actors.ok) {
          r = {false, {}, actors.error};
          break;
        }
        add_hold(actors.value);
        auto armies = read_field_battle_held_armies(w.common, w.military,
                                                    *w.field_battle);
        if (!armies.ok) {
          r = {false, {}, armies.error};
          break;
        }
        field_held_armies = std::move(armies.value);
      }
      add_hold(military_held_actors(w.military));
      if (w.naval_combat_enabled())
        add_hold(naval_battle_held_actors(w.naval, w.military));
      add_hold(personal_held_travel_actors(w.common, w.personal,
                                           *w.catalog->personal));
      if (w.dynamic_enabled())
        dynamic_input = capture_dynamic_step(w, to_tick);
      break;
    }
    case Phase::military_supply: {
      r = military_supply_step(w.common, w.military, *w.catalog->military, ma,
                               to_tick);
      if (r.ok && w.military_supply_attrition_enabled())
        r = step_military_supply_attrition_world(w, dynamic_input);
      if (r.ok && w.military_recovery_enabled())
        r = step_military_wounded_recovery_world(w, dynamic_input);
      if (r.ok && w.military_garrison_rest_enabled())
        r = step_military_garrison_rest_world(w, dynamic_input);
      break;
    }
    case Phase::siege_step: {
      auto engineering_access = ma;
      if (w.military_siege_engineering_enabled())
        engineering_access.siege_engineering =
            make_military_siege_engineering_access(w, &dynamic_input);
      auto step = step_siege_world(w.common, w.military, *w.catalog->military,
                                   engineering_access, to_tick);
      if (!step.ok) {
        r = {false, {}, step.error};
        break;
      }
      sieges = std::move(step.value);
      break;
    }
    case Phase::military_battle: {
      auto engineering_access = ma;
      if (w.military_siege_engineering_enabled())
        engineering_access.siege_engineering =
            make_military_siege_engineering_access(w, &dynamic_input);
      auto step =
          step_military_battles(w.common, w.military, *w.catalog->military,
                                engineering_access, to_tick);
      if (!step.ok) {
        r = {false, {}, step.error};
        break;
      }
      battles = std::move(step.value);
      if (w.naval_combat_enabled()) {
        auto naval = step_naval_battles(
            w.common, w.military, *w.catalog->military, ma, w.naval,
            *w.catalog->naval, to_tick, naval_command_access(w).end_journey);
        if (!naval.ok) {
          r = {false, {}, naval.error};
          break;
        }
      }
      break;
    }
    case Phase::battle_actor_changes:
      r = apply_military_actor_changes_from_sources(
          w.common, w.military, *w.catalog->military, ma, battles.actor_changes,
          battles.casualty_source_ids);
      break;
    case Phase::siege_battles_finished: {
      auto engineering_access = ma;
      if (w.military_siege_engineering_enabled())
        engineering_access.siege_engineering =
            make_military_siege_engineering_access(w, &dynamic_input);
      r = finish_siege_battles(w.common, w.military, *w.catalog->military,
                               engineering_access, battles.completed_battle_ids,
                               to_tick);
      break;
    }
    case Phase::estate_production: {
      if (w.population_military_enabled()) {
        auto context = w.population_write(to_tick);
        auto reconciled = reconcile_population_military(context, to_tick);
        if (!reconciled.ok) {
          r = {false, {}, reconciled.error};
          break;
        }
      }
      if (w.dynamic_enabled()) {
        auto authority = w.services.organization_access();
        std::optional<HouseholdProvisionManagementContext> household_management;
        if (household_provision_management_enabled(w))
          household_management.emplace(HouseholdProvisionManagementContext{
              *w.catalog->household_provision_management,
              w.household_provision_management});
        std::optional<ProductionDisruptionContext> disruption;
        if (w.production_disruptions_enabled())
          disruption.emplace(ProductionDisruptionContext{
              *w.catalog->production_disruptions, w.production_disruptions});
        auto takeover =
            PoliticalTakeoverExecutionWorldAccess::create(w, &dynamic_input);
        if (!takeover.ok) {
          r = {false, {}, takeover.error};
          break;
        }
        std::optional<EconomicTakeoverPhaseContext> execution;
        if (takeover.value->economy())
          execution.emplace(EconomicTakeoverPhaseContext{
              *takeover.value->economy(), w.economic_takeover_execution});
        std::optional<EstateEmploymentWorldAccess> employment;
        if (w.estate_employment_enabled())
          employment.emplace(w, &dynamic_input);
        std::optional<PrivateShopResponsibilitySourceContext> shop_sources;
        if (w.private_shops_enabled())
          shop_sources.emplace(PrivateShopResponsibilitySourceContext{
              w.common, w.local_economy, *w.catalog->local_economy,
              w.private_shops, *w.catalog->private_shops, &dynamic_input,
              w.catalog->private_shop_daily_rent.get(),
              w.private_shop_daily_rent_enabled() ? &w.private_shop_daily_rent
                                                  : nullptr});
        auto economy = step_local_economy_in_candidate(
            w.common, w.local_economy, *w.catalog->local_economy, authority,
            w.services, dynamic_input,
            household_management ? &*household_management : nullptr,
            disruption ? &*disruption : nullptr,
            execution ? &*execution : nullptr,
            employment ? &employment->context() : nullptr,
            w.services.market_history(),
            shop_sources ? &*shop_sources : nullptr);
        if (!economy.ok) {
          r = {false, {}, economy.error};
          break;
        }
        economy_effects = std::move(economy.value);
        auto fiscal = step_economic_fiscal_world(w, dynamic_input);
        if (!fiscal.ok) {
          r = {false, {}, fiscal.error};
          break;
        }
        fiscal_effects = std::move(fiscal.value);
      }
      auto step = step_estate(w.common, w.estate, *w.catalog->estate,
                              *w.catalog->common, w.services, to_tick,
                              effective_hold, &ea);
      r = {step.ok, {}, step.error};
      if (r.ok)
        r = step_estate_employment_world(w, dynamic_input);
      break;
    }
    case Phase::contact_detect: {
      auto detected =
          detect_travel_contacts(w.common, w.travel, *w.catalog->travel,
                                 w.services, to_tick, effective_hold);
      if (!detected.ok) {
        r = {false, {}, detected.error};
        break;
      }
      contacts = std::move(detected.value);
      if (w.catalog->field_battle) {
        auto field = probe_field_road_contacts(
            w.common, w.military, *w.catalog->military, w.travel,
            *w.catalog->travel, w.services, field_held_armies, effective_hold);
        if (!field.ok) {
          r = {false, {}, field.error};
          break;
        }
        field_contacts = std::move(field.value);
        for (const auto &hit : field_contacts) {
          contacted.insert(hit.contact.a);
          contacted.insert(hit.contact.b);
        }
      } else {
        auto military = probe_military_contacts(
            w.common, w.military, *w.catalog->military, w.travel,
            *w.catalog->travel, w.services, effective_hold);
        if (!military.ok) {
          r = {false, {}, military.error};
          break;
        }
        military_contacts = std::move(military.value);
      }
      if (w.naval_combat_enabled()) {
        auto detected_naval = probe_naval_contacts(
            w.common, w.military, *w.catalog->military, ma, w.naval,
            *w.catalog->naval, w.travel, *w.catalog->travel, w.services,
            naval_contact_history(w.naval),
            naval_battle_occupied_fleets(w.naval), effective_hold);
        if (!detected_naval.ok) {
          r = {false, {}, detected_naval.error};
          break;
        }
        naval_contacts = std::move(detected_naval.value);
        for (const auto &hit : naval_contacts.ordered_hits) {
          contacted.insert(hit.contact.contact.a);
          contacted.insert(hit.contact.contact.b);
        }
      }
      for (const auto &hit : military_contacts.ordered_hits) {
        contacted.insert(hit.contact.a);
        contacted.insert(hit.contact.b);
      }
      for (const auto &hit : contacts.ordered_hits) {
        contacted.insert(hit.a);
        contacted.insert(hit.b);
      }
      break;
    }
    case Phase::actions_step: {
      // 陆战已产生本轮真实死亡，先结束原休养；不得先读已死参与者再让整半日失败。
      auto ended = terminate_growth_recovery_world(w, &dynamic_input);
      if (!ended.ok) {
        r = {false, {}, ended.error};
        break;
      }
      for (auto &row : ended.value)
        result.completed_actions.insert(row.first, std::move(row.second));
      auto ids = w.common.action_order();
      std::sort(ids.begin(), ids.end());
      for (const auto &id : ids) {
        auto *p = w.common.action(id);
        if (!p) {
          r = common_failure("MissingAction", id, ErrorKind::internal);
          break;
        }
        auto action = *p;
        ActionCompletion outcome;
        outcome.success = true;
        bool completed = false;
        if (action.kind == "private_shop_operate") {
          auto step = step_private_shop_action_world(w, id, dynamic_input);
          if (!step.ok) {
            r = {false, {}, step.error};
            break;
          }
          if (step.value) {
            const auto *day =
                w.private_shops.operating_results.find(*step.value);
            if (!day) {
              r = common_failure("PrivateShopDayResultMissing", id);
              break;
            }
            ActionCompletion done;
            done.completed = true;
            done.success = day->completed;
            if (!day->reason.empty())
              done.code = day->reason;
            result.completed_actions.insert(id, std::move(done));
          }
          continue;
        } else if (action.kind == contact_inquiry_action_kind) {
          if (action.remaining_ticks != 1) {
            r = common_failure("ContactInquiryActionMismatch", id);
            break;
          }
          ActionMutation mutation;
          mutation.action = action;
          mutation.remaining_ticks = 0;
          r = w.common.apply_action(mutation);
          if (!r.ok)
            break;
          action.remaining_ticks = 0;
          auto step = step_contact_inquiry_world(w, action, dynamic_input);
          if (!step.ok) {
            r = {false, {}, step.error};
            break;
          }
          completed = true;
          outcome = contact_inquiry_completion(step.value);
        } else if (action.kind == visit_appointment_meeting_action_kind) {
          if (action.remaining_ticks != 1) {
            r = common_failure("VisitAppointmentActionMismatch", id);
            break;
          }
          ActionMutation mutation;
          mutation.action = action;
          mutation.remaining_ticks = 0;
          r = w.common.apply_action(mutation);
          if (!r.ok)
            break;
          action.remaining_ticks = 0;
          auto step =
              step_visit_appointment_meeting_world(w, action, dynamic_input);
          if (!step.ok) {
            r = {false, {}, step.error};
            break;
          }
          completed = true;
          outcome.completed = true;
          outcome.success = step.value.success;
          outcome.code = step.value.success
                             ? StableId{"VisitAppointmentMeetingFulfilled"}
                             : step.value.reason;
        } else if (action.kind == personal_appointment_action_kind) {
          if (action.remaining_ticks != 1) {
            r = common_failure("PersonalAppointmentActionMismatch", id);
            break;
          }
          ActionMutation mutation;
          mutation.action = action;
          mutation.remaining_ticks = 0;
          r = w.common.apply_action(mutation);
          if (!r.ok)
            break;
          action.remaining_ticks = 0;
          auto step = step_personal_appointment_world(w, action, dynamic_input);
          if (!step.ok) {
            r = {false, {}, step.error};
            break;
          }
          completed = true;
          outcome = personal_appointment_completion(step.value);
        } else if (action.kind == "task_shop_order_deliver") {
          // 原店单履约仍消费真实一半日；业务完工只在Common行动余时归零后调用。
          ActionMutation mutation;
          mutation.action = action;
          mutation.remaining_ticks = action.remaining_ticks - 1;
          r = w.common.apply_action(mutation);
          if (!r.ok)
            break;
          completed = action.remaining_ticks == 1;
          if (completed) {
            auto step = step_task_shop_order_action_world(w, id, dynamic_input);
            if (!step.ok) {
              r = {false, {}, step.error};
              break;
            }
            outcome = ActionCompletion{};
            outcome.task = std::move(step.value);
          }
        } else if (action.kind == "naval_repair") {
          if (!w.naval_repairs_enabled()) {
            r = common_failure("NavalRepairProfileRequired", id);
            break;
          }
          if (contacted.count(action.actor_id))
            continue;
          auto step =
              step_naval_repair(w.common, w.military, *w.catalog->military, ma,
                                w.naval, *w.catalog->naval, id, dynamic_input);
          if (!step.ok) {
            r = {false, {}, step.error};
            break;
          }
          continue; // 本域已原子释放行动，统一在真实末段来源中生成完成结果。
        } else if (action.kind == "governance_appease") {
          auto step = step_governance_task_world(w, id, dynamic_input);
          if (!step.ok) {
            r = {false, {}, step.error};
            break;
          }
          if (step.value.suspended) {
            result.suspended_actors.push_back(action.actor_id);
            continue;
          }
          completed = step.value.action_finished;
          if (completed) {
            outcome.success = step.value.success;
            outcome.code = step.value.code;
            TaskEffect effect;
            effect.success = step.value.success;
            effect.task_id = step.value.task_id;
            effect.code = step.value.code;
            outcome.task = std::move(effect);
          }
        } else if (action.kind == "envoy_deliver" ||
                   action.kind == "envoy_report") {
          Outcome<EnvoyTaskStep> step;
          if (w.envoy_tasks.version == 2) {
            if (!w.diplomacy.v2) {
              r = common_failure("MissingDiplomacyV2State");
              break;
            }
            step = step_envoy_task_world_with_reports(w, *w.diplomacy.v2, id,
                                                      dynamic_input);
          } else {
            if (action.kind == "envoy_report") {
              r = common_failure("EnvoyReportVersionRequired");
              break;
            }
            step = step_envoy_task_world(w, id, dynamic_input);
          }
          if (!step.ok) {
            r = {false, {}, step.error};
            break;
          }
          if (step.value.suspended) {
            result.suspended_actors.push_back(action.actor_id);
            continue;
          }
          completed = step.value.action_finished;
          if (completed) {
            outcome.success = step.value.success;
            outcome.code = step.value.code;
            TaskEffect effect;
            effect.success = step.value.success;
            effect.task_id = step.value.task_id;
            effect.code = step.value.code;
            outcome.task = std::move(effect);
          }
        } else if (population_military_command_kind(action.kind)) {
          if (!w.population_military_enabled()) {
            r = common_failure("PopulationMilitaryCapabilityRequired", id,
                               ErrorKind::unsupported_capability);
            break;
          }
          ActionMutation mutation;
          mutation.action = action;
          mutation.remaining_ticks = action.remaining_ticks - 1;
          r = w.common.apply_action(mutation);
          if (!r.ok)
            break;
          --action.remaining_ticks;
          completed = action.remaining_ticks == 0;
          if (completed) {
            auto context = w.population_write(to_tick);
            auto end = finish_population_military(context, action);
            if (!end.ok) {
              r = {false, {}, end.error};
              break;
            }
            outcome.success = end.value.success;
            outcome.code = end.value.code;
          }
        } else if (action.kind == "military_train") {
          auto step = step_military_training(
              w.common, w.military, *w.catalog->military, ma, action, to_tick);
          if (!step.ok) {
            r = {false, {}, step.error};
            break;
          }
          if (step.value.suspended) {
            result.suspended_actors.push_back(action.actor_id);
            continue;
          }
          completed = step.value.completed;
          if (completed) {
            auto current = w.common.action(id);
            if (!current) {
              r = common_failure("UnknownAction", id);
              break;
            }
            auto end = finish_military_training(w.common, w.military,
                                                *w.catalog->military, ma,
                                                *current, to_tick);
            if (!end.ok) {
              r = {false, {}, end.error};
              break;
            }
            outcome.success = end.value.success;
            outcome.code = end.value.code;
            outcome.already_completed = end.value.already_completed;
            outcome.military = std::move(end.value);
          }
        } else if (action.kind == "office_accept" ||
                   action.kind == "army_form" || action.kind == "army_return" ||
                   action.kind == "army_resupply") {
          ActionMutation mutation;
          mutation.action = action;
          mutation.remaining_ticks = action.remaining_ticks - 1;
          r = w.common.apply_action(mutation);
          if (!r.ok)
            break;
          action.remaining_ticks -= 1;
          completed = action.remaining_ticks == 0;
          if (completed) {
            auto end = finish_military_action(w.common, w.military,
                                              *w.catalog->military, ma, action);
            if (!end.ok) {
              r = {false, {}, end.error};
              break;
            }
            if (!end.value.army_id.empty())
              w.common.invalidate({ResourceKey::Kind::army, end.value.army_id});
            outcome.success = end.value.success;
            outcome.already_completed = end.value.already_completed;
            if (!end.value.already_completed)
              outcome.code = end.value.code;
            outcome.military = std::move(end.value);
          }
        } else if (action.kind == "travel") {
          auto allowed = can_step_travel(w.common, w.travel, *w.catalog->travel,
                                         w.services, action);
          if (std::find(effective_hold.begin(), effective_hold.end(),
                        action.actor_id) != effective_hold.end() ||
              !allowed.ok) {
            if (!allowed.ok && allowed.error.kind != ErrorKind::rule) {
              r = allowed;
              break;
            }
            result.suspended_actors.push_back(action.actor_id);
            continue;
          }
          const TravelMotion *motion = nullptr;
          for (const auto &m : contacts.motions)
            if (m.action_id == id) {
              motion = &m;
              break;
            }
          // 舟行仍消费原Travel与实船proof，不生成仅陆军模式可消费的行军条件回执。
          const auto *terrain_params = w.travel.actions.find(id);
          const bool navigation = w.naval_navigation_enabled() &&
                                  terrain_params &&
                                  terrain_params->mode == "boat";
          const bool capture =
              navigation ||
              (terrain_params && terrain_params->mode == "army" &&
               capture_military_march(w.common, w.military,
                                      *w.catalog->military, action.actor_id));
          auto step = step_travel(w.common, w.travel, *w.catalog->travel,
                                  w.services, id, capture, motion);
          if (!step.ok) {
            r = {false, {}, step.error};
            break;
          }
          if (navigation) {
            if (!step.value.execution) {
              r = common_failure("NavalNavigationActualStepRequired", id);
              break;
            }
            if (contacted.count(action.actor_id))
              navigation_executions.push_back(*step.value.execution);
            else {
              auto sailed = step_naval_navigation_world(
                  w, *step.value.execution, dynamic_input);
              if (!sailed.ok) {
                r = {false, {}, sailed.error};
                break;
              }
            }
          } else if (step.value.execution) {
            if (contacted.count(action.actor_id))
              military_executions.push_back(*step.value.execution);
            else {
              auto marched = apply_military_march(
                  w.common, w.military, *w.catalog->military, ma,
                  *step.value.execution, to_tick);
              if (!marched.ok) {
                r = {false, {}, marched.error};
                break;
              }
            }
          }
          completed = step.value.completed && !contacted.count(action.actor_id);
          outcome.success.reset();
          outcome.completed = step.value.completed;
        } else if (growth_action_kind(action.kind)) {
          auto ga = w.services.growth_access(&dynamic_input);
          auto end = step_growth(w.common, w.growth, *w.catalog->growth,
                                 *w.catalog->common, w.social, w.interaction,
                                 ga, action);
          if (!end.ok) {
            r = {false, {}, end.error};
            break;
          }
          completed = end.value.completed;
          if (completed)
            outcome.growth = std::move(end.value.effect);
        } else if (action.kind == "tactic_lesson") {
          if (!w.growth_tactic_lessons_enabled() ||
              action.remaining_ticks <= 0) {
            r = common_failure("TacticLessonActualStartRequired", id);
            break;
          }
          ActionMutation mutation;
          mutation.action = action;
          mutation.remaining_ticks = action.remaining_ticks - 1;
          r = w.common.apply_action(mutation);
          if (!r.ok)
            break; /* 原死亡覆盖及实际付款在clock后本域尾。 */
        } else if (action.kind == "family_child_care_relief") {
          if (!w.family_care_relief_enabled() ||
              !w.child_care_relief.starts.find(id) ||
              action.remaining_ticks <= 0) {
            r = common_failure("ChildCareReliefStartRequired", id);
            break;
          }
          ActionMutation mutation;
          mutation.action = action;
          mutation.remaining_ticks = action.remaining_ticks - 1;
          r = w.common.apply_action(mutation);
          if (!r.ok)
            break; /* 实际死亡源在clock提交后先于本域失败完成。 */
        } else if (action.kind == "family_child_care") {
          if (!w.family_birth_enabled() ||
              !w.family_birth.care_starts.find(id) ||
              action.remaining_ticks <= 0) {
            r = common_failure("ChildCareStartRequired", id);
            break;
          }
          ActionMutation mutation;
          mutation.action = action;
          mutation.remaining_ticks = action.remaining_ticks - 1;
          r = w.common.apply_action(mutation);
          if (!r.ok)
            break; /* 本轮先登记真实死亡，再结照护与释放Action。 */
        } else if (action.kind == "family_adopt") {
          ActionMutation mutation;
          mutation.action = action;
          mutation.remaining_ticks = action.remaining_ticks - 1;
          r = w.common.apply_action(mutation);
          if (!r.ok)
            break;
          if (action.remaining_ticks == 1)
            family_completed.push_back(id);
        } else if (action.kind == "estate_build") {
          ActionMutation mutation;
          mutation.action = action;
          mutation.remaining_ticks = action.remaining_ticks - 1;
          r = w.common.apply_action(mutation);
          if (!r.ok)
            break;
          action.remaining_ticks -= 1;
          completed = action.remaining_ticks == 0;
          if (completed) {
            auto end = finish_estate_build(
                w.common, w.estate, *w.catalog->estate, w.services, action);
            if (!end.ok) {
              r = {false, {}, end.error};
              break;
            }
            outcome.success = end.value.success;
            if (end.value.already_completed)
              outcome.already_completed = true;
            else
              outcome.code = end.value.code;
          }
        } else if (action.kind == task_message_return_collect_command) {
          if (action.remaining_ticks != task_handover_ticks) {
            r = common_failure("TaskMessageReturnActiveActionRequired", id);
            break;
          }
          ActionMutation mutation;
          mutation.action = action;
          mutation.remaining_ticks = 0;
          r = w.common.apply_action(mutation);
          if (!r.ok)
            break;
          auto end = finish_task_message_return_world(w, id, dynamic_input);
          if (!end.ok) {
            r = {false, {}, end.error};
            break;
          }
          outcome.task = std::move(end.value);
          completed = true;
        } else if (action.kind == "accept_offer" ||
                   action.kind == "report_offer" ||
                   action.kind == "deliver_message") {
          ActionMutation mutation;
          mutation.action = action;
          mutation.remaining_ticks = action.remaining_ticks - 1;
          r = w.common.apply_action(mutation);
          if (!r.ok)
            break;
          completed = action.remaining_ticks == 1;
          if (completed) {
            auto takeover = PoliticalTakeoverExecutionWorldAccess::create(
                w, w.dynamic_enabled() ? &dynamic_input : nullptr);
            if (!takeover.ok) {
              r = {false, {}, takeover.error};
              break;
            }
            auto notice = make_task_message_notice_services(
                w, w.dynamic_enabled() ? &dynamic_input : nullptr);
            if (!notice.ok) {
              r = {false, {}, notice.error};
              break;
            }
            auto returns =
                TaskMessageReturnWorldAccess::create(w, &dynamic_input);
            if (!returns.ok) {
              r = {false, {}, returns.error};
              break;
            }
            auto personal =
                TaskPersonalMessageWorldAccess::create(w, &dynamic_input);
            if (!personal.ok) {
              r = {false, {}, personal.error};
              break;
            }
            auto reports =
                TaskPersonalReportWorldAccess::create(w, &dynamic_input);
            if (!reports.ok) {
              r = {false, {}, reports.error};
              break;
            }
            auto acceptances =
                TaskPersonalAcceptanceWorldAccess::create(w, &dynamic_input);
            if (!acceptances.ok) {
              r = {false, {}, acceptances.error};
              break;
            }
            auto end = finish_task_action(
                w.common, w.tasks, w.social, *w.catalog->tasks, w.interaction,
                *w.catalog->interaction, ia, action, to_tick,
                w.dynamic_enabled() ? &w.affairs : nullptr,
                takeover.value->tasks(),
                notice.value ? &*notice.value : nullptr,
                returns.value ? &returns.value->services() : nullptr,
                personal.value ? &personal.value->services() : nullptr,
                reports.value ? &reports.value->services() : nullptr,
                acceptances.value ? &acceptances.value->services() : nullptr);
            if (!end.ok) {
              r = {false, {}, end.error};
              break;
            }
            outcome.task = std::move(end.value);
          }
        } else if (action.kind == "wait" || action.kind == "rescue") {
          ActionMutation mutation;
          mutation.action = action;
          mutation.remaining_ticks = action.remaining_ticks - 1;
          r = w.common.apply_action(mutation);
          if (!r.ok)
            break;
          completed = action.remaining_ticks == 1;
          if (completed && action.kind == "rescue") {
            r = finish_rescue(w.common, w.travel, *w.catalog->travel, id);
            if (!r.ok)
              break;
          }
        } else {
          r = common_failure("UnsupportedNativeCapability", id,
                             ErrorKind::unsupported_capability);
          r.error.command = action.kind;
          break;
        }
        if (completed) {
          if (outcome.military && outcome.military->success) {
            r = apply_military_actor_changes_from_sources(
                w.common, w.military, *w.catalog->military, ma,
                outcome.military->actor_changes,
                outcome.military->casualty_source_ids);
            if (!r.ok)
              break;
            if (action.kind == "army_return") {
              outcome.military_field_order = {"ok", "receipt_id", "army_id",
                                              "order_id", "actor_changes"};
              military_returns.push_back(
                  {outcome.military->order_id, outcome.military->receipt_id});
            }
          }
          result.completed_actions.insert(id, std::move(outcome));
          r = release_action(w, id);
          if (!r.ok)
            break;
        }
      }
      if (r.ok && w.dynamic_enabled()) {
        auto activities = step_organization_activities(
            w.common, w.organization, *w.catalog->organization, w.local_economy,
            w.services, w.services, dynamic_input);
        if (!activities.ok)
          r = {false, {}, activities.error};
      }
      break;
    }
    case Phase::contact_install: {
      // 货物处置恢复仍借真实路遇锚点，只保留实际manifest引用的历史来源。
      std::vector<StableId> retained_encounters;
      for (const auto &exposure : w.tasks.cargo_exposures.rows())
        retained_encounters.push_back(exposure.second.encounter_id);
      // 无政策首领仍可能带有政策随从；保留真实leaders才能证明该场未授权货损。
      for (const auto &entry : w.personal.battles.rows()) {
        const auto &battle = entry.second;
        if (battle.source.kind != "encounter")
          continue;
        bool policy_participant = false;
        for (const auto &unit : battle.units.rows())
          if (unit.second.side == 1) {
            const auto *actor = w.common.state().actors.find(unit.first);
            if (!actor)
              continue;
            const auto *terms = w.catalog->personal->road_conflicts.find(
                actor->prototype_id.value_or(actor->id));
            if (terms && terms->cargo_policy) {
              policy_participant = true;
              break;
            }
          }
        if (policy_participant &&
            std::find(retained_encounters.begin(), retained_encounters.end(),
                      battle.source.encounter_id) == retained_encounters.end())
          retained_encounters.push_back(battle.source.encounter_id);
      }

      for (const auto &hit : contacts.ordered_hits) {
        EncounterInstall install;
        install.leaders = {hit.a, hit.b};
        install.action_ids = {hit.first.trace.action_id,
                              hit.second.trace.action_id};
        install.anchors = {Location{hit.first.anchor},
                           Location{hit.second.anchor}};
        install.resume_paused = {hit.first.trace.params.paused,
                                 hit.second.trace.params.paused};
        if (hit.road) {
          install.action_ids = {hit.road->first.trace.action_id,
                                hit.road->second.trace.action_id};
          install.anchors = {Location{hit.road->first.anchor},
                             Location{hit.road->second.anchor}};
          install.resume_paused = {hit.road->first.trace.params.paused,
                                   hit.road->second.trace.params.paused};
          // 原道路锚点先按轨迹输入序构造，再排序leaders；保留内层字典与外层人物键的各自顺序。
          for (auto &order : install.anchor_field_orders)
            order = {"kind", "edge_id", "origin", "progress"};
          install.anchors_order = {hit.a, hit.b};
          if (hit.road->first.trace.action_id >
              hit.road->second.trace.action_id)
            std::swap(install.anchors_order[0], install.anchors_order[1]);
        }
        install.apply_motion = [&w, &hit](const StableId &id,
                                          bool interactive) {
          return apply_contact_truncation(w.common, w.travel, hit, id,
                                          interactive);
        };
        auto installed = install_encounter(
            w.common, w.interaction, w.common.state().metadata.player_actor_id,
            install, retained_encounters);
        if (!installed.ok) {
          r = {false, {}, installed.error};
          break;
        }
      }
      if (!r.ok)
        break;
      // 读口和回调只活在本次接敌栈，不让后续阶段持有局部引用。
      MilitaryCommandedWorldRead commanded_skills(w, dynamic_input);
      auto contact_access = ma;
      MilitaryTacticsWorldRead tactics_skills(w, dynamic_input);
      if (w.military_commanded_damage_enabled())
        contact_access.prepare_commanded_damage =
            [&w, &commanded_skills,
             &dynamic_input](const MilitaryBattleRecord &battle) {
              return prepare_military_commanded_damage_entry(
                  w.common, w.military, *w.catalog->military, battle,
                  commanded_skills, dynamic_input);
            };
      if (w.military_command_resources_enabled())
        contact_access.prepare_command_resources =
            [&w, &tactics_skills,
             &dynamic_input](const MilitaryBattleRecord &battle) {
              return prepare_military_command_resources_entry(
                  w.common, w.military, *w.catalog->military, battle,
                  *w.catalog->military->command_resources_policy,
                  tactics_skills, dynamic_input);
            };
      if (w.military_tactic_learning_enabled())
        contact_access.prepare_learned_tactics =
            [&w, &dynamic_input](const MilitaryBattleRecord &battle) {
              return prepare_military_tactic_entry(
                  w.common, *w.catalog->common, w.military,
                  *w.catalog->military, w.military_tactic_learning, battle,
                  dynamic_input);
            };
      for (const auto &hit : field_contacts) {
        if (!w.field_battle || !w.catalog->field_battle) {
          r = common_failure("FieldBattleSelectedStatePairRequired");
          break;
        }
        if (!w.catalog->common || !w.catalog->family) {
          r = common_failure("FieldOriginalGrowthHistorySourcesRequired");
          break;
        }
        const FieldBattleAbilityReadContext abilities{
            *w.catalog->common,
            *w.catalog->military,
            w.growth,
            GrowthNavalHistoryContext{
                *w.catalog->common, *w.catalog->family, w.family,
                w.family_birth_enabled() ? &w.family_birth : nullptr},
            &w.military_tactic_learning,
            &dynamic_input};
        auto installed = install_field_road_session(
            w.common, w.military, *w.catalog->military, w.travel,
            *w.catalog->travel, *w.catalog->field_battle, *w.field_battle, hit,
            w.common.state().metadata.player_actor_id, ma.battle_seed, w.social,
            w.organization, abilities, w.checkpoint_copy_memory);
        if (!installed.ok) {
          r = {false, {}, installed.error};
          break;
        }
      }
      if (!r.ok)
        break;
      if (w.catalog->field_battle && !military_contacts.ordered_hits.empty()) {
        r = common_failure("NOT_IMPLEMENTED", "field_battle/contact_install",
                           ErrorKind::unsupported_capability);
        break;
      }
      for (const auto &hit : military_contacts.ordered_hits) {
        auto installed = install_military_contact(
            w.common, w.military, *w.catalog->military, contact_access,
            w.travel, hit, military_executions);
        if (!installed.ok) {
          r = {false, {}, installed.error};
          break;
        }
      }
      if (!r.ok)
        break;
      const auto naval_geometry = naval_history_access(w);
      NavalGunneryWorldRead gunnery_skills(w, dynamic_input);
      for (const auto &hit : naval_contacts.ordered_hits) {
        auto installed = install_naval_contact(
            w.common, w.military, *w.catalog->military, ma, w.travel, w.naval,
            *w.catalog->naval, hit,
            w.naval_repairs_enabled() ? &dynamic_input : nullptr,
            w.naval_navigation_enabled() ? &navigation_executions : nullptr,
            w.naval_navigation_enabled() ? &naval_geometry : nullptr,
            w.naval_gunnery_enabled() ? &gunnery_skills : nullptr);
        if (!installed.ok) {
          r = {false, {}, installed.error};
          break;
        }
      }
      if (r.ok)
        r = release_disabled_naval_journeys(w, dynamic_input);
      break;
    }
    case Phase::social_step: {
      auto step =
          step_social(w.common, w.social, *w.catalog->social,
                      *w.catalog->common, w.services, to_tick, &w.services);
      r = {step.ok, {}, step.error};
      break;
    }
    case Phase::siege_results: {
      if (w.catalog->field_battle && !sieges.battle_requests.empty()) {
        r = common_failure("NOT_IMPLEMENTED", "field_battle/siege_entry",
                           ErrorKind::unsupported_capability);
        break;
      }
      for (const auto &entry : sieges.battle_requests) {
        auto made =
            create_siege_battle(w.common, w.military, *w.catalog->military, ma,
                                entry.siege_id, entry.request, to_tick);
        if (!made.ok) {
          r = {false, {}, made.error};
          break;
        }
      }
      if (!r.ok)
        break;
      OrderedTable<std::vector<SiegeEvent>> grouped;
      for (const auto &entry : sieges.events) {
        auto *rows = grouped.find(entry.siege_id);
        if (!rows) {
          grouped.insert(entry.siege_id, {});
          rows = grouped.find(entry.siege_id);
        }
        rows->push_back(entry.event);
      }
      for (const auto &entry : grouped.rows()) {
        auto settled =
            finish_siege_events(w.common, w.military, *w.catalog->military, ma,
                                entry.first, entry.second, to_tick);
        if (!settled.ok) {
          r = {false, {}, settled.error};
          break;
        }
        completed_receipts.insert(completed_receipts.end(),
                                  settled.value.completed_receipt_ids.begin(),
                                  settled.value.completed_receipt_ids.end());
        siege_journeys.insert(siege_journeys.end(),
                              settled.value.journey_requests.begin(),
                              settled.value.journey_requests.end());
        r = apply_military_actor_changes_from_sources(
            w.common, w.military, *w.catalog->military, ma,
            settled.value.actor_changes, settled.value.casualty_source_ids);
        if (!r.ok)
          break;
      }
      break;
    }
    case Phase::campaign_step: {
      auto step =
          step_military_campaigns(w.common, w.military, *w.catalog->military,
                                  ma, to_tick, completed_receipts);
      if (!step.ok) {
        r = {false, {}, step.error};
        break;
      }
      journeys = std::move(step.value);
      break;
    }
    case Phase::release_invalid_commands: {
      auto engineering_access = ma;
      if (w.military_siege_engineering_enabled())
        engineering_access.siege_engineering =
            make_military_siege_engineering_access(w, &dynamic_input);
      r = release_invalid_military_commands(w.common, w.military,
                                            *w.catalog->military,
                                            engineering_access, to_tick);
      if (r.ok)
        journeys.insert(journeys.end(), siege_journeys.begin(),
                        siege_journeys.end());
      break;
    }
    case Phase::clock_commit:
      w.common.state().identity.tick = to_tick;
      r = reconcile_private_shop_daily_rent_world_in_candidate(w,
                                                               dynamic_input);
      break;
    case Phase::military_return_complete:
      r = complete_military_returns(w.common, w.military, *w.catalog->military,
                                    ma, military_returns);
      break;
    case Phase::family_complete: {
      auto family_sources = NativeFamilyInitialSourcesLease::create(w);
      if (!family_sources.ok) {
        r = {false, {}, std::move(family_sources.error)};
        break;
      }
      auto fa = w.services.family_access(
          ia, family_sources.value ? &family_sources.value->origins() : nullptr,
          family_sources.value ? &family_sources.value->initial_age()
                               : nullptr);
      for (const auto &id : family_completed) {
        auto *action = w.common.action(id);
        if (!action) {
          r = common_failure("UnknownAction", id);
          break;
        }
        auto end = finish_family_action(
            w.common, w.family, w.interaction, w.social, *w.catalog->family,
            *w.catalog->common, w.estate, *w.catalog->estate, fa, *action);
        if (!end.ok) {
          r = {false, {}, end.error};
          break;
        }
        ActionCompletion outcome;
        outcome.family = std::move(end.value);
        result.completed_actions.insert(id, std::move(outcome));
        r = release_action(w, id);
        if (!r.ok)
          break;
      }
      break;
    }
    }
    timed(w, phase_names[static_cast<std::size_t>(phase)], usec(phase_start));
    if (!r.ok)
      return {false, {}, phase_error(std::move(r), phase).error};
  }
  const auto tail_start = PhaseClock::now();
  std::vector<std::pair<StableId, std::shared_ptr<const ActionCompletion>>>
      completed;
  for (const auto &row : result.completed_actions.rows())
    completed.emplace_back(
        row.first, checkpoint_make_shared<const ActionCompletion>(row.second));
  auto r = resume_completed_dialogues(w.common, w.interaction,
                                      *w.catalog->interaction, ia, completed);
  if (!r.ok)
    return {false, {}, r.error};
  r = enter_changed_places(w.common, w.interaction, *w.catalog->interaction, ia,
                           old_places);
  if (!r.ok)
    return {false, {}, r.error};
  for (const auto &request : journeys) {
    r = begin_requested_military_journey(w, request);
    if (!r.ok)
      return {false, {}, r.error};
  }
  r = finish_travel_arrivals(w.common, w.travel, w.services);
  if (!r.ok)
    return {false, {}, r.error};
  if (w.population_military_enabled()) {
    auto context = w.population_write();
    auto reconciled = reconcile_population_military(context, w.common.tick());
    if (!reconciled.ok)
      return {false, {}, reconciled.error};
  }
  r = settle_political_fiscal_civil_world(w, dynamic_input, fiscal_effects);
  if (!r.ok)
    return {false, {}, r.error};
  r = settle_governance_task_world(w, dynamic_input);
  if (!r.ok)
    return {false, {}, r.error};
  // 接受后自行行旅仍占原交通行动；这里仅维护没有交接Action的使命责任。
  r = w.envoy_tasks_enabled() && w.envoy_tasks.version == 2
          ? reconcile_envoy_task_world_with_reports(w, dynamic_input)
          : reconcile_envoy_task_world(w, dynamic_input);
  if (!r.ok)
    return {false, {}, r.error};
  if (w.dynamic_enabled() && w.catalog->affairs->fiscal_need_policies) {
    if (!w.fiscal_settlement_enabled())
      return {false, {}, common_failure("FiscalNeedsRequireSettlement").error};
    auto legacy_sources = fiscal_effects.fact_ids;
    if (w.occupation_fiscal_enabled() && !w.occupation_fiscal_needs_enabled()) {
      if (!w.catalog->occupation_fiscal_settlement_enabled)
        return {
            false, {}, common_failure("OccupationFiscalProfileMismatch").error};
      auto routed = legacy_fiscal_need_sources(w.common.state().facts,
                                               w.occupation_fiscal_settlement,
                                               legacy_sources);
      if (!routed.ok)
        return {false, {}, routed.error};
      legacy_sources = std::move(routed.value);
    }
    if (w.occupation_fiscal_needs_enabled())
      r = reconcile_fiscal_needs_in_candidate(
          w.common, w.affairs, w.local_economy, w.fiscal_settlement,
          *w.catalog->fiscal_settlement,
          *w.catalog->affairs->fiscal_need_policies, legacy_sources,
          w.occupation_fiscal_settlement);
    else
      r = reconcile_fiscal_needs_in_candidate(
          w.common, w.affairs, w.local_economy, w.fiscal_settlement,
          *w.catalog->fiscal_settlement,
          *w.catalog->affairs->fiscal_need_policies, legacy_sources);
    if (!r.ok)
      return {false, {}, r.error};
  }
  auto takeover = PoliticalTakeoverExecutionWorldAccess::create(w);
  if (!takeover.ok)
    return {false, {}, takeover.error};
  if (w.dynamic_enabled() && w.catalog->affairs->cohort_wage_need_policies) {
    r = reconcile_cohort_wage_needs_in_candidate(
        w.common, w.affairs, w.local_economy, *w.catalog->local_economy,
        *w.catalog->affairs->cohort_wage_need_policies,
        economy_effects.fact_ids, takeover.value->economy(),
        takeover.value->economy() ? &w.economic_takeover_execution : nullptr);
    if (!r.ok)
      return {false, {}, r.error};
  }
  r = finish_dynamic_boundary(w, dynamic_input);
  if (!r.ok)
    return {false, {}, r.error};
  r = run_naval_settlement_decisions(w);
  if (!r.ok)
    return {false, {}, r.error};
  // 只消费本advance追加的原舟战终止；旧同tick源不能重放，失败完成不冒Travel到达。
  if (w.naval_combat_enabled() || field_losses) {
    const auto &order = w.common.state().facts.order();
    if (order.size() < naval_fact_prefix.size() ||
        !std::equal(naval_fact_prefix.begin(), naval_fact_prefix.end(),
                    order.begin()))
      return {false,
              {},
              common_failure("NavalTerminationFactBoundaryChanged").error};
    std::vector<std::pair<StableId, std::shared_ptr<const ActionCompletion>>>
        interrupted;
    for (std::size_t i = naval_fact_prefix.size(); i < order.size(); ++i) {
      const auto *source = w.common.state().facts.find(order[i]);
      if (field_losses && source && source->domain == "military" &&
          source->kind == "military_casualties") {
        const MilitaryCasualtyFact *casualty = nullptr;
        for (const auto &f : w.military.casualty_facts)
          if (f.id == source->id)
            casualty = &f;
        const auto *round =
            casualty ? w.common.state().facts.find(casualty->battle_id)
                     : nullptr;
        if (round && round->domain == "military" &&
            round->kind == "military_battle") {
          auto stops = query_military_journey_terminations(
              w.common, w.military, *w.catalog->military, source->id);
          if (!stops.ok)
            return {false, {}, stops.error};
          for (const auto &stop : stops.value) {
            if (stop.tick != to_tick ||
                result.completed_actions.find(stop.action_id) ||
                w.common.action(stop.action_id))
              return {false,
                      {},
                      common_failure("MilitaryTerminationCompletionMismatch",
                                     stop.action_id)
                          .error};
            ActionCompletion completion;
            completion.completed = true;
            completion.success = false;
            completion.code = stop.reason;
            result.completed_actions.insert(stop.action_id, completion);
            interrupted.emplace_back(
                stop.action_id, checkpoint_make_shared<const ActionCompletion>(
                                    std::move(completion)));
          }
        }
        continue;
      }
      if (source && source->domain == "military_naval" &&
          source->kind == "naval_navigation_step") {
        const NavalNavigationStepFact *step = nullptr;
        for (const auto &row : w.naval.navigation.steps)
          if (row.id == source->id)
            step = &row;
        if (!step)
          return {false,
                  {},
                  common_failure("NavalNavigationSourceRequired", source->id)
                      .error};
        auto read = query_naval_navigation_termination(
            w.common, w.naval.navigation, step->id, step->movement.action_id);
        if (!read.ok)
          return {false, {}, read.error};
        if (read.value) {
          const auto &done = *read.value;
          if (done.tick != to_tick || w.common.action(done.action_id) ||
              result.completed_actions.find(done.action_id))
            return {false,
                    {},
                    common_failure("NavalNavigationTerminationMismatch",
                                   done.action_id)
                        .error};
          ActionCompletion completion;
          completion.completed = true;
          completion.success = false;
          completion.code = done.reason;
          result.completed_actions.insert(done.action_id, completion);
          interrupted.emplace_back(
              done.action_id, checkpoint_make_shared<const ActionCompletion>(
                                  std::move(completion)));
        }
        continue;
      }
      if (source && source->domain == "military_naval" &&
          source->kind == "naval_repair_result") {
        if (!w.naval_repairs_enabled())
          return {
              false, {}, common_failure("NavalRepairProfileRequired").error};
        const NavalRepairResult *repair = nullptr;
        for (const auto &row : w.naval.repairs.results)
          if (row.id == source->id)
            repair = &row;
        if (!repair)
          return {
              false,
              {},
              common_failure("NavalRepairCompletionSourceRequired", source->id)
                  .error};
        auto checked = query_naval_repair_completion(
            w.common, w.naval, *w.catalog->naval, repair->id, repair->action_id,
            repair->actor_id);
        if (!checked.ok)
          return {false, {}, checked.error};
        const auto &done = checked.value;
        if (done.tick != to_tick ||
            result.completed_actions.find(done.action_id) ||
            w.common.action(done.action_id))
          return {
              false,
              {},
              common_failure("NavalRepairCompletionMismatch", done.action_id)
                  .error};
        ActionCompletion completion;
        completion.completed = true;
        completion.success = done.success;
        completion.code = done.reason;
        result.completed_actions.insert(done.action_id, completion);
        interrupted.emplace_back(done.action_id,
                                 checkpoint_make_shared<const ActionCompletion>(
                                     std::move(completion)));
        continue;
      }
      if (!source || source->domain != "military_naval" ||
          (source->kind != "naval_round" && source->kind != "naval_capture" &&
           source->kind != "naval_continue"))
        continue;
      auto stops = query_naval_journey_terminations(w.common, w.military,
                                                    w.naval, source->id);
      if (!stops.ok)
        return {false, {}, stops.error};
      for (const auto &stop : stops.value) {
        if (stop.tick != to_tick ||
            result.completed_actions.find(stop.action_id) ||
            w.travel.actions.find(stop.action_id))
          return {false,
                  {},
                  common_failure("NavalTerminationCompletionMismatch",
                                 stop.action_id)
                      .error};
        ActionCompletion completion;
        completion.completed = true;
        completion.success = false;
        completion.code = stop.reason;
        result.completed_actions.insert(stop.action_id, completion);
        interrupted.emplace_back(stop.action_id,
                                 checkpoint_make_shared<const ActionCompletion>(
                                     std::move(completion)));
      }
    }
    r = resume_completed_dialogues(w.common, w.interaction,
                                   *w.catalog->interaction, ia, interrupted);
    if (!r.ok)
      return {false, {}, r.error};
  }
  auto trips = travel_schedule_proposals(w.common, w.travel, *w.catalog->travel,
                                         w.services);
  if (!trips.ok)
    return {false, {}, trips.error};
  for (const auto &trip : trips.value) {
    if (director_routes_actor(w, trip.actor_id))
      continue;
    bool applied = false;
    CommandPayload command{
        "travel", TravelCommandPayload{TravelCommand{trip.destination, {}}}};
    r = try_npc_command(w, trip.actor_id, command, applied);
    if (!r.ok)
      return {false, {}, r.error};
    if (applied) {
      r = mark_travel_departure(w.travel, trip);
      if (!r.ok)
        return {false, {}, r.error};
    }
  }
  r = run_siege_integration_decisions(w);
  if (!r.ok)
    return {false, {}, r.error};
  r = run_naval_repair_decisions(w);
  if (!r.ok)
    return {false, {}, r.error};
  auto proposals = military_schedule_proposals(
      w.common, w.military, *w.catalog->military, *w.catalog->common,
      *w.catalog->travel, ia, w.common.state().metadata.player_actor_id, ma,
      w.naval_enabled() ? &w.naval : nullptr,
      w.naval_enabled() ? w.catalog->naval.get() : nullptr,
      w.naval_combat_enabled(),
      w.naval_combat_enabled() ? w.catalog->naval.get() : nullptr);
  if (!proposals.ok)
    return {false, {}, proposals.error};
  for (const auto &proposal : proposals.value) {
    if (director_routes_actor(w, proposal.actor_id))
      continue;
    CommandPayload command;
    std::visit(
        [&](const auto &p) {
          using T = std::decay_t<decltype(p)>;
          command.data = p;
          if constexpr (std::is_same_v<T, MilitaryCommand>)
            command.kind = military_kind(p);
          else if constexpr (std::is_same_v<T, NavalCommand>)
            command.kind = std::holds_alternative<NavalDisband>(p)
                               ? "naval_disband"
                               : "naval_form";
          else
            command.kind = travel_kind(p);
        },
        proposal.command);
    if (const auto *military = std::get_if<MilitaryCommand>(&command.data))
      if (const auto *form = std::get_if<ArmyForm>(military))
        if (auto enrollment =
                siege_population_enrollment(w, proposal.actor_id, *form))
          command = std::move(*enrollment);
    bool applied = false;
    r = try_npc_command(w, proposal.actor_id, command, applied);
    if (!r.ok)
      return {false, {}, r.error};
  }
  r = run_dynamic_decisions(w, w.common.tick());
  if (!r.ok)
    return {false, {}, r.error};
  // 原有半日与末尾事务完成后，沿已经验证的军事/海军原史登记终点；本阶段不转产或切人。
  if (w.family_life_enabled()) {
    auto death_access = w.services.military_access();
    r = validate_military_state(w.common, w.military, *w.catalog->military,
                                death_access);
    if (!r.ok)
      return {false, {}, r.error};
    if (w.naval_enabled()) {
      r = w.naval_gunnery_enabled()
              ? validate_naval_battle_structure_for_gunnery(
                    w.common, w.military, *w.catalog->military, death_access,
                    w.travel, w.naval, *w.catalog->naval,
                    naval_history_access(w))
          : w.naval_combat_enabled()
              ? validate_naval_battle_history(
                    w.common, w.military, *w.catalog->military, death_access,
                    w.travel, w.naval, *w.catalog->naval,
                    naval_history_access(w))
              : validate_naval_world(w.common, w.military, *w.catalog->military,
                                     death_access, w.naval, *w.catalog->naval);
      if (!r.ok)
        return {false, {}, r.error};
    }
    auto deaths = record_verified_family_deaths(w.common, w.family_life,
                                                w.family_death_read());
    if (!deaths.ok)
      return {false, {}, deaths.error};
  }
  r = finish_growth_tactic_lessons_world(w, dynamic_input, result);
  if (!r.ok)
    return {false, {}, r.error};
  r = finish_estate_employment_world(w);
  if (!r.ok)
    return {false, {}, r.error};
  r = finish_private_shop_boundary_world(w);
  if (!r.ok)
    return {false, {}, r.error};
  r = finish_family_birth_world(w, result);
  if (!r.ok)
    return {false, {}, r.error};
  r = finish_child_care_relief_world(w, result);
  if (!r.ok)
    return {false, {}, r.error};
  r = complete_growth_recovery_advance_world(w, recovery_closure_prefix,
                                             result);
  if (!r.ok)
    return {false, {}, r.error};
  timed(w, "advance_tail", usec(tail_start));
  return {true, std::move(result), {}};
}
} // namespace hanmo::resident
