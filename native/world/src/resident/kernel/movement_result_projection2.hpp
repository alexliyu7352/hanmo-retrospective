// S1 remains the only movement producer. P10 reads its immutable source, never derives Action completion from arrival.
#pragma once
#include <memory>
#include "dynamic_result_time2.hpp"
#include "resident/domains/travel/movement_result_read.hpp"
#include "resident/domains/travel/group_movement.hpp"
namespace hanmo::resident {
class CompleteExactWorldBindingOwner;
struct NativeCompleteWorldCoreSources;
// 已选Field的原S1补充来源；分类不复制ID，不以kind前缀猜测权限。
bool native_field_movement_result_kind(const StableId& domain, const StableId& kind) noexcept;
// 同World原Complete owner须活到此借图析构；不重入World getter或造业务事实。
Outcome<std::unique_ptr<const ExactResultOwnerRead>> make_native_field_movement_result_read(
    const WorldBundle&, const CompleteExactWorldBindingOwner&,
    const NativeCompleteWorldCoreSources&);
struct MovementResultProjectionAccess {const CommonStore& common;const MovementExactState& movement;const SpatialCarrierState* carriers=nullptr;const GroupMovementExecutionState* groups=nullptr;const TransportRegistrationState* registrations=nullptr;};
bool native_movement_result_kind(const FactRecord&);
Outcome<ExactOwnerFactRead> read_movement_fact_v2(const MovementResultProjectionAccess&,const FactRecord&);
Outcome<WorldMoment> read_native_movement_source_moment(const MovementResultProjectionAccess&,const MomentOwnerSource&);
}
