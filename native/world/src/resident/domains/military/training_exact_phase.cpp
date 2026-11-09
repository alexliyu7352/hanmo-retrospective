#include "training_exact_phase.hpp"
#include "training_exact_detail.hpp"
#include "training_exact_result_read.hpp"
#include "resident/kernel/command_types.hpp"
namespace hanmo::resident {using namespace training_exact_detail;namespace {
void complete(ExactPhaseWriteContext& writes,const DynamicMilitaryTrainingFact2& row,MilitaryEffect effect){
 if(row.phase!=DynamicMilitaryTrainingPhase::completed||!row.completed_at||!row.terminal_source_ref)
  fail("ExactTrainingActualResultRequired",row.id);
 ActionCompletion result;result.success=effect.success;result.code=effect.code;result.already_completed=effect.already_completed;
 result.military=std::move(effect);
 if(!writes.advance.completed_actions.insert(row.id,std::move(result)))fail("DuplicateMilitaryTrainingActionCompletion",row.id);
}
void current(const ExactTrainingReadContext& c,const WorldMoment& at){
 if(!same_moment(take(c.clock.read_current_moment()),at,c.budget))fail("ExactTrainingCurrentMomentRequired");
}
}
Outcome<std::vector<ExactEventPrediction>> predict_exact_military_training_events(
 const CommonStore& common,const MilitaryState& military,const MilitaryActionTimeState& state,
 const ExactTrainingReadContext& c,const WorldMoment& now,const WorldMoment& upper){return guard<std::vector<ExactEventPrediction>>([&]{
 bound(common,military,c);current(c,now);take(make_moment_span(now,upper,c.budget));
 std::vector<ExactEventPrediction> events;
 for(const auto& item:state.dynamic_training_facts.rows()){
  const auto& row=item.second;if(row.phase==DynamicMilitaryTrainingPhase::completed)continue;
  if(row.phase!=DynamicMilitaryTrainingPhase::started||row.id!=item.first||row.lessons.size()>=4)fail("InvalidExactTrainingPhase",item.first);
  header(common,start_ref(row.id));const auto change=take(c.original.read_training_eligibility_change(row,now));
  if(change){if(!source_valid(change->actual_source.source_ref)||change->actual_source.source_schema<1||change->original_code.empty()||!same_moment(change->occurred_at,now,c.budget))fail("ExactTrainingActualEligibilityChangeRequired",row.id);
   events.push_back({now,ExactStage::immediate_consequences,start_ref(row.id),2,Integer(row.lessons.size())});continue;}
  const auto due=gate(row,Integer(row.lessons.size())+1,c.budget);
  if(take(compare_world_moments(due,now,c.budget))<0)fail("ExactTrainingUnconsumedLesson",row.id);
  if(take(compare_world_moments(due,upper,c.budget))<=0)events.push_back({due,ExactStage::action_completions,start_ref(row.id),2,Integer(row.lessons.size())});
 }
 return events;
});}
Status provide_exact_military_training_interval(CommonStore& common,MilitaryState& military,
 MilitaryActionTimeState& state,ActionElapsedStore& elapsed,const ExactTrainingWriteContext& c,
 const MomentSpan& span,ExactPhaseWriteContext& writes){return status([&]{
 bound(common,military,c.read);current(c.read,span.to);std::vector<StableId> active;
 for(const auto& row:state.dynamic_training_facts.rows())if(row.second.phase==DynamicMilitaryTrainingPhase::started)active.push_back(row.first);
 for(const auto& id:active){take(advance_exact_military_training_in_candidate(common,military,state,elapsed,c,id,span));
  const auto& row=fact(state,id);if(row.phase==DynamicMilitaryTrainingPhase::completed)complete(writes,row,receipt(row));}
});}
Outcome<ExactEventApplied> apply_exact_military_training_event(CommonStore& common,MilitaryState& military,
 MilitaryActionTimeState& state,ActionElapsedStore& elapsed,const ExactTrainingWriteContext& c,
 const ExactEventPrediction& prediction,ExactPhaseWriteContext& writes){return guard<ExactEventApplied>([&]{
 bound(common,military,c.read);current(c.read,prediction.at);const auto& ref=prediction.actual_source_basis;
 if(ref.domain!="military"||ref.kind!="military_training"||prediction.source_schema!=2)fail("ExactTrainingEventSourceRequired",ref.id);
 const auto& before=fact(state,ref.id);if(before.phase!=DynamicMilitaryTrainingPhase::started||prediction.dependency_revision!=Integer(before.lessons.size()))fail("StaleExactTrainingPrediction",ref.id);
 const auto lessons=before.lessons.size();MilitaryEffect effect;
 if(prediction.stage==ExactStage::immediate_consequences){
  const auto change=take(c.read.original.read_training_eligibility_change(before,prediction.at));
  if(!change||!same_moment(change->occurred_at,prediction.at,c.read.budget))fail("ExactTrainingActualEligibilityChangeRequired",ref.id);
  effect=take(interrupt_exact_military_training_in_candidate(common,military,state,elapsed,c,ref.id,change->actual_source));
 }else if(prediction.stage==ExactStage::action_completions){
  effect=take(complete_exact_military_training_gate_in_candidate(common,military,state,elapsed,c,ref.id));
 }else fail("ExactTrainingEventStageMismatch",ref.id);
 const auto& after=fact(state,ref.id);
 if(after.phase==DynamicMilitaryTrainingPhase::completed){complete(writes,after,std::move(effect));return ExactEventApplied{completed_ref(after.id),2};}
 if(after.lessons.size()!=lessons+1||after.lessons.empty())fail("ExactTrainingPredictionChangedWithoutWrite",ref.id);
 return ExactEventApplied{after.lessons.back().fee_effect_ref,2};
});}
Outcome<std::vector<ExactEventApplied>> apply_pending_exact_training_consequences_in_candidate(
 CommonStore& common,MilitaryState& military,MilitaryActionTimeState& state,ActionElapsedStore& elapsed,
 const ExactTrainingWriteContext& c,const MomentOwnerSource& actual_cause,ExactPhaseWriteContext& writes){return guard<std::vector<ExactEventApplied>>([&]{
 bound(common,military,c.read);if(!source_valid(actual_cause.source_ref)||actual_cause.source_schema<1)fail("ExactTrainingActualEligibilityChangeRequired");
 const auto now=take(c.read.clock.read_current_moment());std::vector<StableId> active;
 for(const auto& item:state.dynamic_training_facts.rows())if(item.second.phase==DynamicMilitaryTrainingPhase::started)active.push_back(item.first);
 std::vector<ExactEventApplied> results;
 for(const auto& id:active){const auto& row=fact(state,id);const auto change=take(c.read.original.read_training_eligibility_change(row,now));
  if(!change||!same_source(change->actual_source.source_ref,actual_cause.source_ref)||change->actual_source.source_schema!=actual_cause.source_schema)continue;
  auto effect=take(interrupt_exact_military_training_in_candidate(common,military,state,elapsed,c,id,actual_cause));
  complete(writes,fact(state,id),std::move(effect));results.push_back({completed_ref(id),2});
 }
 return results;
});}
Status validate_exact_military_training_applied_source(const CommonStore& common,const MilitaryState& military,
 const MilitaryActionTimeState& state,const ActionElapsedStore& elapsed,const ExactTrainingReadContext& c,
 const ExactEventApplied& applied){return status([&]{
 if(applied.applied_result.kind!="military_training_lesson"&&applied.applied_result.kind!="military_training_completed")fail("ExactTrainingAppliedKindMismatch",applied.applied_result.id);
 take(read_verified_exact_military_training_source_moment(common,military,state,elapsed,c,applied.applied_result,applied.source_schema));
});}
}
