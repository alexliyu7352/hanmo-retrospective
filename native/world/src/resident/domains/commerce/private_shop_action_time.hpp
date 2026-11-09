#pragma once
#include "private_shop_operation_sources.hpp"
namespace hanmo::resident {
// The original compiled private-shop operation is H2. This names that existing
// fixed rule, not the unrelated market opening definition or an author override.
inline constexpr const char* private_shop_operation_duration_rule="private_shop_operation_duration_h2";
const ActionTimeRegistration& private_shop_operation_action_time_registration();
class NativePrivateShopActionTimeRead final:public ActionTimeOriginalSourceRead {
 const PrivateShopOperationSourceContext&x_;
public:
 explicit NativePrivateShopActionTimeRead(const PrivateShopOperationSourceContext&x):x_(x){}
 Status enumerate_action_time_owners(const ActionTimeRegistration&,ActionTimeOwnerIdSink&)const override;
 Outcome<ActionTimeOwnerProof> read_action_time_owner(const ActionTimeRegistration&,const StableId&)const override;
 Outcome<WorldMoment> read_action_time_source(const ElapsedTimeSource&)const override;
 Status validate_fixed_action_sources(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const ActionElapsedState&,const std::vector<FixedServiceTransition>&)const override;
 Status validate_fixed_action_sources_in_candidate(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const ActionElapsedState&,const std::vector<FixedServiceTransition>&,const ActionElapsedStore&)const override;
 Status validate_shared_action_sources(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const SharedWorkElapsedState&)const override;
 Outcome<RunStop> read_action_continuation_stop(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const WorldMoment&)const override;
 Outcome<OriginalActionRemaining> read_original_action_remaining(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const WorldMoment&)const override;
 Outcome<StableId> read_gate_result_id(const SharedWorkGateResult&)const override;
 Outcome<SharedWorkGateResult> read_original_gate_result(const StableId&)const override;
 Status validate_original_gate_result(const SharedWorkGateResult&)const override;
};
// The only adopted inputs are actual S7 start, actual S4 head, actual S1 head,
// and the registered S4 purpose/reducer locked in the same selected catalog.
Outcome<std::vector<FixedServiceInputRef>> read_private_shop_current_service_inputs(
 const PrivateShopOperationSourceContext&,const StableId& actual_operation);
}
