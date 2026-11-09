// FH 独立入床先读取真实原 Care 交接；不是新的 placement writer 或占位证明。
#pragma once
#include "care_coverage.hpp"

namespace hanmo::resident {
// 同一真实 owner 的借用视图；不保存第二份接受、工时、身体或位置。
struct CareFacilityOriginalHandoverView {
  const AcceptedCaregiver *accepted = nullptr;
  const CareHandoverExecution *execution = nullptr;
  const CareBinding *binding = nullptr;
  const CareTemporalEvent *completed = nullptr;
};
// 反读真实已完成的接受/工作/参与/S1 placement，并核本次全部引用同一链。
// 仅供 FH 入床核验的原输入：不证明床位需求/placement_rule/单位定位已采用，
// 不生成 occupancy，不替代尚需真实 provider 的完整 ReadCareFacilityOccupancy。
Outcome<CareFacilityOriginalHandoverView> read_care_facility_original_handover(
    const CareCoverageSources &, const StableId &accepted_arrangement_id,
    const StableId &handover_id,
    const SourceRef &actual_care_temporal_event_ref,
    const SourceRef &original_receiver_acceptance_ref);
} // namespace hanmo::resident
