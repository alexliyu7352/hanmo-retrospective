#include "growth_tactic_lesson_completion.hpp"
#include "resident/kernel/command_types.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
Outcome<GrowthTacticLessonCompletion> read_growth_tactic_lesson_completion(const CommonStore& c,const GrowthTacticLessonState& state,const StableId& id){
 auto valid=validate_growth_tactic_lesson_shape(state);if(!valid.ok)return {false,{},valid.error};const auto* end=state.closures.find(id);const auto* h=c.state().facts.find(id);const auto* start=end?state.starts.find(end->started_fact_id):nullptr;const auto* sh=end?c.state().facts.find(end->started_fact_id):nullptr;const auto& order=c.state().facts.order();
 if(!end||!start||!h||!sh||h->id!=id||h->kind!=end->kind||h->domain!="growth_tactic_lessons"||sh->id!=end->started_fact_id||sh->kind!=start->kind||sh->domain!=h->domain||end->tick>c.tick()||std::find(order.begin(),order.end(),end->started_fact_id)>=std::find(order.begin(),order.end(),id))return {false,{},common_failure("TacticLessonActualCompletionRequired",id).error};
 GrowthTacticLessonCompletion out;out.source_fact_id=id;out.started_fact_id=end->started_fact_id;out.action_id=end->action_id;out.actor_id=start->agreement.student_id;out.provider_actor_id=start->agreement.teacher_id;out.reason=end->reason;out.tick=end->tick;out.provided_half_days=end->provided_half_days;out.fee_paid=end->fee_paid;out.fee_released=end->fee_released;out.success=end->reason=="completed";
 if(end->reason=="student_cancelled")out.code="TacticLessonStudentCancelled";else if(end->reason=="student_dead")out.code="TacticLessonStudentDied";else if(end->reason=="teacher_dead")out.code="TacticLessonTeacherDied";else if(end->reason=="student_unavailable")out.code="TacticLessonStudentUnavailable";else if(end->reason=="teacher_unavailable")out.code="TacticLessonTeacherUnavailable";return {true,std::move(out),{}};
}
// 此读不重入全域历史；调用方先完整核验，本口只锁定原Action及同一报名/终止链。
Outcome<GrowthTacticLessonActionSource> read_growth_tactic_lesson_action_source(const CommonStore& c,const GrowthTacticLessonState& state,const StableId& student,const StableId& action){
 auto bad=[&]()->Outcome<GrowthTacticLessonActionSource>{return {false,{},common_failure("TacticLessonOriginalActionRequired",action).error};};auto valid=validate_growth_tactic_lesson_shape(state);if(!valid.ok)return {false,{},valid.error};if(student.empty()||action.empty())return bad();
 const GrowthTacticLessonStart* original=nullptr;StableId start_id;for(const auto& row:state.starts.rows())if(row.second.action_id==action){if(original)return bad();original=&row.second;start_id=row.first;}
 const auto* header=original?c.state().facts.find(start_id):nullptr;if(!original||!header||header->id!=start_id||header->kind!=original->kind||header->domain!="growth_tactic_lessons"||original->agreement.student_id!=student||original->agreement.tick>c.tick()||original->commit.result_id.empty())return bad();
 GrowthTacticLessonActionSource out;out.source_identity=c.state().identity;out.started_fact_id=start_id;out.action_id=action;out.agreement=original->agreement;
 for(const auto& row:state.closures.rows())if(row.second.started_fact_id==start_id){if(out.completion||row.second.commit.result_id.empty()||c.action(action))return bad();auto end=read_growth_tactic_lesson_completion(c,state,row.first);if(!end.ok)return {false,{},end.error};if(end.value.action_id!=action||end.value.actor_id!=student)return bad();out.completion=std::move(end.value);}
 if(!out.completion){Integer periods=0;for(const auto& row:state.credits.rows())if(row.second.started_fact_id==start_id)++periods;const auto* live=c.action(action);const auto& a=original->agreement;if(!live||live->kind!="tactic_lesson"||live->actor_id!=student||live->participants!=std::vector<StableId>{student,a.teacher_id}||live->start_tick!=a.tick||periods>=a.duration||live->remaining_ticks!=a.duration-periods||c.tick()-a.tick!=periods)return bad();}
 return {true,std::move(out),{}};
}
Status validate_growth_tactic_lesson_action_completion(const GrowthTacticLessonCompletion& source,const ActionCompletion& done){
 if(!done.ok||done.completed!=std::optional<bool>{true}||done.success!=std::optional<bool>{source.success}||done.code!=(source.code.empty()?std::optional<StableId>{}:std::optional<StableId>{source.code})||done.already_completed||done.military||done.task||done.family||done.growth||!done.military_field_order.empty())return common_failure("TacticLessonCompletionShapeMismatch",source.source_fact_id);
 std::set<StableId> expected{"ok","completed","success"};if(!source.code.empty())expected.insert("code");if(!done.field_order.empty()&&(done.field_order.size()!=expected.size()||std::set<StableId>(done.field_order.begin(),done.field_order.end())!=expected))return common_failure("TacticLessonCompletionShapeMismatch",source.source_fact_id);return common_success();
}
}
