#pragma once
#include "transport_payment_physical.hpp"
namespace hanmo::resident {
// Finite ownership for the final three sections. This is additional to full
// original business cold validation: a retained source may not disappear just
// because its original parent cannot be found by the physical writer.
// 暂定：manifest2 可只保存原静置 fixed/portable 箱；未选 group 的其它业务根逐项拒绝。
// 此门只核物理缺席，箱定义、原固定地址与实际 source 仍由原 owner 反验。
// 领域 Motion 先核其实际可借根；外层再追加真实 registration 根的缺席检查。
Status validate_stationary_inventory_motion_roots(
    const MovementExactState&,const SpatialCarrierState&,
    const GroupMovementExecutionState&,bool assignments_selected);
Status validate_stationary_inventory_transport_roots(
    const MovementExactState&,const SpatialCarrierState&,
    const GroupMovementExecutionState&,const TransportRegistrationState&,
    bool assignments_selected);
Status validate_transport_physical_source_ownership(const MovementExactState&,const SpatialCarrierState*,const GroupMovementExecutionState*,const TransportRegistrationState*);
}
