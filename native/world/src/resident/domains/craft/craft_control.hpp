// Pause/resume consumes the original control/eligibility source and preserves earned work.
#pragma once
#include "craft_job.hpp"
namespace hanmo::resident {
struct FacilityClaimUntilRequest;
struct CraftControlRequest {
 StableId job_id,actor_id;CraftControlKind kind=CraftControlKind::pause;
 Integer expected_revision=0,expected_gate_ordinal=0;
 std::optional<StableId> expected_last_closed_gate_ref;
 std::optional<MomentOwnerSource> actual_cause;
};
struct CraftServiceChronology {
 WorkExecutionPhase phase=WorkExecutionPhase::working;
 WorldMoment gate_opened_at,last_accounted_at;Integer gate_ordinal=0;WorkUnits earned_service_units;
};
Outcome<const CraftControlSource*> read_craft_control(const CraftJobReadContext&,const StableId&);
// Replays the one Common ordering of services, gates and controls. No elapsed writes.
Outcome<CraftServiceChronology> read_craft_service_chronology(const CraftJobReadContext&,const CraftJob&);
// Original input prefix through this real gate. Same chronology rules, but no
// post-gate body/fee/control effects are followed back into their consumers.
Outcome<CraftServiceChronology> read_craft_gate_input_chronology(const CraftJobReadContext&,const CraftJob&,const StableId& actual_gate_id);
Outcome<StableId> apply_craft_control_in_candidate(CommonStore&,CraftJobsState&,ActionElapsedStore&,const CraftJobWriteContext&,const CraftControlRequest&);
// 只有真实设施到期 caller 可走具名入口；普通玩家控制没有该选择位。
Outcome<StableId> apply_craft_facility_until_pause_in_candidate(CommonStore&,CraftJobsState&,ActionElapsedStore&,const CraftJobWriteContext&,const CraftControlRequest&,const FacilityClaimUntilRequest&);
}
