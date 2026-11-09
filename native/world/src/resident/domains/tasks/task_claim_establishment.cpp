// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
#include "task_claim_establishment.hpp"
#include "task_model.hpp"
#include "task_exact_execution.hpp"
#include "task_original_sources.hpp"
#include "resident/domains/affairs/affairs_model.hpp"
#include "resident/kernel/common_command_sources.hpp"
#include "resident/kernel/command_types.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include <set>
namespace hanmo::resident {namespace {
template<class T> Outcome<T> fail(const char* code,const StableId& id={}) {return {false,{},common_failure(code,id,ErrorKind::internal).error};}
bool same(const SourceRef&a,const SourceRef&b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
bool same(const SubjectRef&a,const SubjectRef&b){return a.kind==b.kind&&a.id==b.id;}
Outcome<const TaskClaimEstablishedSource*> original_row(const CommonStore&common,const TaskState&tasks,const AffairsState&affairs,const TaskClaimEstablishedRef&ref){
 if(!affairs.exact_claims||!tasks.exact_quantities||!tasks.claim_establishment||ref.source.domain!="tasks"||ref.source.kind!="task_claim_established"||ref.source_schema!=1)return fail<const TaskClaimEstablishedSource*>("TaskClaimSourceCapabilityMissing",ref.source.id);
 const auto*s=tasks.claim_establishment->claim_establishment_sources.find(ref.source.id);const auto*h=common.state().facts.find(ref.source.id);
 if(!s||!h||h->id!=ref.source.id||h->domain!="tasks"||h->kind!="task_claim_established"||s->source_id!=ref.source.id||s->acceptance_action_id.empty())return fail<const TaskClaimEstablishedSource*>("TaskClaimSourceMissing",ref.source.id);
 const auto*t=tasks.tasks.find(s->task_id);const auto*c=tasks.contracts.find(s->contract_id);const auto*n=affairs.claims.find(s->claim_id);const auto*accepted=n&&n->accepted_source?std::get_if<TaskClaimEstablishedRef>(&*n->accepted_source):nullptr;
 if(!t||!c||!n||!accepted||!same(accepted->source,ref.source)||accepted->source_schema!=1||n->accepted_tick!=0||!t->contract_id||*t->contract_id!=c->id||c->source_binding.claim_id!=s->claim_id||c->source_binding.contract_id!=c->id||c->source_binding.need_id!=n->need_id||n->contract_id!=c->id||n->accepted_amount<=0||n->accepted_amount!=c->frozen_terms.agreed_quantity||!same(c->assignee,n->assignee)||!c->executor_actor_id||*c->executor_actor_id!=t->assignee)return fail<const TaskClaimEstablishedSource*>("TaskClaimSourceBindingMismatch",ref.source.id);
 auto at=compare_world_moments(s->established_at,s->established_at);if(!at.ok)return {false,{},at.error};return {true,s,{}};
}
Status committed(const CommonStore&common,const TaskState&tasks,const TaskClaimEstablishedSource&s,const SourceRef&ref){
 auto result=read_common_command_source(common,ref);if(!result.ok)return {false,{},result.error};const auto&t=*tasks.tasks.find(s.task_id);const auto&r=*result.value.result;const auto*action=r.details?std::get_if<ActionStarted>(&r.details->data):nullptr;
 if(r.kind!="accept_offer"||result.value.history->actor_id!=t.assignee||r.tick!=s.established_at.half_day_tick||!action||action->action_id!=s.acceptance_action_id)return common_failure("TaskClaimActualCommittedResultMismatch",s.source_id,ErrorKind::internal);
 return common_success();
}
}
Outcome<const TaskClaimEstablishedSource*> read_task_claim_established_source(const CommonStore&common,const TaskState&tasks,const AffairsState&affairs,const TaskClaimEstablishedRef&ref,const TaskExactLifeSources*life){
 auto row=original_row(common,tasks,affairs,ref);if(!row.ok)return row;const auto&s=*row.value;
 auto execution=validate_task_claim_acceptance_execution(common,tasks,s,life);if(!execution.ok)return {false,{},execution.error};
 auto result=committed(common,tasks,s,s.committed_result_ref);return result.ok?row:Outcome<const TaskClaimEstablishedSource*>{false,{},result.error};
}
Outcome<WorldMoment> read_task_claim_established_moment(const CommonStore&common,const TaskState&tasks,const AffairsState&affairs,const TaskClaimEstablishedRef&ref){
 auto row=original_row(common,tasks,affairs,ref);if(!row.ok)return {false,{},row.error};
 auto applied=read_task_original_applied_time(common,tasks,ref.source);if(!applied.ok)return {false,{},applied.error};
 if(applied.value.applied_source.source_ref.domain!="tasks"||applied.value.applied_source.source_ref.kind!="task_action_started"||applied.value.applied_source.source_schema!=1)return fail<WorldMoment>("TaskClaimOriginalStartRequired",ref.source.id);
 auto started=read_task_exact_source_moment(common,tasks,applied.value.applied_source.source_ref,1);if(!started.ok)return {false,{},started.error};
 for(const auto*at:{&applied.value.occurred_at,&started.value}){auto equal=compare_world_moments(*at,row.value->established_at);if(!equal.ok)return {false,{},equal.error};if(equal.value)return fail<WorldMoment>("TaskClaimOriginalMomentMismatch",ref.source.id);}
 const auto*claim_header=common.state().facts.find(ref.source.id);const auto*start_header=common.state().facts.find(applied.value.applied_source.source_ref.id);
 if(!claim_header||!start_header)return fail<WorldMoment>("TaskClaimOriginalHeaderMissing",ref.source.id);
 auto claim_position=read_actual_common_fact_position(common,*claim_header);if(!claim_position.ok)return {false,{},claim_position.error};
 auto start_position=read_actual_common_fact_position(common,*start_header);if(!start_position.ok)return {false,{},start_position.error};
 if(claim_position.value>=start_position.value)return fail<WorldMoment>("TaskClaimStartCausalOrderMismatch",ref.source.id);
 return {true,row.value->established_at,{}};
}
// 建立任务承接原始源；分配标识或写入失败时传播原错误，不伪造来源。
Outcome<SourceRef>
establish_task_claim_source(CommonStore &common, TaskState &tasks,
                            AffairsState &affairs, const StableId &task_id,
                            const StableId &action_id, const WorldMoment &at) {
  if (!affairs.exact_claims || !tasks.exact_quantities ||
      !tasks.claim_establishment || !tasks.exact_execution)
    return fail<SourceRef>("TaskClaimSourceCapabilityMissing", task_id);
  auto canonical = compare_world_moments(at, at);
  if (!canonical.ok)
    return {false, {}, canonical.error};
  auto *t = tasks.tasks.find(task_id);
  auto *c =
      t && t->contract_id ? tasks.contracts.find(*t->contract_id) : nullptr;
  auto *n = c ? affairs.claims.find(c->source_binding.claim_id) : nullptr;
  const auto *a = common.action(action_id);
  const auto *parameters = tasks.actions.find(action_id);
  if (!t || !c || !n || !a || !parameters || parameters->task_id != task_id ||
      a->kind != "accept_offer" || a->actor_id != t->assignee ||
      t->status != "accepting")
    return fail<SourceRef>("TaskClaimEstablishmentNotApplied", task_id);
  if (n->accepted_source) {
    const auto *ref =
        std::get_if<TaskClaimEstablishedRef>(&*n->accepted_source);
    if (!ref)
      return fail<SourceRef>("TaskClaimSourceConflict", n->id);
    auto old = original_row(common, tasks, affairs, *ref);
    if (!old.ok)
      return {false, {}, old.error};
    auto equal = compare_world_moments(old.value->established_at, at);
    if (!equal.ok)
      return {false, {}, equal.error};
    if (equal.value || old.value->acceptance_action_id != action_id)
      return fail<SourceRef>("TaskClaimSourceConflict", n->id);
    if (!old.value->committed_result_ref.id.empty()) {
      auto published =
          read_task_claim_established_source(common, tasks, affairs, *ref);
      if (!published.ok)
        return {false, {}, published.error};
    }
    return {true, ref->source, {}};
  }
  for (const auto &row :
       tasks.claim_establishment->claim_establishment_sources.rows())
    if (row.second.claim_id == n->id || row.second.task_id == task_id ||
        row.second.acceptance_action_id == action_id)
      return fail<SourceRef>("DuplicateTaskClaimEstablishment", n->id);
  auto id = common.next_id("task_claim_established");
  if (!id.ok)
    return {false, {}, id.error};
  TaskClaimEstablishedSource source{id.value,  t->id, c->id, n->id,
                                    action_id, at,    {}};
  auto header =
      common.append_fact({id.value, "task_claim_established", "tasks"});
  if (!header.ok)
    return {false, {}, header.error};
  if (!tasks.claim_establishment->claim_establishment_sources.insert(
          id.value, std::move(source)))
    return fail<SourceRef>("DuplicateTaskClaimSource", id.value);
  SourceRef ref{"tasks", "task_claim_established", id.value};
  n->accepted_tick = 0;
  n->accepted_source = TaskClaimEstablishedRef{ref, 1};
  return {true, std::move(ref), {}};
}
Status bind_task_claim_established_result_in_candidate(const CommonStore&common,TaskState&tasks,const AffairsState&affairs,const StableId&action_id,const SourceRef&result,const TaskExactLifeSources*life){
 if(!tasks.claim_establishment||!affairs.exact_claims)return common_failure("TaskClaimSourceCapabilityMissing",action_id,ErrorKind::unsupported_capability);
 TaskClaimEstablishedSource*source=nullptr;for(auto&row:tasks.claim_establishment->claim_establishment_sources.rows())if(row.second.acceptance_action_id==action_id){if(source)return common_failure("DuplicateTaskClaimAcceptanceAction",action_id,ErrorKind::internal);source=&row.second;}
 if(!source)return common_failure("TaskClaimSourceMissing",action_id,ErrorKind::internal);
 auto row=original_row(common,tasks,affairs,{{"tasks","task_claim_established",source->source_id},1});if(!row.ok)return {false,{},row.error};auto execution=validate_task_claim_acceptance_execution(common,tasks,*source,life);if(!execution.ok)return execution;
 auto valid=committed(common,tasks,*source,result);if(!valid.ok)return valid;
 if((!source->committed_result_ref.domain.empty()||!source->committed_result_ref.kind.empty()||!source->committed_result_ref.id.empty())&&!same(source->committed_result_ref,result))return common_failure("TaskClaimCommittedResultConflict",source->source_id,ErrorKind::internal);
 for(const auto&other:tasks.claim_establishment->claim_establishment_sources.rows())if(&other.second!=source&&same(other.second.committed_result_ref,result))return common_failure("TaskClaimCommittedResultReused",source->source_id,ErrorKind::internal);
 source->committed_result_ref=result;auto full=read_task_claim_established_source(common,tasks,affairs,{{"tasks","task_claim_established",source->source_id},1},life);return full.ok?common_success():Status{false,{},full.error};
}
Outcome<WorldMoment> read_need_claim_acceptance_at(const CommonStore&common,const TaskState&tasks,const AffairsState&affairs,const StableId&id,const TaskExactLifeSources*life){
 const auto*c=affairs.claims.find(id);if(!c)return fail<WorldMoment>("NeedClaimMissing",id);if(!affairs.exact_claims)return {true,{c->accepted_tick,{}},{}};
 if(!c->accepted_source)return fail<WorldMoment>("NeedClaimAcceptanceSourceMissing",id);
 if(const auto*legacy=std::get_if<LegacyClaimAcceptedTick>(&*c->accepted_source)){if(legacy->accepted_tick<0||legacy->accepted_tick!=c->accepted_tick)return fail<WorldMoment>("InvalidLegacyClaimAcceptance",id);return {true,{legacy->accepted_tick,{}},{}};}
 auto source=read_task_claim_established_source(common,tasks,affairs,std::get<TaskClaimEstablishedRef>(*c->accepted_source),life);if(!source.ok)return {false,{},source.error};if(source.value->claim_id!=id)return fail<WorldMoment>("NeedClaimAcceptanceSourceMismatch",id);return {true,source.value->established_at,{}};
}
Status validate_task_claim_establishment(const CommonStore&common,const TaskState&tasks,const AffairsState&affairs,const TaskExactLifeSources*life){
 if(!affairs.exact_claims)return tasks.claim_establishment?common_failure("UnexpectedTaskClaimSources"):common_success();if(!tasks.claim_establishment)return common_failure("TaskClaimSourceCapabilityMissing",{},ErrorKind::unsupported_capability);
 std::set<StableId>used,actions,results;for(const auto&row:affairs.claims.rows()){
  auto at=read_need_claim_acceptance_at(common,tasks,affairs,row.first,life);if(!at.ok)return {false,{},at.error};if(const auto*ref=row.second.accepted_source?std::get_if<TaskClaimEstablishedRef>(&*row.second.accepted_source):nullptr){if(!used.insert(ref->source.id).second)return common_failure("TaskClaimSourceReused",ref->source.id);const auto*s=tasks.claim_establishment->claim_establishment_sources.find(ref->source.id);if(!actions.insert(s->acceptance_action_id).second||!results.insert(s->committed_result_ref.id).second)return common_failure("TaskClaimAcceptanceSourceReused",ref->source.id);}
 }
 if(used.size()!=tasks.claim_establishment->claim_establishment_sources.size())return common_failure("OrphanTaskClaimSource");
 for(const auto&row:common.state().facts.rows())if(row.second.kind=="task_claim_established"&&(row.second.domain!="tasks"||row.first!=row.second.id||!used.count(row.first)))return common_failure("OrphanTaskClaimSourceHeader",row.first);
 // A retained exact quantity acceptance cannot silently lose the original claim
 // source even after its Common Action has legitimately been released.
 if(tasks.exact_execution)for(const auto&row:tasks.exact_execution->starts.rows())if(row.second.action_kind=="accept_offer"){
  const auto*t=tasks.tasks.find(row.second.accepted_action.task_id);if(t&&t->contract_id&&!actions.count(row.second.action_id))return common_failure("TaskExactAcceptanceClaimSourceMissing",row.first);
 }
 return common_success();
}
}
