// 真实完成施工到原应用行的候选写点；不提供独立发布或容量自证入口。
#pragma once
#include "facility_activation_read.hpp"
namespace hanmo::resident {
struct FacilityActivationCandidateContext {
  CommonStore &common;
  ProductionWorkApplicationsState &applications;
  CheckpointMemoryCharge &owned_fields;
  const FacilityActivationReadContext &read;
  const EconomicFacilityProviderContext &physical;
};
// index 仅用于同一私有候选的后继物理采用；不能跨根保留指针或独立封口。
struct PendingFacilityActivation {
  std::size_t row_index = 0;
  bool replay = false;
};
// 只接受原 CompletedStage 有限分支，当前 M/原条款/工料全部从实际 owner 取。
// 调用前所有旧应用必须闭合；新行采用后仍必须在同候选完成原生产输入
// close/reopen、真实 holder/容量处置和
// validate_facility_activation_capacity_adoptions。 任一步失败丢弃整个
// World，不保留本行、Common 头或已扣成本来单独重试。
Outcome<PendingFacilityActivation>
record_facility_activation_in_candidate(FacilityActivationCandidateContext &,
                                        const WorkApplicationSource &);
} // namespace hanmo::resident
