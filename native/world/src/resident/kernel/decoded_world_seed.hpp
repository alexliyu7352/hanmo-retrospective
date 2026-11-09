// 私有解码候选独占每个typed域；唯一移动到原WorldBundle，不复制shared
// const初态。
#pragma once
#include <stdexcept>
#include "world_bundle.hpp"
#include "resident/checkpoint/checkpoint_job_memory.hpp"
namespace hanmo::resident {
class DecodedWorldSeedOwner {
public:
  DecodedWorldSeedOwner() = default;
  DecodedWorldSeedOwner(DecodedWorldSeedOwner &&) = default;
  DecodedWorldSeedOwner &operator=(DecodedWorldSeedOwner &&) = default;
  DecodedWorldSeedOwner(const DecodedWorldSeedOwner &) = delete;

private:
  friend struct WorldCheckpointDecoder;
  friend struct WorldBundle;
  friend class WorldRestoreCandidate;
  bool decoded_ = false;
  std::shared_ptr<const PreparedCatalog>
  require_complete(std::shared_ptr<const PreparedCatalog> catalog) {
    if (!decoded_)
      throw std::logic_error("PrivateDecodedWorldSeedRequired");
    return catalog;
  }

  CheckpointJobReservation memory;
  CheckpointUniquePtr<PoliticalRelationsState> political_relations;
  CheckpointUniquePtr<PoliticalPassageDecisionState> political_passage;
  CheckpointUniquePtr<SafeExitState> political_safe_exit;
  CheckpointUniquePtr<CardAcquisitionState> card_acquisitions;
  CheckpointUniquePtr<ProfileDeliveryOutbox> profile_deliveries;
  CheckpointUniquePtr<SocialPeriodSources> social_period_sources;
  // 两段成对存在；新局由真实安装生成，冷恢复不补开期或从旧ticks推历史。
  CheckpointUniquePtr<AuthoredActorFoundationState> actor_foundation;
  CheckpointUniquePtr<CharacterInjurySourcesStateV1> character_injury_sources;
  CheckpointUniquePtr<GrowthPermanentState> growth_permanent;
  CheckpointUniquePtr<LifeAgeState> life_age_checks;
  CheckpointUniquePtr<ControlContinuationState> control_continuation;
  CheckpointUniquePtr<EconomicWorkSourcesState> production_cycles;
  CheckpointUniquePtr<WorldTimelineState> world_timeline;
  CheckpointUniquePtr<ActionElapsedStore> action_elapsed;
  CheckpointUniquePtr<FacilityClaimsState> facility_claims;
  CheckpointUniquePtr<MilitaryExactState> military_exact;
  CheckpointUniquePtr<MilitaryActionTimeState> military_action_time;
  CheckpointUniquePtr<FieldBattleWorldState> field_battle;
  CheckpointUniquePtr<PublicSupplyInspectionState> public_supply_inspection;
  CheckpointUniquePtr<ItemInstancesState> item_instances;
  CheckpointUniquePtr<LotPropertiesState> lot_properties;
  CheckpointUniquePtr<CraftJobsState> craft_jobs;
  CheckpointUniquePtr<EconomicFacilitySourcesState>
      economic_facility_capacity_sources;
  CheckpointUniquePtr<ProductionFacilityConditionsState>
      production_facility_conditions;
  CheckpointUniquePtr<GovernanceWorkExpenseState> governance_work_expense;
  CheckpointUniquePtr<PoliticalBudgetState> political_budgets;
  CheckpointUniquePtr<PoliticalTreatyTermsState> political_treaty_terms;
  CheckpointUniquePtr<BudgetExecutionState> budget_execution;
  CheckpointUniquePtr<PopulationTransportState> population_transport;
  CheckpointUniquePtr<HouseholdPurchaseState> household_purchases;
  CheckpointUniquePtr<HouseholdRetailState> household_retail_settlements;
  CheckpointUniquePtr<MovementExactState> movement_exact;
  CheckpointUniquePtr<SpatialCarrierState> spatial_carriers;
  CheckpointUniquePtr<GroupMovementExecutionState> group_movement_executions;
  CheckpointUniquePtr<TransportRegistrationState> transport_registrations;
  CheckpointUniquePtr<EconomicTransportExpenseState>
      economic_transport_expenses;
  CheckpointUniquePtr<PrivateTransportFundingState> private_transport_funding;

