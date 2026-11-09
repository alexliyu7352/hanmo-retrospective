#pragma once
#include "private_shop_action_time.hpp"
#include <memory>
namespace hanmo::resident {
// Called only for the real already-applied S4 source in this same candidate.
// The returned typed address is the actual S0 change, never a Common fact.
Outcome<FixedServiceTransitionRef> apply_private_shop_condition_change_in_candidate(
 CommonStore&,ActionElapsedStore&,PrivateShopOperationCapabilitiesState&,
 const PrivateShopOperationSourceContext&,const StableId& operation,const MomentOwnerSource& actual_condition);
// Full labor is applied by the sole original labor/wage writer first. This
// completes the original DayResult and adopts its genuine S0 terminal source.
Outcome<StableId> complete_private_shop_operation_in_candidate(CommonStore&,LocalEconomyState&,PrivateShopState&,
 PrivateShopOperationSourcesState&,ActionElapsedStore&,const PrivateShopOperationSourceContext&,
 const StableId& operation,const MomentOwnerSource& actual_final_labor);
// An unsealed command never becomes a durable terminal source. The pending
// object is call-scoped, noncopyable, and bound only to the actual Common seal.
class PendingPrivateShopCancellation {
 struct Impl;std::unique_ptr<Impl>impl_;explicit PendingPrivateShopCancellation(std::unique_ptr<Impl>);
 friend struct PrivateShopCancellationAccess;
public:
 PendingPrivateShopCancellation();~PendingPrivateShopCancellation();
 PendingPrivateShopCancellation(PendingPrivateShopCancellation&&)noexcept;
 PendingPrivateShopCancellation&operator=(PendingPrivateShopCancellation&&)noexcept;
 PendingPrivateShopCancellation(const PendingPrivateShopCancellation&)=delete;
 PendingPrivateShopCancellation&operator=(const PendingPrivateShopCancellation&)=delete;
};
Outcome<PendingPrivateShopCancellation> cancel_private_shop_exact_operation_in_candidate(
 CommonStore&,LocalEconomyState&,PrivateShopState&,const PrivateShopOperationSourceContext&,
 const StableId& actual_actor,const StableId& operation);
Outcome<StableId> read_pending_private_shop_cancellation_result(const PendingPrivateShopCancellation&);
Status bind_private_shop_cancellation_in_candidate(CommonStore&,PrivateShopOperationSourcesState&,
 PrivateShopOperationCapabilitiesState&,ActionElapsedStore&,const PrivateShopOperationSourceContext&,
 PendingPrivateShopCancellation&&,const MomentOwnerSource& actual_command_result);
// Re-reads the already-produced original terminal source; it does not manufacture
// a result, cancellation, elapsed credit, or second terminal table.
Status adopt_private_shop_terminal_time_in_candidate(ActionElapsedStore&,
 const PrivateShopOperationSourceContext&,const MomentOwnerSource& actual_terminal);
}
