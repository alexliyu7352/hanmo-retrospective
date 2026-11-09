#pragma once
#include "cargo_handling.hpp"
#include "resident/kernel/exact_phase_graph.hpp"
namespace hanmo::resident {
// 同motion注册中的实际子叶，不新增S0 provider或另一个时钟。
Outcome<std::vector<ExactEventPrediction>> predict_cargo_handling_events(const CargoHandlingState&,const ActionElapsedStore&,const CargoHandlingReadContext&,const WorldMoment&,const WorldMoment&);
Status provide_cargo_handling_interval(CommonStore&,CargoHandlingState&,ActionElapsedStore&,const CargoHandlingWriteContext&,const MomentSpan&);
Outcome<ExactEventApplied> apply_cargo_handling_event(CommonStore&,CargoHandlingState&,ActionElapsedStore&,const CargoHandlingWriteContext&,const ExactEventPrediction&,ExactPhaseWriteContext&);
Outcome<std::vector<ExactEventApplied>> apply_pending_cargo_handling_consequences_in_candidate(CommonStore&,CargoHandlingState&,ActionElapsedStore&,const CargoHandlingWriteContext&,const SourceRef& actual_cause,const WorldMoment&,ExactPhaseWriteContext&);
Status validate_cargo_handling_applied_source(const CommonStore&,const CargoHandlingState&,const ExactEventApplied&);
}
