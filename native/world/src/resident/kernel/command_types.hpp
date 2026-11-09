#include "hanmo/resident/world_moment.hpp"
#include "resident/domains/commerce/daily_rent/private_shop_daily_rent_boundary.hpp"
#include "resident/domains/commerce/liquidation/private_shop_liquidation_boundary.hpp"
#include "resident/domains/family/apprenticeship_commands.hpp"
#include "resident/domains/family/biological_control_commands.hpp"
#include "resident/domains/family/collateral_control_commands.hpp"
#include "resident/domains/interaction/appointments/personal_appointment_world.hpp"
#include "resident/domains/interaction/born_dialogues/born_personal_dialogue_world.hpp"
#include "resident/domains/interaction/contact_inquiries/contact_inquiry_world.hpp"
#include "resident/domains/interaction/personal_dialogues/known_personal_dialogue_world.hpp"
#include "resident/domains/interaction/visit_appointments/visit_appointment_world.hpp"
#include "resident/domains/interaction/visit_messages/visit_message_world.hpp"
#include "resident/domains/interaction/visit_replies/visit_reply_world.hpp"
#include "resident/domains/politics/governance_agency_intents/political_governance_agency_authored.hpp"
#include "resident/domains/tasks/extensions/task_extension_world.hpp"
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
#include "resident/domains/tasks/shop_orders/task_shop_order_estate_boundary.hpp"
// 中央唯一命令/报价/回执组合；领域明细仍为具体类型，热事务不持JSON或完整世界字典。
#pragma once
#include "diplomacy_integration.hpp"
#include "economic_world_integration.hpp"
#include "estate_employment_integration.hpp"
#include "governance_integration.hpp"
#include "governance_invitation_world.hpp"
#include "hanmo/resident/command_contract.hpp"
#include "household_provision_integration.hpp"
#include "native_household_purchase_commands.hpp"
#include "native_household_report_command.hpp"
#include "organization_contract.hpp"
#include "political_governance_agency_world.hpp"
#include "political_governance_continuation_world.hpp"
#include "political_head_handover_world.hpp"
#include "political_office_reception_world.hpp"
#include "political_office_resignation_world.hpp"
#include "political_shop_registry_integration.hpp"
#include "political_subordinate_appointment_world.hpp"
#include "political_succession_world.hpp"
#include "political_successor_round_world.hpp"
#include "political_takeover_integration.hpp"
#include "political_term_handover_world.hpp"
#include "politics_fiscal_integration.hpp"
#include "politics_integration.hpp"
#include "resident/domains/commerce/private_shop_payload.hpp"
#include "resident/domains/commerce/repair_service/repair_service_command_model.hpp"
#include "resident/domains/estate/estate_model.hpp"
#include "resident/domains/family/child_care_relief_commands.hpp"
#include "resident/domains/family/control_continuation.hpp"
#include "resident/domains/family/control_retirement_cancel.hpp"
#include "resident/domains/family/family_bequest_commands.hpp"
#include "resident/domains/family/family_birth_commands.hpp"
#include "resident/domains/family/family_inheritance_commands.hpp"
#include "resident/domains/family/family_legacy_commands.hpp"
#include "resident/domains/family/family_marriage_commands.hpp"
#include "resident/domains/family/family_model.hpp"
#include "resident/domains/family/voluntary_path_commands.hpp"
#include "resident/domains/growth/growth_model.hpp"
#include "resident/domains/growth/growth_tactic_lesson_commands.hpp"
#include "resident/domains/interaction/interaction_model.hpp"
#include "resident/domains/interaction/party_invitation_commands.hpp"
#include "resident/domains/local_economy/supply/supply_policy_boundary.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/domains/military/field_battle/field_view_model.hpp"
#include "resident/domains/military/field_battle/field_army_exit_command.hpp"
#include "resident/domains/military/military_tactical_ownership_model.hpp"
#include "resident/domains/military/naval_battle_commands.hpp"
#include "resident/domains/military/naval_command_codec.hpp"
#include "resident/domains/military/naval_repair_codec.hpp"
#include "resident/domains/military/population_military.hpp"
#include "resident/domains/military/siege_integration_decisions.hpp"
#include "resident/domains/organization/grant_authority.hpp"
#include "resident/domains/organization/membership_offer.hpp"
#include "resident/domains/organization/organization_model.hpp"
#include "resident/domains/personal/personal_model.hpp"
#include "resident/domains/player_plan/plan_commands.hpp"
#include "resident/domains/politics/field/political_field_appointments_profile.hpp"
#include "resident/domains/politics/governance_targets/governance_program.hpp"
#include "resident/domains/politics/licensing/political_shop_licensing_profile.hpp"
#include "resident/domains/politics/officers/political_military_appointments_profile.hpp"
#include "resident/domains/politics/takeover/politics_occupation_fiscal_command.hpp"
#include "resident/domains/politics/treaty/treaty_passage_command.hpp"
#include "resident/domains/tasks/shop_orders/task_shop_order_business.hpp"
#include "resident/domains/tasks/task_model.hpp"
#include "resident/domains/tasks/task_personal_advance_command.hpp"
#include "resident/domains/travel/travel.hpp"
#include <memory>
namespace hanmo::resident {
class OwnedPoliticalShopLicensingQuote;
// New Law2 prepare stores the immutable owned quote itself. Copying a prepared
// operation shares this owner, never an uncharged duplicate of its payload.
using PoliticalShopLicensingPreparedQuote =
    std::shared_ptr<const OwnedPoliticalShopLicensingQuote>;

// 同一原advance的精确上界；legacy不接受新字段，不另注册平行clock命令。
struct AdvanceCommand {
  std::optional<WorldMoment> requested_to;
};
struct WaitCommand {};
// The accepting actor is authenticated by the command envelope, never this
// payload.
struct AcceptWorkReassignment {
  SourceRef approval_ref;
};
using CommandData = std::variant<
    AdvanceCommand, WaitCommand, TravelCommandPayload, MilitaryCommand,
    InteractionCommand, EstateCommand, TaskCommand, FamilyCommand,
    PersonalCommand, GrowthCommand, OrganizationMemberCommand,
    OrganizationRecruitCommand, OrganizationActivityCommand,
    OrganizationContractCommand, PopulationMilitaryCommand,
    SiegeIntegrationCommand, SiegeIntegrationApply,
    OrganizationOfferMembershipCommand, OrganizationAnswerMembershipCommand,
    OrganizationWithdrawMembershipCommand, PlayerPlanCommand, PoliticsCommand,
    PoliticalSetTaxCommand, ConfigureLocalProductionCommand, GovernanceCommand,
    DiplomacyCommand, PartyParticipationCommand, NavalCommand,
    ConfigureHouseholdProvisionCommand, TaskPersonalAdvanceCommand,
    NavalBattleCommand, PoliticalTakeoverDecisionCommand, FamilyLegacyCommand,
    PoliticalSetOccupationTaxCommand, FamilyBequestCommand,
    TreatyPassageCommand, PoliticalMilitaryAppointmentCommand,
    FamilyInheritanceCommand, FamilyMarriageCommand, EstateEmploymentCommand,
    PoliticalFieldAppointmentCommand, NavalRepairCommand,
    IssueEconomicSupplyPolicyCommand, FamilyBirthCommand, VisitMessageCommand,
    ChildCareReliefCommand, PoliticalShopLicensingCommand, PrivateShopCommand,
    TaskShopOrderCommand, TaskExtensionCommand, PoliticalAcceptShopRegistry,
    PersonalAppointmentCommand, PrivateShopLiquidationCommand,
    TaskShopOrderEstateCancel, ApprenticeshipCommand,
    PoliticalContinueGovernance, ContactInquiryRequest, VisitReplyCommand,
    AuthoredVisitReply, GovernanceInvitationReplyCommand,
    PrivateShopDailyRentCommand, BiologicalControlCommand,
    CollateralControlCommand, OpenBornPersonalDialogue,
    PoliticalSuccessionCommand, VisitAppointmentCommand,
    VisitAppointmentMeetingCommand, AuthoredVisitAppointment,
    PoliticalHeadHandoverCommand, GrowthTacticLessonCommand,
    PoliticalSuccessorRoundCommand, PoliticalTermHandoverCommand,
    OpenKnownPersonalDialogue, PoliticalOfficeReceptionCommand,
    PoliticalGovernanceAgencyCommand, TaskMessageReturnCollect,
    VoluntaryPathCommand, AuthoredGovernanceAgencyCommand,
    TaskPersonalMessageDeliver, PoliticalOfficeResignationCommand,
    AuthoredOfficeResignationCommand, TaskPersonalReport, TaskPersonalCancel,
    PoliticalSubordinateAppointmentCommand,
    AuthoredSubordinateAppointmentCommand, TaskPersonalAcceptance,
    RepairServiceCommandRequest, StabilizeContinuationBoundary,
    DirectForceIntent3, CancelPendingRetirement, CancelContinuationBoundary,
    ApproveGovernanceWorkReassignment, AcceptWorkReassignment,
    GrantChangeCommand, HouseholdPurchaseOriginalCommand,
    TellHouseholdPurchaseMarketReport, FieldArmyExitSelect>;
struct CommandPayload {
  StableId kind;
  CommandData data;
};
struct DurationQuote {
  std::optional<Integer> duration;
};
using QuoteData = std::variant<
    DurationQuote, TravelQuote, PreparedTravelOp, MilitaryQuote,
    InteractionQuote, EstateQuote, TaskQuote, FamilyQuote, PersonalQuote,
    GrowthQuote, PopulationMilitaryQuote, SiegeIntegrationQuote,
    SiegeIntegrationExecutionQuote, MembershipOfferQuote, PlayerPlanQuote,
    PoliticsQuote, PoliticalFiscalQuote, ProductionConfigurationQuote,
    GovernanceQuote, DiplomacyQuote, PartyParticipationQuote, NavalCommandQuote,
    HouseholdProvisionConfigurationQuote, TaskPersonalAdvanceQuote,
    NavalBattleCommandQuote, PoliticalTakeoverQuote, FamilyLegacyQuote,
    OccupationFiscalQuote, FamilyBequestQuote, TreatyPassageQuote,
    PoliticalMilitaryAppointmentQuote, FamilyInheritanceQuote,
    FamilyMarriageQuote, EstateEmploymentQuote, PoliticalFieldAppointmentQuote,
    NavalRepairQuote, EconomicSupplyPolicyQuote, FamilyBirthQuote,
    VisitMessageQuote, ChildCareReliefCommandQuote, PoliticalShopLicensingQuote,
    PrivateShopCommandQuote, TaskShopOrderQuote, TaskExtensionQuote,
    PoliticalShopRegistryAcceptanceQuote, PersonalAppointmentQuote,
    PrivateShopLiquidationQuote, TaskShopOrderEstateCancellationQuote,
    ApprenticeshipQuote, PoliticalGovernanceContinuationQuote,
    ContactInquiryQuote, VisitReplyQuote, AuthoredVisitReplyQuote,
    GovernanceInvitationReplyQuote, PrivateShopDailyRentQuote,
    LegacyControlDecisionQuote, CollateralControlQuote,
    BornPersonalDialogueQuote, PoliticalSuccessionQuote, VisitAppointmentQuote,
    VisitAppointmentMeetingQuote, AuthoredVisitAppointmentQuote,
    PoliticalHeadHandoverQuote, GrowthTacticLessonCommandQuote,
    PoliticalSuccessorRoundQuote, PoliticalTermHandoverQuote,
    KnownPersonalDialogueQuote, PoliticalOfficeReceptionQuote,
    PoliticalGovernanceAgencyQuote, TaskMessageReturnQuote, VoluntaryPathQuote,
    TaskPersonalMessageQuote, PoliticalOfficeResignationQuote,
    TaskPersonalReportQuote, TaskPersonalCancellationQuote,
    PoliticalSubordinateAppointmentQuote, TaskPersonalAcceptanceQuote,
    NativeHouseholdPurchaseCommandQuote, NativeHouseholdReportCommandQuote,
    PoliticalShopLicensingPreparedQuote>;
struct QuoteDetails {
  StableId kind;
  QuoteData data;
};
struct PreparedDomainOp {
  CommandPayload command;
  QuoteDetails quote;
  std::shared_ptr<const PreparedDomainOp> nested{};
};
struct ActionStarted {
  StableId action_id;
};
// 旧完成结果的键存在位不同：travel只有completed，注册军政有success/code/details，wait只有success。
struct ActionCompletion {
  bool ok = true;
  std::optional<bool> completed, success, already_completed;
  std::optional<StableId> code;
  std::optional<MilitaryEffect> military;
  std::optional<TaskEffect> task{};
  std::optional<FamilyEffect> family{};
  std::optional<GrowthEffect> growth{};
  FieldOrder military_field_order{};
  FieldOrder field_order{};
};
struct AdvanceDetails {
  OrderedTable<ActionCompletion> completed_actions;
  std::vector<StableId> suspended_actors;
  FieldOrder field_order{};
};
using ReceiptData = std::variant<
    Done, ActionStarted, AdvanceDetails, MilitaryEffect, InteractionEffect,
    EstateReceipt, FamilyEffect, PersonalReceipt, GrowthEffect,
    OrganizationEffects, SiegeIntegrationEffect, MembershipOfferEffects,
    PoliticsEffects, PoliticalFiscalEffects, LocalEconomyEffects,
    NavalCommandReceipt, HouseholdProvisionManagementEffects,
    NavalBattleCommandReceipt, NavalRepairReceipt, RepairServiceCommandReceipt,
    ContinuationBoundaryReceipt, DirectForceIntentReceipt3,
    PendingRetirementCancelled, ContinuationBoundaryCancelled, FieldBattleReceipt>;
struct ReceiptDetails {
  // Runtime ownership only. It precedes every deep field so the original
  // ordinary payload stays charged until its actual destruction.
  CheckpointCopyLease copy_memory;
  ReceiptData data;
  FieldOrder field_order{};
  ReceiptDetails() = default;
  ReceiptDetails(const ReceiptDetails &);
  ReceiptDetails(ReceiptDetails &&);
  ReceiptDetails &operator=(const ReceiptDetails &);
  ReceiptDetails &operator=(ReceiptDetails &&);
  ReceiptDetails(const ReceiptData &, const FieldOrder & = {});
  template <class T, std::enable_if_t<
                         !std::is_same_v<std::decay_t<T>, ReceiptDetails> &&
                             !std::is_same_v<std::decay_t<T>, ReceiptData> &&
                             std::is_constructible_v<ReceiptData, const T &>,
                         int> = 0>
  ReceiptDetails(const T &value, const FieldOrder &order = {})
      : copy_memory(reserve_payload(value, order)), data(value),
        field_order(order) {}
  void swap(ReceiptDetails &);
  friend void swap(ReceiptDetails &left, ReceiptDetails &right) {
    left.swap(right);
  }

private:
  template <class T>
  static CheckpointCopyLease reserve_payload(const T &value,
                                             const FieldOrder &order) {


    return CheckpointCopyLease::reserve();
  }
};
struct PhaseEffects {
  AdvanceDetails advance;
  std::vector<TerrainExecution> military_executions;
};
// 这些冷边界函数在调用方完成原世界/人物/幂等/版本守卫后才消费业务参数。
Outcome<CommandPayload> decode_resident_command(const StableId &,
                                                const BoundaryValue &params);
Outcome<BoundaryValue>
encode_resident_preview(const Outcome<std::shared_ptr<const PreviewResult>> &,
                        Integer revision);
Outcome<BoundaryValue>
encode_resident_receipt(const Outcome<std::shared_ptr<const CommitResult>> &);
Outcome<std::shared_ptr<const CommitResult>>
decode_resident_receipt(const BoundaryValue &);
Status visit_resident_receipt(CodecVisitor &,
                              std::shared_ptr<const CommitResult> &);
} // namespace hanmo::resident
