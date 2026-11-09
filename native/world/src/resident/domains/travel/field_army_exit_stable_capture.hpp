// 原Field稳定采用的窄S1接头；避免无关位置读口为一个新binder承担依赖。
#pragma once
#include "spatial_location_capture.hpp"
namespace hanmo::resident {
// 同目标聚落的真实Field整军采用：expected InArmy和物理点只由原ready provider给出。
Outcome<BoundSpatialLocationWrite> bind_actual_field_army_exit_stable_location_write(
    const CommonStore &, const MovementExactState &,
    const FieldBattleExitPlacementRead &, const StableId &actor_id,
    const MomentOwnerSource &actual_cancel,
    const std::shared_ptr<const ActorLocationCaptureIdentity> &,
    const std::shared_ptr<const ActorLocationWriteIdentity> &,
    Integer capture_sequence, ExactArithmeticBudget budget = {});

} // namespace hanmo::resident
