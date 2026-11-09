// 真正CraftJob的S0时间owner；专业全源独立核验，不用elapsed通过来证明Job。
#pragma once
#include "craft_job.hpp"
namespace hanmo::resident {
class NativeCraftActionTimeRead final:public ActionTimeOriginalSourceRead {
 const CraftJobReadContext& x_;
 enum class SourcePhase {stable,pending_gate};
 Status validate_shared_source_history(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const SharedWorkElapsedState&,SourcePhase)const;
public:
 explicit NativeCraftActionTimeRead(const CraftJobReadContext&x):x_(x){}
 Status enumerate_action_time_owners(const ActionTimeRegistration&,ActionTimeOwnerIdSink&)const override;
 Outcome<ActionTimeOwnerProof> read_action_time_owner(const ActionTimeRegistration&,const StableId&)const override;
 Outcome<WorldMoment> read_action_time_source(const ElapsedTimeSource&)const override;
 Status validate_fixed_action_sources(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const ActionElapsedState&,const std::vector<FixedServiceTransition>&)const override;
 Status validate_shared_action_sources(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const SharedWorkElapsedState&)const override;
 // Only the real unpublished N-service boundary; never a cold/stable bypass.
 Status validate_pending_craft_gate_sources(const SharedWorkElapsedState&,const WorldMoment& actual_gate_at)const;
 Outcome<RunStop> read_action_continuation_stop(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const WorldMoment&)const override;
 Outcome<OriginalActionRemaining> read_original_action_remaining(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const WorldMoment&)const override;
 Outcome<StableId> read_gate_result_id(const SharedWorkGateResult&)const override;
 Outcome<SharedWorkGateResult> read_original_gate_result(const StableId&)const override;
 Status validate_original_gate_result(const SharedWorkGateResult&)const override;
};
}
