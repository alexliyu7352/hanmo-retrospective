#include "training_exact_result_read.hpp"
#include "training_exact_detail.hpp"
#include "resident/kernel/fixed_service_history.hpp"
namespace hanmo::resident {using namespace training_exact_detail;namespace {
void source_kind(const SourceRef& source,Integer schema){
 if(schema!=2||source.domain!="military"||source.id.empty())
  fail("ExactMilitaryTrainingSourceRequired",source.id);
}
const DynamicMilitaryTrainingFact2& verified_owner(const CommonStore& common,
 const MilitaryState& military,const MilitaryActionTimeState& state,
 const ActionElapsedStore& elapsed,const ExactTrainingReadContext& context,
 const StableId& action){
 bound(common,military,context);
 // A different action may be awaiting its same-M gate. This only admits that
 // producer transient; it does not manufacture a completed result for this row.
 take(validate_exact_military_training_sources(common,military,state,elapsed,context,true));
 const auto& row=fact(state,action);const auto* entry=find_fixed_elapsed(elapsed,action);
 if(!entry)fail("ExactTrainingElapsedRequired",action);
 const auto now=take(context.clock.read_current_moment());
 // S0 independently verifies the actual selected chain, including a zero-service
 // immediate cancellation. It does not validate unrelated pending actions.
 take(read_fixed_service_intervals(elapsed,action,row.started_at,entry->last_settled_at,
                                  now,context.scale,context.budget));
 NativeMilitaryTrainingActionTimeRead original(common,military,state,elapsed,context);
 const auto registration=exact_military_training_registration();
 const auto proof=take(original.read_action_time_owner(registration,action));
 take(read_action_time_projection(registration,proof,elapsed,now,context.scale,original,context.budget));
 return row;
}
}
Outcome<const DynamicMilitaryTrainingFact2*> read_verified_exact_military_training_result(
 const CommonStore& common,const MilitaryState& military,const MilitaryActionTimeState& state,
 const ActionElapsedStore& elapsed,const ExactTrainingReadContext& context,
 const SourceRef& source,Integer schema){return guard<const DynamicMilitaryTrainingFact2*>([&]{
 source_kind(source,schema);if(source.kind!="military_training_completed")
  fail("ExactMilitaryTrainingCompletedSourceRequired",source.id);
 const auto& row=verified_owner(common,military,state,elapsed,context,source.id);
 if(row.phase!=DynamicMilitaryTrainingPhase::completed||!row.completed_at||!row.terminal_source_ref)
  fail("ExactMilitaryTrainingActualResultRequired",source.id);
 return &row;
});}
Outcome<WorldMoment> read_verified_exact_military_training_source_moment(
 const CommonStore& common,const MilitaryState& military,const MilitaryActionTimeState& state,
 const ActionElapsedStore& elapsed,const ExactTrainingReadContext& context,
 const SourceRef& source,Integer schema){return guard<WorldMoment>([&]{
 source_kind(source,schema);
 if(source.kind=="military_training_completed")
  return *take(read_verified_exact_military_training_result(common,military,state,elapsed,context,source,schema))->completed_at;
 StableId action;const WorldMoment* moment=nullptr;
 if(source.kind=="military_training"){
  const auto& row=fact(state,source.id);action=row.id;moment=&row.started_at;
 }else if(source.kind=="military_training_participation"){
  const auto* row=state.training_participation_spans.find(source.id);
  if(!row)fail("ExactTrainingParticipationSourceRequired",source.id);action=row->action_ref;moment=&row->to;
 }else if(source.kind=="military_training_period_witness"){
  const auto* row=state.training_period_witnesses.find(source.id);
  if(!row)fail("ExactTrainingPeriodWitnessSourceRequired",source.id);action=row->action_ref;moment=&row->intersection_to;
 }else if(source.kind=="military_training_drill_frozen"){
  const auto* row=state.drill_freeze_sources.find(source.id);
  if(!row)fail("ExactTrainingDrillFreezeSourceRequired",source.id);action=row->action_ref;moment=&row->frozen_at;
 }else if(source.kind=="military_training_lesson"||source.kind=="military_training_cancelled"){
  for(const auto& owned:state.dynamic_training_facts.rows()){
   const auto& row=owned.second;
   if(source.kind=="military_training_lesson"){
    for(const auto& lesson:row.lessons)if(same_source(lesson.fee_effect_ref,source)){
     if(moment)fail("DuplicateExactTrainingLesson",source.id);action=row.id;moment=&lesson.closed_at;
    }
   }else if(row.terminal_code==std::optional<StableId>{"MilitaryTrainingCancelled"}&&
            row.terminal_source_ref&&same_source(*row.terminal_source_ref,source)){
    if(moment||!row.completed_at)fail("ExactTrainingCancellationSourceMismatch",source.id);
    action=row.id;moment=&*row.completed_at;
   }
  }
 }else fail("UnsupportedExactMilitaryTrainingSource",source.kind);
 if(!moment||action.empty())fail("ExactMilitaryTrainingOriginalSourceMissing",source.id);
 header(common,source);verified_owner(common,military,state,elapsed,context,action);
 return *moment;
});}
}
