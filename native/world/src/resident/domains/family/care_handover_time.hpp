// FH5.3B: original Care boundaries adopt the one S0 Fixed chain. No second clock.
#pragma once
#include "care_handover_worker_read.hpp"
#include "resident/kernel/fixed_service_runtime.hpp"
#include "hanmo/resident_world_api.hpp"
namespace hanmo::resident {
struct CareHandoverOriginalActors {StableId actor_id;std::vector<StableId> participant_actor_ids;};
// The original Care execution composition must supply these independent reads.
// They re-read actual command/obligation, consent, authority, participation,
// facility and outcome owners. They must not invoke ActionTime or validate an
// outcome by its own FSL Terminal. Missing concrete sources prevent admission.
class CareHandoverBusinessRead {
public:
 virtual ~CareHandoverBusinessRead()=default;
 virtual bool bound_to(const CommonStore&)const noexcept=0;
 virtual Outcome<CareHandoverOriginalActors> read_original_actors(const CareHandoverExecution&,const AcceptedCaregiver&)const=0;
 virtual Status validate_original_boundary(const CareHandoverExecution&,const AcceptedCaregiver&,const CareHandoverExecutionBoundary1&)const=0;
 virtual Status validate_original_input_change_coverage(const CareHandoverExecution&,const WorldMoment& actual_now)const=0;
 virtual Outcome<RunStop> read_original_terminal_stop(const CareHandoverExecution&,const CareHandoverExecutionBoundary1&)const=0;
};
Outcome<ActionTimeRegistration> make_care_handover_action_time_registration(Integer selected_care_version);
class CareHandoverActionTimeRead final:public ActionTimeOriginalSourceRead {
 const CareHandoverSourceContext& sources_;const CareHandoverBusinessRead& original_;
 Status validate_fixed(const ActionTimeRegistration&,const ActionTimeOwnerProof&,
  const ActionElapsedState&,const std::vector<FixedServiceTransition>&,bool retained_world)const;
public:
 CareHandoverActionTimeRead(const CareHandoverSourceContext& x,const CareHandoverBusinessRead& original):sources_(x),original_(original){}
 Status enumerate_action_time_owners(const ActionTimeRegistration&,ActionTimeOwnerIdSink&)const override;
 Outcome<ActionTimeOwnerProof> read_action_time_owner(const ActionTimeRegistration&,const StableId&)const override;
 Outcome<WorldMoment> read_action_time_source(const ElapsedTimeSource&)const override;
 Status validate_fixed_action_sources(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const ActionElapsedState&,const std::vector<FixedServiceTransition>&)const override;
 Status validate_retained_fixed_sources(const ActionTimeRegistration&,const StableId&)const;
 Status validate_shared_action_sources(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const SharedWorkElapsedState&)const override;
 Outcome<RunStop> read_action_continuation_stop(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const WorldMoment&)const override;
 Outcome<OriginalActionRemaining> read_original_action_remaining(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const WorldMoment&)const override;
 Outcome<StableId> read_gate_result_id(const SharedWorkGateResult&)const override;
 Outcome<SharedWorkGateResult> read_original_gate_result(const StableId&)const override;
 Status validate_original_gate_result(const SharedWorkGateResult&)const override;
};
// The actual Care handler has already written this boundary and its real
// business effect/release. This adopts it once; any failure discards the whole
// unpublished candidate, including physical placement, fees and body effects.
Outcome<StableId> adopt_care_handover_boundary_in_candidate(CommonStore&,
 OrderedTable<CareHandoverExecution>&,ActionElapsedStore&,
 const CareHandoverSourceContext&,const CareHandoverBusinessRead&,
 const StableId& actual_boundary_id);
}
