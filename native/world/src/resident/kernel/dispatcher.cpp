#include "common_action_occupation.hpp"
#include "complete_growth_command_route.hpp"
#include "dispatcher_exact_bindings.hpp"
#include "exact_complete_world.hpp"
#include "exact_time_world.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "native_control_command_route.hpp"
#include "native_field_army_exit_command_route.hpp"
#include "native_deferred_arcana_absence.hpp"
#include "native_face_to_face_sources.hpp"
#include "native_family_initial_sources.hpp"
#include "native_field_action_start.hpp"
#include "native_field_military_command_route.hpp"
#include "native_growth_market_sources.hpp"
#include "native_local_move_world.hpp"
#include "native_interaction_world.hpp"
#include "native_military_battle_lifecycle.hpp"
#include "native_military_custody_command.hpp"
#include "native_work_lifecycle_command_route.hpp"
#include "naval_navigation_integration.hpp"
#include "political_governance_agency_intent_world.hpp"
#include "political_office_resignation_response_world.hpp"
#include "political_subordinate_appointment_director_world.hpp"
#include "private_shop_liquidation_integration.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/domains/family/native_empty_care_sources.hpp"
#include "resident/domains/growth/growth_started_participation.hpp"
#include "resident/domains/growth/study_service_sources.hpp"
#include "resident/domains/interaction/appointments/personal_appointment_world.hpp"
#include "resident/domains/interaction/born_dialogues/born_personal_dialogue_world.hpp"
#include "resident/domains/interaction/contact_inquiries/contact_inquiry_world.hpp"
#include "resident/domains/interaction/personal_dialogues/known_personal_dialogue_world.hpp"
#include "resident/domains/interaction/visit_appointments/visit_appointment_world.hpp"
#include "resident/domains/interaction/visit_messages/visit_message_world.hpp"
#include "resident/domains/interaction/visit_replies/visit_reply_world.hpp"
#include "resident/domains/military/military_battle_lifecycle.hpp"
#include "resident/domains/tasks/extensions/task_extension_world.hpp"
#include "resident/domains/tasks/message_notices/task_message_notice_world.hpp"
#include "resident/domains/tasks/message_returns/task_message_return_boundary.hpp"
#include "resident/domains/tasks/message_returns/task_message_return_world.hpp"
#include "resident/domains/tasks/personal_acceptances/task_personal_acceptance_boundary.hpp"
#include "resident/domains/tasks/personal_acceptances/task_personal_acceptance_world.hpp"
#include "resident/domains/tasks/personal_cancellations/task_personal_cancellation_boundary.hpp"
#include "resident/domains/tasks/personal_cancellations/task_personal_cancellation_world.hpp"
#include "resident/domains/tasks/personal_messages/task_personal_message_boundary.hpp"
#include "resident/domains/tasks/personal_messages/task_personal_message_world.hpp"
#include "resident/domains/tasks/personal_reports/task_personal_report_boundary.hpp"
#include "resident/domains/tasks/personal_reports/task_personal_report_world.hpp"
#include "resident/domains/travel/movement_command.hpp"
#include "resident/storage/resource_execution_coverage.hpp"
#include "world_read_stamp.hpp"
// 原公共守卫和领域调用次序的唯一组合；业务算法在域内，中央只分派和管理行动生命周期。
#include "dynamic_integration.hpp"
#include "interaction_integration.hpp"
#include "named_membership.hpp"
#include "naval_integration.hpp"
#include "occupation_fiscal_integration.hpp"
#include "organization_grant_route.hpp"
#include "personal_integration.hpp"
#include "player_plan_integration.hpp"
#include "political_takeover_execution_world.hpp"
#include "private_shop_daily_rent_integration.hpp"
#include "private_shop_integration.hpp"
#include "resident/domains/interaction/interaction_phases.hpp"
#include "resident/domains/military/battle_world.hpp"
#include "resident/domains/military/military_clock.hpp"
#include "resident/domains/military/military_phases.hpp"
#include "resident/domains/military/naval_journey_termination.hpp"
#include "resident/domains/military/naval_repair.hpp"
#include "resident/domains/military/naval_repair_history.hpp"
#include "resident/domains/military/siege_world.hpp"
#include "resident/domains/tasks/envoy_task_cancellation.hpp"
#include "resident/domains/tasks/governance_task_rules.hpp"
#include "runtime_task_response_integration.hpp"
#include "siege_integration.hpp"
#include "task_advance_integration.hpp"
#include "treaty_passage_integration.hpp"
#include "world_bundle.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {
namespace {
template <class T> Outcome<T> failed(const Status &r) {
  return {false, {}, r.error};
}
using Prepared = Outcome<std::shared_ptr<const PreviewResult>>;
Prepared reject(const StableId &code) {
  return failed<std::shared_ptr<const PreviewResult>>(common_failure(code));
}
bool complete_motion(const WorldBundle &w) {
  return w.catalog->exact_time &&
         exact_complete_selected(*w.catalog->exact_time);
}
Status begin_selected_travel(WorldBundle &w, const StableId &action,
                             const TravelQuote &quote,
                             const TravelCommand *original = nullptr) {
  if (!complete_motion(w))
    return begin_travel(w.common, w.travel, *w.catalog->travel, action, quote,
                        original);
  auto *writes = w.active_exact_write_context();
  if (!writes || !w.common.actor_condition_capture_identity() ||
      !w.movement_exact)
    return common_failure("ExactTravelActualCaptureRequired", action);
  WorldCurrentMomentRead clock(w);
  auto begun = begin_exact_personal_movement_in_candidate(
      w.common, w.travel, *w.movement_exact, *w.catalog->travel, w.services,
      clock, action, quote, original, writes->spatial_location_writes);
  w.note_write_boundary();
  if (!begun.ok)
    return {false, {}, std::move(begun.error)};
  return consume_complete_growth_spatial_inputs_in_candidate(w, *writes);
}
bool is_control(const StableId &k) {
  return k == "pause_travel" || k == "resume_travel" || k == "travel_wait" ||
         k == "reroute" || k == "return";
}
bool is_dialogue(const StableId &k) {
  return k == "dialogue_next" || k == "dialogue_choose" ||
         k == "end_interaction";
}
// 基础命令的外名与实际载荷必须绑定；typed入口和NPC不能依赖冷decoder代验。
// 显式穷尽Travel载荷；新增变体必须补具名映射，不能默认借用travel。
struct BasicTravelCommandKind {
  const char *operator()(const TravelCommand &) const { return "travel"; }
  const char *operator()(const PauseTravelCommand &) const {
    return "pause_travel";
  }
  const char *operator()(const ResumeTravelCommand &) const {
    return "resume_travel";
  }
  const char *operator()(const TravelWaitCommand &) const {
    return "travel_wait";
  }
  const char *operator()(const RerouteCommand &) const { return "reroute"; }
  const char *operator()(const ReturnTravelCommand &) const { return "return"; }
  const char *operator()(const RescueCommand &) const { return "rescue"; }
  const char *operator()(const MoveLocalCommand &) const {
    return "move_local";
  }
};
bool mismatched_basic_command_binding(const CommandPayload &command) {
  if (std::holds_alternative<AdvanceCommand>(command.data))
    return command.kind != "advance";
  if (std::holds_alternative<WaitCommand>(command.data))
    return command.kind != "wait";
  if (const auto *travel = std::get_if<TravelCommandPayload>(&command.data))
    return command.kind != std::visit(BasicTravelCommandKind{}, *travel);
  // 非基础载荷也不能借基础外名；其余领域仍由各自原有具名守卫处理。
  return command.kind == "advance" || command.kind == "wait" ||
         command.kind == "travel" || is_control(command.kind) ||
         command.kind == "rescue" || command.kind == "move_local";
}
Prepared ready(const WorldBundle &w, const StableId &actor,
               const CommandPayload &command, QuoteData value) {
  auto op = std::make_shared<PreparedDomainOp>();
  op->command = command;
  op->quote = {command.kind, std::move(value)};
  auto result = std::make_shared<PreviewResult>();
  result->source = w.common.state().identity;
  result->actor_id = actor;
  result->kind = command.kind;
  result->operation = op;
  result->quote = std::make_shared<const QuoteDetails>(op->quote);
  return {true, std::move(result), {}};
}
} // namespace
// 此门不消费params，供冷适配器保留旧人物/占用首错；早门的领域语义在prepare继续处理。
Status command_common_guard(const WorldBundle &w, const StableId &id,
                            const StableId &kind) {
  const auto *actor = w.common.actor(id);
  if (!actor)
    return common_failure("UnknownActor");
  auto custody = guard_native_military_custody_common_kind(w, id, kind);
  if (!custody.ok)
    return {false, {}, std::move(custody.error)};
  auto field = w.services.field_battle_bound(id);
  if (!field.ok)
    return {false, {}, field.error};
  if (field.value && !custody.value && !native_field_army_exit_command_kind(kind))
    return common_failure("FieldBattlePending", id);
  if (native_control_cancellation_command_kind(kind))
    return guard_native_control_cancellation(w, id);
  // 唯一死亡后控制选择不执行死者业务；此早门核真实终点，后续prepare再核具体本人许可。
  if (kind == "family_legacy_activate") {
    if (!w.family_legacy_enabled() || !w.family_life_enabled())
      return common_failure("LegacyControlConsentProfileRequired");
    if (id != w.common.state().metadata.player_actor_id || actor->alive)
      return common_failure("LegacyNotCurrentPredecessor");
    const auto *death = family_death_for_actor(w.family_life, id);
    if (!death)
      return common_failure("FamilyDeathSourceUnavailable");
    return validate_family_death_record(w.common, *death,
                                        w.family_death_read());
  }
  // 本人生命终点先于所有零时/战斗/计划早门；不让旧报价绕过死亡确认边界。
  if (w.family_life_enabled()) {
    auto life = guard_family_death_player_command(w.common,
                                                  *w.catalog->family_life, id);
    if (!life.ok)
      return life;
  }
  // 选口不发一般人物动作；仍经过上方原看管/死亡门，具体主将由原领域预览核。
  if (native_field_army_exit_command_kind(kind))
    return guard_native_field_army_exit_command(w, id);
  // LIFE-4/LIFE-9 未安装时明确拒绝请求；保留原死人/看管首错和一般亲属命令。
  // 成人 Medical 的已选业务不由这条出生/照护真空分支关闭。
  if (complete_motion(w)) {
    if (family_birth_command_kind(kind) ||
        child_care_relief_command_kind(kind) || kind == "start_care_handover" ||
        kind == "stop_care") {
      NativeEmptyCareSourceRead empty{
          {w.common, &w.family_birth, nullptr,
           w.actor_foundation ? &*w.actor_foundation : nullptr,
           w.action_elapsed ? &*w.action_elapsed : nullptr, nullptr,
           &w.child_care_relief}};
      return empty.reject_birth_or_care_request();
    }
    if (native_deferred_arcana_command_kind(kind)) {
      auto absent = validate_native_deferred_arcana_absence(w);
      return absent.ok ? common_failure("NOT_IMPLEMENTED", "arcana",
                                        ErrorKind::unsupported_capability)
                       : absent;
    }
  }
  // 计划编辑只管理本人意向；真实执行仍逐命令prepare，不能绕过下方船载/岸工门。
  if (player_plan_command_kind(kind)) {
    if (!w.player_plan_enabled())
      return common_failure("PlayerPlanProfileRequired", {},
                            ErrorKind::unsupported_capability);
    return id == w.common.state().metadata.player_actor_id
               ? common_success()
               : common_failure("PlayerPlanActorRequired");
  }
  // 四种具名野外交接只进入原领域复核；不是一般军团指挥或舟上业务权限。
  const bool tactic_assessment = kind == "military_tactic_practice";
  if (tactic_assessment && !w.military_tactic_learning_enabled())
    return common_failure("MilitaryTacticLearningProfileRequired");
  const bool field_business = political_field_appointment_command_kind(kind) ||
                              kind == "military_field_command_assign";
  // 内河首片的岸上开工须先显式解编，防延迟入货在半日末才超船载重并卡住原Action。
  // 原舟行、时间、返程、交货和既有退出仍可用；不把舰船占用另造成时钟。
  if (w.naval_enabled())
    if (const auto *army = active_army_for_actor(w.military, id))
      if (w.services.army_embarked(army->id)) {
        const std::vector<StableId> afloat = {"naval_repair_start",
                                              "naval_repair_cancel",
                                              "naval_battle_plan",
                                              "naval_capture",
                                              "naval_claim_decline",
                                              "naval_battle_continue",
                                              "naval_form",
                                              "naval_disband",
                                              "naval_execution",
                                              "wait",
                                              "advance",
                                              "travel",
                                              "move_local",
                                              "pause_travel",
                                              "resume_travel",
                                              "travel_wait",
                                              "reroute",
                                              "return",
                                              "campaign_depart",
                                              "campaign_return",
                                              "campaign_cancel",
                                              "campaign_deliver",
                                              "office_resign",
                                              "encounter_talk",
                                              "encounter_leave",
                                              "encounter_fight",
                                              "dialogue_next",
                                              "dialogue_choose",
                                              "dialogue_confirm",
                                              "end_interaction",
                                              "battle_plan",
                                              "battle_close",
                                              "study_cancel",
                                              "rest_cancel",
                                              "military_train_cancel",
                                              "governance_cancel",
                                              "envoy_cancel"};
        if (!field_business && !tactic_assessment &&
            std::find(afloat.begin(), afloat.end(), kind) == afloat.end())
          return common_failure("NavalDisbandRequired", army->id);
      }
  if (kind.rfind("tactic_lesson_", 0) == 0) {
    if (!w.growth_tactic_lessons_enabled() ||
        (kind != "tactic_lesson_start" && kind != "tactic_lesson_cancel"))
      return common_failure("GrowthTacticLessonProfileRequired");
    if (id != w.common.state().metadata.player_actor_id)
      return common_failure("TacticLessonPlayerCommandRequired");
  }
  if (kind.rfind("recovery_", 0) == 0 &&
      (kind != "recovery_cancel" || !w.personal_recovery_enabled()))
    return common_failure("PersonalInjuryRecoveryProfileRequired");
  if (private_shop_daily_rent_command_kind(kind) &&
      !w.private_shop_daily_rent_enabled())
    return common_failure("PrivateShopDailyRentProfileRequired");
  if (private_shop_command_kind(kind) && !w.private_shops_enabled())
    return common_failure("PrivateShopProfileRequired");
  if (kind.rfind("task_shop_order_", 0) == 0 && !w.task_shop_orders_enabled())
    return common_failure("TaskShopOrderProfileRequired");
  if (political_shop_licensing_command_kind(kind) &&
      !w.political_shop_licensing_enabled())
    return common_failure("PoliticalShopLicensingProfileRequired");
  if (kind == political_shop_registry_accept_command &&
      !w.political_shop_registry_successions_enabled())
    return common_failure("PoliticalShopRegistrySuccessionProfileRequired");
  if (kind == political_governance_continue_command &&
      !w.catalog->political_governance_continuations)
    return common_failure("GovernanceContinuationProfileRequired");
  if (is_governance_invitation_reply_command(kind) &&
      !w.catalog->political_governance_invitation_replies)
    return common_failure("GovernanceInvitationProfileRequired");
  if (is_political_head_handover_command(kind) &&
      !w.catalog->political_head_handovers)
    return common_failure("PoliticalHeadHandoverProfileRequired");
  if (is_political_successor_round_command(kind) &&
      !w.catalog->political_successor_rounds)
    return common_failure("PoliticalSuccessorRoundProfileRequired");
  if (is_political_term_handover_command(kind) &&
      !w.catalog->political_term_handovers)
    return common_failure("PoliticalTermHandoverProfileRequired");
  if (is_political_office_reception_command(kind) &&
      !w.catalog->political_office_receptions)
    return common_failure("PoliticalOfficeReceptionProfileRequired");
  if (is_political_office_resignation_command(kind) &&
      !w.catalog->political_office_resignations)
    return common_failure("PoliticalOfficeResignationProfileRequired");
  if (is_political_subordinate_appointment_command(kind) &&
      !w.catalog->political_subordinate_appointments)
    return common_failure("PoliticalSubordinateAppointmentProfileRequired");
  if (is_political_governance_agency_command(kind) &&
      !w.catalog->political_governance_agencies)
    return common_failure("PoliticalGovernanceAgencyProfileRequired");
  if (is_political_succession_command(kind) &&
      !w.catalog->political_successions)
    return common_failure("PoliticalSuccessionProfileRequired");
  if (party_participation_command_kind(kind) && !w.party_invitations_enabled())
    return common_failure("PartyInvitationsUnavailable", {},
                          ErrorKind::unsupported_capability);
  if ((kind == "envoy_report" || kind == "diplomatic_delegate_envoy") &&
      (!w.diplomacy_enabled() || w.diplomacy.version != 2 ||
       w.envoy_tasks.version != 2))
    return common_failure("DiplomaticReportVersionRequired", {},
                          ErrorKind::unsupported_capability);
  if (diplomacy_command_kind(kind) && !w.diplomacy_enabled())
    return common_failure("DiplomacyProfileRequired", {},
                          ErrorKind::unsupported_capability);
  if (kind == political_takeover_command_kind &&
      !w.political_takeover_enabled())
    return common_failure("PoliticalTakeoverProfileRequired", {},
                          ErrorKind::unsupported_capability);
  if (governance_command_kind(kind) && !w.governance_tasks_enabled())
    return common_failure("GovernanceNotEnabled", {},
                          ErrorKind::unsupported_capability);
  if (is_politics_command_kind(kind) && !w.politics_enabled())
    return common_failure("PoliticalProfileRequired", {},
                          ErrorKind::unsupported_capability);
  if (kind == "task_personal_advance" && !w.catalog->task_personal_advances)
    return common_failure("TaskAdvanceProfileRequired", {},
                          ErrorKind::unsupported_capability);
  if (kind == "decline_runtime_offer" &&
      w.catalog->task_offer_response_version != 2)
    return common_failure("RuntimeOfferResponseRequiresV2", {},
                          ErrorKind::unsupported_capability);
  if (kind == "decline_offer" && !w.catalog->task_offer_responses)
    return common_failure("TaskOfferResponseProfileRequired", {},
                          ErrorKind::unsupported_capability);
  if (treaty_passage_command_kind(kind) && !w.treaty_passage_enabled())
    return common_failure("TreatyPassageProfileRequired", {},
                          ErrorKind::unsupported_capability);
  if (political_military_appointment_command_kind(kind) &&
      !w.political_military_appointments_enabled())
    return common_failure("PoliticalMilitaryAppointmentProfileRequired", {},
                          ErrorKind::unsupported_capability);
  if (political_field_appointment_command_kind(kind) &&
      !w.political_field_appointments_enabled())
    return common_failure("PoliticalFieldAppointmentProfileRequired", {},
                          ErrorKind::unsupported_capability);
  if (kind == political_occupation_fiscal_command_kind &&
      !w.occupation_fiscal_enabled())
    return common_failure("OccupationFiscalProfileRequired", {},
                          ErrorKind::unsupported_capability);
  if (kind == "political_set_tax" && !w.political_fiscal_enabled())
    return common_failure("PoliticalFiscalProfileRequired", {},
                          ErrorKind::unsupported_capability);
  if (kind == "configure_local_production" &&
      !w.production_management_enabled())
    return common_failure("ProductionManagementUnavailable", {},
                          ErrorKind::unsupported_capability);
  if ((kind == "talk_actor" || kind == "ask_news") &&
      !w.regional_life_enabled())
    return common_failure("RegionalLifeCapabilityRequired", {},
                          ErrorKind::unsupported_capability);
  if (population_military_command_kind(kind) &&
      !w.population_military_enabled())
    return common_failure("PopulationMilitaryCapabilityRequired", {},
                          ErrorKind::unsupported_capability);
  if (named_membership_command_kind(kind) && !w.named_membership_enabled())
    return common_failure("NamedMembershipCapabilityRequired", {},
                          ErrorKind::unsupported_capability);
  if (dynamic_command_kind(kind) && !w.dynamic_enabled())
    return common_failure("DynamicProfileRequired", {},
                          ErrorKind::unsupported_capability);
  if (siege_integration_command_kind(kind))
    return w.siege_integration_enabled()
               ? common_success()
               : common_failure("SiegeIntegrationCapabilityRequired", {},
                                ErrorKind::unsupported_capability);
  if (kind == "naval_repair_start" || kind == "naval_repair_cancel")
    return w.naval_repairs_enabled()
               ? common_success()
               : common_failure("NavalRepairProfileRequired");
  if (naval_battle_command_codec::command_kind(kind))
    return w.naval_combat_enabled()
               ? common_success()
               : common_failure("NavalCombatProfileRequired", {},
                                ErrorKind::unsupported_capability);
  if (kind == "naval_form" || kind == "naval_disband")
    return w.naval_enabled()
               ? common_success()
               : common_failure("NavalProfileRequired", {},
                                ErrorKind::unsupported_capability);
  if (kind == "military_field_command_assign" &&
      (!w.dynamic_enabled() || !w.catalog->military ||
       !w.catalog->military->field_command_policy ||
       !w.catalog->military->officer_assignment_policy))
    return common_failure("MilitaryFieldCommandProfileRequired");
  if (kind == "military_officer_assign")
    return w.dynamic_enabled() && w.catalog->military &&
                   w.catalog->military->officer_assignment_policy
               ? common_success()
               : common_failure("MilitaryOfficerAssignmentProfileRequired");
  if (kind == "military_request")
    return w.dynamic_enabled() && w.catalog->military &&
                   w.catalog->military->request_policy
               ? common_success()
               : common_failure("MilitaryRequestPolicyRequired");
  if (w.catalog->field_battle &&
      (kind == "military_plan" || kind == "military_close"))
    return common_failure("NOT_IMPLEMENTED", "field_battle/legacy_command",
                          ErrorKind::unsupported_capability);
  if (kind == "military_plan" || kind == "military_close" ||
      kind == "siege_start" || kind == "siege_join" || kind == "siege_plan" ||
      kind == "siege_strategy" || kind == "siege_close")
    return common_success();
  if (kind == "duel_start" || kind == "encounter_fight" ||
      kind == "battle_plan" || kind == "battle_close")
    return military_actor_in_battle(w.military, id)
               ? common_failure("BattlePending")
               : common_success();
  if (personal_pauses_world(w.personal))
    return common_failure("BattlePending");
  if (!custody.value && w.services.in_open_battle(id) && kind != "advance" &&
      kind != "travel_wait")
    return common_failure("BattlePending");
  if (!custody.value && !siege_for_actor(w.military, id).empty() &&
      kind != "advance" && kind != "travel_wait" && kind != "campaign_deliver")
    return common_failure("SiegePending");
  const bool army_clock =
      std::holds_alternative<InArmyLocation>(actor->location) &&
      (kind == "wait" || kind == "advance");
  if (std::holds_alternative<InArmyLocation>(actor->location) &&
      !custody.value) {
    if (!army_clock && !field_business && !tactic_assessment)
      return common_failure("NotArmyCommander");
    if (army_clock) {
      auto member = military_clock_leader(w.common, w.military, id);
      if (!member.ok)
        return {false, {}, member.error};
    }
  }
  const bool party_clock =
      std::holds_alternative<FollowingLocation>(actor->location) &&
      (kind == "wait" || kind == "advance");
  if (std::holds_alternative<FollowingLocation>(actor->location)) {
    if (id != w.common.state().metadata.player_actor_id ||
        (!party_clock && kind != "party_leave" && kind != "end_interaction"))
      return common_failure("NotPartyLeader");
    auto member = party_clock ? party_clock_leader(w.common, id)
                              : party_following_leader(w.common, id);
    if (!member.ok)
      return {false, {}, member.error};
  }
  if (kind == "encounter_talk" || kind == "encounter_leave")
    return common_success();
  const auto *meeting = pending_encounter(w.interaction, id);
  if ((kind == "advance" || kind == "travel_wait") &&
      pending_encounter(w.interaction,
                        w.common.state().metadata.player_actor_id))
    return common_failure("EncounterPending");
  if (meeting && !is_dialogue(kind))
    return common_failure("EncounterPending");
  // 本人仍是请求者；只通过真实军籍让同一世界时间继续，不解除军团占用。
  if (army_clock || party_clock)
    return common_success();
  if (kind == "join_party" || kind == "dismiss_companion")
    return common_success();
  if (is_control(kind)) {
    auto *action = w.common.action(actor->action_id);
    if (!action || action->kind != "travel" ||
        (!std::holds_alternative<WildernessLocation>(actor->location) &&
         !std::holds_alternative<OnRouteLocation>(actor->location)))
      return common_failure("NotTravelling");
    if (!actor->alive || !actor->available)
      return common_failure("ActorUnavailable");
    return common_success();
  }
  if (kind == "advance" || kind == "rescue")
    return common_success();
  if (kind == "long_rest" || kind == "rest_cancel")
    return w.catalog->growth->family ? common_success()
                                     : common_failure("FamilyUnavailable");
  if (kind == "tactic_lesson_cancel")
    return w.growth_tactic_lessons_enabled()
               ? common_success()
               : common_failure("GrowthTacticLessonProfileRequired");
  if (kind == "recovery_cancel")
    return w.personal_recovery_enabled()
               ? common_success()
               : common_failure("PersonalInjuryRecoveryProfileRequired");
  if (growth_busy_command(kind))
    return w.catalog->growth->cultivation
               ? common_success()
               : common_failure("CultivationUnavailable");
  if (kind == "military_train_cancel" || kind == "military_drill_draft" ||
      kind == "military_drill_confirm" ||
      kind == "military_drill_cancel_pending" || kind == "military_drill_exit")
    return w.catalog->military->military
               ? common_success()
               : common_failure("MilitaryUnavailable");
  if (!actor->alive || (!actor->available && !custody.value))
    return common_failure("ActorUnavailable");
  if ((kind == "campaign_return" || kind == "campaign_cancel") &&
      !actor->action_id.empty()) {
    auto *action = w.common.action(actor->action_id);
    if (action && action->kind == "travel")
      return common_success();
  }
  if (dynamic_command_kind(kind))
    return common_success();
  // 只让本人原治理动作进入取消目标复核；其它忙碌命令仍拒绝。
  if (kind == "governance_cancel" &&
      governance_cancellation_busy_allowed(w.common, w.tasks,
                                           w.governance_tasks, id))
    return common_success();
  if (kind == "envoy_cancel" && w.envoy_tasks_enabled() &&
      envoy_cancellation_busy_allowed(w.common, w.tasks, w.envoy_tasks, id))
    return common_success();
  if (kind == "family_care_relief_cancel" && w.family_care_relief_enabled()) {
    const auto *care = w.common.action(actor->action_id);
    if (care && care->kind == "family_child_care_relief" &&
        care->actor_id == id && w.child_care_relief.starts.find(care->id))
      return common_success();
  }
  if (kind == "family_child_care_cancel" && w.family_birth_enabled()) {
    const auto *care = w.common.action(actor->action_id);
    if (care && care->kind == "family_child_care" && care->actor_id == id &&
        w.family_birth.care_starts.find(care->id))
      return common_success();
  }
  if (kind == "private_shop_cancel_operation" && w.private_shops_enabled()) {
    const auto *a = w.common.action(actor->action_id);
    if (a && a->kind == "private_shop_operate" && a->actor_id == id &&
        w.private_shops.operations.find(a->id))
      return common_success();
  }
  bool same_meeting = false;
  if (meeting && is_dialogue(kind))
    for (const auto &session : w.interaction.interactions)
      if (session.actor_id == id && session.context.encounter_id == meeting->id)
        same_meeting = true;
  if (!actor->action_id.empty() && !same_meeting)
    return common_failure("ActorBusy");
  if (tactic_assessment && w.services.in_interaction(id))
    return common_failure("ActorBusy", id);
  if (field_business && w.services.in_interaction(id))
    return common_failure("PoliticalFieldActorBusy", id);
  if (growth_command_kind(kind) && kind != "service" && kind != "trade") {
    if (kind == "trade_bargain_open" || kind == "trade_bargain_choose" ||
        kind == "trade_bargain_exit")
      return w.catalog->growth->economy ? common_success()
                                        : common_failure("EconomyUnavailable");
    return w.catalog->growth->cultivation
               ? common_success()
               : common_failure("CultivationUnavailable");
  }
  if (kind == "office_accept" || kind == "campaign_accept" ||
      kind == "army_form" || kind == "campaign_depart" ||
      kind == "campaign_return" || kind == "campaign_cancel" ||
      kind == "campaign_deliver" || kind == "campaign_report" ||
      kind == "army_return" || kind == "army_resupply" ||
      kind == "office_resign" || kind == "army_pledge" ||
      kind == "military_train")
    return w.catalog->military->military
               ? common_success()
               : common_failure("MilitaryUnavailable");
  return common_success();
}
static Prepared
prepare_command_bound(const WorldBundle &w, const StableId &id,
                      const CommandPayload &command,
                      const CompleteExactWorldReadBindings *actual) {
  if (mismatched_basic_command_binding(command))
    return reject("InvalidCommand");
  // 任何早返之前核实际命令；死亡/控制/计划外名也不能绕过看管白名单。
  auto custody = guard_native_military_custody_payload(w, id, command);
  if (!custody.ok)
    return {false, {}, std::move(custody.error)};
  if (native_field_army_exit_command_kind(command.kind) !=
      std::holds_alternative<FieldArmyExitSelect>(command.data))
    return reject("InvalidFieldArmyExitSelect");
  if (organization_grant_command_kind(command.kind) !=
      std::holds_alternative<GrantChangeCommand>(command.data))
    return reject("CommandPayloadKindMismatch");
  if (native_control_cancellation_command_kind(command.kind) ||
      std::holds_alternative<CancelPendingRetirement>(command.data) ||
      std::holds_alternative<CancelContinuationBoundary>(command.data)) {
    auto valid = preview_native_control_cancellation(w, id, command, actual);
    if (!valid.ok)
      return {false, {}, std::move(valid.error)};
    return ready(w, id, command, DurationQuote{0});
  }
  const bool work_lifecycle = native_work_lifecycle_command_payload(command);
  const auto *repair_service =
      std::get_if<RepairServiceCommandRequest>(&command.data);
  if (repair_service_command_kind(command.kind) != (repair_service != nullptr))
    return reject("InvalidRepairServiceCommand");
  if (repair_service) {
    if ((command.kind == "offer_repair_service") !=
        std::holds_alternative<OfferRepairServiceRequest>(*repair_service))
      return reject("InvalidRepairServiceCommand");
    return reject("RepairServiceOriginalWorldOwnerRequired");
  }
  const auto *message_return =
      std::get_if<TaskMessageReturnCollect>(&command.data);
  if ((command.kind == task_message_return_collect_command) !=
      (message_return != nullptr))
    return reject("InvalidTaskMessageReturnCommand");
  const auto *personal_message =
      std::get_if<TaskPersonalMessageDeliver>(&command.data);
  if ((command.kind == task_personal_message_command) !=
      (personal_message != nullptr))
    return reject("InvalidTaskPersonalMessageCommand");
  const auto *personal_report = std::get_if<TaskPersonalReport>(&command.data);
  if ((command.kind == task_personal_report_command) !=
      (personal_report != nullptr))
    return reject("InvalidTaskPersonalReportCommand");
  const auto *personal_acceptance =
      std::get_if<TaskPersonalAcceptance>(&command.data);
  if ((command.kind == task_personal_acceptance_command) !=
      (personal_acceptance != nullptr))
    return reject("InvalidTaskPersonalAcceptanceCommand");
  const auto *personal_cancel = std::get_if<TaskPersonalCancel>(&command.data);
  if ((command.kind == task_personal_cancellation_command) !=
      (personal_cancel != nullptr))
    return reject("InvalidTaskPersonalCancellationCommand");
  const auto *extension = std::get_if<TaskExtensionCommand>(&command.data);
  if ((command.kind == task_extension_command_kind) != (extension != nullptr))
    return reject("InvalidTaskExtensionCommand");
  const auto *appointment =
      std::get_if<PersonalAppointmentCommand>(&command.data);
  if (is_personal_appointment_command(command.kind) !=
          (appointment != nullptr) ||
      (appointment &&
       personal_appointment_command_kind(*appointment) != command.kind))
    return reject("InvalidPersonalAppointmentCommand");
  const auto *inquiry = std::get_if<ContactInquiryRequest>(&command.data);
  if ((command.kind == contact_inquiry_action_kind) != (inquiry != nullptr))
    return reject("InvalidContactInquiryCommand");
  const auto *visit_reply = std::get_if<VisitReplyCommand>(&command.data);
  if (visit_reply_command_kind(command.kind) != (visit_reply != nullptr) ||
      (visit_reply && visit_reply_command_kind(*visit_reply) != command.kind))
    return reject("InvalidVisitReplyCommand");
  const auto *visit_invitation =
      std::get_if<VisitAppointmentCommand>(&command.data);
  if (is_visit_appointment_command(command.kind) !=
          (visit_invitation != nullptr) ||
      (visit_invitation &&
       visit_appointment_command_kind(*visit_invitation) != command.kind))
    return reject("InvalidVisitAppointmentCommand");
  const auto *visit_meeting =
      std::get_if<VisitAppointmentMeetingCommand>(&command.data);
  if (is_visit_appointment_meeting_command(command.kind) !=
          (visit_meeting != nullptr) ||
      (visit_meeting &&
       visit_appointment_meeting_command_kind(*visit_meeting) != command.kind))
    return reject("InvalidVisitAppointmentCommand");
  const auto *authored_invitation =
      std::get_if<AuthoredVisitAppointment>(&command.data);
  if ((command.kind == authored_visit_appointment_command) !=
      (authored_invitation != nullptr))
    return reject("InvalidVisitAppointmentCommand");
  const auto *born_dialogue =
      std::get_if<OpenBornPersonalDialogue>(&command.data);
  if ((command.kind == "open_born_personal_dialogue") !=
      (born_dialogue != nullptr))
    return reject("InvalidBornPersonalDialogueCommand");
  const auto *known_dialogue =
      std::get_if<OpenKnownPersonalDialogue>(&command.data);
  if ((command.kind == "open_known_personal_dialogue") !=
      (known_dialogue != nullptr))
    return reject("InvalidKnownPersonalDialogueCommand");
  const auto *authored_reply = std::get_if<AuthoredVisitReply>(&command.data);
  if ((command.kind == authored_visit_reply_command) !=
      (authored_reply != nullptr))
    return reject("InvalidAuthoredVisitReplyCommand");
  const auto *liquidation =
      std::get_if<PrivateShopLiquidationCommand>(&command.data);
  if ((command.kind == "private_shop_appoint_executor" ||
       command.kind == "private_shop_accept_executorship" ||
       command.kind == "private_shop_revoke_executor" ||
       command.kind == "private_shop_estate_close") !=
          (liquidation != nullptr) ||
      (liquidation &&
       private_shop_liquidation_command_kind(*liquidation) != command.kind))
    return reject("InvalidPrivateShopLiquidationCommand");
  const auto *estate_order =
      std::get_if<TaskShopOrderEstateCancel>(&command.data);
  if ((command.kind == task_shop_order_estate_cancel_command) !=
      (estate_order != nullptr))
    return reject("InvalidTaskShopOrderEstateCommand");
  const auto *rent = std::get_if<PrivateShopDailyRentCommand>(&command.data);
  if (private_shop_daily_rent_command_kind(command.kind) != (rent != nullptr) ||
      (rent && private_shop_daily_rent_command_kind(*rent) != command.kind))
    return reject("InvalidPrivateShopDailyRentCommand");
  const auto *household_report =
      std::get_if<TellHouseholdPurchaseMarketReport>(&command.data);
  if ((command.kind == household_report_command_kind) !=
      (household_report != nullptr))
    return reject("InvalidHouseholdReportCommand");
  const auto *household_purchase =
      std::get_if<HouseholdPurchaseOriginalCommand>(&command.data);
  const bool household_purchase_kind =
      command.kind == household_purchase_policy_command_kind ||
      command.kind == household_purchase_plan_command_kind;
  if (household_purchase_kind != (household_purchase != nullptr) ||
      (household_purchase &&
       ((command.kind == household_purchase_policy_command_kind) !=
        std::holds_alternative<HouseholdPurchasePolicyCommand1>(
            *household_purchase))))
    return reject("InvalidHouseholdPurchaseCommand");
  const auto *shop = std::get_if<PrivateShopCommand>(&command.data);
  if (private_shop_command_kind(command.kind) != (shop != nullptr) ||
      (shop && private_shop_command_kind(*shop) != command.kind))
    return reject("InvalidPrivateShopCommand");
  const auto *shop_task = std::get_if<TaskShopOrderCommand>(&command.data);
  const bool shop_task_kind = command.kind == "task_shop_order_accept" ||
                              command.kind == "task_shop_order_deliver" ||
                              command.kind == "task_shop_order_cancel";
  if (shop_task_kind != (shop_task != nullptr) ||
      (shop_task && task_shop_order_command_kind(*shop_task) != command.kind))
    return reject("InvalidTaskShopOrderCommand");
  const auto *shop_registry =
      std::get_if<PoliticalAcceptShopRegistry>(&command.data);
  if ((command.kind == political_shop_registry_accept_command) !=
      (shop_registry != nullptr))
    return reject("InvalidPoliticalShopRegistryCommand");
  const auto *governance_continuation =
      std::get_if<PoliticalContinueGovernance>(&command.data);
  if ((command.kind == political_governance_continue_command) !=
      (governance_continuation != nullptr))
    return reject("InvalidGovernanceContinuationCommand");
  const auto *invitation_reply =
      std::get_if<GovernanceInvitationReplyCommand>(&command.data);
  if (is_governance_invitation_reply_command(command.kind) !=
          (invitation_reply != nullptr) ||
      (invitation_reply &&
       command.kind !=
           governance_invitation_reply_command_kind(*invitation_reply)))
    return reject("InvalidGovernanceInvitationCommand");
  const auto *handover =
      std::get_if<PoliticalHeadHandoverCommand>(&command.data);
  if (is_political_head_handover_command(command.kind) !=
          (handover != nullptr) ||
      (handover &&
       command.kind != political_head_handover_command_kind(*handover)))
    return reject("InvalidPoliticalHeadHandoverCommand");
  const auto *next_round =
      std::get_if<PoliticalSuccessorRoundCommand>(&command.data);
  if (is_political_successor_round_command(command.kind) !=
          (next_round != nullptr) ||
      (next_round &&
       command.kind != political_successor_round_command_kind(*next_round)))
    return reject("InvalidPoliticalSuccessorRoundCommand");
  const auto *term_handover =
      std::get_if<PoliticalTermHandoverCommand>(&command.data);
  if (is_political_term_handover_command(command.kind) !=
          (term_handover != nullptr) ||
      (term_handover &&
       command.kind != political_term_handover_command_kind(*term_handover)))
    return reject("InvalidPoliticalTermHandoverCommand");
  const auto *office_reception =
      std::get_if<PoliticalOfficeReceptionCommand>(&command.data);
  if (is_political_office_reception_command(command.kind) !=
          (office_reception != nullptr) ||
      (office_reception &&
       command.kind !=
           political_office_reception_command_kind(*office_reception)))
    return reject("InvalidPoliticalOfficeReceptionCommand");
  const auto *office_resignation =
      std::get_if<PoliticalOfficeResignationCommand>(&command.data);
  const auto *authored_resignation =
      std::get_if<AuthoredOfficeResignationCommand>(&command.data);
  if (is_political_office_resignation_command(command.kind) !=
          (office_resignation != nullptr || authored_resignation != nullptr) ||
      (office_resignation &&
       command.kind !=
           political_office_resignation_command_kind(*office_resignation)) ||
      (authored_resignation &&
       command.kind != political_office_resignation_answer_command))
    return reject("InvalidPoliticalOfficeResignationCommand");
  const auto *subordinate_appointment =
      std::get_if<PoliticalSubordinateAppointmentCommand>(&command.data);
  const auto *authored_subordinate =
      std::get_if<AuthoredSubordinateAppointmentCommand>(&command.data);
  if (is_political_subordinate_appointment_command(command.kind) !=
          (subordinate_appointment != nullptr ||
           authored_subordinate != nullptr) ||
      (subordinate_appointment &&
       command.kind != political_subordinate_appointment_command_kind(
                           *subordinate_appointment)) ||
      (authored_subordinate &&
       command.kind != political_subordinate_appointment_command_kind(
                           authored_subordinate->command)))
    return reject("InvalidPoliticalSubordinateAppointmentCommand");
  const auto *governance_agency =
      std::get_if<PoliticalGovernanceAgencyCommand>(&command.data);
  const auto *authored_agency =
      std::get_if<AuthoredGovernanceAgencyCommand>(&command.data);
  const auto *agency_command =
      authored_agency ? &authored_agency->command : governance_agency;
  if (is_political_governance_agency_command(command.kind) !=
          (agency_command != nullptr) ||
      (agency_command &&
       command.kind !=
           political_governance_agency_command_kind(*agency_command)))
    return reject("InvalidPoliticalGovernanceAgencyCommand");
  const auto *succession =
      std::get_if<PoliticalSuccessionCommand>(&command.data);
  if (is_political_succession_command(command.kind) !=
          (succession != nullptr) ||
      (succession &&
       command.kind != political_succession_command_kind(*succession)))
    return reject("InvalidPoliticalSuccessionCommand");
  const auto *shop_law =
      std::get_if<PoliticalShopLicensingCommand>(&command.data);
  if (political_shop_licensing_command_kind(command.kind) !=
          (shop_law != nullptr) ||
      (shop_law &&
       political_shop_licensing_command_kind(*shop_law) != command.kind))
    return reject("InvalidPoliticalShopLicensingCommand");

  const auto *lesson = std::get_if<GrowthTacticLessonCommand>(&command.data);
  if ((command.kind.rfind("tactic_lesson_", 0) == 0) != (lesson != nullptr) ||
      (lesson && lesson->kind != command.kind))
    return reject("InvalidTacticLessonCommand");
  const auto *relief = std::get_if<ChildCareReliefCommand>(&command.data);
  if (child_care_relief_command_kind(command.kind) != (relief != nullptr) ||
      (relief && relief->kind != command.kind))
    return reject("InvalidChildCareReliefCommand");
  const auto *visit = std::get_if<VisitMessageCommand>(&command.data);
  if (visit_message_command_kind(command.kind) != (visit != nullptr) ||
      (visit && visit_message_command_kind(*visit) != command.kind))
    return reject("InvalidVisitMessageCommand");
  // 字符串守卫豁免前先锁真实typed变体，其他命令不能借field名称跳军籍/舟行门。
  const auto *field =
      std::get_if<PoliticalFieldAppointmentCommand>(&command.data);
  if (political_field_appointment_command_kind(command.kind) !=
          (field != nullptr) ||
      (field &&
       political_field_appointment_command_kind(*field) != command.kind))
    return reject("InvalidPoliticalFieldAppointmentCommand");
  const auto *tactic_military = std::get_if<MilitaryCommand>(&command.data);
  const bool tactic_claim =
      tactic_military &&
      std::holds_alternative<MilitaryTacticPracticeClaim>(*tactic_military);
  if ((command.kind == "military_tactic_practice") != tactic_claim)
    return reject("InvalidMilitaryTacticPracticeRequest");
  const auto *field_military = std::get_if<MilitaryCommand>(&command.data);
  const bool field_assign =
      field_military &&
      std::holds_alternative<MilitaryFieldCommandAssign>(*field_military);
  if ((command.kind == "military_field_command_assign") != field_assign)
    return reject("InvalidMilitaryFieldCommand");
  if ((command.kind == "political_issue_supply_policy") !=
      std::holds_alternative<IssueEconomicSupplyPolicyCommand>(command.data))
    return reject("InvalidEconomicSupplyPolicyCommand");
  const auto *repair = std::get_if<NavalRepairCommand>(&command.data);
  const bool repair_kind = command.kind == "naval_repair_start" ||
                           command.kind == "naval_repair_cancel";
  if (repair_kind != (repair != nullptr) ||
      (repair && (std::holds_alternative<NavalRepairStart>(*repair)
                      ? "naval_repair_start"
                      : "naval_repair_cancel") != command.kind))
    return reject("InvalidNavalRepairCommand");
  const auto *birth = std::get_if<FamilyBirthCommand>(&command.data);
  if (family_birth_command_kind(command.kind) != (birth != nullptr) ||
      (birth && birth->kind != command.kind))
    return reject("InvalidFamilyBirthCommand");
  const auto *employment = std::get_if<EstateEmploymentCommand>(&command.data);
  const bool employment_kind = command.kind.rfind("estate_employment_", 0) == 0;
  if (employment_kind != (employment != nullptr) ||
      (employment &&
       estate_employment_command_kind(*employment) != command.kind))
    return reject("InvalidEstateEmploymentCommand");
  const auto *apprentice = std::get_if<ApprenticeshipCommand>(&command.data);
  if (apprenticeship_command_kind(command.kind) != (apprentice != nullptr) ||
      (apprentice && apprentice->kind != command.kind) ||
      (!apprentice && command.kind.rfind("family_apprenticeship_", 0) == 0))
    return reject("InvalidApprenticeshipCommand");
  const auto *marriage = std::get_if<FamilyMarriageCommand>(&command.data);
  const bool marriage_kind = command.kind == "family_marriage_receive" ||
                             command.kind == "family_marriage_reply" ||
                             command.kind == "family_marriage_dissolve";
  if (marriage_kind != (marriage != nullptr) ||
      (marriage && marriage->kind != command.kind))
    return reject("InvalidFamilyMarriageCommand");
  const auto *inherited = std::get_if<FamilyInheritanceCommand>(&command.data);
  const bool inherited_kind = command.kind == "family_bequest_disclose" ||
                              command.kind == "family_inheritance_accept" ||
                              command.kind == "family_inheritance_settle";
  if (inherited_kind != (inherited != nullptr) ||
      (inherited && inherited->kind != command.kind))
    return reject("InvalidFamilyInheritanceCommand");
  const auto *bequest = std::get_if<FamilyBequestCommand>(&command.data);
  const bool bequest_kind = command.kind == "family_bequest_declare" ||
                            command.kind == "family_bequest_revoke";
  if (bequest_kind != (bequest != nullptr) ||
      (bequest && bequest->kind != command.kind))
    return reject("InvalidFamilyBequestCommand");
  // 对新死亡旁路先核typed种类绑定，防其他业务借外层名称越过死亡守卫。
  const auto *voluntary = std::get_if<VoluntaryPathCommand>(&command.data);
  const bool voluntary_kind =
      command.kind == "family_voluntary_path_accept" ||
      command.kind == "family_voluntary_path_continue" ||
      command.kind == "family_voluntary_path_cancel" ||
      command.kind == "family_voluntary_path_withdraw";
  if (voluntary_kind != (voluntary != nullptr) ||
      (voluntary && voluntary->kind != command.kind) ||
      (!voluntary_kind && command.kind.rfind("family_voluntary_path_", 0) == 0))
    return reject("InvalidVoluntaryPathCommand");
  const auto *collateral = std::get_if<CollateralControlCommand>(&command.data);
  const bool collateral_kind = command.kind == "family_collateral_recognize" ||
                               command.kind == "family_collateral_accept" ||
                               command.kind == "family_collateral_revoke";
  if (collateral_kind != (collateral != nullptr) ||
      (collateral && collateral->kind != command.kind))
    return reject("InvalidCollateralControlCommand");
  const auto *biological = std::get_if<BiologicalControlCommand>(&command.data);
  const bool biological_kind = command.kind == "family_biological_accept" ||
                               command.kind == "family_biological_revoke";
  if (biological_kind != (biological != nullptr) ||
      (biological && biological->kind != command.kind))
    return reject("InvalidBiologicalControlCommand");
  const auto *legacy = std::get_if<FamilyLegacyCommand>(&command.data);
  const bool legacy_kind = command.kind == "family_legacy_accept" ||
                           command.kind == "family_legacy_revoke" ||
                           command.kind == "family_legacy_activate";
  if (legacy_kind != (legacy != nullptr) ||
      (legacy && legacy->kind != command.kind))
    return reject("InvalidFamilyLegacyCommand");

  auto takeover = PoliticalTakeoverExecutionWorldAccess::create(w);
  if (!takeover.ok)
    return {false, {}, takeover.error};
  auto guard = command_common_guard(w, id, command.kind);
  if (!guard.ok)
    return failed<std::shared_ptr<const PreviewResult>>(guard);
  if (native_field_army_exit_command_kind(command.kind)) {
    auto valid = preview_native_field_army_exit_command(w, id, command, actual);
    if (!valid.ok) return {false, {}, std::move(valid.error)};
    return ready(w, id, command, DurationQuote{0});
  }
  if (extension) {
    auto quote = prepare_task_extension_world(w, id, *extension);
    if (!quote.ok)
      return {false, {}, quote.error};
    return ready(w, id, command, std::move(quote.value));
  }
  if (appointment) {
    auto quote = prepare_personal_appointment_world(w, id, *appointment);
    if (!quote.ok)
      return {false, {}, quote.error};
    return ready(w, id, command, std::move(quote.value));
  }
  if (inquiry) {
    auto quote = prepare_contact_inquiry_world(w, id, *inquiry);
    if (!quote.ok)
      return {false, {}, quote.error};
    return ready(w, id, command, std::move(quote.value));
  }
  if (w.catalog->exact_time) {
    if (!exact_complete_selected(*w.catalog->exact_time) &&
        id != w.common.state().metadata.player_actor_id)
      return reject("ExactRestPlayerRouteRequired");
    const auto *advance = std::get_if<AdvanceCommand>(&command.data);
    if (advance ||
        (custody.value && std::holds_alternative<WaitCommand>(command.data))) {
      auto target = prepare_exact_advance_target(
          w, advance ? advance->requested_to : std::optional<WorldMoment>{});
      if (!target.ok)
        return {false, {}, target.error};
      return ready(w, id, command, DurationQuote{});
    }
    // 旧首链仍保原限制；完整选择沿各原writer及实际compiled
    // action门，不继承rest-only限制。
    if (!exact_complete_selected(*w.catalog->exact_time)) {
      const auto *service = std::get_if<GrowthCommand>(&command.data);
      if (!service || command.kind != "service" || service->activity != "rest")
        return reject("ExactRestCommandNotConnected");
    }
  } else if (const auto *advance = std::get_if<AdvanceCommand>(&command.data);
             advance && advance->requested_to)
    return reject("ExactAdvanceProfileRequired");
  if (work_lifecycle) {
    auto valid = preview_native_work_lifecycle_command(w, id, command);
    if (!valid.ok)
      return {false, {}, std::move(valid.error)};
    return ready(w, id, command, DurationQuote{0});
  }
  if (message_return) {
    auto quote = preview_task_message_return_world(w, id, *message_return);
    if (!quote.ok)
      return {false, {}, quote.error};
    return ready(w, id, command, std::move(quote.value));
  }
  if (personal_message) {
    auto quote = preview_task_personal_message_world(w, id, *personal_message);
    if (!quote.ok)
      return {false, {}, quote.error};
    return ready(w, id, command, std::move(quote.value));
  }
  if (personal_report) {
    auto quote = preview_task_personal_report_world(w, id, *personal_report);
    if (!quote.ok)
      return {false, {}, quote.error};
    return ready(w, id, command, std::move(quote.value));
  }
  if (personal_acceptance) {
    auto quote =
        preview_task_personal_acceptance_world(w, id, *personal_acceptance);
    if (!quote.ok)
      return {false, {}, quote.error};
    return ready(w, id, command, std::move(quote.value));
  }
  if (personal_cancel) {
    auto quote =
        preview_task_personal_cancellation_world(w, id, *personal_cancel);
    if (!quote.ok)
      return {false, {}, quote.error};
    return ready(w, id, command, std::move(quote.value));
  }
  if (visit_reply) {
    auto quote = prepare_visit_reply_world(w, id, *visit_reply);
    if (!quote.ok)
      return {false, {}, quote.error};
    return ready(w, id, command, std::move(quote.value));
  }
  if (visit_invitation) {
    auto q = prepare_visit_appointment_world(w, id, *visit_invitation);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (visit_meeting) {
    auto q = prepare_visit_appointment_meeting_world(w, id, *visit_meeting);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (authored_invitation) {
    const VisitAppointmentMeetingTime2Context *context = nullptr;
    if (actual) {
      auto group = read_native_interaction_world(w, *actual);
      if (!group.ok)
        return {false, {}, std::move(group.error)};
      if (!group.value->visit())
        return {false,
                {},
                common_failure("VisitAppointmentActualOwningGroupRequired", {},
                               ErrorKind::unsupported_capability)
                    .error};
      auto x = group.value->visit()->context();
      if (!x.ok)
        return {false, {}, std::move(x.error)};
      context = x.value;
    }
    auto q = prepare_authored_visit_appointment_world(
        w, id, *authored_invitation, context, actual);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (born_dialogue) {
    auto quote = prepare_born_personal_dialogue_world(w, id, *born_dialogue);
    if (!quote.ok)
      return {false, {}, quote.error};
    return ready(w, id, command, std::move(quote.value));
  }
  if (known_dialogue) {
    auto quote = prepare_known_personal_dialogue_world(w, id, *known_dialogue);
    if (!quote.ok)
      return {false, {}, quote.error};
    return ready(w, id, command, std::move(quote.value));
  }
  if (authored_reply) {
    const VisitReplyReadContext *context = nullptr;
    if (actual) {
      auto group = read_native_interaction_world(w, *actual);
      if (!group.ok)
        return {false, {}, std::move(group.error)};
      if (!group.value->replies())
        return {false,
                {},
                common_failure("VisitReplyActualOwningGroupRequired", {},
                               ErrorKind::unsupported_capability)
                    .error};
      auto x = group.value->replies()->context();
      if (!x.ok)
        return {false, {}, std::move(x.error)};
      context = x.value;
    }
    auto quote = prepare_authored_visit_reply_world(w, id, *authored_reply,
                                                    context, actual);
    if (!quote.ok)
      return {false, {}, quote.error};
    return ready(w, id, command, std::move(quote.value));
  }
  if (liquidation) {
    auto q = prepare_private_shop_liquidation_world(w, id, *liquidation);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (estate_order) {
    auto q = prepare_task_shop_order_estate_world(w, id, *estate_order);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (rent) {
    auto q = prepare_private_shop_daily_rent_world(w, id, *rent);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (household_report) {
    auto dependencies = make_native_household_report_dependencies(w);
    if (!dependencies.ok)
      return {false, {}, dependencies.error};
    auto q = preview_native_household_report_command(w, id, *household_report,
                                                     dependencies.value);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (household_purchase) {
    auto q =
        preview_native_household_purchase_command(w, id, *household_purchase);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (shop) {
    auto q = prepare_private_shop_world(w, id, *shop);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (shop_task) {
    auto q = prepare_task_shop_order_world(w, id, *shop_task);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (shop_registry) {
    auto q = prepare_political_shop_registry_world(w, id, *shop_registry);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (governance_continuation) {
    auto q = prepare_political_governance_continuation_world(
        w, id, *governance_continuation);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (invitation_reply) {
    auto q = prepare_governance_invitation_world(w, id, *invitation_reply);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (handover) {
    auto q = prepare_political_head_handover_world(w, id, *handover);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (next_round) {
    auto q = prepare_political_successor_round_world(w, id, *next_round);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (term_handover) {
    auto q = prepare_political_term_handover_world(w, id, *term_handover);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (office_reception) {
    auto q = prepare_political_office_reception_world(w, id, *office_reception);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (authored_resignation) {
    auto q =
        prepare_authored_office_resignation_world(w, id, *authored_resignation);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (authored_subordinate) {
    auto q = prepare_authored_subordinate_appointment_world(
        w, id, *authored_subordinate);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (office_resignation) {
    auto q =
        prepare_political_office_resignation_world(w, id, *office_resignation);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (subordinate_appointment) {
    auto q = prepare_political_subordinate_appointment_world(
        w, id, *subordinate_appointment);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (authored_agency) {
    auto q = prepare_authored_governance_agency_world(w, id, *authored_agency);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (governance_agency) {
    auto q =
        prepare_political_governance_agency_world(w, id, *governance_agency);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (succession) {
    auto q = prepare_political_succession_world(w, id, *succession);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (shop_law) {
    if (w.political_shop_licensing.version == 2) {
      auto q = prepare_political_shop_licensing_world_owned(w, id, *shop_law);
      if (!q.ok)
        return {false, {}, std::move(q.error)};
      PoliticalShopLicensingPreparedQuote retained =
          checkpoint_make_shared<OwnedPoliticalShopLicensingQuote>(
              std::move(q.value));
      return ready(w, id, command, std::move(retained));
    }
    auto q = prepare_political_shop_licensing_world(w, id, *shop_law);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (visit) {
    auto quote = prepare_visit_message_world(w, id, *visit, actual);
    if (!quote.ok)
      return {false, {}, quote.error};
    return ready(w, id, command, std::move(quote.value));
  }
  if (const auto *c =
          std::get_if<IssueEconomicSupplyPolicyCommand>(&command.data)) {
    auto q = prepare_economic_supply_world(w, id, *c);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (const auto *plan = std::get_if<PlayerPlanCommand>(&command.data)) {
    auto quote = preview_player_plan_command(w, id, *plan);
    if (!quote.ok)
      return {false, {}, quote.error};
    return ready(w, id, command, std::move(quote.value));
  }
  if (repair) {
    auto ia = w.services.interaction_access();
    auto ma = w.services.military_access();
    ma.interaction_access = &ia;
    auto q = preview_naval_repair(w.common, w.military, *w.catalog->military,
                                  ma, w.naval, *w.catalog->naval, id, *repair);
    if (!q.ok)
      return {false, {}, q.error};
    if (!q.value.replayed &&
        std::holds_alternative<NavalRepairStart>(*repair)) {
      auto fresh = command_common_guard(w, id, "naval_execution");
      if (!fresh.ok)
        return failed<std::shared_ptr<const PreviewResult>>(fresh);
    }
    return ready(w, id, command, std::move(q.value));
  }
  if (const auto *naval = std::get_if<NavalBattleCommand>(&command.data)) {
    if (!w.naval_combat_enabled())
      return reject("NavalCombatProfileRequired");
    auto ia = w.services.interaction_access();
    auto ma = w.services.military_access();
    ma.interaction_access = &ia;
    auto access = naval_preview_access(w);
    auto q = preview_naval_battle_command(
        w.common, w.military, *w.catalog->military, ma, w.naval,
        *w.catalog->naval, id, *naval, access);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (const auto *naval = std::get_if<NavalCommand>(&command.data)) {
    auto ia = w.services.interaction_access();
    auto ma = w.services.military_access();
    ma.interaction_access = &ia;
    auto q = preview_naval_command(w.common, w.military, *w.catalog->military,
                                   ma, w.naval, *w.catalog->naval, id, *naval);
    if (!q.ok)
      return {false, {}, q.error};
    if (!q.value.replayed) {
      auto fresh = command_common_guard(w, id, "naval_execution");
      if (!fresh.ok)
        return failed<std::shared_ptr<const PreviewResult>>(fresh);
    }
    return ready(w, id, command, std::move(q.value));
  }
  if (const auto *advance =
          std::get_if<TaskPersonalAdvanceCommand>(&command.data)) {
    auto q = preview_task_advance_world(w, id, *advance);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (const auto *party =
          std::get_if<PartyParticipationCommand>(&command.data)) {
    auto ia = w.services.interaction_access();
    auto q = preview_party_participation_command(
        w.common, w.interaction, *w.catalog->interaction,
        w.catalog->interaction->party_invitations.get(), ia, id, *party);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (const auto *diplomacy = std::get_if<DiplomacyCommand>(&command.data)) {
    auto ia = w.services.interaction_access();
    auto q = preview_diplomacy_business(w.diplomacy_read(ia), id, *diplomacy);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (const auto *takeover =
          std::get_if<PoliticalTakeoverDecisionCommand>(&command.data)) {
    auto q = prepare_political_takeover_world_command(
        w, *w.catalog->political_takeover, w.political_takeover, id, *takeover,
        w.common.state().identity);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (const auto *governance = std::get_if<GovernanceCommand>(&command.data)) {
    auto q = preview_governance_business(w.governance_read(), id, *governance);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (const auto *politics = std::get_if<PoliticsCommand>(&command.data)) {
    auto pending = preview_political_subordinate_legacy_world(w, *politics);
    if (!pending.ok)
      return {false, {}, pending.error};
    auto q = preview_political_business(w.politics_read(), id, *politics);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (const auto *household =
          std::get_if<ConfigureHouseholdProvisionCommand>(&command.data)) {
    auto q = prepare_household_provision_configuration(w, id, *household);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (const auto *economic =
          std::get_if<ConfigureLocalProductionCommand>(&command.data)) {
    auto q = prepare_economic_configuration(w, id, *economic);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (const auto *treaty = std::get_if<TreatyPassageCommand>(&command.data)) {
    auto q = prepare_treaty_passage_world(w, id, *treaty);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (const auto *c =
          std::get_if<PoliticalMilitaryAppointmentCommand>(&command.data)) {
    auto q = prepare_political_military_appointment_world(w, id, *c);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (const auto *c =
          std::get_if<PoliticalFieldAppointmentCommand>(&command.data)) {
    auto q = prepare_political_field_appointment_world(w, id, *c);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (const auto *fiscal =
          std::get_if<PoliticalSetOccupationTaxCommand>(&command.data)) {
    auto q = prepare_occupation_fiscal_world(w, id, *fiscal);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (const auto *fiscal = std::get_if<PoliticalSetTaxCommand>(&command.data)) {
    auto legacy = require_legacy_fiscal_world(w, fiscal->jurisdiction_id);
    if (!legacy.ok)
      return {false, {}, legacy.error};
    auto q = preview_political_fiscal_business(w.political_fiscal_read(), id,
                                               *fiscal);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (command.kind == "dialogue_confirm")
    return prepare_dialogue_confirmation(w, id, command);
  if (w.population_military_enabled())
    if (const auto *recruit =
            std::get_if<OrganizationRecruitCommand>(&command.data))
      for (const auto &pending : w.population_military.actions.rows())
        if (recruit->source_cohort_id ==
                pending.second.terms.reserve_cohort_id ||
            recruit->target_cohort_id ==
                pending.second.terms.reserve_cohort_id ||
            recruit->source_cohort_id ==
                pending.second.terms.serving_cohort_id ||
            recruit->target_cohort_id == pending.second.terms.serving_cohort_id)
          return failed<std::shared_ptr<const PreviewResult>>(
              common_failure("PopulationCohortReserved", pending.first));
  if (const auto *integration =
          std::get_if<SiegeIntegrationCommand>(&command.data)) {
    auto ia = w.services.interaction_access();
    auto ma = w.services.military_access();
    ma.interaction_access = &ia;
    SiegeIntegrationReadContext context{w.common,
                                        w.military,
                                        *w.catalog->military,
                                        ma,
                                        w.military.siege_integration,
                                        w.catalog->military->siege_integration};
    auto q = preview_siege_integration(context, id, *integration);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (const auto *apply = std::get_if<SiegeIntegrationApply>(&command.data)) {
    auto q = preview_siege_handover(w, id, *apply);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (const auto *population =
          std::get_if<PopulationMilitaryCommand>(&command.data)) {
    if (!w.population_military_enabled())
      return failed<std::shared_ptr<const PreviewResult>>(
          common_failure("PopulationMilitaryCapabilityRequired", {},
                         ErrorKind::unsupported_capability));
    auto q = preview_population_military(w.population_read(), id, *population);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (const auto *organization =
          std::get_if<OrganizationContractCommand>(&command.data)) {
    auto quote = preview_organization_contract(w, id, *organization);
    if (!quote.ok)
      return {false, {}, quote.error};
    return ready(w, id, command, std::move(quote.value));
  }
  if (named_membership_command_kind(command.kind)) {
    auto q = preview_named_membership(w, id, command);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (dynamic_command_kind(command.kind)) {
    auto checked = preview_dynamic_command(w, id, command);
    if (!checked.ok)
      return failed<std::shared_ptr<const PreviewResult>>(checked);
    return ready(w, id, command, DurationQuote{0});
  }
  if (auto *personal = std::get_if<PersonalCommand>(&command.data)) {
    auto q = preview_personal_integration(w, id, *personal);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  const bool army_wait =
      std::holds_alternative<WaitCommand>(command.data) &&
      std::holds_alternative<InArmyLocation>(w.common.actor(id)->location);
  const bool party_wait =
      std::holds_alternative<WaitCommand>(command.data) &&
      std::holds_alternative<FollowingLocation>(w.common.actor(id)->location);
  const bool custody_wait =
      custody.value && std::holds_alternative<WaitCommand>(command.data);
  if (std::holds_alternative<AdvanceCommand>(command.data) || army_wait ||
      party_wait || custody_wait) {
    StableId clock_actor = id;
    if (std::holds_alternative<FollowingLocation>(
            w.common.actor(id)->location)) {
      auto member = party_clock_leader(w.common, id);
      if (!member.ok)
        return {false, {}, member.error};
      clock_actor = member.value;
    }
    if (!custody.value &&
        std::holds_alternative<InArmyLocation>(w.common.actor(id)->location)) {
      auto member = military_clock_leader(w.common, w.military, id);
      if (!member.ok)
        return {false, {}, member.error};
      clock_actor = member.value;
    }
    if (w.common.tick() == std::numeric_limits<Integer>::max())
      return reject("WorldTickOverflow");
    auto ia = w.services.interaction_access();
    auto ma = w.services.military_access();
    ma.interaction_access = &ia;
    auto preflight = military_preflight(
        w.common, w.military, *w.catalog->military, ma, w.common.tick() + 1);
    if (!preflight.ok)
      return failed<std::shared_ptr<const PreviewResult>>(preflight);
    bool ready_battle = false;
    for (const auto &battle : w.military.battles)
      if (battle.phase == "planning" &&
          battle.created_tick <= w.common.tick() &&
          battle.last_tick != w.common.tick() + 1)
        ready_battle = true;
    bool ready_siege = false;
    for (const auto &siege : w.military.sieges)
      if (siege.phase == "active")
        ready_siege = true;
    if (!custody.value && !army_wait && !party_wait &&
        w.common.action_order().empty() && !ready_battle && !ready_siege &&
        !w.dynamic_enabled())
      return reject("NoCurrentAction");
    for (const auto &key : w.common.action_order()) {
      auto *action = w.common.action(key);
      if (action->actor_id == clock_actor && action->kind == "travel" &&
          !w.services.in_open_battle(clock_actor)) {
        auto r = can_step_travel(w.common, w.travel, *w.catalog->travel,
                                 w.services, *action);
        if (!r.ok)
          return failed<std::shared_ptr<const PreviewResult>>(r);
      }
    }
    return ready(w, id, command, DurationQuote{1});
  }
  if (std::holds_alternative<WaitCommand>(command.data))
    return ready(w, id, command, DurationQuote{1});
  if (auto *travel = std::get_if<TravelCommandPayload>(&command.data)) {
    if (auto *p = std::get_if<TravelCommand>(travel)) {
      auto q = plan_travel(w.common, w.travel, *w.catalog->travel, w.services,
                           id, p->destination, p->via);
      if (!q.ok)
        return {false, {}, q.error};
      return ready(w, id, command, std::move(q.value));
    }
    if (std::holds_alternative<RescueCommand>(*travel)) {
      auto q = preview_rescue(w.common, w.travel, *w.catalog->travel,
                              w.services, id);
      if (!q.ok)
        return failed<std::shared_ptr<const PreviewResult>>(q);
      return ready(w, id, command, DurationQuote{2});
    }
    if (auto *p = std::get_if<MoveLocalCommand>(travel)) {
      auto q = preview_move_local(w.common, *w.catalog->travel, w.services, id,
                                  p->place_id);
      if (!q.ok)
        return failed<std::shared_ptr<const PreviewResult>>(q);
      return ready(w, id, command, DurationQuote{0});
    }
    auto q = prepare_travel_control(w.common, w.travel, *w.catalog->travel,
                                    w.services, id, *travel);
    if (!q.ok)
      return {false, {}, q.error};
    if (complete_motion(w) &&
        std::holds_alternative<TravelWaitCommand>(*travel) &&
        (!w.world_timeline ||
         w.world_timeline->boundary_state != PeriodBoundaryState::open))
      return reject("ExactTravelWaitReadyOpeningOrderRequired");
    return ready(w, id, command, std::move(q.value));
  }
  auto ia = w.services.interaction_access(actual);
  auto ea = w.services.estate_access();
  if (auto *p = std::get_if<MilitaryCommand>(&command.data)) {
    if (native_field_campaign_departure_selected(w, *p)) {
      // 普通公开 preview 没有外借绑定；只在本有限路由取得同 World 真 owner。
      std::unique_ptr<CompleteExactWorldBindingOwner> field_owner;
      const auto *field_actual = actual;
      if (!field_actual) {
        auto original = w.complete_exact_time_bindings();
        if (!original.ok)
          return {false, {}, std::move(original.error)};
        field_owner = std::move(original.value);
        if (!field_owner)
          return reject("FieldDepartureOriginalCompleteOwnerRequired");
        field_actual = &field_owner->reads();
      }
      auto q = preview_native_field_campaign_departure(
          w, *field_actual, id, std::get<CampaignDepart>(*p));
      if (!q.ok)
        return {false, {}, std::move(q.error)};
      return ready(w, id, command, std::move(q.value));
    }
    if (native_field_original_military_quote_selected(w, *p)) {
      auto q = preview_native_field_original_military_command(w, id, *p);
      if (!q.ok)
        return {false, {}, std::move(q.error)};
      return ready(w, id, command, std::move(q.value));
    }
    auto ma = w.services.military_access();
    ma.interaction_access = &ia;
    auto q = preview_military(w.common, w.military, *w.catalog->military, ma,
                              id, *p);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (auto *p = std::get_if<InteractionCommand>(&command.data)) {
    auto q = preview_interaction(w.common, w.interaction,
                                 *w.catalog->interaction, ia, id, *p);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (auto *p = std::get_if<GrowthCommand>(&command.data)) {
    auto q = preview_selected_growth_command(w, id, *p);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (auto *p = std::get_if<TaskCommand>(&command.data)) {
    if (p->kind == "decline_runtime_offer") {
      auto q = preview_runtime_task_response(w, id, *p);
      if (!q.ok)
        return {false, {}, q.error};
      return ready(w, id, command, std::move(q.value));
    }
    if (p->kind == "decline_offer") {
      if (!w.tasks.offer_responses)
        return reject("MissingTaskOfferResponseDomain");
      auto q = preview_task_offer_decline(w.common, w.tasks, *w.catalog->tasks,
                                          w.interaction,
                                          *w.catalog->interaction, ia, id, *p);
      if (!q.ok)
        return {false, {}, q.error};
      return ready(w, id, command, std::move(q.value));
    }
    std::optional<KnownSubjectView> known;
    if (w.dynamic_enabled()) {
      auto authority = w.services.organization_access();
      known =
          known_subject_view(w.common, w.social, authority, id, &w.interaction);
    }
    auto notice = make_task_message_notice_services(w);
    if (!notice.ok)
      return {false, {}, notice.error};
    auto returns = TaskMessageReturnWorldAccess::create(w);
    if (!returns.ok)
      return {false, {}, returns.error};
    auto q = preview_task(
        w.common, w.tasks, *w.catalog->tasks, *w.catalog->common, w.interaction,
        *w.catalog->interaction, ia, id, *p,
        w.dynamic_enabled() ? &w.affairs : nullptr, known ? &*known : nullptr,
        takeover.value->tasks(), notice.value ? &*notice.value : nullptr,
        returns.value ? &returns.value->services() : nullptr);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (const auto *c = std::get_if<EstateEmploymentCommand>(&command.data)) {
    auto q = prepare_estate_employment_world(w, id, *c);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (const auto *c = std::get_if<EstateCommand>(&command.data)) {
    auto allowed = guard_estate_employment_world_management(w, *c);
    if (!allowed.ok)
      return {false, {}, allowed.error};
  }
  if (auto *p = std::get_if<GrowthTacticLessonCommand>(&command.data)) {
    auto q = preview_growth_tactic_lesson_world(w, id, *p);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (auto *p = std::get_if<ChildCareReliefCommand>(&command.data)) {
    if (!w.family_care_relief_enabled())
      return reject("ChildCareReliefProfileRequired");
    auto q = preview_child_care_relief_command(
        w.common, w.interaction, *w.catalog->interaction, ia,
        *w.catalog->family_care_relief, w.family_birth, w.family_life,
        w.child_care_relief, w.family_care_relief_access(), id, *p);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (auto *p = std::get_if<FamilyBirthCommand>(&command.data)) {
    if (!w.family_birth_enabled())
      return reject("BirthProfileRequired");
    auto q = preview_family_birth_command(
        w.common, w.interaction, *w.catalog->interaction, ia,
        *w.catalog->family, *w.catalog->family_birth, w.family_birth,
        w.family_birth_access(), id, *p);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (auto *p = std::get_if<ApprenticeshipCommand>(&command.data)) {
    if (!w.family_apprenticeship_enabled())
      return reject("ApprenticeshipProfileRequired");
    auto q = preview_apprenticeship_command(
        w.common, w.interaction, *w.catalog->interaction, ia,
        *w.catalog->family_apprenticeship, w.family_apprenticeship,
        w.family_apprenticeship_access(), id, *p);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (auto *p = std::get_if<FamilyMarriageCommand>(&command.data)) {
    if (!w.family_marriage_enabled())
      return reject("MarriageProfileRequired");
    auto q = preview_family_marriage_command(
        w.common, w.family, w.family_life, w.interaction,
        *w.catalog->interaction, ia, *w.catalog->family_marriage,
        w.family_marriage, w.family_marriage_access(), id, *p);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (auto *p = std::get_if<FamilyInheritanceCommand>(&command.data)) {
    if (!w.family_inheritance_enabled())
      return reject("FamilyInheritanceProfileRequired");
    auto q = preview_family_inheritance_command(
        w.common, w.family_bequest, w.family_life, w.interaction,
        *w.catalog->interaction, ia, *w.catalog->family_inheritance,
        w.family_inheritance, w.family_inheritance_access(), id, *p);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (auto *p = std::get_if<FamilyBequestCommand>(&command.data)) {
    if (!w.family_bequest_enabled())
      return reject("FamilyBequestProfileRequired");
    auto q = preview_family_bequest_command(
        w.common, w.family, w.family_life, w.family_death_read(),
        *w.catalog->family_bequest, w.family_bequest,
        w.family_bequest_player_read(), id, *p);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (auto *p = std::get_if<VoluntaryPathCommand>(&command.data)) {
    auto q = preview_family_voluntary_path_world(w, id, *p);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (auto *p = std::get_if<CollateralControlCommand>(&command.data)) {
    auto q = preview_family_collateral_world(w, id, *p);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (auto *p = std::get_if<BiologicalControlCommand>(&command.data)) {
    auto q = preview_family_biological_world(w, id, *p);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (auto *p = std::get_if<FamilyLegacyCommand>(&command.data)) {
    if (!w.family_legacy_enabled())
      return reject("LegacyControlConsentProfileRequired");
    auto q = preview_family_legacy_world(w, id, *p);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (auto *family_request = std::get_if<FamilyCommand>(&command.data);
      family_request && family_request->kind == "family_adopt_existing" &&
      family_request->adoptee_actor_id && w.family_marriage_enabled()) {
    auto state = validate_family_marriage(
        w.common, w.family, w.family_life, *w.catalog->interaction,
        *w.catalog->family_marriage, w.family_marriage,
        w.family_marriage_access());
    if (!state.ok)
      return {false, {}, state.error};
    auto valid = validate_marriage_adoption_boundary(
        state.value, id, *family_request->adoptee_actor_id);
    if (!valid.ok)
      return {false, {}, valid.error};
  }
  if (auto *p = std::get_if<FamilyCommand>(&command.data)) {
    auto family_sources = NativeFamilyInitialSourcesLease::create(w);
    if (!family_sources.ok)
      return {false, {}, std::move(family_sources.error)};
    auto fa = w.services.family_access(
        ia, family_sources.value ? &family_sources.value->origins() : nullptr,
        family_sources.value ? &family_sources.value->initial_age() : nullptr);
    auto q = (w.family_voluntary_paths_enabled() &&
              (p->kind == "family_agree_succession" ||
               p->kind == "family_cancel_succession" ||
               p->kind == "family_continue_as"))
                 ? preview_family_voluntary_original_world(w, id, *p)
                 : preview_family(w.common, w.family, *w.catalog->family,
                                  *w.catalog->common, w.estate,
                                  *w.catalog->estate, fa, id, *p);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  if (auto *p = std::get_if<EstateCommand>(&command.data)) {
    auto q = preview_estate(w.common, w.estate, *w.catalog->estate,
                            *w.catalog->common, w.services, id, *p, &ea);
    if (!q.ok)
      return {false, {}, q.error};
    return ready(w, id, command, std::move(q.value));
  }
  return reject("UnsupportedAction");
}
namespace {
Status exact_dispatch_binding(const WorldBundle &w,
                              const CompleteExactWorldReadBindings &actual) {
  if (!w.catalog || !w.catalog->complete_exact_time_runtime ||
      &actual.runtime != w.catalog->complete_exact_time_runtime.get())
    return common_failure("OriginalDispatchCompleteCandidateMismatch", {},
                          ErrorKind::internal);
  auto source = actual.character_conditions.sources();
  if (!source.ok)
    return {false, {}, std::move(source.error)};
  return &source.value.common == &w.common
             ? common_success()
             : common_failure("OriginalDispatchCompleteCandidateMismatch", {},
                              ErrorKind::internal);
}
} // namespace
Prepared prepare_command(const WorldBundle &w, const StableId &id,
                         const CommandPayload &command) {
  return prepare_command_bound(w, id, command, nullptr);
}
Prepared prepare_command_with_exact_bindings(
    const WorldBundle &w, const StableId &id, const CommandPayload &command,
    const CompleteExactWorldReadBindings &actual) {
  auto valid = exact_dispatch_binding(w, actual);
  if (!valid.ok)
    return {false, {}, std::move(valid.error)};
  return prepare_command_bound(w, id, command, &actual);
}
// 按原ID→行动头→人物占用→领域start顺序建立；真History2交易只拥有本次新开的
// 资源捕获，既有捕获只借。原业务及覆盖封口失败均交外层丢弃候选，不发布半笔交易。
Outcome<ActionStarted> begin_action(WorldBundle &w, const StableId &actor,
                                    const CommandPayload &command,
                                    const QuoteDetails &q,
                                    CompleteExactCommandCapture* capture) {
  auto takeover = PoliticalTakeoverExecutionWorldAccess::create(w);
  if (!takeover.ok)
    return {false, {}, takeover.error};
  ActionCoreView action;
  action.kind = command.kind;
  action.actor_id = actor;
  action.start_tick = w.common.tick();
  action.participants = {actor};
  if (auto *travel = std::get_if<TravelQuote>(&q.data)) {
    auto members = w.common.members(actor);
    if (!members)
      return failed<ActionStarted>(common_failure("InvalidParty"));
    action.participants = *members;
    action.remaining_ticks = travel->duration;
  } else if (auto *population = std::get_if<PopulationMilitaryQuote>(&q.data)) {
    action.participants = population->participants;
    action.remaining_ticks = population->duration;
  } else if (auto *military = std::get_if<MilitaryQuote>(&q.data)) {
    action.participants = military->participants;
    action.remaining_ticks = military->duration;
  } else if (auto *task = std::get_if<TaskQuote>(&q.data)) {
    action.remaining_ticks = task->duration;
    if (!task->context.provider_actor_id.empty() &&
        task->context.provider_actor_id != actor)
      action.participants.push_back(task->context.provider_actor_id);
  } else if (auto *personal = std::get_if<TaskPersonalMessageQuote>(&q.data)) {
    const auto *c = std::get_if<TaskPersonalMessageDeliver>(&command.data);
    if (!c || command.kind != task_personal_message_command)
      return failed<ActionStarted>(
          common_failure("InvalidTaskPersonalMessageCommand"));
    auto valid =
        revalidate_task_personal_message_quote_world(w, actor, *c, *personal);
    if (!valid.ok)
      return failed<ActionStarted>(valid);
    action.kind = "deliver_message";
    action.remaining_ticks = personal->duration;
    action.participants.push_back(
        personal->recipient_declaration.declaration.meeting.provider_actor_id);
  } else if (auto *report = std::get_if<TaskPersonalReportQuote>(&q.data)) {
    const auto *c = std::get_if<TaskPersonalReport>(&command.data);
    if (!c || command.kind != task_personal_report_command)
      return failed<ActionStarted>(
          common_failure("InvalidTaskPersonalReportCommand"));
    auto valid =
        revalidate_task_personal_report_quote_world(w, actor, *c, *report);
    if (!valid.ok)
      return failed<ActionStarted>(valid);
    action.kind = "report_offer";
    action.remaining_ticks = report->duration;
    action.participants.push_back(
        report->sender_declaration.declaration.meeting.provider_actor_id);
  } else if (auto *acceptance =
                 std::get_if<TaskPersonalAcceptanceQuote>(&q.data)) {
    const auto *c = std::get_if<TaskPersonalAcceptance>(&command.data);
    if (!c || command.kind != task_personal_acceptance_command)
      return failed<ActionStarted>(
          common_failure("InvalidTaskPersonalAcceptanceCommand"));
    auto valid = revalidate_task_personal_acceptance_quote_world(w, actor, *c,
                                                                 *acceptance);
    if (!valid.ok)
      return failed<ActionStarted>(valid);
    action.kind = "accept_offer";
    action.remaining_ticks = acceptance->duration;
    action.participants.push_back(
        acceptance->sender_declaration.declaration.meeting.provider_actor_id);
  } else if (auto *growth = std::get_if<GrowthQuote>(&q.data)) {
    action.remaining_ticks = growth->duration;
    if (growth->context && growth->context->provider_actor_id != actor)
      action.participants.push_back(growth->context->provider_actor_id);
  } else if (auto *relief = std::get_if<ChildCareReliefCommandQuote>(&q.data)) {
    if (command.kind != "family_child_care_relief" || !relief->care ||
        relief->care->provider_actor_id != actor)
      return failed<ActionStarted>(
          common_failure("InvalidChildCareReliefQuote"));
    auto ia = w.services.interaction_access();
    auto checked = revalidate_child_care_relief_quote(
        w.common, w.interaction, *w.catalog->interaction, ia,
        *w.catalog->family_care_relief, w.family_birth, w.family_life,
        w.child_care_relief, w.family_care_relief_access(), actor,
        std::get<ChildCareReliefCommand>(command.data), *relief);
    if (!checked.ok)
      return failed<ActionStarted>(checked);
    action.remaining_ticks = relief->care->duration;
  } else if (auto *birth = std::get_if<FamilyBirthQuote>(&q.data)) {
    if (command.kind != "family_child_care" || !birth->care ||
        birth->care->caregiver_actor_id != actor)
      return failed<ActionStarted>(common_failure("InvalidChildCareQuote"));
    auto ia = w.services.interaction_access();
    auto checked = revalidate_family_birth_quote(
        w.common, w.interaction, *w.catalog->interaction, ia,
        *w.catalog->family, *w.catalog->family_birth, w.family_birth,
        w.family_birth_access(), actor,
        std::get<FamilyBirthCommand>(command.data), *birth);
    if (!checked.ok)
      return failed<ActionStarted>(checked);
    action.remaining_ticks = birth->care->duration;
  } else if (auto *family = std::get_if<FamilyQuote>(&q.data)) {
    action.remaining_ticks = family->duration;
    if (family->context && !family->context->provider_actor_id.empty() &&
        family->context->provider_actor_id != actor)
      action.participants.push_back(family->context->provider_actor_id);
  } else if (auto *estate = std::get_if<EstateQuote>(&q.data)) {
    action.remaining_ticks = estate->duration;
    action.participants = estate->participants;
  } else if (auto *duration = std::get_if<DurationQuote>(&q.data))
    action.remaining_ticks = duration->duration.value_or(0);
  else
    return failed<ActionStarted>(common_failure("InvalidActionPreparation"));
  if (action.remaining_ticks <= 0 ||
      std::find(action.participants.begin(), action.participants.end(),
                actor) == action.participants.end())
    return failed<ActionStarted>(common_failure("InvalidActionPreparation"));
  std::optional<NavalVoyageFact> voyage;
  if (const auto *travel = std::get_if<TravelQuote>(&q.data)) {
    auto prepared = prepare_naval_voyage_world(w, actor, *travel);
    if (!prepared.ok)
      return {false, {}, prepared.error};
    voyage = std::move(prepared.value);
  }
  // 真开工 admission 在 next_id/共同占用之前建立。phase 先于 owner 声明，
  // 保证其借读一直活到本次领域 start 结束；const preview 仍仅作原只读报价。
  std::unique_ptr<PhaseCurrentReadScope> growth_start_phase;
  std::unique_ptr<CompleteExactWorldBindingOwner> growth_start_owner;
  std::optional<GrowthExactStartContext> growth_exact_start;
  if (std::holds_alternative<GrowthQuote>(q.data) && complete_motion(w)) {
    if (!w.active_exact_write_context() ||
        !w.common.actor_condition_capture_identity() || !w.action_elapsed ||
        !w.actor_foundation || !w.catalog->actor_foundation)
      return failed<ActionStarted>(
          common_failure("GrowthActualStartCaptureRequired", {},
                         ErrorKind::unsupported_capability));
    auto at = w.actual_phase_moment();
    if (!at.ok)
      return {false, {}, std::move(at.error)};
    if (!w.has_active_read_phase()) {
      auto phase = w.enter_phase_read_context("growth_command_start", "growth",
                                              {at.value, at.value}, at.value);
      if (!phase.ok)
        return {false, {}, std::move(phase.error)};
      growth_start_phase = std::move(phase.value);
    }
    auto owner = w.complete_exact_time_bindings();
    if (!owner.ok)
      return {false, {}, std::move(owner.error)};
    if (!owner.value)
      return failed<ActionStarted>(
          common_failure("CompleteGrowthOwningLeaseRequired", {},
                         ErrorKind::unsupported_capability));
    growth_start_owner = std::move(owner.value);
    auto binding = exact_dispatch_binding(w, growth_start_owner->reads());
    if (!binding.ok)
      return failed<ActionStarted>(binding);
    auto body = growth_start_owner->reads().character_conditions.sources();
    if (!body.ok)
      return {false, {}, std::move(body.error)};
    if (&body.value.common != &w.common ||
        &body.value.state != &*w.actor_foundation ||
        &body.value.catalog != w.catalog->actor_foundation.get() ||
        body.value.growth != &w.growth ||
        body.value.growth_catalog != w.catalog->growth.get() ||
        body.value.shared != w.catalog->common.get() ||
        body.value.elapsed != &*w.action_elapsed)
      return failed<ActionStarted>(common_failure(
          "CompleteGrowthCandidateBindingMismatch", {}, ErrorKind::internal));
    auto same = compare_world_moments(body.value.actual_now, at.value);
    if (!same.ok)
      return {false, {}, std::move(same.error)};
    if (same.value != 0)
      return failed<ActionStarted>(common_failure(
          "GrowthActualStartMomentMismatch", {}, ErrorKind::internal));
    // 正片内 Common tick 尚未结期；原 Action 标签来自本次真实开工 M。
    action.start_tick = at.value.half_day_tick;
    auto *participation =
        growth_start_owner->growth_start_participation_writer();
    if (participation && !participation->bound_to(w.common, w.growth))
      return failed<ActionStarted>(
          common_failure("GrowthActualStartParticipationCandidateMismatch", {},
                         ErrorKind::internal));
    auto ready = validate_growth_start_participation_writer(
        w.common, w.growth, body.value.catalog, participation);
    if (!ready.ok)
      return failed<ActionStarted>(ready);
    growth_exact_start.emplace(GrowthExactStartContext{
        body.value.catalog, std::move(at.value), participation});
  }
  std::optional<GrowthTeachingStartAdmission> teaching_admission;
  if (const auto *growth = std::get_if<GrowthQuote>(&q.data)) {
    auto access = w.services.growth_access();
    auto prepared = prepare_growth_start(w.common, *w.catalog->growth, access,
                                         actor, *growth);
    if (!prepared.ok)
      return {false, {}, prepared.error};
    teaching_admission = std::move(prepared.value);
  }
  // 同 M 读窗必须先于原 Military lease 声明；原 Common adopt 后不重建全图。
  std::unique_ptr<PhaseCurrentReadScope> field_start_phase;
  std::shared_ptr<MilitaryExactWriteLease> field_start_lease;
  if (std::holds_alternative<MilitaryQuote>(q.data)) {
    const auto *military = std::get_if<MilitaryCommand>(&command.data);
    if (military && native_field_elapsed_action_selected(w, *military)) {
      auto at = w.actual_phase_moment();
      if (!at.ok)
        return {false, {}, std::move(at.error)};
      if (!w.has_active_read_phase()) {
        auto phase = w.enter_phase_read_context(
            "field_military_start", "military", {at.value, at.value}, at.value);
        if (!phase.ok)
          return {false, {}, std::move(phase.error)};
        field_start_phase = std::move(phase.value);
      }
      auto lease = prepare_native_field_elapsed_action_start(w, *military);
      if (!lease.ok)
        return {false, {}, std::move(lease.error)};
      if (!lease.value)
        return failed<ActionStarted>(common_failure(
            "FieldActionOriginalWriterRequired", {}, ErrorKind::internal));
      field_start_lease = std::move(lease.value);
      action.start_tick = at.value.half_day_tick;
    }
  }
  // 真实 exact 开工准入已绑定同候选；仅原 trade 载荷和 History2 选型补资源边界。
  // Director/嵌套调用已持有的捕获不重开、不清前缀，也不夺取其最终覆盖检查。
  std::optional<ResourceExecutionCoverageBoundary> growth_trade_coverage;
  bool selected_growth_trade = false;
  const auto *growth_trade = std::get_if<GrowthQuote>(&q.data);
  const auto *growth_command = std::get_if<GrowthCommand>(&command.data);
  if (growth_exact_start && growth_trade && growth_command &&
      command.kind == "trade" && growth_command->kind == "trade" &&
      growth_trade->command.kind == "trade" &&
      std::holds_alternative<TradeTerms>(growth_trade->data)) {
    auto market = selected_native_growth_market_version(w.catalog->identity);
    if (!market.ok)
      return {false, {}, std::move(market.error)};
    selected_growth_trade = market.value == 2;
    if (selected_growth_trade &&
        !w.common.resource_execution_capture_identity()) {
      auto opened = begin_resource_execution_coverage(w.common);
      if (!opened.ok)
        return {false, {}, std::move(opened.error)};
      growth_trade_coverage.emplace(std::move(opened.value));
    }
  }
  auto id = w.common.next_id("action");
  if (!id.ok)
    return {false, {}, id.error};
  action.id = id.value;
  // 原行动采用前保存真实输入切面；采用成功后、领域写入前才开启局部寿命监视。
  if(capture) {auto pending=capture->prepare_pending_growth_start(action);
    if(!pending.ok)return failed<ActionStarted>(pending);}
  auto status = adopt_started_action_in_candidate(w.common, action);
  if (!status.ok)
    return failed<ActionStarted>(status);
  if(capture) {auto pending=capture->observe_pending_growth_adoption(action.id);
    if(!pending.ok)return failed<ActionStarted>(pending);}
  if (auto *travel = std::get_if<TravelQuote>(&q.data)) {
    // 原道路开工先保留命令destination/via的存在性和顺序，再由行旅域追加运行字段。
    const auto *payload = std::get_if<TravelCommandPayload>(&command.data);
    const auto *source =
        payload ? std::get_if<TravelCommand>(payload) : nullptr;
    status = begin_selected_travel(w, action.id, *travel, source);
    if (status.ok)
      status = record_naval_voyage_world(w, voyage, action.id);
  } else if (auto *population = std::get_if<PopulationMilitaryQuote>(&q.data)) {
    auto context = w.population_write();
    status = start_population_military(
        context, action, std::get<PopulationMilitaryCommand>(command.data),
        *population);
  } else if (auto *military = std::get_if<MilitaryQuote>(&q.data)) {
    if (field_start_lease) {
      status = start_native_field_elapsed_action_in_candidate(
          w, *field_start_lease, action,
          std::get<MilitaryCommand>(command.data), *military);
    } else {
      auto ia = w.services.interaction_access();
      auto ma = w.services.military_access();
      ma.interaction_access = &ia;
      status = start_military_action(
          w.common, w.military, *w.catalog->military, ma, action,
          std::get<MilitaryCommand>(command.data), *military);
    }
  } else if (auto *task = std::get_if<TaskQuote>(&q.data)) {
    auto returns = TaskMessageReturnWorldAccess::create(w);
    if (!returns.ok)
      return {false, {}, returns.error};
    status = start_task_action(
        w.common, w.tasks, *w.catalog->tasks, action,
        std::get<TaskCommand>(command.data), *task,
        w.dynamic_enabled() ? &w.affairs : nullptr, takeover.value->tasks(),
        returns.value ? &returns.value->services() : nullptr);
  } else if (auto *personal = std::get_if<TaskPersonalMessageQuote>(&q.data))
    status = start_task_personal_message_world(w, action, *personal,
                                               takeover.value->tasks());
  else if (auto *report = std::get_if<TaskPersonalReportQuote>(&q.data))
    status = start_task_personal_report_world(w, action, *report,
                                              takeover.value->tasks());
  else if (auto *acceptance = std::get_if<TaskPersonalAcceptanceQuote>(&q.data))
    status = start_task_personal_acceptance_world(w, action, *acceptance,
                                                  takeover.value->tasks());
  else if (auto *estate = std::get_if<EstateQuote>(&q.data)) {
    auto access = w.services.estate_access();
    status = begin_estate_build(w.common, w.estate, *w.catalog->estate,
                                *w.catalog->common, w.services, action, *estate,
                                &access);
  } else if (auto *relief = std::get_if<ChildCareReliefCommandQuote>(&q.data))
    status = start_child_care_relief_action_in_candidate(
        w.common, *w.catalog->interaction, *w.catalog->family_care_relief,
        w.family_birth, w.family_life, w.child_care_relief,
        w.family_care_relief_access(), action, *relief->care);
  else if (auto *birth = std::get_if<FamilyBirthQuote>(&q.data))
    status = start_child_care_action_in_candidate(
        w.common, *w.catalog->interaction, *w.catalog->family,
        *w.catalog->family_birth, w.family_birth, w.family_birth_access(),
        action, *birth->care);
  else if (auto *family = std::get_if<FamilyQuote>(&q.data)) {
    auto ia = w.services.interaction_access();
    auto family_sources = NativeFamilyInitialSourcesLease::create(w);
    if (!family_sources.ok)
      return {false, {}, std::move(family_sources.error)};
    auto fa = w.services.family_access(
        ia, family_sources.value ? &family_sources.value->origins() : nullptr,
        family_sources.value ? &family_sources.value->initial_age() : nullptr);
    status = start_family_action(
        w.common, w.family, *w.catalog->family, *w.catalog->common, w.estate,
        *w.catalog->estate, fa, action, std::get<FamilyCommand>(command.data),
        *family);
  } else if (auto *growth = std::get_if<GrowthQuote>(&q.data)) {
    auto ga = w.services.growth_access();
    status =
        growth_exact_start
            ? start_growth_at_moment(
                  w.common, w.growth, *w.catalog->growth, *w.catalog->common,
                  w.social, ga, action, *growth,
                  teaching_admission ? &*teaching_admission : nullptr,
                  *growth_exact_start)
            : start_growth(w.common, w.growth, *w.catalog->growth,
                           *w.catalog->common, w.social, ga, action, *growth,
                           teaching_admission ? &*teaching_admission : nullptr);
  }
  if (!status.ok)
    return failed<ActionStarted>(status);
  // 原业务已经成功，立即保护真实Common头，不让后续保存点重建同ID冒充它。
  if(capture) {auto pending=capture->observe_pending_growth_business(action.id);
    if(!pending.ok)return failed<ActionStarted>(pending);}
  status = authorize_task_advance_after_accept(w, action.id);
  if (!status.ok)
    return failed<ActionStarted>(status);
  // 原 FIFO apply、生命活动登记和市场 original_record/commit_trade 必须共用同一
  // 活捕获；原业务 scope 已关闭且完整覆盖成功后才允许外层继续封存命令结果。
  if (selected_growth_trade &&
      (w.growth_market_capture.open_scopes != 0 ||
       w.growth_market_capture.unbound_business))
    return failed<ActionStarted>(
        common_failure("GrowthMarketUnboundBusiness", {}, ErrorKind::internal));
  if (growth_trade_coverage) {
    auto covered =
        finish_resource_execution_coverage(w.common, *growth_trade_coverage);
    if (!covered.ok)
      return {false, {}, std::move(covered.error)};
  }
  return {true, {action.id}, {}};
}
// 保留原四参入口及其链接符号；未持有候选捕获的旧调用只委托原业务路径。
Outcome<ActionStarted> begin_action(WorldBundle &w, const StableId &actor,
                                    const CommandPayload &command,
                                    const QuoteDetails &quote) {
  return begin_action(w, actor, command, quote, nullptr);
}
// 原MilitaryCoordination.journey：先核目的地，再规划，最后重定向或新建真实行动。
Status begin_requested_military_journey(WorldBundle &w,
                                        const MilitaryJourneyRequest &request) {
  auto army = std::find_if(
      w.military.armies.begin(), w.military.armies.end(),
      [&](const auto &value) { return value.id == request.army_id; });
  if (army == w.military.armies.end())
    return common_failure("UnknownArmy", request.army_id, ErrorKind::internal);
  const auto actor_id = army->commander_actor_id;
  auto field = w.services.field_battle_bound(actor_id);
  if (!field.ok)
    return {false, {}, field.error};
  if (field.value)
    return common_failure("FieldBattleMovementBlocked", actor_id);
  if (army_at_settlement(w.common, w.military, *w.catalog->travel,
                         request.army_id, request.destination_settlement_id))
    return common_success();
  auto quote = plan_travel(w.common, w.travel, *w.catalog->travel, w.services,
                           actor_id, request.destination_settlement_id);
  if (!quote.ok)
    return {false, {}, quote.error};
  const auto *actor = w.common.actor(actor_id);
  if (!actor)
    return common_failure("UnknownActor", actor_id, ErrorKind::internal);
  const auto action_id = actor->action_id;
  if (!action_id.empty()) {
    const auto *action = w.common.action(action_id);
    if (!action)
      return common_failure("UnknownAction", action_id, ErrorKind::internal);
    if (action->kind != "travel")
      return common_failure("MilitaryCommanderBusy");
    auto voyage = prepare_naval_voyage_world(w, actor_id, quote.value);
    if (!voyage.ok)
      return {false, {}, voyage.error};
    auto started = begin_selected_travel(w, action_id, quote.value);
    return started.ok ? record_naval_voyage_world(w, voyage.value, action_id)
                      : started;
  }
  CommandPayload command{"travel", TravelCommandPayload{TravelCommand{
                                       request.destination_settlement_id, {}}}};
  auto begun = begin_action(w, actor_id, command,
                            QuoteDetails{"travel", std::move(quote.value)});
  return {begun.ok, {}, begun.error};
}
static Outcome<ReceiptDetails> apply_command_domain(
    WorldBundle &w, const StableId &id, const PreparedDomainOp &op,
    const CompleteExactWorldReadBindings *actual, bool actual_custody,
    CompleteExactCommandCapture *capture) {
  if (native_field_army_exit_command_kind(op.command.kind) ||
      std::holds_alternative<FieldArmyExitSelect>(op.command.data))
    return apply_native_field_army_exit_command(w, id, op, actual);
  if (native_work_lifecycle_command_payload(op.command))
    return apply_native_work_lifecycle_command(w, id, op);
  if (native_control_cancellation_command_kind(op.command.kind) ||
      std::holds_alternative<CancelPendingRetirement>(op.command.data) ||
      std::holds_alternative<CancelContinuationBoundary>(op.command.data))
    return apply_native_control_cancellation(w, id, op, actual);
  const auto *repair_service =
      std::get_if<RepairServiceCommandRequest>(&op.command.data);
  if (repair_service_command_kind(op.command.kind) !=
          (repair_service != nullptr) ||
      (repair_service &&
       ((op.command.kind == "offer_repair_service") !=
        std::holds_alternative<OfferRepairServiceRequest>(*repair_service))))
    return failed<ReceiptDetails>(
        common_failure("InvalidRepairServiceCommand"));
  if (repair_service)
    return failed<ReceiptDetails>(
        common_failure("RepairServiceOriginalWorldOwnerRequired", {},
                       ErrorKind::unsupported_capability));
  if (const auto *c =
          std::get_if<PrivateShopLiquidationCommand>(&op.command.data)) {
    if (private_shop_liquidation_command_kind(*c) != op.command.kind ||
        !std::holds_alternative<PrivateShopLiquidationQuote>(op.quote.data))
      return failed<ReceiptDetails>(
          common_failure("InvalidPrivateShopLiquidationCommand"));
  } else if ((op.command.kind == "private_shop_appoint_executor" ||
              op.command.kind == "private_shop_accept_executorship" ||
              op.command.kind == "private_shop_revoke_executor" ||
              op.command.kind == "private_shop_estate_close"))
    return failed<ReceiptDetails>(
        common_failure("InvalidPrivateShopLiquidationCommand"));
  if ((op.command.kind == task_shop_order_estate_cancel_command) !=
          std::holds_alternative<TaskShopOrderEstateCancel>(op.command.data) ||
      (op.command.kind == task_shop_order_estate_cancel_command &&
       !std::holds_alternative<TaskShopOrderEstateCancellationQuote>(
           op.quote.data)))
    return failed<ReceiptDetails>(
        common_failure("InvalidTaskShopOrderEstateCommand"));
  if (const auto *c =
          std::get_if<PrivateShopDailyRentCommand>(&op.command.data)) {
    if (private_shop_daily_rent_command_kind(*c) != op.command.kind ||
        !std::holds_alternative<PrivateShopDailyRentQuote>(op.quote.data))
      return failed<ReceiptDetails>(
          common_failure("InvalidPrivateShopDailyRentCommand"));
  } else if (private_shop_daily_rent_command_kind(op.command.kind))
    return failed<ReceiptDetails>(
        common_failure("InvalidPrivateShopDailyRentCommand"));
  if ((op.command.kind == household_report_command_kind) !=
          std::holds_alternative<TellHouseholdPurchaseMarketReport>(
              op.command.data) ||
      (op.command.kind == household_report_command_kind &&
       !std::holds_alternative<NativeHouseholdReportCommandQuote>(
           op.quote.data)))
    return failed<ReceiptDetails>(
        common_failure("InvalidHouseholdReportCommand"));
  if (const auto *c =
          std::get_if<HouseholdPurchaseOriginalCommand>(&op.command.data)) {
    const auto expected =
        std::holds_alternative<HouseholdPurchasePolicyCommand1>(*c)
            ? household_purchase_policy_command_kind
            : household_purchase_plan_command_kind;
    if (op.command.kind != expected ||
        !std::holds_alternative<NativeHouseholdPurchaseCommandQuote>(
            op.quote.data))
      return failed<ReceiptDetails>(
          common_failure("InvalidHouseholdPurchaseCommand"));
  } else if (op.command.kind == household_purchase_policy_command_kind ||
             op.command.kind == household_purchase_plan_command_kind)
    return failed<ReceiptDetails>(
        common_failure("InvalidHouseholdPurchaseCommand"));
  if (const auto *c = std::get_if<PrivateShopCommand>(&op.command.data)) {
    if (private_shop_command_kind(*c) != op.command.kind ||
        !std::holds_alternative<PrivateShopCommandQuote>(op.quote.data))
      return failed<ReceiptDetails>(
          common_failure("InvalidPrivateShopCommand"));
  } else if (private_shop_command_kind(op.command.kind))
    return failed<ReceiptDetails>(common_failure("InvalidPrivateShopCommand"));
  if (const auto *c = std::get_if<TaskShopOrderCommand>(&op.command.data)) {
    if (task_shop_order_command_kind(*c) != op.command.kind ||
        !std::holds_alternative<TaskShopOrderQuote>(op.quote.data))
      return failed<ReceiptDetails>(
          common_failure("InvalidTaskShopOrderCommand"));
  } else if ((op.command.kind == "task_shop_order_accept" ||
              op.command.kind == "task_shop_order_deliver" ||
              op.command.kind == "task_shop_order_cancel"))
    return failed<ReceiptDetails>(
        common_failure("InvalidTaskShopOrderCommand"));
  if (const auto *c =
          std::get_if<PoliticalAcceptShopRegistry>(&op.command.data)) {
    if (op.command.kind != political_shop_registry_accept_command ||
        !std::holds_alternative<PoliticalShopRegistryAcceptanceQuote>(
            op.quote.data))
      return failed<ReceiptDetails>(
          common_failure("InvalidPoliticalShopRegistryCommand"));
  } else if (op.command.kind == political_shop_registry_accept_command)
    return failed<ReceiptDetails>(
        common_failure("InvalidPoliticalShopRegistryCommand"));
  if (const auto *c =
          std::get_if<PoliticalContinueGovernance>(&op.command.data)) {
    if (op.command.kind != political_governance_continue_command ||
        !std::holds_alternative<PoliticalGovernanceContinuationQuote>(
            op.quote.data))
      return failed<ReceiptDetails>(
          common_failure("InvalidGovernanceContinuationCommand"));
  } else if (op.command.kind == political_governance_continue_command)
    return failed<ReceiptDetails>(
        common_failure("InvalidGovernanceContinuationCommand"));
  if (const auto *c =
          std::get_if<GovernanceInvitationReplyCommand>(&op.command.data)) {
    if (op.command.kind != governance_invitation_reply_command_kind(*c) ||
        !std::holds_alternative<GovernanceInvitationReplyQuote>(op.quote.data))
      return failed<ReceiptDetails>(
          common_failure("InvalidGovernanceInvitationCommand"));
  } else if (is_governance_invitation_reply_command(op.command.kind))
    return failed<ReceiptDetails>(
        common_failure("InvalidGovernanceInvitationCommand"));
  if (const auto *c =
          std::get_if<PoliticalHeadHandoverCommand>(&op.command.data)) {
    if (op.command.kind != political_head_handover_command_kind(*c) ||
        !std::holds_alternative<PoliticalHeadHandoverQuote>(op.quote.data))
      return failed<ReceiptDetails>(
          common_failure("InvalidPoliticalHeadHandoverCommand"));
  } else if (is_political_head_handover_command(op.command.kind))
    return failed<ReceiptDetails>(
        common_failure("InvalidPoliticalHeadHandoverCommand"));
  if (const auto *c =
          std::get_if<PoliticalSuccessorRoundCommand>(&op.command.data)) {
    if (op.command.kind != political_successor_round_command_kind(*c) ||
        !std::holds_alternative<PoliticalSuccessorRoundQuote>(op.quote.data))
      return failed<ReceiptDetails>(
          common_failure("InvalidPoliticalSuccessorRoundCommand"));
  } else if (is_political_successor_round_command(op.command.kind))
    return failed<ReceiptDetails>(
        common_failure("InvalidPoliticalSuccessorRoundCommand"));
  if (const auto *c =
          std::get_if<PoliticalTermHandoverCommand>(&op.command.data)) {
    if (op.command.kind != political_term_handover_command_kind(*c) ||
        !std::holds_alternative<PoliticalTermHandoverQuote>(op.quote.data))
      return failed<ReceiptDetails>(
          common_failure("InvalidPoliticalTermHandoverCommand"));
  } else if (is_political_term_handover_command(op.command.kind))
    return failed<ReceiptDetails>(
        common_failure("InvalidPoliticalTermHandoverCommand"));
  if (const auto *c =
          std::get_if<PoliticalOfficeReceptionCommand>(&op.command.data)) {
    if (op.command.kind != political_office_reception_command_kind(*c) ||
        !std::holds_alternative<PoliticalOfficeReceptionQuote>(op.quote.data))
      return failed<ReceiptDetails>(
          common_failure("InvalidPoliticalOfficeReceptionCommand"));
  } else if (is_political_office_reception_command(op.command.kind))
    return failed<ReceiptDetails>(
        common_failure("InvalidPoliticalOfficeReceptionCommand"));
  if (const auto *c =
          std::get_if<PoliticalOfficeResignationCommand>(&op.command.data)) {
    if (op.command.kind != political_office_resignation_command_kind(*c) ||
        !std::holds_alternative<PoliticalOfficeResignationQuote>(op.quote.data))
      return failed<ReceiptDetails>(
          common_failure("InvalidPoliticalOfficeResignationCommand"));
  } else if (std::holds_alternative<AuthoredOfficeResignationCommand>(
                 op.command.data)) {
    if (op.command.kind != political_office_resignation_answer_command ||
        !std::holds_alternative<PoliticalOfficeResignationQuote>(op.quote.data))
      return failed<ReceiptDetails>(
          common_failure("InvalidPoliticalOfficeResignationCommand"));
  } else if (is_political_office_resignation_command(op.command.kind))
    return failed<ReceiptDetails>(
        common_failure("InvalidPoliticalOfficeResignationCommand"));
  if (const auto *c = std::get_if<PoliticalSubordinateAppointmentCommand>(
          &op.command.data)) {
    if (op.command.kind != political_subordinate_appointment_command_kind(*c) ||
        !std::holds_alternative<PoliticalSubordinateAppointmentQuote>(
            op.quote.data))
      return failed<ReceiptDetails>(
          common_failure("InvalidPoliticalSubordinateAppointmentCommand"));
  } else if (std::holds_alternative<AuthoredSubordinateAppointmentCommand>(
                 op.command.data)) {
    if (op.command.kind !=
            political_subordinate_appointment_command_kind(
                std::get<AuthoredSubordinateAppointmentCommand>(op.command.data)
                    .command) ||
        !std::holds_alternative<PoliticalSubordinateAppointmentQuote>(
            op.quote.data))
      return failed<ReceiptDetails>(
          common_failure("InvalidPoliticalSubordinateAppointmentCommand"));
  } else if (is_political_subordinate_appointment_command(op.command.kind))
    return failed<ReceiptDetails>(
        common_failure("InvalidPoliticalSubordinateAppointmentCommand"));
  if (const auto *c =
          std::get_if<PoliticalGovernanceAgencyCommand>(&op.command.data)) {
    if (op.command.kind != political_governance_agency_command_kind(*c) ||
        !std::holds_alternative<PoliticalGovernanceAgencyQuote>(op.quote.data))
      return failed<ReceiptDetails>(
          common_failure("InvalidPoliticalGovernanceAgencyCommand"));
  } else if (const auto *c = std::get_if<AuthoredGovernanceAgencyCommand>(
                 &op.command.data)) {
    if (op.command.kind !=
            political_governance_agency_command_kind(c->command) ||
        !std::holds_alternative<PoliticalGovernanceAgencyQuote>(op.quote.data))
      return failed<ReceiptDetails>(
          common_failure("InvalidPoliticalGovernanceAgencyCommand"));
  } else if (is_political_governance_agency_command(op.command.kind))
    return failed<ReceiptDetails>(
        common_failure("InvalidPoliticalGovernanceAgencyCommand"));
  if (const auto *c =
          std::get_if<PoliticalSuccessionCommand>(&op.command.data)) {
    if (op.command.kind != political_succession_command_kind(*c) ||
        !std::holds_alternative<PoliticalSuccessionQuote>(op.quote.data))
      return failed<ReceiptDetails>(
          common_failure("InvalidPoliticalSuccessionCommand"));
  } else if (is_political_succession_command(op.command.kind))
    return failed<ReceiptDetails>(
        common_failure("InvalidPoliticalSuccessionCommand"));
  if (const auto *c =
          std::get_if<PoliticalShopLicensingCommand>(&op.command.data)) {
    const auto *owned =
        std::get_if<PoliticalShopLicensingPreparedQuote>(&op.quote.data);
    const bool quote_shape =
        w.political_shop_licensing.version == 2
            ? owned && bool(*owned)
            : std::holds_alternative<PoliticalShopLicensingQuote>(
                  op.quote.data);
    if (political_shop_licensing_command_kind(*c) != op.command.kind ||
        !quote_shape)
      return failed<ReceiptDetails>(
          common_failure("InvalidPoliticalShopLicensingCommand"));
  } else if (political_shop_licensing_command_kind(op.command.kind))
    return failed<ReceiptDetails>(
        common_failure("InvalidPoliticalShopLicensingCommand"));
  if ((op.command.kind == "political_issue_supply_policy") !=
          std::holds_alternative<IssueEconomicSupplyPolicyCommand>(
              op.command.data) ||
      (op.command.kind == "political_issue_supply_policy" &&
       !std::holds_alternative<EconomicSupplyPolicyQuote>(op.quote.data)))
    return failed<ReceiptDetails>(
        common_failure("InvalidEconomicSupplyPolicyCommand"));
  const auto *lesson = std::get_if<GrowthTacticLessonCommand>(&op.command.data);
  if ((op.command.kind.rfind("tactic_lesson_", 0) == 0) !=
          (lesson != nullptr) ||
      (lesson && (lesson->kind != op.command.kind ||
                  !std::holds_alternative<GrowthTacticLessonCommandQuote>(
                      op.quote.data))))
    return failed<ReceiptDetails>(common_failure("InvalidTacticLessonCommand"));
  const auto *birth = std::get_if<FamilyBirthCommand>(&op.command.data);
  if (family_birth_command_kind(op.command.kind) != (birth != nullptr) ||
      (birth && (birth->kind != op.command.kind ||
                 !std::holds_alternative<FamilyBirthQuote>(op.quote.data))))
    return failed<ReceiptDetails>(common_failure("InvalidFamilyBirthCommand"));
  const auto *visit = std::get_if<VisitMessageCommand>(&op.command.data);
  if (visit_message_command_kind(op.command.kind) != (visit != nullptr) ||
      (visit && (visit_message_command_kind(*visit) != op.command.kind ||
                 !std::holds_alternative<VisitMessageQuote>(op.quote.data))))
    return failed<ReceiptDetails>(common_failure("InvalidVisitMessageCommand"));
  const auto *extension = std::get_if<TaskExtensionCommand>(&op.command.data);
  if ((op.command.kind == task_extension_command_kind) !=
          (extension != nullptr) ||
      (extension && !std::holds_alternative<TaskExtensionQuote>(op.quote.data)))
    return failed<ReceiptDetails>(
        common_failure("InvalidTaskExtensionCommand"));
  const auto *appointment =
      std::get_if<PersonalAppointmentCommand>(&op.command.data);
  if (is_personal_appointment_command(op.command.kind) !=
          (appointment != nullptr) ||
      (appointment &&
       (personal_appointment_command_kind(*appointment) != op.command.kind ||
        !std::holds_alternative<PersonalAppointmentQuote>(op.quote.data))))
    return failed<ReceiptDetails>(
        common_failure("InvalidPersonalAppointmentCommand"));
  const auto *inquiry = std::get_if<ContactInquiryRequest>(&op.command.data);
  if ((op.command.kind == contact_inquiry_action_kind) !=
          (inquiry != nullptr) ||
      (inquiry && !std::holds_alternative<ContactInquiryQuote>(op.quote.data)))
    return failed<ReceiptDetails>(
        common_failure("InvalidContactInquiryCommand"));
  const auto *message_return =
      std::get_if<TaskMessageReturnCollect>(&op.command.data);
  if ((op.command.kind == task_message_return_collect_command) !=
          (message_return != nullptr) ||
      (message_return &&
       !std::holds_alternative<TaskMessageReturnQuote>(op.quote.data)))
    return failed<ReceiptDetails>(
        common_failure("InvalidTaskMessageReturnCommand"));
  const auto *personal_message =
      std::get_if<TaskPersonalMessageDeliver>(&op.command.data);
  if ((op.command.kind == task_personal_message_command) !=
          (personal_message != nullptr) ||
      (personal_message &&
       !std::holds_alternative<TaskPersonalMessageQuote>(op.quote.data)))
    return failed<ReceiptDetails>(
        common_failure("InvalidTaskPersonalMessageCommand"));
  const auto *personal_report =
      std::get_if<TaskPersonalReport>(&op.command.data);
  if ((op.command.kind == task_personal_report_command) !=
          (personal_report != nullptr) ||
      (personal_report &&
       !std::holds_alternative<TaskPersonalReportQuote>(op.quote.data)))
    return failed<ReceiptDetails>(
        common_failure("InvalidTaskPersonalReportCommand"));
  const auto *personal_acceptance =
      std::get_if<TaskPersonalAcceptance>(&op.command.data);
  if ((op.command.kind == task_personal_acceptance_command) !=
          (personal_acceptance != nullptr) ||
      (personal_acceptance &&
       !std::holds_alternative<TaskPersonalAcceptanceQuote>(op.quote.data)))
    return failed<ReceiptDetails>(
        common_failure("InvalidTaskPersonalAcceptanceCommand"));
  const auto *personal_cancel =
      std::get_if<TaskPersonalCancel>(&op.command.data);
  if ((op.command.kind == task_personal_cancellation_command) !=
          (personal_cancel != nullptr) ||
      (personal_cancel &&
       !std::holds_alternative<TaskPersonalCancellationQuote>(op.quote.data)))
    return failed<ReceiptDetails>(
        common_failure("InvalidTaskPersonalCancellationCommand"));
  const auto *visit_reply = std::get_if<VisitReplyCommand>(&op.command.data);
  if (visit_reply_command_kind(op.command.kind) != (visit_reply != nullptr) ||
      (visit_reply &&
       (visit_reply_command_kind(*visit_reply) != op.command.kind ||
        !std::holds_alternative<VisitReplyQuote>(op.quote.data))))
    return failed<ReceiptDetails>(common_failure("InvalidVisitReplyCommand"));
  const auto *visit_invitation =
      std::get_if<VisitAppointmentCommand>(&op.command.data);
  if (is_visit_appointment_command(op.command.kind) !=
          (visit_invitation != nullptr) ||
      (visit_invitation &&
       visit_appointment_command_kind(*visit_invitation) != op.command.kind))
    return failed<ReceiptDetails>(
        common_failure("InvalidVisitAppointmentCommand"));
  const auto *visit_meeting =
      std::get_if<VisitAppointmentMeetingCommand>(&op.command.data);
  if (is_visit_appointment_meeting_command(op.command.kind) !=
          (visit_meeting != nullptr) ||
      (visit_meeting && visit_appointment_meeting_command_kind(
                            *visit_meeting) != op.command.kind))
    return failed<ReceiptDetails>(
        common_failure("InvalidVisitAppointmentCommand"));
  const auto *authored_invitation =
      std::get_if<AuthoredVisitAppointment>(&op.command.data);
  if ((op.command.kind == authored_visit_appointment_command) !=
      (authored_invitation != nullptr))
    return failed<ReceiptDetails>(
        common_failure("InvalidVisitAppointmentCommand"));
  if ((visit_invitation &&
       !std::holds_alternative<VisitAppointmentQuote>(op.quote.data)) ||
      (visit_meeting &&
       !std::holds_alternative<VisitAppointmentMeetingQuote>(op.quote.data)) ||
      (authored_invitation &&
       !std::holds_alternative<AuthoredVisitAppointmentQuote>(op.quote.data)))
    return failed<ReceiptDetails>(
        common_failure("InvalidVisitAppointmentCommand"));
  const auto *born_dialogue =
      std::get_if<OpenBornPersonalDialogue>(&op.command.data);
  if ((op.command.kind == "open_born_personal_dialogue") !=
          (born_dialogue != nullptr) ||
      (born_dialogue &&
       !std::holds_alternative<BornPersonalDialogueQuote>(op.quote.data)))
    return failed<ReceiptDetails>(
        common_failure("InvalidBornPersonalDialogueCommand"));
  const auto *known_dialogue =
      std::get_if<OpenKnownPersonalDialogue>(&op.command.data);
  if ((op.command.kind == "open_known_personal_dialogue") !=
          (known_dialogue != nullptr) ||
      (known_dialogue &&
       !std::holds_alternative<KnownPersonalDialogueQuote>(op.quote.data)))
    return failed<ReceiptDetails>(
        common_failure("InvalidKnownPersonalDialogueCommand"));
  const auto *authored_reply =
      std::get_if<AuthoredVisitReply>(&op.command.data);
  if ((op.command.kind == authored_visit_reply_command) !=
          (authored_reply != nullptr) ||
      (authored_reply &&
       !std::holds_alternative<AuthoredVisitReplyQuote>(op.quote.data)))
    return failed<ReceiptDetails>(
        common_failure("InvalidAuthoredVisitReplyCommand"));
  const auto *employment =
      std::get_if<EstateEmploymentCommand>(&op.command.data);
  const bool employment_kind =
      op.command.kind.rfind("estate_employment_", 0) == 0;
  if (employment_kind != (employment != nullptr) ||
      (employment &&
       estate_employment_command_kind(*employment) != op.command.kind) ||
      (employment_kind &&
       !std::holds_alternative<EstateEmploymentQuote>(op.quote.data)))
    return failed<ReceiptDetails>(
        common_failure("InvalidEstateEmploymentCommand"));
  const auto *apprentice = std::get_if<ApprenticeshipCommand>(&op.command.data);
  if (apprenticeship_command_kind(op.command.kind) != (apprentice != nullptr) ||
      (apprentice &&
       (apprentice->kind != op.command.kind ||
        !std::holds_alternative<ApprenticeshipQuote>(op.quote.data))) ||
      (!apprentice && op.command.kind.rfind("family_apprenticeship_", 0) == 0))
    return failed<ReceiptDetails>(
        common_failure("InvalidApprenticeshipCommand"));
  const auto *marriage = std::get_if<FamilyMarriageCommand>(&op.command.data);
  const bool marriage_kind = op.command.kind == "family_marriage_receive" ||
                             op.command.kind == "family_marriage_reply" ||
                             op.command.kind == "family_marriage_dissolve";
  if (marriage_kind != (marriage != nullptr) ||
      (marriage && marriage->kind != op.command.kind) ||
      (marriage_kind &&
       !std::holds_alternative<FamilyMarriageQuote>(op.quote.data)))
    return failed<ReceiptDetails>(
        common_failure("InvalidFamilyMarriageCommand"));
  const auto *inherited =
      std::get_if<FamilyInheritanceCommand>(&op.command.data);
  const bool inherited_kind = op.command.kind == "family_bequest_disclose" ||
                              op.command.kind == "family_inheritance_accept" ||
                              op.command.kind == "family_inheritance_settle";
  if (inherited_kind != (inherited != nullptr) ||
      (inherited && inherited->kind != op.command.kind) ||
      (inherited_kind &&
       !std::holds_alternative<FamilyInheritanceQuote>(op.quote.data)))
    return failed<ReceiptDetails>(
        common_failure("InvalidFamilyInheritanceCommand"));
  const auto *bequest = std::get_if<FamilyBequestCommand>(&op.command.data);
  const bool bequest_kind = op.command.kind == "family_bequest_declare" ||
                            op.command.kind == "family_bequest_revoke";
  if (bequest_kind != (bequest != nullptr) ||
      (bequest && bequest->kind != op.command.kind) ||
      (bequest_kind &&
       !std::holds_alternative<FamilyBequestQuote>(op.quote.data)))
    return failed<ReceiptDetails>(
        common_failure("InvalidFamilyBequestCommand"));
  const auto *legacy_payload =
      std::get_if<FamilyLegacyCommand>(&op.command.data);
  const auto *voluntary_payload =
      std::get_if<VoluntaryPathCommand>(&op.command.data);
  const bool voluntary_kind =
      op.command.kind == "family_voluntary_path_accept" ||
      op.command.kind == "family_voluntary_path_continue" ||
      op.command.kind == "family_voluntary_path_cancel" ||
      op.command.kind == "family_voluntary_path_withdraw";
  if (voluntary_kind != (voluntary_payload != nullptr) ||
      (voluntary_payload && voluntary_payload->kind != op.command.kind) ||
      (voluntary_kind &&
       !std::holds_alternative<VoluntaryPathQuote>(op.quote.data)) ||
      (!voluntary_kind &&
       op.command.kind.rfind("family_voluntary_path_", 0) == 0))
    return failed<ReceiptDetails>(
        common_failure("InvalidVoluntaryPathCommand"));
  const auto *collateral_payload =
      std::get_if<CollateralControlCommand>(&op.command.data);
  const bool collateral_kind =
      op.command.kind == "family_collateral_recognize" ||
      op.command.kind == "family_collateral_accept" ||
      op.command.kind == "family_collateral_revoke";
  if (collateral_kind != (collateral_payload != nullptr) ||
      (collateral_payload && collateral_payload->kind != op.command.kind) ||
      (collateral_kind &&
       !std::holds_alternative<CollateralControlQuote>(op.quote.data)))
    return failed<ReceiptDetails>(
        common_failure("InvalidCollateralControlCommand"));
  const auto *biological_payload =
      std::get_if<BiologicalControlCommand>(&op.command.data);
  const bool biological_kind = op.command.kind == "family_biological_accept" ||
                               op.command.kind == "family_biological_revoke";
  if (biological_kind != (biological_payload != nullptr) ||
      (biological_payload && biological_payload->kind != op.command.kind) ||
      (biological_kind &&
       !std::holds_alternative<LegacyControlDecisionQuote>(op.quote.data)))
    return failed<ReceiptDetails>(
        common_failure("InvalidBiologicalControlCommand"));
  const bool legacy_kind = op.command.kind == "family_legacy_accept" ||
                           op.command.kind == "family_legacy_revoke" ||
                           op.command.kind == "family_legacy_activate";
  if (legacy_kind != (legacy_payload != nullptr) ||
      (legacy_payload && legacy_payload->kind != op.command.kind) ||
      (legacy_kind &&
       !std::holds_alternative<FamilyLegacyQuote>(op.quote.data)))
    return failed<ReceiptDetails>(common_failure("InvalidFamilyLegacyCommand"));
  if (const auto *repair = std::get_if<NavalRepairCommand>(&op.command.data)) {
    if (!w.naval_repairs_enabled())
      return failed<ReceiptDetails>(
          common_failure("NavalRepairProfileRequired"));
    auto ia = w.services.interaction_access();
    auto ma = w.services.military_access();
    ma.interaction_access = &ia;
    auto ended =
        apply_naval_repair(w.common, w.military, *w.catalog->military, ma,
                           w.naval, *w.catalog->naval, id, *repair);
    if (!ended.ok)
      return {false, {}, ended.error};
    if (ended.value.completed && !ended.value.replayed) {
      ActionCompletion completion;
      completion.completed = true;
      completion.success = ended.value.success;
      completion.code = ended.value.reason;
      std::vector<std::pair<StableId, std::shared_ptr<const ActionCompletion>>>
          completed{
              {ended.value.action_id,
               checkpoint_make_shared<const ActionCompletion>(completion)}};
      auto resumed = resume_completed_dialogues(
          w.common, w.interaction, *w.catalog->interaction, ia, completed);
      if (!resumed.ok)
        return {false, {}, resumed.error};
    }
    return {true, ReceiptDetails{std::move(ended.value), {}}, {}};
  }
  if (const auto *naval = std::get_if<NavalBattleCommand>(&op.command.data)) {
    if (!w.naval_combat_enabled())
      return failed<ReceiptDetails>(
          common_failure("NavalCombatProfileRequired"));
    auto ia = w.services.interaction_access();
    auto ma = w.services.military_access();
    ma.interaction_access = &ia;
    auto access = naval_command_access(w);
    auto result = apply_naval_battle_command(
        w.common, w.military, *w.catalog->military, ma, w.naval,
        *w.catalog->naval, id, *naval, access);
    if (!result.ok)
      return {false, {}, result.error};
    // 零时终止也恢复原等待对白；失败来源来自刚返回的永久事实，不能留下已删pending_action。
    if (result.value.kind == "naval_capture" ||
        result.value.kind == "naval_battle_continue") {
      auto stops = query_naval_journey_terminations(
          w.common, w.military, w.naval, result.value.receipt_id);
      if (!stops.ok)
        return {false, {}, stops.error};
      std::vector<std::pair<StableId, std::shared_ptr<const ActionCompletion>>>
          completed;
      for (const auto &stop : stops.value) {
        ActionCompletion end;
        end.completed = true;
        end.success = false;
        end.code = stop.reason;
        completed.emplace_back(
            stop.action_id,
            checkpoint_make_shared<const ActionCompletion>(std::move(end)));
      }
      auto resumed = resume_completed_dialogues(
          w.common, w.interaction, *w.catalog->interaction, ia, completed);
      if (!resumed.ok)
        return {false, {}, resumed.error};
    }
    return {true, ReceiptDetails{std::move(result.value), {}}, {}};
  }
  auto takeover = PoliticalTakeoverExecutionWorldAccess::create(w);
  if (!takeover.ok)
    return {false, {}, takeover.error};
  if (std::holds_alternative<PoliticalTakeoverDecisionCommand>(
          op.command.data)) {
    if (!w.political_takeover_enabled())
      return failed<ReceiptDetails>(
          common_failure("PoliticalTakeoverProfileRequired"));
    const auto *q = std::get_if<PoliticalTakeoverQuote>(&op.quote.data);
    if (!q)
      return failed<ReceiptDetails>(
          common_failure("InvalidPoliticalTakeoverQuote"));
    auto r = commit_political_takeover_world_command(
        w, *w.catalog->political_takeover, w.political_takeover, id, *q);
    if (!r.ok)
      return {false, {}, r.error};
    return {true, ReceiptDetails{Done{}, {}}, {}};
  }
  if (const auto *naval = std::get_if<NavalCommand>(&op.command.data)) {
    if (!w.naval_enabled())
      return failed<ReceiptDetails>(common_failure("NavalProfileRequired"));
    auto ia = w.services.interaction_access();
    auto ma = w.services.military_access();
    ma.interaction_access = &ia;
    auto r = apply_naval_command(w.common, w.military, *w.catalog->military, ma,
                                 w.naval, *w.catalog->naval, id, *naval);
    if (!r.ok)
      return {false, {}, r.error};
    return {true, ReceiptDetails{std::move(r.value), {}}, {}};
  }
  if (const auto *advance =
          std::get_if<TaskPersonalAdvanceCommand>(&op.command.data)) {
    const auto *q = std::get_if<TaskPersonalAdvanceQuote>(&op.quote.data);
    if (!q)
      return failed<ReceiptDetails>(common_failure("InvalidTaskAdvanceQuote"));
    auto r = apply_task_advance_world(w, id, *advance, *q);
    if (!r.ok)
      return failed<ReceiptDetails>(r);
    return {true, ReceiptDetails{Done{}, {}}, {}};
  }
  if (const auto *party =
          std::get_if<PartyParticipationCommand>(&op.command.data)) {
    const auto *q = std::get_if<PartyParticipationQuote>(&op.quote.data);
    if (!q)
      return failed<ReceiptDetails>(
          common_failure("PartyInvitationQuoteMismatch"));
    auto ia = w.services.interaction_access();
    auto result = apply_party_participation_command(
        w.common, w.interaction, *w.catalog->interaction,
        w.catalog->interaction->party_invitations.get(), ia, id, *party, *q);
    if (!result.ok)
      return {false, {}, result.error};
    return {true, ReceiptDetails{Done{}, {}}, {}};
  }
  if (const auto *diplomacy = std::get_if<DiplomacyCommand>(&op.command.data)) {
    if (!w.diplomacy_enabled())
      return failed<ReceiptDetails>(common_failure("DiplomacyProfileRequired"));
    const auto *q = std::get_if<DiplomacyQuote>(&op.quote.data);
    if (!q)
      return failed<ReceiptDetails>(common_failure("InvalidDiplomacyQuote"));
    auto ia = w.services.interaction_access();
    auto context = w.diplomacy_write(ia);
    auto result = apply_diplomacy_business(context, id, *diplomacy, *q);
    if (!result.ok)
      return {false, {}, result.error};
    if (!result.value.action_id.empty())
      return {
          true, ReceiptDetails{ActionStarted{result.value.action_id}, {}}, {}};
    return {true, ReceiptDetails{Done{}, {}}, {}};
  }

  if (const auto *governance =
          std::get_if<GovernanceCommand>(&op.command.data)) {
    if (!w.governance_tasks_enabled())
      return failed<ReceiptDetails>(common_failure("GovernanceNotEnabled"));
    const auto *q = std::get_if<GovernanceQuote>(&op.quote.data);
    if (!q)
      return failed<ReceiptDetails>(common_failure("InvalidGovernanceQuote"));
    auto context = w.governance_write();
    auto result = apply_governance_business(context, id, *governance, *q);
    if (!result.ok)
      return {false, {}, result.error};
    if (std::holds_alternative<GovernanceAcceptCommand>(*governance))
      return {
          true, ReceiptDetails{ActionStarted{result.value.action_id}, {}}, {}};
    return {true, ReceiptDetails{Done{}, {}}, {}};
  }
  if (const auto *politics = std::get_if<PoliticsCommand>(&op.command.data)) {
    auto pending = preview_political_subordinate_legacy_world(w, *politics);
    if (!pending.ok)
      return {false, {}, pending.error};
    if (!w.politics_enabled())
      return failed<ReceiptDetails>(common_failure("PoliticalProfileRequired"));
    const auto *q = std::get_if<PoliticsQuote>(&op.quote.data);
    if (!q)
      return failed<ReceiptDetails>(common_failure("InvalidPoliticalQuote"));
    auto context = w.politics_write();
    auto result = apply_political_business(context, id, *politics, *q);
    if (!result.ok)
      return {false, {}, result.error};
    return {true, {std::move(result.value)}, {}};
  }
  if (const auto *household =
          std::get_if<ConfigureHouseholdProvisionCommand>(&op.command.data)) {
    const auto *q =
        std::get_if<HouseholdProvisionConfigurationQuote>(&op.quote.data);
    if (!q)
      return failed<ReceiptDetails>(
          common_failure("InvalidHouseholdProvisionQuote"));
    auto checked = prepare_household_provision_configuration(w, id, *household);
    if (!checked.ok)
      return {false, {}, checked.error};
    auto old = render_boundary_json(
        encode_household_provision_configuration_quote(*q));
    auto now = render_boundary_json(
        encode_household_provision_configuration_quote(checked.value));
    if (!old.ok || !now.ok || old.value != now.value)
      return failed<ReceiptDetails>(
          common_failure("StaleHouseholdProvisionQuote"));
    auto effect = apply_household_provision_configuration(w, id, *household);
    if (!effect.ok)
      return {false, {}, effect.error};
    return {true, ReceiptDetails{std::move(effect.value), {}}, {}};
  }
  if (const auto *economic =
          std::get_if<ConfigureLocalProductionCommand>(&op.command.data)) {
    const auto *q = std::get_if<ProductionConfigurationQuote>(&op.quote.data);
    if (!q)
      return failed<ReceiptDetails>(
          common_failure("InvalidProductionConfigurationQuote"));
    auto checked = prepare_economic_configuration(w, id, *economic);
    if (!checked.ok)
      return {false, {}, checked.error};
    auto a = render_boundary_json(encode_production_configuration_quote(*q));
    auto z = render_boundary_json(
        encode_production_configuration_quote(checked.value));
    if (!a.ok || !z.ok || a.value != z.value)
      return failed<ReceiptDetails>(
          common_failure("StaleProductionConfigurationQuote"));
    auto effect = apply_economic_configuration(w, id, *economic);
    if (!effect.ok)
      return {false, {}, effect.error};
    return {true, ReceiptDetails{std::move(effect.value), {}}, {}};
  }
  if (const auto *treaty =
          std::get_if<TreatyPassageCommand>(&op.command.data)) {
    if (op.command.kind != treaty_passage_command_kind(*treaty))
      return failed<ReceiptDetails>(
          common_failure("InvalidTreatyPassageCommand"));
    const auto *q = std::get_if<TreatyPassageQuote>(&op.quote.data);
    if (!q)
      return failed<ReceiptDetails>(
          common_failure("InvalidTreatyPassageQuote"));
    auto r = commit_treaty_passage_world(w, id, *treaty, *q);
    if (!r.ok)
      return {false, {}, r.error};
    return {true, {Done{}}, {}};
  }
  if (const auto *c =
          std::get_if<PoliticalMilitaryAppointmentCommand>(&op.command.data)) {
    if (op.command.kind != political_military_appointment_command_kind(*c))
      return failed<ReceiptDetails>(
          common_failure("InvalidPoliticalMilitaryAppointmentCommand"));
    const auto *q =
        std::get_if<PoliticalMilitaryAppointmentQuote>(&op.quote.data);
    if (!q)
      return failed<ReceiptDetails>(
          common_failure("InvalidPoliticalMilitaryAppointmentQuote"));
    auto r = commit_political_military_appointment_world(w, id, *c, *q);
    if (!r.ok)
      return {false, {}, r.error};
    return {true, {Done{}}, {}};
  }
  if (const auto *c =
          std::get_if<PoliticalFieldAppointmentCommand>(&op.command.data)) {
    if (op.command.kind != political_field_appointment_command_kind(*c))
      return failed<ReceiptDetails>(
          common_failure("InvalidPoliticalFieldAppointmentCommand"));
    const auto *q = std::get_if<PoliticalFieldAppointmentQuote>(&op.quote.data);
    if (!q)
      return failed<ReceiptDetails>(
          common_failure("InvalidPoliticalFieldAppointmentQuote"));
    auto r = commit_political_field_appointment_world(w, id, *c, *q);
    if (!r.ok)
      return {false, {}, r.error};
    return {true, {Done{}}, {}};
  }
  if (const auto *fiscal =
          std::get_if<PoliticalSetOccupationTaxCommand>(&op.command.data)) {
    const auto *q = std::get_if<OccupationFiscalQuote>(&op.quote.data);
    if (!q)
      return failed<ReceiptDetails>(
          common_failure("InvalidOccupationFiscalQuote"));
    auto result = commit_occupation_fiscal_world(w, id, *fiscal, *q);
    if (!result.ok)
      return {false, {}, result.error};
    return {true, {std::move(result.value)}, {}};
  }
  if (const auto *fiscal =
          std::get_if<PoliticalSetTaxCommand>(&op.command.data)) {
    auto legacy = require_legacy_fiscal_world(w, fiscal->jurisdiction_id);
    if (!legacy.ok)
      return {false, {}, legacy.error};
    if (!w.political_fiscal_enabled())
      return failed<ReceiptDetails>(
          common_failure("PoliticalFiscalProfileRequired"));
    const auto *q = std::get_if<PoliticalFiscalQuote>(&op.quote.data);
    if (!q)
      return failed<ReceiptDetails>(
          common_failure("InvalidPoliticalFiscalQuote"));
    auto context = w.political_fiscal_write();
    auto result = apply_political_fiscal_business(context, id, *fiscal, *q);
    if (!result.ok)
      return {false, {}, result.error};
    return {true, {std::move(result.value)}, {}};
  }

  const auto &command = op.command;
  if (const auto *integration =
          std::get_if<SiegeIntegrationCommand>(&command.data)) {
    auto ia = w.services.interaction_access();
    auto ma = w.services.military_access();
    ma.interaction_access = &ia;
    SiegeIntegrationWriteContext context{
        w.common,
        w.military,
        *w.catalog->military,
        ma,
        w.military.siege_integration,
        w.catalog->military->siege_integration};
    auto effect = apply_siege_integration(context, id, *integration);
    if (!effect.ok)
      return {false, {}, effect.error};
    return {true, {std::move(effect.value), {}}, {}};
  }
  if (const auto *apply = std::get_if<SiegeIntegrationApply>(&command.data)) {
    auto effect = apply_siege_handover(
        w, id, *apply, std::get<SiegeIntegrationExecutionQuote>(op.quote.data));
    if (!effect.ok)
      return {false, {}, effect.error};
    return {true, {std::move(effect.value), {}}, {}};
  }
  if (std::holds_alternative<PopulationMilitaryCommand>(command.data)) {
    auto started = begin_action(w, id, command, op.quote, capture);
    if (!started.ok)
      return {false, {}, started.error};
    return {true, ReceiptDetails{started.value, {}}, {}};
  }
  if (const auto *organization =
          std::get_if<OrganizationContractCommand>(&command.data)) {
    auto started = start_organization_contract(w, id, *organization);
    if (!started.ok)
      return {false, {}, started.error};
    return {true, ReceiptDetails{ActionStarted{started.value}, {}}, {}};
  }
  if (const auto *plan = std::get_if<PlayerPlanQuote>(&op.quote.data))
    return apply_player_plan_command(w, id, *plan, capture);
  if (named_membership_command_kind(command.kind)) {
    auto effect = apply_named_membership(w, id, command);
    if (!effect.ok)
      return {false, {}, effect.error};
    return {true, ReceiptDetails{std::move(effect.value), {}}, {}};
  }
  if (dynamic_command_kind(command.kind)) {
    auto effect = apply_dynamic_command(w, id, command);
    if (!effect.ok)
      return {false, {}, effect.error};
    return {true, ReceiptDetails{std::move(effect.value), {}}, {}};
  }
  if (command.kind == "dialogue_confirm")
    return apply_dialogue_confirmation(w, id, op, capture);
  if (auto *personal = std::get_if<PersonalCommand>(&command.data)) {
    auto r = apply_personal_integration(w, id, *personal,
                                        std::get<PersonalQuote>(op.quote.data));
    if (!r.ok)
      return {false, {}, r.error};
    return {true, {std::move(r.value), {}}, {}};
  }
  if (auto *growth = std::get_if<GrowthCommand>(&command.data)) {
    const auto &quote = std::get<GrowthQuote>(op.quote.data);
    if (growth_action_kind(command.kind)) {
      auto r = begin_action(w, id, command, op.quote, capture);
      if (!r.ok)
        return {false, {}, r.error};
      return {true, {r.value, {}}, {}};
    }
    auto r = apply_selected_growth_instant(w, id, quote);
    if (!r.ok)
      return {false, {}, r.error};
    if (command.kind == "study_cancel" || command.kind == "rest_cancel" ||
        command.kind == "recovery_cancel") {
      auto released = release_action(w, r.value.action_id);
      if (!released.ok)
        return failed<ReceiptDetails>(released);
      if (command.kind == "recovery_cancel") {
        ActionCompletion stopped;
        stopped.completed = true;
        stopped.success = false;
        stopped.code = "RecoveryCancelled";
        auto ia = w.services.interaction_access();
        auto resumed = resume_completed_dialogues(
            w.common, w.interaction, *w.catalog->interaction, ia,
            {{r.value.action_id, checkpoint_make_shared<const ActionCompletion>(
                                     std::move(stopped))}});
        if (!resumed.ok)
          return failed<ReceiptDetails>(resumed);
      }
    }
    if (command.kind == "equip_weapon" || command.kind == "equip_armor" ||
        command.kind == "configure_heart")
      return {true, {}, {}};
    (void)growth;
    return {true, {std::move(r.value), {}}, {}};
  }
  if (std::holds_alternative<AdvanceCommand>(command.data) ||
      (std::holds_alternative<WaitCommand>(command.data) &&
       (actual_custody ||
        std::holds_alternative<InArmyLocation>(w.common.actor(id)->location) ||
        std::holds_alternative<FollowingLocation>(
            w.common.actor(id)->location)))) {
    const auto *advance = std::get_if<AdvanceCommand>(&command.data);
    auto r =
        w.catalog->exact_time
            ? advance_exact_world(w, advance ? advance->requested_to
                                             : std::optional<WorldMoment>{})
            : advance_world(w);
    if (!r.ok)
      return {false, {}, r.error};
    return {true, {std::move(r.value), {}}, {}};
  }
  if (std::holds_alternative<WaitCommand>(command.data) ||
      command.kind == "travel" || command.kind == "office_accept" ||
      command.kind == "army_form" || command.kind == "accept_offer" ||
      command.kind == "report_offer" || command.kind == "deliver_message" ||
      command.kind == "estate_build" || command.kind == "family_adopt" ||
      command.kind == "army_return" || command.kind == "army_resupply" ||
      command.kind == "military_train") {
    auto r = begin_action(w, id, command, op.quote, capture);
    if (!r.ok)
      return {false, {}, r.error};
    return {true, {r.value, {}}, {}};
  }
  if (auto *travel = std::get_if<TravelCommandPayload>(&command.data)) {
    if (std::holds_alternative<RescueCommand>(*travel)) {
      auto r = begin_rescue(w.common, w.travel, id);
      if (!r.ok)
        return failed<ReceiptDetails>(r);
      return {true, {}, {}};
    }
    if (auto *move = std::get_if<MoveLocalCommand>(travel)) {
      auto r = apply_native_local_move_in_candidate(w, id, move->place_id, actual);
      if (!r.ok)
        return failed<ReceiptDetails>(r);
      auto ia = w.services.interaction_access();
      r = after_local_move(w.common, w.interaction, *w.catalog->interaction, ia,
                           id);
      if (!r.ok)
        return failed<ReceiptDetails>(r);
      return {true, {}, {}};
    }
    const auto &prepared = std::get<PreparedTravelOp>(op.quote.data);
    if (complete_motion(w)) {
      const bool wait = std::holds_alternative<TravelWaitCommand>(*travel);
      std::unique_ptr<CompleteExactCommandCapture> wait_capture;
      if (wait) {
        if (!w.world_timeline ||
            w.world_timeline->boundary_state != PeriodBoundaryState::open)
          return failed<ReceiptDetails>(
              common_failure("ExactTravelWaitReadyOpeningOrderRequired"));
        if (w.active_exact_write_context())
          return failed<ReceiptDetails>(
              common_failure("NestedExactTravelWaitAdvanceRequired"));
        auto begun = CompleteExactCommandCapture::begin(w);
        if (!begun.ok)
          return {false, {}, std::move(begun.error)};
        wait_capture = std::move(begun.value);
      }
      auto *writes = w.active_exact_write_context();
      if (!writes || !w.movement_exact ||
          !w.common.actor_condition_capture_identity())
        return failed<ReceiptDetails>(
            common_failure("ExactTravelActualCaptureRequired", id));
      WorldCurrentMomentRead clock(w);
      auto controlled = apply_exact_personal_control_command_in_candidate(
          w.common, w.travel, *w.movement_exact, *w.catalog->travel, w.services,
          clock, prepared);
      w.note_write_boundary();
      if (!controlled.ok)
        return {false, {}, std::move(controlled.error)};
      auto adopted =
          consume_complete_growth_spatial_inputs_in_candidate(w, *writes);
      if (!adopted.ok)
        return failed<ReceiptDetails>(adopted);
      if (wait) {
        auto closed = wait_capture->finish();
        if (!closed.ok)
          return failed<ReceiptDetails>(closed);
        // The original command first holds this actor. S0 then selects
        // the next real positive slice; no legacy skip-set or auto-resume.
        auto advanced = advance_exact_world(w, std::nullopt);
        if (!advanced.ok)
          return {false, {}, std::move(advanced.error)};
        return {true, {std::move(advanced.value), {}}, {}};
      }
      return {true, {}, {}};
    }
    std::optional<NavalVoyageFact> voyage;
    StableId voyage_action;
    if (prepared.quote) {
      auto value = prepare_naval_voyage_world(w, id, *prepared.quote);
      if (!value.ok)
        return {false, {}, value.error};
      voyage = std::move(value.value);
      const auto *actor = w.common.actor(id);
      if (actor)
        voyage_action = actor->action_id;
    }
    auto r =
        apply_travel_control(w.common, w.travel, *w.catalog->travel, prepared);
    if (!r.ok)
      return failed<ReceiptDetails>(r);
    r = record_naval_voyage_world(w, voyage, voyage_action);
    if (!r.ok)
      return failed<ReceiptDetails>(r);
    // 原地折返可能立即结束；release会清人物字段，先拥有ID避免后续领域清理读到空引用。
    if (prepared.quote && prepared.quote->duration == 0) {
      auto actor = w.common.actor(id);
      if (actor && !actor->action_id.empty()) {
        const auto action_id = actor->action_id;
        r = release_action(w, action_id);
        if (!r.ok)
          return failed<ReceiptDetails>(r);
      }
    }
    if (std::holds_alternative<TravelWaitCommand>(*travel)) {
      auto advanced = advance_world(w, {id});
      if (!advanced.ok)
        return {false, {}, advanced.error};
      return {true, {std::move(advanced.value), {}}, {}};
    }
    return {true, {}, {}};
  }
  auto ia = w.services.interaction_access(actual);
  if (auto *p = std::get_if<MilitaryCommand>(&command.data)) {
    if (native_field_campaign_departure_selected(w, *p)) {
      // 原 submit capture 的构图短票已退出；真实出征票须覆盖这次同 M writer。
      // phase 先于 owner/Group 声明，已有真实阶段则直接借用，不能冒 committed。
      std::unique_ptr<PhaseCurrentReadScope> field_phase;
      if (!w.has_active_read_phase()) {
        auto at = w.actual_phase_moment();
        if (!at.ok)
          return {false, {}, std::move(at.error)};
        auto phase =
            w.enter_phase_read_context("field_campaign_depart", "military",
                                       {at.value, at.value}, at.value);
        if (!phase.ok)
          return {false, {}, std::move(phase.error)};
        field_phase = std::move(phase.value);
      }
      // submit 继续借已有真实 capture；owner 保活到唯一原出征 writer 返回。
      std::unique_ptr<CompleteExactWorldBindingOwner> field_owner;
      const auto *field_actual = actual;
      if (!field_actual) {
        auto original = w.complete_exact_time_bindings();
        if (!original.ok)
          return {false, {}, std::move(original.error)};
        field_owner = std::move(original.value);
        if (!field_owner)
          return failed<ReceiptDetails>(
              common_failure("FieldDepartureOriginalCompleteOwnerRequired"));
        field_actual = &field_owner->reads();
      }
      auto result = apply_native_field_campaign_departure_in_candidate(
          w, *field_actual, id, std::get<CampaignDepart>(*p));
      if (!result.ok)
        return {false, {}, std::move(result.error)};
      return {true,
              {std::move(result.value), {"campaign_id", "journey_request"}},
              {}};
    }
    if (std::holds_alternative<CampaignAccept>(*p) &&
        native_field_original_military_quote_selected(w, *p)) {
      auto result = apply_native_field_campaign_acceptance_in_candidate(
          w, id, std::get<CampaignAccept>(*p));
      if (!result.ok)
        return {false, {}, std::move(result.error)};
      return {true, {std::move(result.value), {}}, {}};
    }
    auto lifecycle = validate_native_military_battle_lifecycle_layout(w);
    if (!lifecycle.ok)
      return {false, {}, std::move(lifecycle.error)};
    WorldCurrentMomentRead lifecycle_clock(w);
    MilitaryBattleLifecycleWriteContext lifecycle_context{w.common, w.military,
                                                          lifecycle_clock};
    auto ma = w.services.military_access();
    ma.interaction_access = &ia;
    ma.battle_lifecycle =
        ma.military_battle_lifecycle_schema ? &lifecycle_context : nullptr;
    auto r = apply_military_instant(w.common, w.military, *w.catalog->military,
                                    ma, id, *p);
    if (!r.ok)
      return {false, {}, r.error};
    if (r.value.integration_request) {
      SiegeIntegrationWriteContext context{
          w.common,
          w.military,
          *w.catalog->military,
          ma,
          w.military.siege_integration,
          w.catalog->military->siege_integration};
      auto effect = apply_siege_integration(
          context, id, SiegeIntegrationCommand{*r.value.integration_request});
      if (!effect.ok)
        return {false, {}, effect.error};
      return {true, {std::move(effect.value), {}}, {}};
    }
    if (command.kind == "military_train_cancel") {
      auto released =
          release_action(w, std::get<MilitaryQuote>(op.quote.data).action_id);
      if (!released.ok)
        return failed<ReceiptDetails>(released);
    }
    if (r.value.journey_request) {
      auto journey =
          begin_requested_military_journey(w, *r.value.journey_request);
      if (!journey.ok)
        return {false, {}, journey.error};
    }
    if (command.kind == "military_close") {
      auto refreshed =
          refresh_siege_plans(w.common, w.military, *w.catalog->military, ma);
      if (!refreshed.ok)
        return failed<ReceiptDetails>(refreshed);
    }
    auto changed = apply_military_actor_changes_from_sources(
        w.common, w.military, *w.catalog->military, ma, r.value.actor_changes,
        r.value.casualty_source_ids);
    if (!changed.ok)
      return failed<ReceiptDetails>(changed);
    for (const auto &request : r.value.journey_requests) {
      auto journey = begin_requested_military_journey(w, request);
      if (!journey.ok)
        return failed<ReceiptDetails>(journey);
    }
    if (command.kind == "office_resign" || command.kind == "army_pledge" ||
        command.kind == "military_drill_draft" ||
        command.kind == "military_drill_confirm" ||
        command.kind == "military_drill_cancel_pending" ||
        command.kind == "military_drill_exit")
      return {true, {}, {}};
    FieldOrder order;
    if (command.kind == "siege_strategy")
      order = {"siege_id", "completed_receipt_ids", "actor_changes",
               "journey_requests"};
    else if (command.kind == "siege_join")
      order = {"siege_id", "army_id"};
    else if (command.kind == "siege_start" || command.kind == "siege_plan" ||
             command.kind == "siege_close")
      order = {"siege_id"};
    else if (command.kind == "military_field_command_assign")
      order = {"receipt_id", "source_id", "office_id", "army_id",
               "already_completed"};
    else if (command.kind == "military_tactic_practice")
      order = {"fact_id", "receipt_id", "source_id", "already_completed"};
    else if (command.kind == "military_officer_assign")
      order = {"receipt_id", "source_id", "office_id", "already_completed"};
    else if (command.kind == "military_request")
      order = {"battle_id", "receipt_id", "already_completed",
               "request_answer"};
    else if (command.kind == "military_plan" ||
             command.kind == "military_close")
      order = {"battle_id", "plan_submitted"};
    else if (command.kind.rfind("campaign_", 0) == 0) {
      order = {"campaign_id"};
      if (command.kind == "campaign_deliver")
        order.push_back("receipt_id");
      if (command.kind == "campaign_depart" || r.value.return_wait_reason)
        order.push_back("journey_request");
      if (r.value.return_wait_reason)
        order.push_back("return_wait_reason");
    }
    return {true, {std::move(r.value), std::move(order)}, {}};
  }
  if (auto *p = std::get_if<InteractionCommand>(&command.data)) {
    std::optional<WorldMoment> actual;
    if (w.world_timeline) {
      auto now = w.actual_phase_moment();
      if (!now.ok)
        return {false, {}, std::move(now.error)};
      actual = std::move(now.value);
    }
    auto *writes = w.active_exact_write_context();
    auto r = apply_interaction(
        w.common, w.interaction, *w.catalog->interaction, ia, id, *p,
        std::get<InteractionQuote>(op.quote.data), actual ? &*actual : nullptr,
        writes ? writes->selected_interaction_response : nullptr);
    if (!r.ok)
      return {false, {}, r.error};
    return {true, {std::move(r.value), {}}, {}};
  }
  if (auto *p = std::get_if<TaskCommand>(&command.data)) {
    if (p->kind == "decline_runtime_offer") {
      const auto *q = std::get_if<TaskQuote>(&op.quote.data);
      if (!q)
        return failed<ReceiptDetails>(
            common_failure("InvalidRuntimeOfferResponseQuote"));
      auto result = apply_runtime_task_response(w, id, *p, *q);
      if (!result.ok)
        return failed<ReceiptDetails>(result);
      return {true, {}, {}};
    }
    if (p->kind == "decline_offer") {
      if (!w.catalog->task_offer_responses || !w.tasks.offer_responses)
        return failed<ReceiptDetails>(
            common_failure("TaskOfferResponseProfileRequired"));
      auto access = w.services.interaction_access();
      auto result = record_task_offer_decline(
          w.common, w.tasks, *w.catalog->tasks, w.interaction,
          *w.catalog->interaction, access, id, *p, *w.tasks.offer_responses);
      if (!result.ok)
        return failed<ReceiptDetails>(result);
      return {true, {}, {}};
    }
    const auto &q = std::get<TaskQuote>(op.quote.data);
    if (!q.task_id)
      return failed<ReceiptDetails>(common_failure("MissingTaskPreparation"));
    auto r = cancel_task(
        w.common, w.tasks, *w.catalog->tasks, *w.catalog->common, *q.task_id,
        w.dynamic_enabled() ? &w.affairs : nullptr, takeover.value->tasks());
    if (!r.ok)
      return failed<ReceiptDetails>(r);
    return {true, {}, {}};
  }
  if (const auto *c =
          std::get_if<PrivateShopLiquidationCommand>(&command.data)) {
    auto r = commit_private_shop_liquidation_world(
        w, id, *c, std::get<PrivateShopLiquidationQuote>(op.quote.data));
    if (!r.ok)
      return {false, {}, r.error};
    InteractionEffect effect;
    effect.settlement_result = r.value.source_fact_id;
    return {true, ReceiptDetails{std::move(effect), {}}, {}};
  }
  if (const auto *c = std::get_if<TaskShopOrderEstateCancel>(&command.data)) {
    auto r = commit_task_shop_order_estate_world(
        w, id, *c,
        std::get<TaskShopOrderEstateCancellationQuote>(op.quote.data));
    if (!r.ok)
      return {false, {}, r.error};
    return {true, ReceiptDetails{std::move(r.value), {}}, {}};
  }
  if (const auto *c = std::get_if<PrivateShopDailyRentCommand>(&command.data)) {
    auto r = commit_private_shop_daily_rent_world(
        w, id, *c, std::get<PrivateShopDailyRentQuote>(op.quote.data));
    if (!r.ok)
      return {false, {}, r.error};
    InteractionEffect effect;
    effect.settlement_result = r.value.source_fact_id;
    return {true, ReceiptDetails{std::move(effect), {}}, {}};
  }
  if (const auto *c =
          std::get_if<TellHouseholdPurchaseMarketReport>(&command.data)) {
    auto dependencies = make_native_household_report_dependencies(w);
    if (!dependencies.ok)
      return {false, {}, dependencies.error};
    auto applied = apply_native_household_report_command_in_candidate(
        w, id, *c, std::get<NativeHouseholdReportCommandQuote>(op.quote.data),
        dependencies.value);
    if (!applied.ok)
      return {false, {}, applied.error};
    if (!applied.value || applied.value->domain != "interaction" ||
        applied.value->kind != "household_purchase_report_disclosed")
      return failed<ReceiptDetails>(
          common_failure("HouseholdReportActualResultRequired"));
    // 沿原消息回执的实际事实 ID 槽；这里只留真告知头，不建立钱款结算。


    auto temporary = CheckpointMemoryCharge(0);
    InteractionEffect effect;
    effect.settlement_result = applied.value->id;
    return {true, ReceiptDetails{effect, {}}, {}};
  }
  if (const auto *c =
          std::get_if<HouseholdPurchaseOriginalCommand>(&command.data)) {
    auto applied = apply_native_household_purchase_command_in_candidate(
        w, id, *c,
        std::get<NativeHouseholdPurchaseCommandQuote>(op.quote.data));
    if (!applied.ok)
      return {false, {}, applied.error};
    const auto expected =
        std::holds_alternative<HouseholdPurchasePolicyCommand1>(*c)
            ? "household_purchase_policy_accepted"
            : "household_purchase_planned";
    if (!applied.value || applied.value->domain != "local_economy" ||
        applied.value->kind != expected)
      return failed<ReceiptDetails>(
          common_failure("HouseholdPurchaseActualResultRequired"));
    // 原 S5 writer 已生成实际事实；临时与最终回执分别持原复制租约。


    auto temporary = CheckpointMemoryCharge(0);
    InteractionEffect effect;
    effect.settlement_result = applied.value->id;
    return {true, ReceiptDetails{effect, {}}, {}};
  }
  if (const auto *c = std::get_if<PrivateShopCommand>(&command.data)) {
    if (capture && (std::holds_alternative<PrivateShopOperate>(*c) ||
                    std::holds_alternative<PrivateShopOperateWithCustody>(*c) ||
                    std::holds_alternative<PrivateShopCancelOperation>(*c)))
      return capture->apply_private_shop_command(id, op);
    auto r = commit_private_shop_world(
        w, id, *c, std::get<PrivateShopCommandQuote>(op.quote.data));
    if (!r.ok)
      return {false, {}, r.error};
    if (!r.value.action_id.empty())
      return {true, ReceiptDetails{ActionStarted{r.value.action_id}, {}}, {}};
    InteractionEffect effect;
    effect.settlement_result = r.value.source_fact_id;
    return {true, ReceiptDetails{std::move(effect), {}}, {}};
  }
  if (const auto *c = std::get_if<TaskShopOrderCommand>(&command.data)) {
    auto r = commit_task_shop_order_world(
        w, id, *c, std::get<TaskShopOrderQuote>(op.quote.data));
    if (!r.ok)
      return {false, {}, r.error};
    if (!r.value.action_id.empty())
      return {true, ReceiptDetails{ActionStarted{r.value.action_id}, {}}, {}};
    InteractionEffect effect;
    effect.settlement_result = r.value.source_fact_id;
    return {true, ReceiptDetails{std::move(effect), {}}, {}};
  }
  if (const auto *c = std::get_if<PoliticalAcceptShopRegistry>(&command.data)) {
    auto r = commit_political_shop_registry_world(
        w, id, *c,
        std::get<PoliticalShopRegistryAcceptanceQuote>(op.quote.data));
    if (!r.ok)
      return {false, {}, r.error};
    return {true, ReceiptDetails{Done{}, {}}, {}};
  }
  if (const auto *c = std::get_if<PoliticalContinueGovernance>(&command.data)) {
    auto r = commit_political_governance_continuation_world(
        w, id, *c,
        std::get<PoliticalGovernanceContinuationQuote>(op.quote.data));
    if (!r.ok)
      return {false, {}, r.error};
    return {true, ReceiptDetails{Done{}, {}}, {}};
  }
  if (const auto *c =
          std::get_if<GovernanceInvitationReplyCommand>(&command.data)) {
    auto r = commit_governance_invitation_world(
        w, id, *c, std::get<GovernanceInvitationReplyQuote>(op.quote.data));
    if (!r.ok)
      return {false, {}, r.error};
    return {true, ReceiptDetails{Done{}, {}}, {}};
  }
  if (const auto *c =
          std::get_if<PoliticalHeadHandoverCommand>(&command.data)) {
    auto result = commit_political_head_handover_world(
        w, id, *c, std::get<PoliticalHeadHandoverQuote>(op.quote.data));
    if (!result.ok)
      return {false, {}, result.error};
    return {true, ReceiptDetails{std::move(result.value), {}}, {}};
  }
  if (const auto *c =
          std::get_if<PoliticalSuccessorRoundCommand>(&command.data)) {
    auto result = commit_political_successor_round_world(
        w, id, *c, std::get<PoliticalSuccessorRoundQuote>(op.quote.data));
    if (!result.ok)
      return {false, {}, result.error};
    return {true, ReceiptDetails{std::move(result.value), {}}, {}};
  }
  if (const auto *c =
          std::get_if<PoliticalTermHandoverCommand>(&command.data)) {
    auto result = commit_political_term_handover_world(
        w, id, *c, std::get<PoliticalTermHandoverQuote>(op.quote.data));
    if (!result.ok)
      return {false, {}, result.error};
    return {true, ReceiptDetails{std::move(result.value), {}}, {}};
  }
  if (const auto *c =
          std::get_if<PoliticalOfficeReceptionCommand>(&command.data)) {
    auto result = commit_political_office_reception_world(
        w, id, *c, std::get<PoliticalOfficeReceptionQuote>(op.quote.data));
    if (!result.ok)
      return {false, {}, result.error};
    return {true, ReceiptDetails{std::move(result.value), {}}, {}};
  }
  if (const auto *c =
          std::get_if<AuthoredOfficeResignationCommand>(&command.data)) {
    auto result = commit_authored_office_resignation_world(
        w, id, *c, std::get<PoliticalOfficeResignationQuote>(op.quote.data));
    if (!result.ok)
      return {false, {}, result.error};
    return {true, ReceiptDetails{std::move(result.value), {}}, {}};
  }
  if (const auto *c =
          std::get_if<AuthoredSubordinateAppointmentCommand>(&command.data)) {
    auto result = commit_authored_subordinate_appointment_world(
        w, id, *c,
        std::get<PoliticalSubordinateAppointmentQuote>(op.quote.data));
    if (!result.ok)
      return {false, {}, result.error};
    return {true, ReceiptDetails{std::move(result.value), {}}, {}};
  }
  if (const auto *c =
          std::get_if<PoliticalOfficeResignationCommand>(&command.data)) {
    auto result = commit_political_office_resignation_world(
        w, id, *c, std::get<PoliticalOfficeResignationQuote>(op.quote.data));
    if (!result.ok)
      return {false, {}, result.error};
    return {true, ReceiptDetails{std::move(result.value), {}}, {}};
  }
  if (const auto *c =
          std::get_if<PoliticalSubordinateAppointmentCommand>(&command.data)) {
    auto result = commit_political_subordinate_appointment_world(
        w, id, *c,
        std::get<PoliticalSubordinateAppointmentQuote>(op.quote.data));
    if (!result.ok)
      return {false, {}, result.error};
    return {true, ReceiptDetails{std::move(result.value), {}}, {}};
  }
  if (const auto *c =
          std::get_if<AuthoredGovernanceAgencyCommand>(&command.data)) {
    auto result = commit_authored_governance_agency_world(
        w, id, *c, std::get<PoliticalGovernanceAgencyQuote>(op.quote.data));
    if (!result.ok)
      return {false, {}, result.error};
    return {true, ReceiptDetails{std::move(result.value), {}}, {}};
  }
  if (const auto *c =
          std::get_if<PoliticalGovernanceAgencyCommand>(&command.data)) {
    auto result = commit_political_governance_agency_world(
        w, id, *c, std::get<PoliticalGovernanceAgencyQuote>(op.quote.data));
    if (!result.ok)
      return {false, {}, result.error};
    return {true, ReceiptDetails{std::move(result.value), {}}, {}};
  }
  if (const auto *c = std::get_if<PoliticalSuccessionCommand>(&command.data)) {
    auto result = commit_political_succession_world(
        w, id, *c, std::get<PoliticalSuccessionQuote>(op.quote.data));
    if (!result.ok)
      return {false, {}, result.error};
    return {true, ReceiptDetails{std::move(result.value), {}}, {}};
  }
  if (const auto *c =
          std::get_if<PoliticalShopLicensingCommand>(&command.data)) {
    // Real produced IDs remain charged through this original Done boundary;
    // the receipt has never contained an extra invented ID payload.
    const PoliticalShopLicensingQuote *quote =
        std::get_if<PoliticalShopLicensingQuote>(&op.quote.data);
    if (const auto *owned =
            std::get_if<PoliticalShopLicensingPreparedQuote>(&op.quote.data)) {
      if (!*owned)
        return failed<ReceiptDetails>(
            common_failure("PoliticalShopOwnedQuoteRequired"));
      quote = &(*owned)->quote();
    }
    if (!quote)
      return failed<ReceiptDetails>(
          common_failure("InvalidPoliticalShopLicensingCommand"));
    auto r = commit_political_shop_licensing_world_owned(w, id, *c, *quote);
    if (!r.ok)
      return {false, {}, r.error};
    return {true, ReceiptDetails{Done{}, {}}, {}};
  }
  if (const auto *c =
          std::get_if<IssueEconomicSupplyPolicyCommand>(&command.data)) {
    auto r = commit_economic_supply_world(
        w, id, *c, std::get<EconomicSupplyPolicyQuote>(op.quote.data));
    if (!r.ok)
      return {false, {}, r.error};
    return {true, {std::move(r.value), {}}, {}};
  }
  if (const auto *c = std::get_if<EstateEmploymentCommand>(&command.data)) {
    auto result = commit_estate_employment_world(
        w, id, *c, std::get<EstateEmploymentQuote>(op.quote.data));
    if (!result.ok)
      return {false, {}, result.error};
    return {true, {std::move(result.value), {}}, {}};
  }
  if (visit) {
    auto result = commit_visit_message_world(
        w, id, *visit, std::get<VisitMessageQuote>(op.quote.data), actual);
    if (!result.ok)
      return {false, {}, result.error};
    return {true, ReceiptDetails{std::move(result.value), {}}, {}};
  }
  if (extension) {
    auto result = commit_task_extension_world(
        w, id, *extension, std::get<TaskExtensionQuote>(op.quote.data));
    if (!result.ok)
      return {false, {}, result.error};
    return {true, ReceiptDetails{std::move(result.value), {}}, {}};
  }
  if (appointment) {
    auto result = commit_personal_appointment_world(
        w, id, *appointment, std::get<PersonalAppointmentQuote>(op.quote.data));
    if (!result.ok)
      return {false, {}, result.error};
    if (result.value.action_started)
      return {
          true, ReceiptDetails{ActionStarted{result.value.source_id}, {}}, {}};
    InteractionEffect effect;
    effect.settlement_result = result.value.source_id;
    return {true, ReceiptDetails{std::move(effect), {}}, {}};
  }
  if (inquiry) {
    auto result = commit_contact_inquiry_world(
        w, id, *inquiry, std::get<ContactInquiryQuote>(op.quote.data));
    if (!result.ok)
      return {false, {}, result.error};
    return {true, ReceiptDetails{ActionStarted{result.value}, {}}, {}};
  }
  if (message_return) {
    auto result = commit_task_message_return_world(
        w, id, *message_return,
        std::get<TaskMessageReturnQuote>(op.quote.data));
    if (!result.ok)
      return {false, {}, result.error};
    return {true, ReceiptDetails{ActionStarted{result.value}, {}}, {}};
  }
  if (personal_message) {
    auto result = begin_action(w, id, command, op.quote, capture);
    if (!result.ok)
      return {false, {}, result.error};
    return {true, ReceiptDetails{std::move(result.value), {}}, {}};
  }
  if (personal_report) {
    auto result = begin_action(w, id, command, op.quote, capture);
    if (!result.ok)
      return {false, {}, result.error};
    return {true, ReceiptDetails{std::move(result.value), {}}, {}};
  }
  if (personal_acceptance) {
    auto result = begin_action(w, id, command, op.quote, capture);
    if (!result.ok)
      return {false, {}, result.error};
    return {true, ReceiptDetails{std::move(result.value), {}}, {}};
  }
  if (personal_cancel) {
    const auto *quote =
        std::get_if<TaskPersonalCancellationQuote>(&op.quote.data);
    if (!quote)
      return failed<ReceiptDetails>(
          common_failure("TaskPersonalCancellationQuoteRequired"));
    auto result =
        apply_task_personal_cancellation_world(w, id, *personal_cancel, *quote);
    if (!result.ok)
      return {false, {}, result.error};
    InteractionEffect effect;
    effect.settlement_result = result.value;
    return {true, ReceiptDetails{std::move(effect), {}}, {}};
  }
  if (visit_reply) {
    auto result = commit_visit_reply_world(
        w, id, *visit_reply, std::get<VisitReplyQuote>(op.quote.data));
    if (!result.ok)
      return {false, {}, result.error};
    return {true, ReceiptDetails{std::move(result.value), {}}, {}};
  }
  if (visit_invitation) {
    auto result = commit_visit_appointment_world(
        w, id, *visit_invitation,
        std::get<VisitAppointmentQuote>(op.quote.data));
    if (!result.ok)
      return {false, {}, result.error};
    return {true, ReceiptDetails{std::move(result.value), {}}, {}};
  }
  if (authored_invitation) {
    const VisitAppointmentMeetingTime2Context *context = nullptr;
    if (actual) {
      auto group = read_native_interaction_world(w, *actual);
      if (!group.ok)
        return {false, {}, std::move(group.error)};
      if (!group.value->visit())
        return {false,
                {},
                common_failure("VisitAppointmentActualOwningGroupRequired", {},
                               ErrorKind::unsupported_capability)
                    .error};
      auto x = group.value->visit()->context();
      if (!x.ok)
        return {false, {}, std::move(x.error)};
      context = x.value;
    }
    auto result = commit_authored_visit_appointment_world(
        w, id, *authored_invitation,
        std::get<AuthoredVisitAppointmentQuote>(op.quote.data), context,
        actual);
    if (!result.ok)
      return {false, {}, result.error};
    return {true, ReceiptDetails{std::move(result.value), {}}, {}};
  }
  if (visit_meeting) {
    auto result = commit_visit_appointment_meeting_world(
        w, id, *visit_meeting,
        std::get<VisitAppointmentMeetingQuote>(op.quote.data));
    if (!result.ok)
      return {false, {}, result.error};
    if (result.value.action_started)
      return {
          true, ReceiptDetails{ActionStarted{result.value.source_id}, {}}, {}};
    InteractionEffect effect;
    effect.settlement_result = result.value.source_id;
    return {true, ReceiptDetails{std::move(effect), {}}, {}};
  }
  if (born_dialogue) {
    auto result = commit_born_personal_dialogue_world(
        w, id, *born_dialogue,
        std::get<BornPersonalDialogueQuote>(op.quote.data));
    if (!result.ok)
      return {false, {}, result.error};
    return {true, ReceiptDetails{std::move(result.value), {}}, {}};
  }
  if (known_dialogue) {
    auto result = commit_known_personal_dialogue_world(
        w, id, *known_dialogue,
        std::get<KnownPersonalDialogueQuote>(op.quote.data));
    if (!result.ok)
      return {false, {}, result.error};
    return {true, ReceiptDetails{std::move(result.value), {}}, {}};
  }
  if (authored_reply) {
    const VisitReplyReadContext *context = nullptr;
    if (actual) {
      auto group = read_native_interaction_world(w, *actual);
      if (!group.ok)
        return {false, {}, std::move(group.error)};
      if (!group.value->replies())
        return {false,
                {},
                common_failure("VisitReplyActualOwningGroupRequired", {},
                               ErrorKind::unsupported_capability)
                    .error};
      auto x = group.value->replies()->context();
      if (!x.ok)
        return {false, {}, std::move(x.error)};
      context = x.value;
    }
    auto result = commit_authored_visit_reply_world(
        w, id, *authored_reply,
        std::get<AuthoredVisitReplyQuote>(op.quote.data), context, actual);
    if (!result.ok)
      return {false, {}, result.error};
    return {true, ReceiptDetails{std::move(result.value), {}}, {}};
  }
  if (auto *p = std::get_if<GrowthTacticLessonCommand>(&command.data))
    return apply_growth_tactic_lesson_world(
        w, id, *p, std::get<GrowthTacticLessonCommandQuote>(op.quote.data));
  if (auto *p = std::get_if<ChildCareReliefCommand>(&command.data)) {
    if (!w.family_care_relief_enabled())
      return failed<ReceiptDetails>(
          common_failure("ChildCareReliefProfileRequired"));
    if (command.kind == "family_child_care_relief") {
      auto begun = begin_action(w, id, command, op.quote, capture);
      return begun.ok ? Outcome<ReceiptDetails>{true,
                                                ReceiptDetails{begun.value, {}},
                                                {}}
                      : Outcome<ReceiptDetails>{false, {}, begun.error};
    }
    auto ia = w.services.interaction_access();
    auto result = apply_child_care_relief_command_in_candidate(
        w.common, w.interaction, *w.catalog->interaction, ia,
        *w.catalog->family_care_relief, w.family_birth, w.family_life,
        w.child_care_relief, w.family_care_relief_access(), id, *p,
        std::get<ChildCareReliefCommandQuote>(op.quote.data));
    if (!result.ok)
      return {false, {}, result.error};
    if (!result.value.release_action_id.empty()) {
      ActionCompletion done;
      done.completed = true;
      done.success = false;
      done.code = "CareReliefProviderStopped";
      auto effect = result.value.effect;
      effect.success = false;
      effect.code = "CareReliefProviderStopped";
      done.family = std::move(effect);
      auto released = release_completed_child_care_relief(
          w, result.value.release_action_id, done);
      if (!released.ok)
        return failed<ReceiptDetails>(released);
    }
    return {true, ReceiptDetails{std::move(result.value.effect), {}}, {}};
  }
  if (auto *p = std::get_if<FamilyBirthCommand>(&command.data)) {
    if (!w.family_birth_enabled())
      return failed<ReceiptDetails>(common_failure("BirthProfileRequired"));
    if (command.kind == "family_child_care") {
      auto begun = begin_action(w, id, command, op.quote, capture);
      return begun.ok ? Outcome<ReceiptDetails>{true,
                                                ReceiptDetails{begun.value, {}},
                                                {}}
                      : Outcome<ReceiptDetails>{false, {}, begun.error};
    }
    auto ia = w.services.interaction_access();
    auto result = apply_family_birth_command_in_candidate(
        w.common, w.interaction, *w.catalog->interaction, ia,
        *w.catalog->family, *w.catalog->family_birth, w.family_birth,
        w.family_birth_access(), id, *p,
        std::get<FamilyBirthQuote>(op.quote.data));
    if (!result.ok)
      return {false, {}, result.error};
    if (!result.value.release_action_id.empty()) {
      ActionCompletion done;
      done.completed = true;
      done.success = false;
      done.code = "CaregiverStopped";
      auto fact = result.value.effect;
      fact.success = false;
      fact.code = "CaregiverStopped";
      done.family = std::move(fact);
      auto released =
          release_completed_child_care(w, result.value.release_action_id, done);
      if (!released.ok)
        return failed<ReceiptDetails>(released);
    }
    return {true, ReceiptDetails{std::move(result.value.effect), {}}, {}};
  }
  if (auto *p = std::get_if<ApprenticeshipCommand>(&command.data)) {
    if (!w.family_apprenticeship_enabled())
      return failed<ReceiptDetails>(
          common_failure("ApprenticeshipProfileRequired"));
    auto ia = w.services.interaction_access();
    auto effect = apply_apprenticeship_command_in_candidate(
        w.common, w.interaction, *w.catalog->interaction, ia,
        *w.catalog->family_apprenticeship, w.family_apprenticeship,
        w.family_apprenticeship_access(), id, *p,
        std::get<ApprenticeshipQuote>(op.quote.data));
    if (!effect.ok)
      return {false, {}, effect.error};
    return {true, {std::move(effect.value), {}}, {}};
  }
  if (auto *p = std::get_if<FamilyMarriageCommand>(&command.data)) {
    if (!w.family_marriage_enabled())
      return failed<ReceiptDetails>(common_failure("MarriageProfileRequired"));
    auto ia = w.services.interaction_access();
    auto result = apply_family_marriage_command(
        w.common, w.family, w.family_life, w.interaction,
        *w.catalog->interaction, ia, *w.catalog->family_marriage,
        w.family_marriage, w.family_marriage_access(), id, *p,
        std::get<FamilyMarriageQuote>(op.quote.data));
    if (!result.ok)
      return {false, {}, result.error};
    return {true, {std::move(result.value), {}}, {}};
  }
  if (auto *p = std::get_if<FamilyInheritanceCommand>(&command.data)) {
    if (!w.family_inheritance_enabled())
      return failed<ReceiptDetails>(
          common_failure("FamilyInheritanceProfileRequired"));
    auto ia = w.services.interaction_access();
    auto result = apply_family_inheritance_command(
        w.common, w.family_bequest, w.family_life, w.interaction,
        *w.catalog->interaction, ia, *w.catalog->family_inheritance,
        w.family_inheritance, w.family_inheritance_access(), id, *p,
        std::get<FamilyInheritanceQuote>(op.quote.data));
    if (!result.ok)
      return {false, {}, result.error};
    return {true, {std::move(result.value), {}}, {}};
  }
  if (auto *p = std::get_if<FamilyBequestCommand>(&command.data)) {
    if (!w.family_bequest_enabled())
      return failed<ReceiptDetails>(
          common_failure("FamilyBequestProfileRequired"));
    auto r = apply_family_bequest_command(
        w.common, w.family, w.family_life, w.family_death_read(),
        *w.catalog->family_bequest, w.family_bequest,
        w.family_bequest_player_read(), id, *p,
        std::get<FamilyBequestQuote>(op.quote.data));
    if (!r.ok)
      return {false, {}, r.error};
    return {true, {std::move(r.value), {}}, {}};
  }
  if (auto *p = std::get_if<VoluntaryPathCommand>(&command.data)) {
    auto r = apply_family_voluntary_path_world(
        w, id, *p, std::get<VoluntaryPathQuote>(op.quote.data));
    if (!r.ok)
      return {false, {}, r.error};
    return {true, {std::move(r.value), {}}, {}};
  }
  if (auto *p = std::get_if<CollateralControlCommand>(&command.data)) {
    auto r = apply_family_collateral_world(
        w, id, *p, std::get<CollateralControlQuote>(op.quote.data));
    if (!r.ok)
      return {false, {}, r.error};
    return {true, {std::move(r.value), {}}, {}};
  }
  if (auto *p = std::get_if<BiologicalControlCommand>(&command.data)) {
    auto r = apply_family_biological_world(
        w, id, *p, std::get<LegacyControlDecisionQuote>(op.quote.data));
    if (!r.ok)
      return {false, {}, r.error};
    return {true, {std::move(r.value), {}}, {}};
  }
  if (auto *p = std::get_if<FamilyLegacyCommand>(&command.data)) {
    if (!w.family_legacy_enabled())
      return failed<ReceiptDetails>(
          common_failure("LegacyControlConsentProfileRequired"));
    auto r = apply_family_legacy_world(
        w, id, *p, std::get<FamilyLegacyQuote>(op.quote.data));
    if (!r.ok)
      return {false, {}, r.error};
    return {true, {std::move(r.value), {}}, {}};
  }
  if (auto *p = std::get_if<FamilyCommand>(&command.data)) {
    auto family_sources = NativeFamilyInitialSourcesLease::create(w);
    if (!family_sources.ok)
      return {false, {}, std::move(family_sources.error)};
    auto fa = w.services.family_access(
        ia, family_sources.value ? &family_sources.value->origins() : nullptr,
        family_sources.value ? &family_sources.value->initial_age() : nullptr);
    auto r = (w.family_voluntary_paths_enabled() &&
              (p->kind == "family_agree_succession" ||
               p->kind == "family_cancel_succession" ||
               p->kind == "family_continue_as"))
                 ? apply_family_voluntary_original_world(
                       w, id, *p, std::get<FamilyQuote>(op.quote.data))
                 : apply_family_instant(w.common, w.family, w.estate,
                                        *w.catalog->family, *w.catalog->common,
                                        *w.catalog->estate, fa, id, *p,
                                        std::get<FamilyQuote>(op.quote.data));
    if (!r.ok)
      return {false, {}, r.error};
    return {true, {std::move(r.value), {}}, {}};
  }
  if (auto *p = std::get_if<EstateCommand>(&command.data)) {
    auto allowed = guard_estate_employment_world_management(w, *p);
    if (!allowed.ok)
      return {false, {}, allowed.error};
    auto access = w.services.estate_access();
    auto r = apply_estate(w.common, w.estate, *w.catalog->estate,
                          *w.catalog->common, w.services, id,
                          std::get<EstateQuote>(op.quote.data), &access);
    if (!r.ok)
      return {false, {}, r.error};
    return {true, {std::move(r.value), {}}, {}};
  }
  return failed<ReceiptDetails>(common_failure("UnsupportedAction"));
}
// 同候选完成业务后派生可见暂停；不会发观察或再开Common保存点。
static Outcome<ReceiptDetails>
apply_command_bound(WorldBundle &w, const StableId &id,
                    const PreparedDomainOp &op,
                    const CompleteExactWorldReadBindings *actual,
                    CompleteExactCommandCapture *capture = nullptr) {
  ReadStampWriteBoundary stamp_boundary(*w.read_stamps);
  if (mismatched_basic_command_binding(op.command))
    return failed<ReceiptDetails>(common_failure("InvalidCommand"));
  // 采用时重新读当前看管；预备过的对白内层动作也必须再经过此真实门。
  auto custody = guard_native_military_custody_payload(w, id, op.command);
  if (!custody.ok)
    return {false, {}, std::move(custody.error)};
  const bool grant_command =
      std::holds_alternative<GrantChangeCommand>(op.command.data);
  if (organization_grant_command_kind(op.command.kind) != grant_command ||
      (grant_command &&
       (!std::holds_alternative<DurationQuote>(op.quote.data) ||
        op.quote.kind != op.command.kind ||
        std::get<DurationQuote>(op.quote.data).duration != Integer{0})))
    return failed<ReceiptDetails>(common_failure("CommandPayloadKindMismatch"));
  const auto *relief = std::get_if<ChildCareReliefCommand>(&op.command.data);
  if (child_care_relief_command_kind(op.command.kind) != (relief != nullptr) ||
      (relief &&
       (relief->kind != op.command.kind ||
        !std::holds_alternative<ChildCareReliefCommandQuote>(op.quote.data))))
    return failed<ReceiptDetails>(
        common_failure("InvalidChildCareReliefCommand"));
  auto result = apply_command_domain(w, id, op, actual, custody.value, capture);
  if (!result.ok)
    return result;
  w.note_write_boundary();
  auto projected = project_economic_supply_world_in_candidate(w);
  if (!projected.ok)
    return {false, {}, projected.error};
  return result;
}

Outcome<ReceiptDetails> apply_command(WorldBundle &w, const StableId &id,
                                      const PreparedDomainOp &op) {
  return apply_command_bound(w, id, op, nullptr);
}
// Internal transport of the actual submission capture; all original gates and
// post-apply projection remain in apply_command_bound.
Outcome<ReceiptDetails>
apply_command_with_complete_capture(WorldBundle &w, const StableId &id,
                                    const PreparedDomainOp &op,
                                    CompleteExactCommandCapture &capture) {
  if (!capture.bound_to(w))
    return failed<ReceiptDetails>(
        common_failure("CompleteCommandCaptureNotLive"));
  return apply_command_bound(w, id, op, nullptr, &capture);
}
Outcome<ReceiptDetails> apply_command_with_exact_bindings(
    WorldBundle &w, const StableId &id, const PreparedDomainOp &op,
    const CompleteExactWorldReadBindings &actual) {
  auto valid = exact_dispatch_binding(w, actual);
  if (!valid.ok)
    return {false, {}, std::move(valid.error)};
  return apply_command_bound(w, id, op, &actual);
}
} // namespace hanmo::resident
