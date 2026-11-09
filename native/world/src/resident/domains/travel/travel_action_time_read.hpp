// One original travel owner, with finite movement/handling dispatch.
#pragma once
#include "movement_action_time.hpp"
#include "cargo_handling.hpp"
#include <memory>
namespace hanmo::resident {
class NativeTravelActionTimeRead final:public ActionTimeOriginalSourceRead {
 std::shared_ptr<const ActionTimeOriginalSourceRead> movement_,cargo_;
 const ActionTimeOriginalSourceRead& owner(const ActionTimeRegistration&)const;
public:
 NativeTravelActionTimeRead(std::shared_ptr<const ActionTimeOriginalSourceRead> movement,std::shared_ptr<const ActionTimeOriginalSourceRead> cargo):movement_(std::move(movement)),cargo_(std::move(cargo)){}
 Status enumerate_action_time_owners(const ActionTimeRegistration&,ActionTimeOwnerIdSink&)const override;
 Outcome<ActionTimeOwnerProof> read_action_time_owner(const ActionTimeRegistration&,const StableId&)const override;
 Outcome<WorldMoment> read_action_time_source(const ElapsedTimeSource&)const override;
 Status validate_fixed_action_sources(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const ActionElapsedState&,const std::vector<FixedServiceTransition>&)const override;
 Status validate_shared_action_sources(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const SharedWorkElapsedState&)const override;
 Outcome<RunStop> read_action_continuation_stop(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const WorldMoment&)const override;
 Outcome<OriginalActionRemaining> read_original_action_remaining(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const WorldMoment&)const override;
 Outcome<StableId> read_gate_result_id(const SharedWorkGateResult&)const override;
 Outcome<SharedWorkGateResult> read_original_gate_result(const StableId&)const override;
 Status validate_original_gate_result(const SharedWorkGateResult&)const override;
};
}
