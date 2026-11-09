#include "office_exact_phase.hpp"
#include "office_exact_detail.hpp"
#include "resident/kernel/command_types.hpp"
namespace hanmo::resident {using namespace office_exact_detail;namespace {
void current(const ExactOfficeReadContext& x,const WorldMoment& at){if(!same_moment(take(x.clock.read_current_moment()),at,x.budget))fail("ExactOfficeCurrentMomentRequired");}
}
Outcome<std::vector<ExactEventPrediction>> predict_exact_office_events(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& s,const ExactOfficeReadContext& x,const WorldMoment& now,const WorldMoment& upper){return guard<std::vector<ExactEventPrediction>>([&]{
 bound(c,m,s,x);current(x,now);
 // 原预测查询是闭区间：同刻仍枚举真实due；反向沿原错误拒绝，正片writer不变。
 if(take(compare_world_moments(now,upper,x.budget))>0)
  take(make_moment_span(now,upper,x.budget));
 std::vector<ExactEventPrediction> out;
 for(const auto& item:s.dynamic_action_facts.rows())if(item.second.command=="office_accept"){
  const auto& f=fact(s,item.first);header(c,f.id);if(f.phase=="completed")continue;if(f.phase!="started")fail("InvalidExactOfficePhase",f.id);take(x.original.validate_office_acceptance(f));const auto completion=due(f,x.budget);
  if(take(compare_world_moments(completion,now,x.budget))<0)fail("ExactOfficeUnconsumedCompletion",f.id);
  if(take(compare_world_moments(completion,upper,x.budget))<=0)out.push_back({completion,ExactStage::action_completions,start_ref(f.id),2,0});
 }return out;
});}
Status provide_exact_office_interval(CommonStore& c,MilitaryState& m,MilitaryActionTimeState& s,ActionElapsedStore& elapsed,const ExactOfficeWriteContext& x,const MomentSpan& span){return status([&]{bound(c,m,s,x.read);current(x.read,span.to);for(const auto& item:s.dynamic_action_facts.rows())if(item.second.command=="office_accept"&&item.second.phase=="started")take(advance_exact_office_action_in_candidate(c,m,s,elapsed,x,item.first,span));});}
Outcome<ExactEventApplied> apply_exact_office_event(CommonStore& c,MilitaryState& m,MilitaryActionTimeState& s,ActionElapsedStore& elapsed,const ExactOfficeWriteContext& x,const ExactEventPrediction& p,ExactPhaseWriteContext& writes){return guard<ExactEventApplied>([&]{
 bound(c,m,s,x.read);current(x.read,p.at);if(p.stage!=ExactStage::action_completions||p.source_schema!=2||p.actual_source_basis.domain!="military"||p.actual_source_basis.kind!="military_action"||p.dependency_revision!=0)fail("ExactOfficePredictionSourceMismatch",p.actual_source_basis.id);
 const auto& before=fact(s,p.actual_source_basis.id);if(before.phase!="started"||!same_moment(due(before,x.read.budget),p.at,x.read.budget))fail("StaleExactOfficePrediction",before.id);
 auto effect=take(complete_exact_office_action_in_candidate(c,m,s,elapsed,x,before.id));const auto& after=fact(s,p.actual_source_basis.id);if(after.phase!="completed"||!after.completed_at||!same_moment(*after.completed_at,p.at,x.read.budget))fail("ExactOfficePredictionChangedWithoutResult",after.id);
 ActionCompletion completion;completion.success=effect.success;completion.code=effect.code;completion.already_completed=effect.already_completed;completion.military=std::move(effect);if(!writes.advance.completed_actions.insert(after.id,std::move(completion)))fail("DuplicateExactOfficeActionCompletion",after.id);
 return ExactEventApplied{completed_ref(after.id),2};
});}
Status validate_exact_office_applied_source(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& s,const ActionElapsedStore& elapsed,const ExactOfficeReadContext& x,const ExactEventApplied& applied){return status([&]{take(read_verified_exact_office_result(c,m,s,elapsed,x,applied.applied_result,applied.source_schema));});}
}
