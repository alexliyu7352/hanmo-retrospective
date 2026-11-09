// 青川有限作者许可的真实 S1/S6 适配；不是一般道路默认准入或 P2 条约实现。
#pragma once
#include "field_group_passage_catalog.hpp"
#include "political_group_toll.hpp"
#include "resident/domains/military/military_formation_source.hpp"
#include "resident/kernel/original_world_read_boundary.hpp"
#include "safe_exit_entry.hpp"
namespace hanmo::resident {
struct FieldGroupPassageContext {
  const CommonStore &common;
  const PoliticalPassageDecisionState &decisions;
  const SafeExitState &safe_exit;
  const FieldGroupPassageCatalog &policies;
  const OrganizationCatalog &organizations;
  const TravelCatalog &travel;
  const SpatialCarrierState &carriers;
  const MovementExactState &movement;
  const GroupMovementExecutionState &executions;
  const TransportManifestHistorySourceRead &manifests;
  const EconomicTransportExpenseState &payments;
  const MilitaryState &military;
  const MilitaryFormationSourceRead &formations;
  const OrganizationAuthorityClock &clock;
  const CurrentMomentRead &moment_clock;
  const OriginalWorldReadBoundary &read_boundary;
  const ReadStampAuthority &stamps;
  const DomainReadVersionSource &versions;
  ReadMode read_mode;
  ExactArithmeticBudget arithmetic;
  // 仅实际同候选writer借可写腿；cold不授可写引用，不能用const_cast补。
  PoliticalPassageDecisionState *candidate_decisions = nullptr;
};
// 一个词法 owner
// 同时完成热政策、采用原事实和冷来源验证；不缓存跨mutation的行地址。
class NativeFieldGroupPassage final : public GroupPoliticalPassage,
                                      public PoliticalPassageSources,
                                      public PoliticalPassageHistoryRead {
  const FieldGroupPassageContext &x_;
  GroupPoliticalPassageEntryContext entries() const;
  PoliticalPassageContext policy_context() const;

public:
  explicit NativeFieldGroupPassage(const FieldGroupPassageContext &x) : x_(x) {}
  Outcome<std::vector<PreparedGroupPassage>>
  prepare_actual_entry(const GroupMovementJourney &,
                       const TransportManifestRead &, const MovementBasis &,
                       const WorldMoment &) const override;
  Outcome<SourceRef>
  adopt_actual_decision_in_candidate(CommonStore &,
                                     const PreparedGroupPassage &) override;
  Outcome<ActualTransportExpense>
  read_adopted_toll(const SourceRef &, const GroupMovementJourney &,
                    const WorldMoment &) const override;
  Status
  validate_crossing_decision(const BoundaryCrossingReceipt &) const override;
  Outcome<std::optional<SourceRef>>
  adopt_after_positive_crossing_in_candidate(CommonStore &,
                                             const SourceRef &) override;
  Status validate_after_positive_crossing(
      const BoundaryCrossingReceipt &) const override;
  Status
  adopt_after_boundary_completion_in_candidate(CommonStore &,
                                               const SourceRef &) override;
  Status validate_after_boundary_completion(
      const BoundaryCrossingReceipt &) const override;
  Status verify_actual_request(const PassageRequest &,
                               const TransportManifestRead &) const override;
  Outcome<PoliticalPassagePolicyRead>
  read_current_policy(const PassageRequest &,
                      const TransportManifestRead &) const override;
  Status verify_tariff_payee(const PassageBoundaryRef &,
                             const PassageTariff &) const override;
  Status validate_historical_decision(
      const PoliticalPassageDecisionSource &) const override;
  // 完整有限冷门，含真实 Common 头反扫；不能只由空表宣称可用。
  Status validate_all() const;
};
} // namespace hanmo::resident
