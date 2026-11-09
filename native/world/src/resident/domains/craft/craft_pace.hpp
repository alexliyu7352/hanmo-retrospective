// CRAFT-PACE-1: only future empty service gates, same original Job and price terms.
#pragma once
#include "craft_job.hpp"
namespace hanmo::resident {
struct CraftPaceSelection {CraftPace pace=CraftPace::normal;StableId actual_acceptance_id;Integer pace_revision=0;};
struct ChangeCraftPaceRequest {StableId job_id,actor_id;CraftPace pace=CraftPace::normal;Integer expected_gate_ordinal=0,expected_pace_revision=0;std::optional<StableId> expected_last_closed_gate_ref;std::optional<MomentOwnerSource> original_contract_change;};
Outcome<const CraftPaceChange*> read_craft_pace_change(const CraftJobReadContext&,const StableId&);
Outcome<CraftPaceSelection> read_craft_pace_for_gate(const CraftJobReadContext&,const CraftJob&,Integer gate_ordinal);
// The immutable accepted pace chain, without re-entering downstream condition
// or fee effects that are now consuming the gate's original work input.
Outcome<CraftPaceSelection> read_craft_pace_for_original_effect(const CraftJobReadContext&,const CraftJob&,Integer gate_ordinal);
// Writer uses the S0 original shared-work pace consumer; no direct elapsed write.
Outcome<StableId> change_craft_pace_in_candidate(CommonStore&,CraftJobsState&,ActionElapsedStore&,const CraftJobWriteContext&,const ChangeCraftPaceRequest&);
}
