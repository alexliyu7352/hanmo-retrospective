#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/politics/relations/political_relations.hpp"
#include "resident/domains/politics/treaty/political_passage_source.hpp"
#include "resident/domains/politics/treaty/safe_exit_entry.hpp"
#include "exact_time_profile.hpp"
#include "growth_recovery_integration.hpp"
#include "private_shop_command_results_integration.hpp"
#include "private_shop_daily_rent_integration.hpp"
#include "private_shop_liquidation_integration.hpp"
#include "profile_delivery.hpp"
#include "read_stamp.hpp"
#include "resident/domains/actor_foundation/model.hpp"
#include "resident/domains/actor_foundation/injury_sources_v1.hpp"
#include "resident/domains/commerce/household_retail.hpp"
#include "resident/domains/commerce/private_shop_capability_model.hpp"
#include "resident/domains/commerce/private_shop_operation_sources_model.hpp"
#include "resident/domains/commerce/self_transport_funding.hpp"
#include "resident/domains/craft/craft_job_model.hpp"
#include "resident/domains/craft/item_service.hpp"
#include "resident/domains/family/apprenticeship_world.hpp"
#include "resident/domains/family/biological_control_world.hpp"
#include "resident/domains/family/collateral_control_world.hpp"
#include "resident/domains/family/control_continuation.hpp"
#include "resident/domains/family/disciple_control_world.hpp"
#include "resident/domains/family/life_age.hpp"
#include "resident/domains/growth/growth_permanent_contributions.hpp"
#include "resident/domains/growth/profile_acquisition_world.hpp"
#include "resident/domains/local_economy/economic_facility_sources.hpp"
#include "resident/domains/local_economy/economic_work_sources.hpp"
#include "resident/domains/local_economy/fiscal/budget_execution_contract.hpp"
#include "resident/domains/local_economy/governance_work/governance_work_expense_contract.hpp"
#include "resident/domains/local_economy/household_purchase_contract.hpp"
#include "resident/domains/local_economy/obligations/obligation_sources_model.hpp"
#include "resident/domains/local_economy/population_transport.hpp"
#include "resident/domains/local_economy/private_shop_household_custody.hpp"
#include "resident/domains/local_economy/production/facility_condition_contract.hpp"
#include "resident/domains/military/military_exact_sources.hpp"
#include "resident/domains/military/military_tactic_learning_model.hpp"
#include "resident/domains/military/training_exact_model.hpp"
#include "resident/domains/organization/organization_runtime.hpp"
#include "resident/domains/politics/budget/political_budget_contract.hpp"
#include "resident/domains/politics/governance_targets/governance_program.hpp"
#include "resident/domains/politics/treaty/treaty_terms_state.hpp"
#include "resident/domains/social/social_period_sources.hpp"
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
#include "resident/domains/tasks/shop_order_contacts/task_shop_order_contact_world.hpp"
#include "resident/domains/travel/group_movement.hpp"
#include "resident/facility_claims/model.hpp"
// 唯一候选的组合所有者；clone复制typed权威并重绑借用服务，不复制第二份成员/资产世界。
#pragma once
#include "command_types.hpp"
#include "diplomacy_integration.hpp"
#include "economic_supply_policy_integration.hpp"
#include "estate_employment_integration.hpp"
#include "governance_invitation_world.hpp"
#include "growth_market_history_integration.hpp"
#include "growth_tactic_lesson_integration.hpp"
#include "household_provision_integration.hpp"
#include "native_actor_social_occupancy.hpp"
#include "political_dismissal_world.hpp"
#include "political_field_appointment_integration.hpp"
#include "political_governance_agency_world.hpp"
#include "political_head_handover_world.hpp"
#include "political_military_appointment_integration.hpp"
#include "political_office_reception_world.hpp"
#include "political_office_resignation_world.hpp"
#include "political_shop_licensing_integration.hpp"
#include "political_shop_registry_integration.hpp"
#include "political_subordinate_appointment_world.hpp"
#include "political_succession_world.hpp"
#include "political_successor_round_world.hpp"
#include "political_term_handover_world.hpp"
#include "resident/checkpoint/checkpoint_job_budget.hpp"
#include "resident/domains/affairs/affairs_model.hpp"
#include "resident/domains/commerce/private_shop.hpp"
#include "resident/domains/commerce/private_shop_staff_exclusion.hpp"
#include "resident/domains/director/director_model.hpp"
#include "resident/domains/family/child_care_relief_world.hpp"
#include "resident/domains/family/family_bequest_commands.hpp"
#include "resident/domains/family/family_birth_commands.hpp"
#include "resident/domains/family/family_birth_world.hpp"
#include "resident/domains/family/family_death.hpp"
#include "resident/domains/family/family_inheritance_reads.hpp"
#include "resident/domains/family/family_legacy_commands.hpp"
#include "resident/domains/family/family_marriage_commands.hpp"
#include "resident/domains/family/voluntary_path_world.hpp"
#include "resident/domains/growth/growth_model.hpp"
#include "resident/domains/growth/growth_tactic_lessons.hpp"
#include "resident/domains/interaction/public_supply_inspection.hpp"
#include "resident/domains/local_economy/disruptions/production_disruptions.hpp"
#include "resident/domains/local_economy/fiscal/fiscal_settlement_view.hpp"
#include "resident/domains/local_economy/fiscal/fiscal_taxpayer_continuity.hpp"
#include "resident/domains/local_economy/fiscal/occupation_fiscal_settlement.hpp"
#include "resident/domains/local_economy/relief/public_relief_rules.hpp"
#include "resident/domains/local_economy/takeover/execution_phase.hpp"
#include "resident/domains/local_economy/views/local_economy_view.hpp"
#include "resident/domains/military/field_battle/field_world.hpp"
#include "resident/domains/personal/personal_model.hpp"
#include "resident/domains/player_plan/plan_model.hpp"
#include "resident/domains/politics/diplomacy/diplomacy_profile.hpp"
#include "resident/domains/politics/diplomacy/diplomacy_projection.hpp"
#include "resident/domains/politics/governance_continuation/political_governance_continuation.hpp"
#include "resident/domains/politics/licensing/political_shop_licensing_result_bindings.hpp"
#include "resident/domains/politics/politics_governance_validation.hpp"
#include "resident/domains/social/social_model.hpp"
#include "resident/domains/tasks/envoy_task_payloads.hpp"
#include "resident/domains/tasks/envoy_task_view.hpp"
#include "resident/domains/tasks/governance_task_payloads.hpp"
#include "resident/domains/tasks/governance_task_view.hpp"
#include "resident/domains/tasks/task_public_account_payment.hpp"
#include "resident/model/regional_life_contract.hpp"
#include "resident/views/map_presence.hpp"
#include "resident/views/world_observation.hpp"
#include "world_read_stamp.hpp"
namespace hanmo::resident {
struct CompleteExactWorldReadBindings;
class ProjectionWriter;
struct DecodedWorldSeedOwner;

class CheckpointRetentionCensus;
struct WorldBundle;
class BundleServices final : public MembershipResolver,
                             public ActivityRead,
                             public TravelServices,
                             public SocialFactRead,
                             public TaskOwnerRead,
                             public FamilyIdentityRead,
                             public HouseholdOwnerRead,
                             public OrganizationOwnerRead,
                             public ReservationOwnerRead,
                             public DynamicResultRead,
                             public GrowthLifeRead,
                             public MembershipReleaseRead,
                             public GovernanceInvitationReplyRead,
                             public PoliticalGovernanceAgencyRead {
public:
  explicit BundleServices(WorldBundle &);
  Outcome<PoliticalGovernanceAgencySource>
  agency_for_commission(const StableId &, const StableId &) const override;
  Outcome<PoliticalGovernanceAgencySource>
  agency_before_issue(const StableId &, const StableId &,
                      Integer) const override;
  Outcome<PoliticalGovernanceAgencySource>
  agency_for_issued_authority(const StableId &) const override;
  Outcome<std::optional<GovernanceInvitationReplySource>>
  governance_invitation_reply(const StableId &) const override;
  const std::vector<StableId> *
  active_army_members(const StableId &) const override;
  const StableId *army_leader(const StableId &) const override;
  bool field_battle_binding_ready() const override { return true; }
  Outcome<bool> field_battle_bound(const StableId &) const override;
  Outcome<std::optional<FieldBattleSpatialView>>
  field_battle_position(const StableId &) const override;
  bool in_open_battle(const StableId &) const override;
  bool in_interaction(const StableId &,
                      const StableId &allowed_speaker = {}) const override;
  Outcome<Location> resolved_location(const StableId &) const override;
  StableId party_id(const StableId &) const override;
  Integer party_extra_cost(const StableId &) const override;
  Outcome<std::optional<ArmyTravelView>>
  active_army(const StableId &) const override;
  bool army_embarked(const StableId &) const override;
  Status naval_voyage_allowed(const StableId &,
                              const TravelQuote &) const override;
  bool naval_berth(const StableId &) const override;
  Outcome<std::optional<VesselRemainsLocationView>>
  vessel_remains(const StableId &) const override;
  Outcome<std::optional<VesselFleetLocationView>>
  vessel_location(const StableId &) const override;
  Outcome<std::optional<VesselFleetTravelView>>
  vessel_journey(const StableId &, const StableId &,
                 const StableId &) const override;
  Outcome<std::optional<VesselFleetTravelView>>
  vessel_fleet(const StableId &) const override;
  bool talking(const StableId &) const override;
  Outcome<bool> admission_allowed(const StableId &,
                                  const StableId &) const override;
  const terrain::Flags &terrain_flags() const override;
  // 完整填入原派生flags后冻结；新candidate的BundleServices仍独立未冻结。
  void prepare_checkpoint_read_cache();
  void retain_checkpoint_read_cache(CheckpointRetentionCensus &) const;
  const TravelScheduleDefinition *schedule(const StableId &) const override;
  const StableId &player_actor_id() const override;
  std::optional<SocialObservedFact>
  social_fact(const StableId &) const override;
  bool permits_named_membership_facts() const override;
  bool task_exists(const StableId &) const override;
  bool household_exists(const StableId &) const override;
  bool organization_exists(const StableId &) const override;
  bool reservation_exists(const ReservationRef &) const override;
  bool supports_action_result(const StableId &) const override;
  std::optional<DynamicActionResult>
  action_result(const StableId &) const override;
  const DeliveryFact *delivery(const StableId &) const override;
  std::optional<DynamicFactView> fact(const StableId &) const override;
  std::vector<StableId> fact_ids(Integer after_tick,
                                 Integer through_tick) const override;
  std::optional<LifeActivityView>
  life_result(const StableId &action_id) const override;
  std::vector<StableId> life_fact_ids(Integer after_tick,
                                      Integer through_tick) const override;
  bool has_existing_contract_service(const StableId &provider_actor_id,
                                     const StableId &role_id) const override;
  OrganizationAccess organization_access() const;
  Outcome<MembershipReleaseView>
  membership_release_view(const StableId &) const override;
  Status validate_dynamic_actor(const CommonRead &,
                                const ActorRecord &) const override;
  FamilyAccess family_access(const InteractionAccess &,
                             const FamilyOriginV2Sources *origins = nullptr,
                             const LifeAgeSources *initial_age = nullptr) const;
  InteractionAccess interaction_access(
      const CompleteExactWorldReadBindings *actual = nullptr) const;
  MilitaryAccess
  military_access(std::optional<Integer> population_phase_tick = {},
                  const DynamicStepInput *occupation_phase = nullptr) const;
  EstateAccess estate_access() const;
  PersonalAccess personal_access() const;
  GrowthAccess growth_access(const DynamicStepInput *phase = nullptr) const;
  GrowthMarketHistoryWorldAccess *market_history() const {
    return market_history_.get();
  }
  NativeActorSocialOccupancyWorld *
  actor_social_occupancy_world() const noexcept {
    return social_occupancy_.get();
  }

private:
  WorldBundle &world_;
  WorldCurrentMomentRead occupation_clock_;
  PoliticalMilitaryAppointmentWorldAccess military_appointments_;
  PoliticalFieldAppointmentWorldAccess field_appointments_;
  mutable CheckpointMemoryCharge flags_memory_;
  mutable terrain::Flags flags_;
  bool checkpoint_read_cache_frozen_ = false;
  std::unique_ptr<GrowthMarketHistoryWorldAccess> market_history_;
  std::unique_ptr<NativeActorSocialOccupancyWorld> social_occupancy_;
  std::optional<PrivateShopWorkerCommitmentAccess> shop_workers_;
};
struct ActiveReadPhase;
class PhaseCurrentReadScope;
class WorldCurrentReadStampRead;
class CheckpointRetentionCensus;
class CompleteExactWorldBindingOwner;
class CompleteExactCommandCapture;
class WorldExactWriteScope;
class JsonDecodedWorldSeedOwner;
class JsonDecodedSeedState;
struct WorldBundle {
  std::shared_ptr<const JsonDecodedSeedState> checkpoint_json_preimage;
  CheckpointJobReservation checkpoint_candidate_memory;
  CheckpointMemoryCharge
      checkpoint_copy_memory; // ordinary-STL fresh-copy reserve; last released
                              // with this owner
  // 热借用控制块不复制/不持久化；显式copy构造也自动创建新authority。
  std::unique_ptr<ReadStampAuthority> read_stamps{new ReadStampAuthority};
  void note_write_boundary() noexcept { read_stamps->note_mutation_boundary(); }
  Outcome<std::unique_ptr<PhaseCurrentReadScope>>
  enter_phase_read_context(const StableId &phase, const StableId &provider,
                           const MomentSpan &,
                           const WorldMoment &actual_moment) const;
  Outcome<WorldMoment> actual_phase_moment() const;
  Status prepare_checkpoint_read_cache();
  std::unique_ptr<CurrentReadStampRead>
  make_current_read_stamp_reader(const DomainReadVersionSource &,
                                 ReadStampLimits) const;
  // Owns the real same-candidate domain revision resolver for the entire read
  // lease.
  std::unique_ptr<CurrentReadStampRead>
      make_current_read_stamp_reader(ReadStampLimits) const;
  Outcome<std::unique_ptr<CompleteExactWorldBindingOwner>>
  complete_exact_time_bindings() const;
  // Mutable candidate factories may expose actual writers; const readers never
  // do.
  Outcome<std::unique_ptr<CompleteExactWorldBindingOwner>>
  complete_exact_time_bindings();
  bool has_active_read_phase() const noexcept {
    return bool(active_read_phase_);
  }
  ExactPhaseWriteContext *active_exact_write_context() const noexcept {
    return active_exact_writes_;
  }

private:
  friend class PhaseCurrentReadScope;
  friend class WorldExactWriteScope;
  // 原 Field 窗口只读真实 S0 span/写栈身份，不向领域开放 phase setter。
  friend class NativeFieldBattleWorldRead;
  friend class OriginalWorldReadBoundary;
  ExactPhaseWriteContext *active_exact_writes_ =
      nullptr; // transient, never copied or persisted
  friend class WorldCurrentReadStampRead;
  mutable std::shared_ptr<const ActiveReadPhase> active_read_phase_;
  mutable std::uint64_t read_phase_generation_ = 0;
  void leave_phase_read_context(std::uint64_t) const noexcept;

public:
  std::shared_ptr<const PreparedCatalog> catalog;
  // 独立 P3 原关系根；只在真实显式模块组合存在。
  std::optional<PoliticalRelationsState> political_relations;
  // M-PASSAGE/3 唯一原决定与 SafeExit 拥有体，不复用旧条约送达表。
  std::optional<PoliticalPassageDecisionState> political_passage;
  std::optional<SafeExitState> political_safe_exit;
  CommonStore common;
  // 当前Moment唯一由common.tick与此余量组成；配对elapsed仍附原Action。
  std::optional<AuthoredActorFoundationState> actor_foundation;
  // 原 source/1 逐笔应用来源；不保存第二份当前身体或恢复余额。
  std::optional<CharacterInjurySourcesStateV1> character_injury_sources;
  std::optional<GrowthPermanentState> growth_permanent;
  std::optional<LifeAgeState> life_age_checks;
  std::optional<ControlContinuationState> control_continuation;
  std::optional<EconomicWorkSourcesState> production_cycles;
  bool life_age_checks_seed_complete = false;
  std::optional<WorldTimelineState> world_timeline;
  std::optional<SocialPeriodSources> social_period_sources;
  std::optional<ActionElapsedStore> action_elapsed;
  std::optional<FacilityClaimsState> facility_claims;
  TravelState travel;
  std::optional<MovementExactState> movement_exact;
  std::optional<SpatialCarrierState> spatial_carriers;
  std::optional<GroupMovementExecutionState> group_movement_executions;
  std::optional<TransportRegistrationState> transport_registrations;
  std::optional<EconomicTransportExpenseState> economic_transport_expenses;
  std::optional<PrivateTransportFundingState> private_transport_funding;
  MilitaryState military;
  // Sole selected dynamic_military/2|3 owner, including original Duty child.
  std::optional<MilitaryExactState> military_exact;
  // Separate original military_action_time/1 owner, never a copied Duty root.
  std::optional<MilitaryActionTimeState> military_action_time;
  // field_battle/1 的唯一可变根；未选此能力时必须 absent。
  std::optional<FieldBattleWorldState> field_battle;
  // 真实 Inspect 的唯一取得来源根；不另存 Report 或 Knowledge 副本。
  std::optional<PublicSupplyInspectionState> public_supply_inspection;
  // Repair jobs/contracts stay embedded in the one original Item owner.
  std::optional<ItemInstancesState> item_instances;
  std::optional<LotPropertiesState> lot_properties;
  std::optional<CraftJobsState> craft_jobs;
  std::optional<EconomicFacilitySourcesState>
      economic_facility_capacity_sources;
  std::optional<ProductionFacilityConditionsState>
      production_facility_conditions;
  std::optional<GovernanceWorkExpenseState> governance_work_expense;
  // 同一候选内拥有的唯一预算根；缺真实来源时不得启用 profile。
  std::optional<PoliticalBudgetState> political_budgets;
  // 明示 /1 或 /2 的同一物理根；resource_body 只归该根拥有。
  std::optional<PoliticalTreatyTermsState> political_treaty_terms;
  std::optional<BudgetExecutionState> budget_execution;
  std::optional<PopulationTransportState> population_transport;
  std::optional<HouseholdPurchaseState> household_purchases;
  std::optional<HouseholdRetailState> household_retail_settlements;
  bool military_siege_engineering_enabled() const {
    return catalog->military &&
           catalog->military->siege_engineering_policy.has_value();
  }
  MilitaryTacticLearningState military_tactic_learning;
  bool military_tactic_learning_seed_complete = false;
  bool military_tactic_learning_enabled() const {
    return catalog->military &&
           catalog->military->battle_catalog.tactic_learning.has_value();
  }
  NavalState naval;
  bool naval_seed_complete = false;
  bool military_command_resources_enabled() const {
    return catalog->military &&
           catalog->military->command_resources_policy.has_value();
  }
  bool military_sequential_enabled() const {
    return catalog->military &&
           catalog->military->sequential_round_policy.has_value();
  }
  bool military_commanded_damage_enabled() const {
    return catalog->military &&
           catalog->military->commanded_damage_policy.has_value();
  }
  bool military_combat_conditions_enabled() const {
    return catalog->military &&
           catalog->military->combat_condition_policy.has_value();
  }
  bool military_garrison_rest_enabled() const {
    return catalog->military &&
           catalog->military->garrison_rest_policy.has_value();
  }
  bool military_command_shock_enabled() const {
    return catalog->military &&
           catalog->military->command_shock_policy.has_value();
  }
  bool military_rout_enabled() const {
    return catalog->military &&
           catalog->military->rout_retreat_policy.has_value();
  }
  bool military_supply_attrition_enabled() const {
    return catalog->military &&
           catalog->military->supply_attrition_policy.has_value();
  }
  bool military_recovery_enabled() const {
    return catalog->military && catalog->military->recovery_policy.has_value();
  }
  bool naval_enabled() const { return bool(catalog->naval); }
  bool naval_repairs_enabled() const {
    return naval_enabled() && catalog->naval->repair_policy.has_value();
  }
  bool naval_repair_charges_enabled() const {
    return naval_repairs_enabled() &&
           catalog->naval->repair_charge_policy.has_value();
  }
  bool naval_navigation_enabled() const {
    return naval_repair_charges_enabled() &&
           catalog->naval->navigation_policy.has_value();
  }
  bool naval_gunnery_enabled() const {
    return naval_navigation_enabled() && catalog->naval->combat_policy &&
           catalog->naval->combat_policy->combat.gunnery.has_value();
  }
  bool naval_combat_enabled() const {
    return naval_enabled() && catalog->naval->combat_policy.has_value();
  }
  InteractionState interaction;
  SocialState social;
  EstateState estate;
  TaskState tasks;
  EnvoyTaskState envoy_tasks;
  bool envoy_tasks_seed_complete = false;
  bool envoy_tasks_enabled() const { return catalog->envoy_tasks_enabled; }
  bool party_invitations_enabled() const {
    return catalog->party_invitations_enabled;
  }
  GovernanceTaskState governance_tasks;
  bool governance_tasks_seed_complete = false;
  bool governance_tasks_enabled() const {
    return catalog->governance_tasks_enabled;
  }
  FamilyState family;
  VoluntaryPathState family_voluntary_paths;
  bool family_voluntary_paths_seed_complete = false;
  bool family_voluntary_paths_enabled() const {
    return catalog->family_voluntary_paths &&
           catalog->family_voluntary_paths->enabled;
  }
  FamilyDeathState family_life;
  bool family_life_seed_complete = false;
  bool family_life_enabled() const {
    return catalog->family_life && catalog->family_life->enabled;
  }
  // 借同候选已核军事损失，只返回精确终点来源，不复制第二份死亡名册。
  FamilyDeathOriginRead family_death_read() const;
  FamilyLegacyState family_legacy;
  bool family_legacy_seed_complete = false;
  bool family_legacy_enabled() const {
    return catalog->family_legacy && catalog->family_legacy->enabled;
  }
  // 仅借原Military/Naval的资格纯读；不闭包回读旧Family控制候选。
  FamilyLegacyPlayableRead family_legacy_playable_read() const;
  FamilyBequestState family_bequest;
  bool family_bequest_seed_complete = false;
  bool family_bequest_enabled() const {
    return catalog->family_bequest && catalog->family_bequest->enabled;
  }
  // 原控制链只借当前Family/legacy；不捕获声明副本或改写历史。
  FamilyBequestPlayerRead family_bequest_player_read() const;
  FamilyInheritanceState family_inheritance;
  bool family_inheritance_seed_complete = false;
  bool family_inheritance_enabled() const {
    return catalog->family_inheritance && catalog->family_inheritance->enabled;
  }
  // 借本候选原通知及各责任域纯读；不捕获旧家事或临时Estate指针。
  FamilyInheritanceAccess family_inheritance_access() const;
  CollateralControlState family_collateral_legacy;
  bool family_collateral_legacy_seed_complete = false;
  bool family_collateral_legacy_enabled() const {
    return catalog->family_collateral_legacy &&
           catalog->family_collateral_legacy->enabled;
  }
  BiologicalControlState family_biological_legacy;
  bool family_biological_legacy_seed_complete = false;
  bool family_biological_legacy_enabled() const {
    return catalog->family_biological_legacy &&
           catalog->family_biological_legacy->enabled;
  }
  DiscipleControlState family_disciple_legacy;
  bool family_disciple_legacy_seed_complete = false;
  bool family_disciple_legacy_enabled() const {
    return catalog->family_disciple_legacy &&
           catalog->family_disciple_legacy->enabled;
  }
  FamilyApprenticeshipState family_apprenticeship;
  bool family_apprenticeship_seed_complete = false;
  bool family_apprenticeship_enabled() const {
    return catalog->family_apprenticeship &&
           catalog->family_apprenticeship->enabled;
  }
  ApprenticeshipIdentityRead family_apprenticeship_access() const;
  FamilyMarriageState family_marriage;
  bool family_marriage_seed_complete = false;
  EstateEmploymentState estate_employment;
  bool estate_employment_seed_complete = false;
  EconomicSupplyPolicyState economic_supply_policies;
  bool economic_supply_policies_seed_complete = false;
  PoliticalSupplyAdoptionState political_supply_adoptions;
  bool political_supply_adoptions_seed_complete = false;
  bool economic_supply_policies_enabled() const {
    return catalog->economic_supply_policies_enabled;
  }
  bool estate_employment_enabled() const {
    return bool(catalog->estate_employment);
  }
  bool family_marriage_enabled() const {
    return catalog->family_marriage && catalog->family_marriage->enabled;
  }
  // 历史本人和具名军事处境只借当前候选，不捕获旧婚姻账。
  MarriageAccess family_marriage_access() const;
  FamilyBirthState family_birth;
  bool family_birth_seed_complete = false;
  bool family_birth_enabled() const {
    return catalog->family_birth && catalog->family_birth->enabled;
  }
  BirthArrangementAccess family_birth_access() const;
  ChildCareReliefState child_care_relief;
  bool child_care_relief_seed_complete = false;
  bool family_care_relief_enabled() const {
    return catalog->family_care_relief && catalog->family_care_relief->enabled;
  }
  ChildCareReliefAccess family_care_relief_access() const;
  PersonalState personal;
  GrowthState growth;
  ProfileDeliveryOutbox profile_deliveries;
  Status record_profile_acquisition(const ProfileAcquisitionSource &);
  Status validate_profile_deliveries() const;
  CardAcquisitionState card_acquisitions;
  bool card_acquisitions_seed_complete = false;
  Outcome<std::unique_ptr<ProfileAcquisitionWorldRead>>
  profile_acquisition_read() const;
  GrowthTacticLessonState growth_tactic_lessons;
  bool growth_tactic_lessons_seed_complete = false;
  bool growth_tactic_lessons_enabled() const {
    return bool(catalog->growth_tactic_lessons);
  }
  GrowthRecoveryState personal_recovery;
  bool personal_recovery_seed_complete = false;
  bool personal_recovery_enabled() const {
    return bool(catalog->personal_recovery);
  }
  GrowthMarketHistoryState growth_market_history;
  bool growth_market_history_seed_complete = false;
  GrowthMarketCaptureState growth_market_capture;
  PrivateShopState private_shops;
  bool private_shops_seed_complete = false;
  std::optional<PrivateShopOperationSourcesState>
      private_shop_operation_sources;
  std::optional<PrivateShopOperationCapabilitiesState>
      private_shop_operation_capabilities;
  // 明确同意的独立原源；不扩旧 operation /2 载荷。
  std::optional<PrivateShopHouseholdCustodyState>
      private_shop_household_custody;
  PrivateShopDailyRentState private_shop_daily_rent;
  bool private_shop_daily_rent_seed_complete = false;
  PrivateShopCommandResultsState private_shop_command_results;
  bool private_shop_command_results_seed_complete = false;
  bool private_shop_command_results_enabled() const {
    return bool(catalog->private_shop_command_results);
  }
  bool private_shop_daily_rent_enabled() const {
    return bool(catalog->private_shop_daily_rent);
  }
  PrivateShopLiquidationState private_shop_liquidations;
  bool private_shop_liquidations_seed_complete = false;
  bool private_shop_liquidations_enabled() const {
    return bool(catalog->private_shop_liquidations);
  }
  PoliticalShopLicensingState political_shop_licensing;
  bool political_shop_licensing_seed_complete = false;
  std::optional<PoliticalShopLicensingReceiptState>
      political_shop_licensing_receipts;
  std::optional<PoliticalGovernanceContinuationState>
      political_governance_continuations;
  std::optional<GovernanceInvitationReplyState> governance_invitation_replies;
  std::optional<PoliticalHeadHandoverState> political_head_handovers;
  std::optional<PoliticalSuccessorRoundState> political_successor_rounds;
  std::optional<PoliticalTermHandoverState> political_term_handovers;
  std::optional<PoliticalOfficeReceptionState> political_office_receptions;
  std::optional<PoliticalOfficeResignationState> political_office_resignations;
  std::optional<PoliticalSubordinateAppointmentState>
      political_subordinate_appointments;
  std::optional<PoliticalDismissalSourceState> political_dismissal_sources;
  std::optional<PoliticalGovernanceAgencyState> political_governance_agencies;
  std::optional<PoliticalSuccessionState> political_successions;
  PoliticalShopRegistrySuccessionState political_shop_registry_successions;
  bool political_shop_registry_successions_seed_complete = false;
  bool growth_market_history_enabled() const {
    return bool(catalog->growth_market_history);
  }
  bool private_shops_enabled() const { return bool(catalog->private_shops); }
  bool political_shop_licensing_enabled() const {
    return bool(catalog->political_shop_licensing);
  }
  bool political_shop_registry_successions_enabled() const {
    return bool(catalog->political_shop_registry_successions);
  }
  bool task_shop_orders_enabled() const {
    return catalog->task_shop_orders_enabled;
  }
  LocalEconomyState local_economy;
  std::optional<EconomicObligationSourcesState> economic_obligation_sources;
  OrganizationState organization;
  std::optional<OrganizationRuntime> organization_runtime;
  std::optional<GovernanceProgramState> governance_programs;
  AffairsState affairs;
  DirectorState director;
  PopulationMilitaryState population_military;
  TreatyPassageState treaty_passage;
  bool treaty_passage_seed_complete = false;
  bool treaty_passage_enabled() const {
    return catalog->treaty_passage && catalog->treaty_passage->enabled;
  }
  PoliticalMilitaryAppointmentState political_military_appointments;
  bool political_military_appointments_seed_complete = false;
  PoliticalFieldAppointmentState political_field_appointments;
  bool political_field_appointments_seed_complete = false;
  bool political_military_appointments_enabled() const {
    return catalog->political_military_appointments &&
           catalog->political_military_appointments->enabled;
  }
  bool political_field_appointments_enabled() const {
    return catalog->political_field_appointments &&
           catalog->political_field_appointments->enabled;
  }
  DiplomacyState diplomacy;
  bool diplomacy_seed_complete = false;
  DiplomacyBusinessViewIndex diplomacy_view_index;
  bool diplomacy_enabled() const { return catalog->envoy_tasks_enabled; }
  DiplomacyReadContext
  diplomacy_read(const InteractionAccess &,
                 const DiplomacyNavigationRead *navigation = nullptr) const;
  DiplomacyWriteContext
  diplomacy_write(const InteractionAccess &,
                  const DiplomacyNavigationRead *navigation = nullptr);
  void rebuild_diplomacy_index();
  PoliticalTakeoverState political_takeover;
  bool political_takeover_seed_complete = false;
  EconomicTakeoverExecutionState economic_takeover_execution;
  bool economic_takeover_seed_complete = false;
  bool political_takeover_enabled() const {
    return dynamic_enabled() && catalog->political_takeover &&
           catalog->political_takeover->enabled;
  }
  OccupationFiscalState occupation_fiscal;
  bool occupation_fiscal_seed_complete = false;
  OccupationFiscalSettlementState occupation_fiscal_settlement;
  bool occupation_fiscal_settlement_seed_complete = false;
  bool occupation_fiscal_enabled() const {
    return political_takeover_enabled() && catalog->occupation_fiscal &&
           catalog->occupation_fiscal->enabled;
  }
  bool occupation_fiscal_needs_enabled() const {
    return occupation_fiscal_enabled() &&
           catalog->occupation_fiscal_needs_enabled;
  }
  PoliticsState politics;
  PoliticalFiscalState political_fiscal;
  PoliticsGovernanceState politics_governance;
  bool politics_governance_seed_complete = false;
  bool politics_governance_enabled() const {
    return catalog->governance_tasks_enabled;
  }
  PoliticsViewIndex politics_view_index;
  GovernanceViewIndex governance_view_index;
  FiscalSettlementState fiscal_settlement;
  bool fiscal_settlement_seed_complete = false;
  FiscalTaxpayerContinuityState fiscal_taxpayer_continuity;
  bool fiscal_taxpayer_continuity_seed_complete = false;
  bool fiscal_taxpayer_continuity_enabled() const {
    return catalog->fiscal_taxpayer_continuity_enabled &&
           fiscal_settlement_enabled() && political_takeover_enabled();
  }
  PublicReliefState public_relief;
  bool public_relief_seed_complete = false;
  HouseholdProvisionManagementState household_provision_management;
  bool household_provision_management_seed_complete = false;
  HouseholdProvisionViewIndex household_provision_index;
  ProductionDisruptionState production_disruptions;
  bool production_disruptions_seed_complete = false;
  bool production_disruptions_enabled() const {
    return dynamic_enabled() && catalog->production_disruptions &&
           catalog->production_disruptions->version == 1;
  }
  bool public_relief_enabled() const {
    return catalog->governance_tasks_enabled;
  }
  bool fiscal_settlement_enabled() const {
    return political_fiscal_enabled() && catalog->fiscal_settlement &&
           catalog->fiscal_settlement->version == 1;
  }
  bool production_management_enabled() const {
    return dynamic_enabled() && catalog->local_economy &&
           catalog->local_economy->production_management &&
           catalog->local_economy->production_management->version == 1;
  }
  bool household_control_enabled() const {
    return dynamic_enabled() && catalog->family &&
           catalog->family->household_control.has_value();
  }
  FamilyHouseholdControlAccess
  household_control_read(const LifeAgeSources *initial_age = nullptr) const {
    return {common,           family,        *catalog->family,
            *catalog->common, local_economy, initial_age};
  }
  bool household_provisioning_enabled() const {
    return dynamic_enabled() && catalog->local_economy &&
           catalog->local_economy->household_provisioning &&
           catalog->local_economy->household_provisioning->version == 1;
  }
  bool politics_seed_complete = false, political_fiscal_seed_complete = false;
  bool politics_enabled() const {
    return dynamic_enabled() && catalog->politics && catalog->politics->enabled;
  }
  bool political_fiscal_enabled() const {
    return politics_enabled() && catalog->political_fiscal &&
           catalog->political_fiscal->enabled;
  }
  PoliticsReadContext
  politics_read(const PoliticalNavigationRead *navigation = nullptr) const;
  PoliticsWriteContext
  politics_write(const PoliticalNavigationRead *navigation = nullptr);
  PoliticalFiscalReadContext political_fiscal_read() const;
  PoliticalFiscalWriteContext political_fiscal_write();
  GovernanceReadContext governance_read() const;
  GovernanceWriteContext governance_write();
  PlayerPlanState player_plan;
  bool player_plan_seed_complete = false;
  bool population_seed_complete = false;
  bool dynamic_seed_complete = false;
  bool player_plan_enabled() const { return catalog->player_plans; }
  bool dynamic_enabled() const {
    return catalog->dynamic_profile == DynamicProfile::local_society_v1;
  }
  bool siege_integration_enabled() const {
    return dynamic_enabled() && catalog->military->siege_integration.enabled;
  }
  bool regional_life_enabled() const {
    return dynamic_enabled() && catalog->regional_life;
  }
  bool named_membership_enabled() const {
    return dynamic_enabled() && catalog->named_membership;
  }
  bool population_military_enabled() const {
    return dynamic_enabled() && catalog->population_military &&
           catalog->population_military->enabled;
  }
  PopulationMilitaryReadContext
  population_read(std::optional<Integer> phase_finish_tick = {}) const;
  PopulationMilitaryWriteContext
  population_write(std::optional<Integer> phase_finish_tick = {});
  BundleServices services;
  // 只保存派生可见源ID，随同候选构造/提交重建；不进入任何银行格式。
  WorldObservationIndex observation_index;
  MapPresenceIndex map_presence_index;
  LocalEconomyIndex economic_index;
  FiscalSettlementIndex fiscal_index;
  // 只诊断本次调用成本，不参与世界值、checkpoint或重放语义。
  std::vector<std::pair<StableId, Integer>> timing;
  explicit WorldBundle(std::shared_ptr<const PreparedCatalog>,
                       InitialWorldSeed &&);
  explicit WorldBundle(std::shared_ptr<const PreparedCatalog>,
                       JsonDecodedWorldSeedOwner &&);
  explicit WorldBundle(std::shared_ptr<const PreparedCatalog>,
                       DecodedWorldSeedOwner &&);

