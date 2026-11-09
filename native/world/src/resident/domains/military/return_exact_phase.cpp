#include "return_exact_phase.hpp"
#include "return_exact_detail.hpp"
#include "resident/kernel/command_types.hpp"
namespace hanmo::resident {using namespace return_exact_detail;namespace {
void current(const ExactReturnReadContext& x,const WorldMoment& at){if(!same_moment(take(x.clock.read_current_moment()),at,x.budget))fail("ExactReturnCurrentMomentRequired");}
}
Outcome<std::vector<ExactEventPrediction>> predict_exact_return_events(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& s,const ExactReturnReadContext& x,const WorldMoment& now,const WorldMoment& upper){return guard<std::vector<ExactEventPrediction>>([&]{
 bound(c,m,s,x);current(x,now);take(make_moment_span(now,upper,x.budget));std::vector<ExactEventPrediction> out;
 for(const auto& item:s.dynamic_logistics_facts.rows())if(item.second.command=="army_return"){
  const auto& f=fact(s,item.first);header(c,f.id);if(f.phase=="completed")continue;if(f.phase!="started")fail("InvalidExactReturnPhase",f.id);take(validate_exact_return_office_authority(c,m,s,x,f));take(x.original.validate_return_acceptance(f));const auto completion=due(f,x.budget);
  if(take(compare_world_moments(completion,now,x.budget))<0)fail("ExactReturnUnconsumedCompletion",f.id);
  if(take(compare_world_moments(completion,upper,x.budget))<=0)out.push_back({completion,ExactStage::action_completions,start_ref(f.id),2,0});
 }return out;
});}
Status provide_exact_return_interval(CommonStore& c,MilitaryState& m,MilitaryActionTimeState& s,ActionElapsedStore& elapsed,const ExactReturnWriteContext& x,const MomentSpan& span){return status([&]{bound(c,m,s,x.read);current(x.read,span.to);for(const auto& item:s.dynamic_logistics_facts.rows())if(item.second.command=="army_return"&&item.second.phase=="started")take(advance_exact_return_action_in_candidate(c,m,s,elapsed,x,item.first,span));});}
Outcome<ExactEventApplied> apply_exact_return_event(CommonStore& c,MilitaryState& m,MilitaryActionTimeState& s,ActionElapsedStore& elapsed,const ExactReturnWriteContext& x,const ExactEventPrediction& p,ExactPhaseWriteContext& writes){return guard<ExactEventApplied>([&]{
 bound(c,m,s,x.read);current(x.read,p.at);if(p.stage!=ExactStage::action_completions||p.source_schema!=2||p.actual_source_basis.domain!="military"||p.actual_source_basis.kind!="military_action"||p.dependency_revision!=0)fail("ExactReturnPredictionSourceMismatch",p.actual_source_basis.id);
 const auto& before=fact(s,p.actual_source_basis.id);if(before.phase!="started"||!same_moment(due(before,x.read.budget),p.at,x.read.budget))fail("StaleExactReturnPrediction",before.id);
 auto effect=take(complete_exact_return_action_in_candidate(c,m,s,elapsed,x,before.id));const auto& after=fact(s,p.actual_source_basis.id);if(after.phase!="completed"||!after.completed_at||!same_moment(*after.completed_at,p.at,x.read.budget))fail("ExactReturnPredictionChangedWithoutResult",after.id);
 ActionCompletion completion;completion.success=effect.success;completion.code=effect.code;completion.already_completed=effect.already_completed;completion.military=std::move(effect);if(!writes.advance.completed_actions.insert(after.id,std::move(completion)))fail("DuplicateExactReturnActionCompletion",after.id);
 return ExactEventApplied{completed_ref(after.id),2};
});}
Status validate_exact_return_applied_source(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& s,const ActionElapsedStore& elapsed,const ExactReturnReadContext& x,const ExactEventApplied& applied){return status([&]{take(read_verified_exact_return_result(c,m,s,elapsed,x,applied.applied_result,applied.source_schema));});}
}
