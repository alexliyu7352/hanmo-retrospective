// Original S7 immutable service adoption. S0 alone retains elapsed/state
// history.
#pragma once
#include "resident/domains/local_economy/market_service_model.hpp"
#include "resident/kernel/fixed_service_work_period.hpp"
namespace hanmo::resident {
struct PrivateShopOperationAcceptance {
private:
  CheckpointCopyLease copied_fields_;

public:
  PrivateShopOperationAcceptance() = default;
  PrivateShopOperationAcceptance(const StableId &, const SourceRef &,
                                 const ActorFoundationRuleRef &,
                                 const WorldMoment &, const MomentSpan &,
                                 const SourceRef &);
  PrivateShopOperationAcceptance(const PrivateShopOperationAcceptance &);
  PrivateShopOperationAcceptance(PrivateShopOperationAcceptance &&);
  PrivateShopOperationAcceptance &
  operator=(const PrivateShopOperationAcceptance &);
  PrivateShopOperationAcceptance &
  operator=(PrivateShopOperationAcceptance &&) noexcept;
  void swap(PrivateShopOperationAcceptance &) noexcept;
  const CheckpointCopyLease &checkpoint_payload_memory() const noexcept {
    return copied_fields_;
  }
  friend void swap(PrivateShopOperationAcceptance &a,
                   PrivateShopOperationAcceptance &b) noexcept {
    a.swap(b);
  }
  StableId operation_ref;
  SourceRef original_shop_epoch_ref;
  ActorFoundationRuleRef market_definition_ref;
  WorldMoment accepted_at;
  MomentSpan committed_service_window;
  SourceRef actual_presence_source_ref;
};
// Original applied whole service period. Credit stays solely in ActionElapsed;
// this row owns the actual S7 labor application and original wage dependency.
struct PrivateShopLaborAppliedSource {
  StableId id, operation_ref;
  WorkPeriodRef2 work_period_ref;
  WorldMoment occurred_at;
  std::optional<SourceRef> original_staff_service_ref;
};
// Exact original result provenance only. The legacy DayResult remains the
// sole result/financial owner and S0 remains the sole transition/credit owner.
struct PrivateShopOperationTerminalSource {
  StableId operation_ref;
  SourceRef original_result_ref;
  WorldMoment occurred_at;
  MomentOwnerSource actual_cause_ref;
};
struct PrivateShopOperationSourcesState {
  Integer schema_version = 1;
  CheckpointVector<PrivateShopOperationAcceptance> accepted_operations;
  std::vector<PrivateShopLaborAppliedSource> labor_applied_sources;
  std::vector<PrivateShopOperationTerminalSource> terminal_sources;
};
class ProjectionWriter;
class CodecVisitor;
Outcome<PrivateShopOperationSourcesState>
decode_private_shop_operation_sources(const BoundaryValue &);
Outcome<BoundaryValue>
encode_private_shop_operation_sources(const PrivateShopOperationSourcesState &);
Status
emit_private_shop_operation_sources(ProjectionWriter &, const StableId &,
                                    const PrivateShopOperationSourcesState &);
Status emit_private_shop_operation_sources_fields(
    ProjectionWriter &, const PrivateShopOperationSourcesState &);
Status visit_private_shop_operation_sources(CodecVisitor &,
                                            PrivateShopOperationSourcesState &);
} // namespace hanmo::resident