  WorldBundle(const WorldBundle &);
  WorldBundle &operator=(const WorldBundle &) = delete;
  WorldBundle(WorldBundle &&) = delete;
  WorldRead read() const;
  Txn write();
  Status validate() const;
  // 冷路径显式同job只读值核上下文；不赋予发布能力，不跳过任何原validator。
  Status validate(ProjectionWriter &) const;

private:
  Status validate_impl(ProjectionWriter *) const;
};
// 中央调度跨域真实操作；普通规则失败也不得泄漏半份NPC apply写入。
Status command_common_guard(const WorldBundle &, const StableId &,
                            const StableId &);
Outcome<std::shared_ptr<const PreviewResult>>
prepare_command(const WorldBundle &, const StableId &, const CommandPayload &);
Outcome<ReceiptDetails> apply_command(WorldBundle &, const StableId &,
                                      const PreparedDomainOp &);
Outcome<AdvanceDetails> advance_world(WorldBundle &,
                                      const std::vector<StableId> &hold = {});
Status try_npc_command(WorldBundle &, const StableId &, const CommandPayload &,
                       bool &applied);
Outcome<Integer>
military_return_route(const WorldBundle &, const StableId &army_id,
                      const StableId &destination_settlement_id);
Status begin_requested_military_journey(WorldBundle &,
                                        const MilitaryJourneyRequest &);
Status release_action(WorldBundle &, const StableId &);
Outcome<ActionStarted> begin_action(WorldBundle &, const StableId &,
                                    const CommandPayload &,
                                    const QuoteDetails &);
Status commit_tail(
    WorldBundle &, const StableId &, Integer, const StableId &,
    const ReceiptDetails &, std::shared_ptr<const CommitResult> &,
    const PlayerPlanExecution *plan_execution = nullptr,
    const StableId &registry_acceptance_id = {},
    const std::vector<StableId> &appointment_sources = {},
    const GrowthRecoveryState *recovery_before = nullptr,
    const StableId &licensing_source_id = {},
    const PrivateShopLiquidationState *liquidation_before = nullptr,
    const TaskShopOrderEstateCancellationState *estate_orders_before = nullptr,
    const FamilyApprenticeshipState *apprenticeship_before = nullptr,
    const StableId &governance_continuation_id = {},
    const std::vector<StableId> &inquiry_sources = {},
    const StableId &visit_reply_source = {},
    const FamilyLegacyState *disciple_legacy_before = nullptr,
    const DiscipleControlState *disciple_bindings_before = nullptr,
    const StableId &invitation_reply_source_id = {},
    const PrivateShopDailyRentState *daily_rent_before = nullptr,
    const StableId &born_dialogue_source = {},
    const FamilyLegacyState *biological_legacy_before = nullptr,
    const BiologicalControlState *biological_bindings_before = nullptr,
    const StableId &political_succession_source_id = {},
    const StableId &visit_invitation_source = {},
    const std::vector<StableId> &visit_meeting_sources = {},
    const StableId &political_head_handover_source_id = {},
    const GrowthTacticLessonState *tactic_lesson_before = nullptr,
    Integer tactic_lesson_from_tick = -1,
    const StableId &political_successor_round_source_id = {},
    const WorldBundle *shop_command_before = nullptr,
    const StableId &political_term_handover_source_id = {},
    const VisitMessageResultSources &visit_message_sources = {},
    const StableId &known_dialogue_source = {},
    const StableId &political_office_reception_source_id = {},
    const StableId &shop_order_contact_source = {},
    const FamilyLegacyState *collateral_legacy_before = nullptr,
    const CollateralControlState *collateral_before = nullptr,
    const StableId &political_governance_agency_source_id = {},
    const TaskMessageReturnResultBatch &task_return_sources = {},
    const WorldBundle *voluntary_before = nullptr,
    const TaskPersonalMessageResultBatch &personal_message_sources = {},
    const StableId &political_office_resignation_source_id = {},
    const TaskPersonalReportResultBatch &personal_report_sources = {},
    const StableId &personal_cancellation_source = {},
    const StableId &subordinate_appointment_source_id = {},
    const TaskPersonalAcceptanceResultBatch &personal_acceptance_sources = {},
    const StableId &dismissal_source_id = {},
    CompleteExactCommandCapture *actual_capture = nullptr);
} // namespace hanmo::resident
