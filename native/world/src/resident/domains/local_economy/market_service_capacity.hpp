#pragma once
#include "market_service_model.hpp"
#include "public_market_staffing.hpp"
#include "resident/facility_claims/facility_claim_runtime.hpp"
namespace hanmo::resident {
inline constexpr const char *market_service_unit = "buyer_service";
struct MarketServiceOriginalInputs {
  MarketServiceAssetBasis asset;
  MarketServiceOpeningBasis opening;
  std::vector<MarketServiceStaffingBinding> staffing;
  MarketServiceConditionBasis condition;
  MarketServiceUsageRightBasis usage;
};
struct MarketServiceWorkerSegment {
  MomentSpan window;
  Integer qualified_workers = 0;
  std::vector<SourceRef> actual_sources;
};
struct MarketServiceConditionSegment {
  MomentSpan window;
  Integer available_physical_slots = 0;
  SourceRef actual_source;
};
// 原asset/premises/产权和原经营资格源各自纯读；不能由external报价给capacity/人员或时刻。
class MarketServiceOriginalRead {
public:
  virtual ~MarketServiceOriginalRead() = default;
  virtual Outcome<MarketServiceChangeCause>
  resolve_original_market_service_change(const MarketServiceDefinition &,
                                         const FacilitySourceRef &) const = 0;
  virtual Outcome<MarketServiceOriginalInputs>
  read_original_market_service_inputs(
      const MarketServiceDefinition &,
      const MarketServiceChangeCause &) const = 0;
  virtual Status validate_original_market_service_inputs(
      const MarketServiceDefinition &,
      const MarketServiceCapacitySource &) const = 0;
  virtual Outcome<FacilitySourceRef> read_original_market_change_source(
      const FacilitySlotRef &, const MarketServiceChangeCause &) const = 0;
  virtual Outcome<WorldMoment>
  read_original_market_change_at(const FacilitySlotRef &,
                                 const MarketServiceChangeCause &) const = 0;
  virtual Outcome<std::vector<MarketServiceWorkerSegment>>
  read_private_market_service_workers(const PrivateMarketOperationSource &,
                                      const MomentSpan &) const = 0;
  virtual Outcome<std::vector<MarketServiceConditionSegment>>
  read_market_service_condition(const MarketServiceDefinition &,
                                const MarketServiceConditionBasis &,
                                const MomentSpan &) const = 0;
  virtual Outcome<bool>
  market_claim_conflict_visible(const ReservationRef &,
                                const FacilityClaim2 &) const = 0;
};
// 暂定：仅未选公市时可不借公市人员上下文；公分派不允许空源成功。
struct MarketServiceCapacityContext {
  const CommonStore &common;
  const HouseholdPurchaseState &purchases;
  const FacilityClaimsState &claims;
  const PublicMarketStaffingContext *public_staffing;
  const MarketServiceOriginalRead &originals;
  const FacilityHolderSourceRead &holders;
  WorldMoment now;
};
class NativeMarketServiceCapacityRead final : public FacilityClaimSourceRead {
  const MarketServiceCapacityContext &x_;

public:
  explicit NativeMarketServiceCapacityRead(
      const MarketServiceCapacityContext &x)
      : x_(x) {}
  Status validate_market_source(const MarketServiceCapacitySource &) const;
  Outcome<WorldMoment>
  initial_source_at(const FacilitySlotRef &) const override;
  Outcome<std::vector<VerifiedFacilityCapacitySegment>>
  read_verified_capacity(const FacilitySourceRef &, const FacilitySlotRef &,
                         const MomentSpan &) const override;
  Outcome<FacilityCapacityChange>
  read_capacity_history_base(const FacilitySlotRef &,
                             const FacilityHistoryBase &) const override;
  Outcome<FacilitySourceRef> read_capacity_reconciliation_source(
      const FacilitySlotRef &, const FacilityCapacityChange &) const override;
  Outcome<FacilityMutationCapacityOrder> compare_mutation_to_capacity_change(
      const FacilitySlotRef &, const FacilityClaimMutationReceipt &,
      const FacilityCapacityChange &) const override;
  Outcome<FacilitySourceRef>
  capacity_source_at(const FacilitySlotRef &,
                     const WorldMoment &) const override;
  Outcome<std::vector<FacilityCapacityChange>>
  enumerate_capacity_changes(const FacilitySlotRef &, const WorldMoment &,
                             const WorldMoment &) const override;
  Outcome<std::vector<FacilityClaimDependency>>
  enumerate_claim_dependencies(const FacilitySlotRef &) const override;
  Status
  validate_claim_admission(const FacilityClaim2 &,
                           const FacilityClaimMutationReceipt &) const override;
  Status
  validate_claim_close(const FacilityClaim2 &, FacilityClaimCloseReason,
                       const FacilityClaimMutationReceipt &) const override;
  Status
  validate_mutation_source(const FacilitySlotRef &,
                           const FacilityClaimMutationReceipt &) const override;
  Status
  validate_pruned_prefix(const FacilitySlotRef &,
                         const VacantPrunedFacilityPrefix &) const override;
  Status validate_retention_and_physical_occupancy(
      const FacilitySlotRef &, const MomentSpan &,
      const std::vector<const FacilityClaim2 *> &,
      const FacilityCapacitySource2 &) const override;
  Outcome<Integer> read_additional_normal_capacity(
      const FacilitySlotRef &, const MomentSpan &,
      const std::vector<const FacilityClaim2 *> &,
      const FacilityCapacitySource2 &) const override;
  Outcome<bool>
  is_verified_retained_claim(const FacilityClaim2 &, const MomentSpan &,
                             const FacilityCapacitySource2 &) const override;
  Outcome<bool> source_change_affects_claims(
      const FacilityCapacityChange &,
      const std::vector<const FacilityClaim2 *> &) const override;
  Outcome<bool>
  conflicting_source_visible(const ReservationRef &,
                             const FacilityClaim2 &) const override;
};
// Finalizer调用：原change已经实际应用，所有旧服务/holder处置已发生；本叶只构造唯一新source。
Outcome<StableId> record_market_service_capacity_source_in_candidate(
    CommonStore &, HouseholdPurchaseState &,
    const MarketServiceCapacityContext &, const ActorFoundationRuleRef &,
    const MarketServiceChangeCause &);
struct ProspectiveMarketCapacitySegment {
  MomentSpan window;
  FacilitySlotRef slot_ref;
  StableId place_ref;
  Integer capacity = 0, prospective_revision = 0;
  std::vector<MomentSpan> availability_intervals;
  SourceRef usage_right_ref, original_change;
  std::optional<SourceRef> previous_source_ref;
};
Outcome<std::vector<ProspectiveMarketCapacitySegment>>
read_prospective_market_service_capacity(const MarketServiceCapacityContext &,
                                         const ActorFoundationRuleRef &,
                                         const MarketServiceChangeCause &,
                                         const MomentSpan &);
// Replays the capacity actually adopted by one original change. It never uses
// the latest source as a historical proposal or fabricates a successor
// revision.
Outcome<std::vector<ProspectiveMarketCapacitySegment>>
read_market_service_adopted_change_projection(
    const MarketServiceCapacityContext &, const FacilitySlotRef &,
    const SourceRef &actual_change, const WorldMoment &actual_at,
    const MomentSpan &);
Status validate_market_service_sources(const MarketServiceCapacityContext &);
} // namespace hanmo::resident
