// E§21.1唯一购置owner：接受、服务、实际持货与交付分别反读原来源，不持第二钱粮余额。
#pragma once
#include "household_purchase_types.hpp"
#include "economic_transport_sources.hpp"
#include "market_service_model.hpp"
#include "resident/domains/commerce/household_retail.hpp"
#include "resident/facility_claims/facility_claims.hpp"
#include "resident/kernel/fixed_service_runtime.hpp"
namespace hanmo::resident {
struct PrivateShopHouseholdCustodyContext;
struct TransportRegistrationState;
struct HouseholdMobileCustodyContext;
struct HouseholdMobileCustodyRequest;
struct HouseholdMarketServiceDefinition {
  StableId id, market_ref, place_id, custody_inventory_id;
  FacilitySlotRef service_slot;
  SourceRef custody_terms_ref, service_usage_right_ref, handling_rule_ref;
};
struct HouseholdReserveRule {
  StableId id, household_id, item_id, destination_inventory_id;
  Integer minimum_stock = 0, minimum_cash_reserve = 0;
};
struct HouseholdPurchaseCatalog {
  StableId duration_definition_id, continuation_rule_id;
  OrderedTable<HouseholdMarketServiceDefinition> market_services;
  std::vector<MarketServiceDefinition> market_service_definitions;
  std::vector<HouseholdReserveRule> household_reserve_rules;
};
struct HouseholdPurchaseAcceptedTerms {
  SourceRef source;
  HouseholdPurchasePlan plan;
  HouseholdRetailSeller seller;
  StableId item_id;
  Integer quantity = 0, quoted_total_qian = 0;
  WorldMoment accepted_at;
  SourceRef presence_source_ref;
  std::vector<ReadVersion> accepted_versions;
};
struct HouseholdPurchase {
  StableId purchase_id, plan_ref;
  SourceRef accepted_terms_ref;
  HouseholdBuyer executor;
  WorkExecutionBinding execution_binding;
  StableId service_claim_ref, retail_receipt_ref, holding_inventory_ref;
  std::optional<SourceRef> delivery_binding;
  HouseholdPurchasePhase state = HouseholdPurchasePhase::planned;
  Integer phase_revision = 1;
  std::optional<WorkPeriodRef> last_consumed_period_ref;
  std::optional<SourceRef> terminal_result_ref;
};
struct HouseholdPurchaseServiceResult {
  StableId id, purchase_id;
  MomentSpan serviced_window;
  SourceRef coverage_source;
  FacilityClaimReceiptRef close_receipt;
  std::optional<WorkPeriodRef> period_ref;
  WorldMoment completed_at;
};
struct HouseholdPurchaseHandover {
  StableId id, purchase_id;
  SourceRef actual_delivery_source;
  WorldMoment occurred_at;
  StableId from_inventory_id, to_inventory_id;
  std::vector<VerifiedTradeLot> actual_lots;
};
struct HouseholdPurchasePhaseChange {
  StableId id, purchase_id;
  HouseholdPurchasePhase before, after;
  Integer revision_before = 0, revision_after = 0;
  SourceRef actual_cause;
  WorldMoment occurred_at;
};
struct HouseholdServiceSegment {
  StableId purchase_id;
  MomentSpan window;
  SourceRef actual_coverage_source;
  std::vector<StableId> labor_allocation_refs;
  std::vector<SourceRef> exertion_source_refs;
};
struct HouseholdPurchaseState {
  OrderedTable<HouseholdPurchasePolicy> policies;
  OrderedTable<HouseholdPurchasePlan> plans;
  OrderedTable<HouseholdPurchaseAcceptedTerms> accepted_terms;
  OrderedTable<HouseholdPurchase> purchases;
  OrderedTable<HouseholdPurchaseServiceResult> service_results;
  OrderedTable<WorkPeriodView> work_periods;
  OrderedTable<HouseholdPurchaseHandover> handovers;
  OrderedTable<HouseholdPurchasePhaseChange> phase_changes;
  std::vector<HouseholdServiceSegment> service_segments;
  OrderedTable<HouseholdServiceEvent2> named_service_events;
  std::vector<NamedServiceClaimEpisode2> named_service_claim_episodes;
  std::vector<MarketServiceDefinition> market_service_definitions;
  MarketServiceSourceTables market_service_sources;
  OrderedTable<PublicMarketStaffingOffer> public_staffing_offers;
  OrderedTable<PublicMarketStaffingConsent> public_staffing_consents;
  OrderedTable<PublicMarketStaffingBinding> public_staffing_bindings;
  OrderedTable<PublicMarketStaffingEvent> public_staffing_events;
  OrderedTable<PublicMarketStaffingReleaseConsent>
      public_staffing_release_consents;
};
struct HouseholdPurchasePresence {
  SourceRef actual_source;
  StableId place_id;
  std::vector<ReadVersion> versions;
};
struct HouseholdPurchaseCoverage {
  SourceRef actual_source;
  MomentSpan window;
  std::vector<StableId> actual_labor_allocations;
  std::vector<SourceRef> exertion_source_refs;
};
struct HouseholdPurchaseDeliverySource {
  SourceRef actual_source;
  WorldMoment occurred_at;
  StableId purchase_id, from_inventory_id, to_inventory_id;
  std::vector<CargoSelection> cargo;
  bool actual_loss = false;
};
// S1实际到场、载货/交接和原参与source；实现必须反查原payload，不能由请求自报already_arrived。
class HouseholdPurchaseEnvironmentRead {
public:
  virtual ~HouseholdPurchaseEnvironmentRead() = default;
  virtual Outcome<HouseholdPurchasePresence>
  read_purchase_presence_for_service(const HouseholdPurchasePlan &,
                                     const HouseholdMarketServiceDefinition &,
                                     const WorldMoment &) const = 0;
  virtual Outcome<HouseholdPurchasePresence>
  read_purchase_presence(const HouseholdPurchasePlan &, const StableId &place,
                         const WorldMoment &) const = 0;
  virtual Outcome<HouseholdPurchaseCoverage>
  read_purchase_service_coverage(const HouseholdPurchase &,
                                 const MomentSpan &) const = 0;
  virtual Status
  validate_purchase_service_segment(const HouseholdPurchase &,
                                    const HouseholdServiceSegment &) const = 0;
  virtual Outcome<bool>
  read_local_purchase_handover(const HouseholdPurchase &,
                               const StableId &destination,
                               const WorldMoment &) const = 0;
  virtual Outcome<HouseholdPurchaseDeliverySource>
  read_purchase_delivery(const HouseholdPurchase &,
                         const SourceRef &) const = 0;
  virtual Status validate_named_purchase_service(const NamedHouseholdBuyer &,
                                                 const HouseholdPurchasePlan &,
                                                 const WorldMoment &) const = 0;
  virtual Outcome<std::vector<FixedServiceInputRef>>
  read_named_service_adoption_inputs(const HouseholdPurchase &,
                                     const HouseholdServiceEvent2 &) const = 0;
  virtual Status validate_named_service_adoption_inputs(
      const HouseholdPurchase &, const HouseholdServiceEvent2 &,
      const std::vector<FixedServiceInputRef> &) const = 0;
  virtual Outcome<FacilityClaimCloseReason>
  read_named_service_interruption(const HouseholdPurchase &, const SourceRef &,
                                  const WorldMoment &,
                                  bool terminate) const = 0;
  virtual Outcome<std::vector<SourceRef>>
  read_named_service_participation(const HouseholdPurchase &,
                                   const MomentSpan &) const = 0;
  virtual Status
  validate_named_service_continuation(const HouseholdPurchase &,
                                      const WorldMoment &) const = 0;
  virtual Status validate_purchase_phase_cause(const HouseholdPurchase &,
                                               HouseholdPurchasePhase,
                                               const SourceRef &,
                                               const WorldMoment &) const = 0;
};
struct HouseholdPurchaseContext {
  const CommonStore &common;
  const CommonCatalog &shared;
  const LocalEconomyState &economy;
  const PopulationTransportState &population;
  const HouseholdPurchaseCatalog &catalog;
  const HouseholdPurchaseState &state;
  const HouseholdProvisionControlRead &controls;
  const HouseholdPurchaseEnvironmentRead &environment;
  const FacilityClaimsState &claims;
  const FacilityClaimsCatalog &claim_catalog;
  const FacilityClaimSourceRead &claim_sources;
  const HouseholdRetailReadContext &retail;
  const ActionElapsedStore &elapsed;
  const TimeScaleRef &scale;
  WorldMoment now;
  const EconomicTransportExpenseState &transport_expenses;
  const TransportRegistrationState *transport_registrations = nullptr;
  Integer selected_household_version = 2;
  const HouseholdMobileCustodyContext *mobile_custody = nullptr;
  const PrivateShopHouseholdCustodyContext *private_market_custody = nullptr;
};
struct HouseholdPurchaseQuote {
  WorldIdentity identity;
  StableId plan_id;
  HouseholdRetailQuote retail;
  HouseholdPurchasePresence presence;
  FacilityClaimQuote service_claim;
  std::vector<FacilitySourceRef> capacity_sources;
  WorldMoment accepted_at;
};
// 有限预算从同policy已接受购置/S7真实成交投影，不保存另一个remaining字段。
struct HouseholdPurchaseAllowance {
  SourceRef policy_ref;
  Integer policy_revision = 0, merchandise_cap_qian = 0,
          merchandise_gross_paid = 0, merchandise_available = 0,
          transport_cap_qian = 0, transport_gross_paid = 0, total_cap_qian = 0,
          total_gross_paid = 0, total_available = 0;
};
class HouseholdPurchaseContracts final
    : public HouseholdRetailAcceptanceRead,
      public ActionTimeOriginalSourceRead,
      public WorkPeriodSourceRead,
      public DomainActivityReservationOwnerRead {
  const HouseholdPurchaseContext &x_;

public:
  explicit HouseholdPurchaseContracts(const HouseholdPurchaseContext &x)
      : x_(x) {}
  Outcome<HouseholdRetailAcceptance>
  read_household_retail_acceptance(const StableId &) const override;
  Status validate_household_retail_settlement(
      const HouseholdRetailSettlement &) const override;
  Status enumerate_action_time_owners(const ActionTimeRegistration &,
                                      ActionTimeOwnerIdSink &) const override;
  Outcome<ActionTimeOwnerProof>
  read_action_time_owner(const ActionTimeRegistration &,
                         const StableId &) const override;
  Outcome<WorldMoment>
  read_action_time_source(const ElapsedTimeSource &) const override;
  Status validate_fixed_action_sources(
      const ActionTimeRegistration &, const ActionTimeOwnerProof &,
      const ActionElapsedState &,
      const std::vector<FixedServiceTransition> &) const override;
  Status
  validate_shared_action_sources(const ActionTimeRegistration &,
                                 const ActionTimeOwnerProof &,
                                 const SharedWorkElapsedState &) const override;
  Outcome<RunStop>
  read_action_continuation_stop(const ActionTimeRegistration &,
                                const ActionTimeOwnerProof &,
                                const WorldMoment &) const override;
  Outcome<OriginalActionRemaining>
  read_original_action_remaining(const ActionTimeRegistration &,
                                 const ActionTimeOwnerProof &,
                                 const WorldMoment &) const override;
  Outcome<StableId>
  read_gate_result_id(const SharedWorkGateResult &) const override;
  Outcome<SharedWorkGateResult>
  read_original_gate_result(const StableId &) const override;
  Status
  validate_original_gate_result(const SharedWorkGateResult &) const override;
  Outcome<WorkPeriodView>
  read_work_period(const WorkPeriodRef &) const override;
  Status validate_actor_role(const WorkPeriodRef &,
                             const ActualActorWorkRole &) const override;
  Status validate_labor_allocation(const WorkPeriodRef &,
                                   const StableId &) const override;
  Status validate_facility_service(const WorkPeriodRef &,
                                   const StableId &) const override;
  Status validate_resource_effect(const WorkPeriodRef &,
                                  const SourceRef &) const override;
  Status validate_exertion_source(const WorkPeriodRef &,
                                  const SourceRef &) const override;
  Status validate_stage_result(const WorkPeriodRef &,
                               const SourceRef &) const override;
  Outcome<ReservationOwnerView>
  read_domain_activity_reservation_owner(const ReservationRef &) const override;
};
const ActionTimeRegistration &household_purchase_action_time_registration();
const ActionTimeRegistration &household_purchase_action_time_registration_v3();
const ActionTimeRegistration &
household_purchase_action_time_registration(const HouseholdPurchaseContext &);
// 无写预览原政策全部先验；控制权必须来自真实 controls，热提交重新核。
Status validate_original_household_purchase_policy_input(
    const HouseholdPurchaseContext &, const HouseholdPurchasePolicy &);
// 无写预览原计划；不反读尚未分配的群体选择，不接受产物标识。
Status validate_original_household_purchase_plan_input(
    const HouseholdPurchaseContext &, const HouseholdPurchasePlan &,
    const HouseholdMobileCustodyRequest *mobile_request = nullptr);
// 原命令结果借真实 Common 头；临时新 ID 租约由同一原 writer 保活到入根。
Outcome<const FactRecord *> accept_household_purchase_policy_fact_in_candidate(
    CommonStore &, HouseholdPurchaseState &, const HouseholdPurchaseContext &,
    HouseholdPurchasePolicy);
Outcome<const FactRecord *> plan_household_purchase_fact_in_candidate(
    CommonStore &, LocalEconomyState &, PopulationTransportState &,
    HouseholdPurchaseState &, const HouseholdPurchaseContext &,
    HouseholdPurchasePlan,
    const HouseholdMobileCustodyRequest *mobile_request = nullptr);
// 旧裸 ID ABI 保留；它不能代表返回副本已满足新命令预算边界。
Outcome<StableId> accept_household_purchase_policy_in_candidate(
    CommonStore &, HouseholdPurchaseState &, const HouseholdPurchaseContext &,
    HouseholdPurchasePolicy);
Outcome<StableId> plan_household_purchase_in_candidate(
    CommonStore &, LocalEconomyState &, PopulationTransportState &,
    HouseholdPurchaseState &, const HouseholdPurchaseContext &,
    HouseholdPurchasePlan,
    const HouseholdMobileCustodyRequest *mobile_request = nullptr);
Outcome<PopulationSelectionAuthority>
read_household_population_selection_authority(
    const HouseholdPurchaseContext &, const PopulationSelectionRequest &);
Outcome<HouseholdPurchaseAllowance>
read_household_purchase_allowance(const HouseholdPurchaseContext &,
                                  const SourceRef &, Integer);
Outcome<HouseholdPurchaseQuote>
preview_household_purchase(const HouseholdPurchaseContext &,
                           const StableId &plan_id,
                           const HouseholdRetailTerms &requested);
Outcome<StableId> accept_household_purchase_in_candidate(
    CommonStore &, LocalEconomyState &, HouseholdPurchaseState &,
    FacilityClaimsState &, ActionElapsedStore &, HouseholdRetailState &,
    const HouseholdPurchaseContext &, const HouseholdPurchaseQuote &);
// S0每真实服务片调用；不begin Common
// capture。只有真服务coverage才能增加原elapsed。
Status provide_household_purchase_service_interval_in_candidate(
    HouseholdPurchaseState &, ActionElapsedStore &,
    const HouseholdPurchaseContext &, const StableId &, const MomentSpan &);
Outcome<FacilityClaimMutationReceipt>
close_household_purchase_service_owner_in_candidate(
    CommonStore &, HouseholdPurchaseState &, ActionElapsedStore &,
    const HouseholdPurchaseContext &, const StableId &);
Outcome<StableId> complete_household_purchase_service_in_candidate(
    CommonStore &, HouseholdPurchaseState &, FacilityClaimsState &,
    ActionElapsedStore &, const HouseholdPurchaseContext &, const StableId &);
Outcome<StableId> apply_household_purchase_delivery_in_candidate(
    CommonStore &, HouseholdPurchaseState &, const HouseholdPurchaseContext &,
    const StableId &, const SourceRef &);
Outcome<StableId> change_household_purchase_phase_in_candidate(
    CommonStore &, HouseholdPurchaseState &, const HouseholdPurchaseContext &,
    const StableId &, HouseholdPurchasePhase, const SourceRef &);
Status validate_household_purchases(const HouseholdPurchaseContext &);
Status validate_household_purchase_claim_source(
    const HouseholdPurchaseContext &, const FacilityClaim2 &,
    const FacilityClaimMutationReceipt &, bool admission);
BoundaryValue encode_household_purchases(const HouseholdPurchaseState &,
                                         Integer selected_version = 2);
Outcome<HouseholdPurchaseState>
decode_household_purchases(const BoundaryValue &, Integer selected_version = 2);
Status visit_household_purchases(CodecVisitor &, HouseholdPurchaseState &,
                                 Integer selected_version = 2);
class ProjectionWriter;
Status emit_household_purchases(ProjectionWriter &,
                                const HouseholdPurchaseState &,
                                Integer selected_version = 2);
BoundaryValue
encode_household_purchase_catalog(const HouseholdPurchaseCatalog &);
Outcome<HouseholdPurchaseCatalog>
decode_household_purchase_catalog(const BoundaryValue &);
Status visit_household_purchase_catalog(CodecVisitor &,
                                        HouseholdPurchaseCatalog &);
Status emit_household_purchase_catalog(ProjectionWriter &,
                                       const HouseholdPurchaseCatalog &);

} // namespace hanmo::resident
