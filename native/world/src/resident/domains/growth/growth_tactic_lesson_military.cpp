#include "growth_tactic_lesson_military.hpp"
#include <algorithm>
#include <set>
#include <tuple>
namespace hanmo::resident {namespace {
bool identity(const WorldIdentity& a,const WorldIdentity& b){return std::tie(a.world_id,a.execution_epoch,a.revision,a.tick)==std::tie(b.world_id,b.execution_epoch,b.revision,b.tick);}
std::size_t position(const CommonStore& c,const StableId& id){const auto& order=c.state().facts.order();return std::find(order.begin(),order.end(),id)-order.begin();}
Outcome<MilitaryTacticTeachingCompletion> read(const CommonStore& c,const GrowthTacticLessonState& state,const StableId& id){
 auto source=read_growth_tactic_lesson_completion(c,state,id);if(!source.ok)return {false,{},source.error};if(!source.value.success)return {false,{},common_failure("TacticLessonSuccessfulCompletionRequired",id).error};const auto& end=*state.closures.find(id);const auto& start=*state.starts.find(end.started_fact_id);const auto& s=start.agreement;MilitaryTacticTeachingCompletion p;p.source_identity=c.state().identity;p.source_fact_id=id;p.started_fact_id=end.started_fact_id;p.action_id=start.action_id;p.tactic_id=s.tactic_id;p.student_id=s.student_id;p.teacher_id=s.teacher_id;p.place_id=s.place_id;
 // 原started载荷永久保存这次真实教师say和本人报名；没有另分配虚假的声明来源头。
 p.agreement_id=end.started_fact_id;p.teacher_say_source=end.started_fact_id;p.started_tick=s.tick;p.completed_tick=end.tick;p.prerequisite_skill_id=s.prerequisite.skill_id;p.student_minimum_experience=s.prerequisite.minimum_xp;p.student_minimum_level=s.prerequisite.minimum_level;p.qualification_observed_tick=s.prerequisite.observed_tick;p.teacher_acquisition_fact_id=s.teacher_source.acquisition_fact_id;p.teacher_initial_grant_id=s.teacher_source.initial_grant_id;p.qualification_causal_fact_ids=s.prerequisite.initial_source_ids;p.qualification_causal_fact_ids.insert(p.qualification_causal_fact_ids.end(),s.prerequisite.study_source_ids.begin(),s.prerequisite.study_source_ids.end());std::set<StableId> unique;
 for(const auto& fact:p.qualification_causal_fact_ids)if(!unique.insert(fact).second||!c.state().facts.find(fact)||position(c,fact)>=position(c,end.started_fact_id))return {false,{},common_failure("TacticLessonQualificationSourceOrderMismatch",fact).error};std::sort(p.qualification_causal_fact_ids.begin(),p.qualification_causal_fact_ids.end(),[&](const StableId& a,const StableId& b){return position(c,a)<position(c,b);});return {true,std::move(p),{}};
}
}
Outcome<MilitaryTacticTeachingCompletion> read_growth_tactic_teaching_completion(const CommonStore& c,const GrowthTacticLessonState& state,const StableId& id){auto valid=validate_growth_tactic_lesson_result_bindings(c,state);if(!valid.ok)return {false,{},valid.error};return read(c,state,id);}
Outcome<MilitaryTacticTeachingCompletion> read_growth_tactic_teaching_completion_after_half_day(const CommonStore& c,const GrowthTacticLessonState& state,const StableId& id,const DynamicStepInput& step){
 const auto* end=state.closures.find(id);const auto* start=end?state.starts.find(end->started_fact_id):nullptr;if(step.from_tick<0||step.from_tick>=2147483647||step.to_tick!=step.from_tick+1||step.to_tick!=c.tick()||!end||!start||end->tick!=step.to_tick||end->reason!="completed"||!end->commit.result_id.empty()||start->commit.result_id.empty()||start->agreement.tick>=step.to_tick)return {false,{},common_failure("TacticLessonNewCommittedCompletionRequired",id).error};return read(c,state,id);
}
Status validate_growth_tactic_teaching_acquisition_coverage(const CommonStore& c,const GrowthTacticLessonState& state,const GrowthTacticTeachingAcquiredRead& acquired){
 auto valid=validate_growth_tactic_lesson_result_bindings(c,state);if(!valid.ok)return valid;if(!acquired)return common_failure("TacticLessonAcquisitionReadRequired");std::set<StableId> used;
 for(const auto& row:state.closures.rows()){if(row.second.reason!="completed")continue;auto p=read(c,state,row.first);if(!p.ok)return {false,{},p.error};auto got=acquired(row.first);if(!got.ok)return {false,{},got.error};const auto& a=got.value;const auto* h=c.state().facts.find(a.acquisition_fact_id);if(!identity(a.source_identity,c.state().identity)||a.source_kind!=MilitaryTacticLearningSourceKind::teaching||a.source_fact_id!=row.first||a.actor_id!=p.value.student_id||a.tactic_id!=p.value.tactic_id||a.acquired_tick!=p.value.completed_tick||!a.initial_grant_id.empty()||!a.before_fact_id.empty()||a.observed_tick!=c.tick()||!h||h->id!=a.acquisition_fact_id||h->kind!="military_tactic_learned"||h->domain!="military_tactics"||a.acquisition_fact_id.rfind("military_tactic:",0)!=0||position(c,row.first)>=position(c,a.acquisition_fact_id)||!used.insert(a.acquisition_fact_id).second)return common_failure("TacticLessonAcquisitionCoverageMismatch",row.first);}
 return common_success();
}
}