  CheckpointUniquePtr<MilitaryTacticLearningState> military_tactic_learning;
  CheckpointUniquePtr<TreatyPassageState> treaty_passage;
  CheckpointUniquePtr<PoliticalMilitaryAppointmentState>
      political_military_appointments;
  CheckpointUniquePtr<PoliticalFieldAppointmentState>
      political_field_appointments;
  CheckpointUniquePtr<OccupationFiscalState> occupation_fiscal;
  CheckpointUniquePtr<OccupationFiscalSettlementState>
      occupation_fiscal_settlement;
  CheckpointUniquePtr<FiscalTaxpayerContinuityState> fiscal_taxpayer_continuity;
  CheckpointUniquePtr<PoliticalTakeoverState> political_takeover;
  CheckpointUniquePtr<EconomicTakeoverExecutionState>
      economic_takeover_execution;
  CheckpointUniquePtr<FamilyDeathState> family_life;
  CheckpointUniquePtr<FamilyLegacyState> family_legacy;
  CheckpointUniquePtr<VoluntaryPathState> family_voluntary_paths;
  CheckpointUniquePtr<FamilyBequestState> family_bequest;
  CheckpointUniquePtr<FamilyInheritanceState> family_inheritance;
  CheckpointUniquePtr<FamilyMarriageState> family_marriage;
  CheckpointUniquePtr<FamilyApprenticeshipState> family_apprenticeship;
  CheckpointUniquePtr<DiscipleControlState> family_disciple_legacy;
  CheckpointUniquePtr<BiologicalControlState> family_biological_legacy;
  CheckpointUniquePtr<CollateralControlState> family_collateral_legacy;
  CheckpointUniquePtr<FamilyBirthState> family_birth;
  CheckpointUniquePtr<ChildCareReliefState> child_care_relief;
  CheckpointUniquePtr<EstateEmploymentState> estate_employment;
  CheckpointUniquePtr<EconomicSupplyPolicyState> economic_supply_policies;
  CheckpointUniquePtr<PoliticalSupplyAdoptionState> political_supply_adoptions;
  CheckpointUniquePtr<GrowthMarketHistoryState> growth_market_history;
  CheckpointUniquePtr<GrowthRecoveryState> personal_recovery;
  CheckpointUniquePtr<GrowthTacticLessonState> growth_tactic_lessons;
  CheckpointUniquePtr<PrivateShopState> private_shops;
  CheckpointUniquePtr<PrivateShopOperationSourcesState>
      private_shop_operation_sources;
  CheckpointUniquePtr<PrivateShopOperationCapabilitiesState>
      private_shop_operation_capabilities;
  CheckpointUniquePtr<PrivateShopHouseholdCustodyState>
      private_shop_household_custody;
  CheckpointUniquePtr<PrivateShopDailyRentState> private_shop_daily_rent;
  CheckpointUniquePtr<PrivateShopCommandResultsState>
      private_shop_command_results;
  CheckpointUniquePtr<PrivateShopLiquidationState> private_shop_liquidations;
  CheckpointUniquePtr<PoliticalShopLicensingState> political_shop_licensing;
  CheckpointUniquePtr<PoliticalShopLicensingReceiptState>
      political_shop_licensing_receipts;
  CheckpointUniquePtr<PoliticalGovernanceContinuationState>
      political_governance_continuations;
  CheckpointUniquePtr<GovernanceInvitationReplyState>
      governance_invitation_replies;
  CheckpointUniquePtr<PoliticalHeadHandoverState> political_head_handovers;
  CheckpointUniquePtr<PoliticalSuccessorRoundState> political_successor_rounds;
  CheckpointUniquePtr<PoliticalTermHandoverState> political_term_handovers;
  CheckpointUniquePtr<PoliticalOfficeReceptionState>
      political_office_receptions;
  CheckpointUniquePtr<PoliticalOfficeResignationState>
      political_office_resignations;
  CheckpointUniquePtr<PoliticalSubordinateAppointmentState>
      political_subordinate_appointments;
  CheckpointUniquePtr<PoliticalDismissalSourceState>
      political_dismissal_sources;
  CheckpointUniquePtr<PoliticalGovernanceAgencyState>
      political_governance_agencies;
  CheckpointUniquePtr<PoliticalSuccessionState> political_successions;
  CheckpointUniquePtr<PoliticalShopRegistrySuccessionState>
      political_shop_registry_successions;
  CheckpointUniquePtr<HouseholdProvisionManagementState>
      household_provision_management;
  CheckpointUniquePtr<ProductionDisruptionState> production_disruptions;
  CheckpointUniquePtr<NavalState> naval;
  CheckpointUniquePtr<EnvoyTaskState> envoy_tasks;
  CheckpointUniquePtr<DiplomacyState> diplomacy;
  CheckpointUniquePtr<PoliticsGovernanceState> politics_governance;
  CheckpointUniquePtr<GovernanceTaskState> governance_tasks;
  CheckpointUniquePtr<PoliticsState> politics;
  CheckpointUniquePtr<PoliticalFiscalState> political_fiscal;
  CheckpointUniquePtr<FiscalSettlementState> fiscal_settlement;
  CheckpointUniquePtr<PublicReliefState> public_relief;
  CommonState common;
  CheckpointUniquePtr<TravelState> travel;
  CheckpointUniquePtr<MilitaryState> military;
  CheckpointUniquePtr<InteractionState> interaction;
  CheckpointUniquePtr<SocialState> social;
  CheckpointUniquePtr<EstateState> estate;
  CheckpointUniquePtr<TaskState> tasks;
  CheckpointUniquePtr<FamilyState> family;
  CheckpointUniquePtr<PersonalState> personal;
  CheckpointUniquePtr<GrowthState> growth;
  CheckpointUniquePtr<PopulationMilitaryState> population_military;
  CheckpointUniquePtr<PlayerPlanState> player_plan;
  CheckpointUniquePtr<LocalEconomyState> local_economy;
  CheckpointUniquePtr<EconomicObligationSourcesState>
      economic_obligation_sources;
  CheckpointUniquePtr<OrganizationState> organization;
  CheckpointUniquePtr<OrganizationRuntime> organization_runtime;
  CheckpointUniquePtr<GovernanceProgramState> governance_programs;
  CheckpointUniquePtr<AffairsState> affairs;
  CheckpointUniquePtr<DirectorState> director;
};
} // namespace hanmo::resident
