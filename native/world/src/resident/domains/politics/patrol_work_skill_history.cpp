// 只消费真实S3结果前态的完整K3值；源反查不调用全Work读取，避免技能捕获循环。
#include "politics_security.hpp"
#include "resident/domains/tasks/task_model.hpp"
namespace hanmo::resident {
Outcome<GrowthSkillQualificationV2> NativePatrolWorkSkillHistoryRead::read_patrol_skill_before_work_result(
 const StableId& actor,const StableId& skill,const SourceRef& ref,const WorldMoment& at)const{
 auto fail=[&](const StableId& code){return Outcome<GrowthSkillQualificationV2>{false,{},common_failure(code,ref.id).error};};
 if(!tasks_.work_execution||!task_work_skill_inputs_version(tasks_.work_execution->version)||!inputs_.bound_to(common_,tasks_)||ref.domain!="tasks"||ref.kind!="patrol_work_result")return fail("PatrolWorkSkillInputsRequired");
 const auto* head=common_.state().facts.find(ref.id);const auto* result=tasks_.work_execution->results.find(ref.id);
 if(!head||head->domain!=ref.domain||head->kind!=ref.kind||!result||result->fact_id!=ref.id||result->kind!=WorkKind::patrol||result->disposition!=WorkResultDisposition::completed||!result->skill_input||!result->skill_input->qualification)return fail("PatrolActualSkillInputMissing");
 const auto* episode=tasks_.work_execution->episodes.find(result->work_id);
 if(!episode||episode->terminal_result_ref!=result->fact_id)return fail("PatrolActualSkillResultMismatch");
 auto same=compare_world_moments(at,result->finished_at);if(!same.ok)return {false,{},same.error};if(same.value)return fail("PatrolActualSkillMomentMismatch");
 auto valid=inputs_.validate_result_input(*episode,*result);if(!valid.ok)return {false,{},valid.error};
 const auto& value=*result->skill_input->qualification;
 if(result->skill_input->condition_before.actor_id!=actor||result->skill_input->skill_id!=skill||value.actor_id!=actor||value.skill_id!=skill||value.actual_boundary_ref.source_schema!=1||!same_source_ref(value.actual_boundary_ref.original_source_ref,ref))return fail("PatrolActualSkillIdentityMismatch");
 return {true,value,{}};
}
}
