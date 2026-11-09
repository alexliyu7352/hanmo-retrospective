#pragma once
#include "state_need.hpp"
#include "resident/kernel/exact_phase_graph.hpp"
namespace hanmo::resident {
// A null period means the actual S0 Ready state, never an omitted provider.
Status validate_state_need_claim_window(const StateNeedState&,Integer claim_until_tick,const WorldMoment&,const ExactPeriodContext* original_open_period);
// state_needs/2 only: capture once at the actual S0 Ready->Open boundary.
Status open_state_need_period(StateNeedState&,const ExactPeriodContext&,const StateNeedPolicyRead&,const StateTaskRead&);
Status close_state_need_period(CommonStore&,AffairsState&,const ExactPeriodContext&,const StateNeedPolicyRead&,const StateGoalRead&,StateTaskConsumer&,const WorldMoment&);
Status validate_state_need_period(const StateNeedState&,const ExactPeriodContext&,bool closed,const StateNeedPolicyRead&,const StateTaskRead&);
Status validate_state_need_period_history(const StateNeedState&,const WorldTimelineState&,const WorldMoment&,const StateNeedPolicyRead&,const StateTaskRead&);
Outcome<WorldMoment> read_state_need_period_opening(const StateNeedState&,const SourceRef&,Integer schema);
// A prior opening may be released only after its real period ended and no closure
// still uses that frozen deadline/claim sample. The current opening is never cut.
Outcome<bool> state_need_opening_releasable(const StateNeedState&,const StableId&,const WorldTimelineState&,const WorldMoment&);
}
