// A borrowed same-candidate composition of the original owners. No persistent clock or result index.
#pragma once
#include "action_time_validation.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
namespace hanmo::resident {
// Compiled root construction only. This type is not decoded from a command or author file.
struct CompiledActionTimeOwnerBinding {
 StableId owner_domain;
 std::shared_ptr<const ActionTimeOriginalSourceRead> source;
};
class NativeActionTimeSourceRead final:public ActionTimeOriginalSourceRead {
 const std::vector<ActionTimeRegistration>& registrations_;
 CheckpointVector<CompiledActionTimeOwnerBinding> owners_;
 const ActionTimeOriginalSourceRead* owner(const StableId&)const;
 Outcome<const ActionTimeOriginalSourceRead*> selected(const ActionTimeRegistration&)const;
 Outcome<const ActionTimeOriginalSourceRead*> gate_owner(const SharedWorkGateResult&)const;
public:
 NativeActionTimeSourceRead(const std::vector<ActionTimeRegistration>& r,CheckpointVector<CompiledActionTimeOwnerBinding> owners):registrations_(r),owners_(std::move(owners)){}
 Status validate_owner_bindings()const;
 Status enumerate_action_time_owners(const ActionTimeRegistration&,ActionTimeOwnerIdSink&)const override;
 Outcome<ActionTimeOwnerProof> read_action_time_owner(const ActionTimeRegistration&,const StableId&)const override;
 Outcome<WorldMoment> read_action_time_source(const ElapsedTimeSource&)const override;
 Status validate_fixed_action_sources(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const ActionElapsedState&,const std::vector<FixedServiceTransition>&)const override;
 Status validate_fixed_action_sources_in_candidate(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const ActionElapsedState&,const std::vector<FixedServiceTransition>&,const ActionElapsedStore&)const override;
 Status validate_shared_action_sources(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const SharedWorkElapsedState&)const override;
 Outcome<RunStop> read_action_continuation_stop(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const WorldMoment&)const override;
 Outcome<OriginalActionRemaining> read_original_action_remaining(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const WorldMoment&)const override;
 Outcome<ExhaustedAuthorizationWindow> read_exhausted_authorization_window(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const WorldMoment&)const override;
 Outcome<StableId> read_gate_result_id(const SharedWorkGateResult&)const override;
 Outcome<SharedWorkGateResult> read_original_gate_result(const StableId&)const override;
 Status validate_original_gate_result(const SharedWorkGateResult&)const override;
 const LifeWorkPeriodSourceRead* life_work_period_sources(const LifeWorkPeriodSourceRef&)const noexcept override;
};
}
