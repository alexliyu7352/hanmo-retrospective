// 成功分类沿原接受时条款解析；静态settlement只有原ID，不伪造或要求不存在的公共fact头。
#include "task_completion_evidence.hpp"
#include <set>
namespace hanmo::resident {
Outcome<std::vector<TaskCompletionEvidence>> task_completion_evidence_for_actor(const TaskState& state,const TaskCatalog& catalog,const StableId& actor){
    if(actor.empty())return {false,{},common_failure("InvalidTaskCompletionActor").error};
    std::vector<TaskCompletionEvidence> out;std::set<StableId> ids,settlements;
    for(const auto& row:state.tasks.rows()){
        const auto& task=row.second;if(task.assignee!=actor||task.status!="success")continue;
        const auto* definition=resolve_task_definition(state,catalog,task);
        if(!definition||(definition->template_id!="procurement"&&definition->template_id!="message"&&definition->template_id!="escort"))continue;
        if(row.first!=task.id||task.id.empty()||task.settlement_id.empty()||!task.reported_tick||*task.reported_tick<task.accepted_tick||task.accepted_tick<0||!ids.insert(task.id).second||!settlements.insert(task.settlement_id).second)return {false,{},common_failure("InvalidTaskCompletionEvidence",row.first,ErrorKind::internal).error};
        if(task.contract_id){
            const auto* contract=state.contracts.find(*task.contract_id);
            if(!contract||contract->id!=*task.contract_id||contract->current_task_id!=task.id||contract->executor_actor_id!=actor||contract->status!=ContractStatus::settled||contract->settlement_id!=task.settlement_id||!contract->execution_stopped||!contract->restitution_refs.empty()||definition->template_id!="procurement"||contract->frozen_terms.agreed_quantity<=0||contract->delivered_quantity!=contract->frozen_terms.agreed_quantity)return {false,{},common_failure("InvalidTaskCompletionContract",task.id,ErrorKind::internal).error};
        }
        out.push_back({task.id,definition->template_id,task.settlement_id,*task.reported_tick});
    }
    return {true,std::move(out),{}};
}
Outcome<std::array<std::optional<TaskCompletionEvidence>,2>> read_first_two_procurement_completion_evidence(const TaskState&state,const TaskCatalog&catalog,const StableId&actor){
 using Result=std::array<std::optional<TaskCompletionEvidence>,2>;if(actor.empty())return {false,{},common_failure("InvalidTaskCompletionActor").error};Result out;
 auto earlier=[](const TaskCompletionEvidence&a,const TaskCompletionEvidence&b){return a.reported_tick<b.reported_tick||(a.reported_tick==b.reported_tick&&a.task_id<b.task_id);};
 for(std::size_t i=0;i<state.tasks.rows().size();++i){const auto&row=state.tasks.rows()[i];const auto&t=row.second;if(t.assignee!=actor||t.status!="success")continue;const auto*d=resolve_task_definition(state,catalog,t);if(!d||d->template_id!="procurement")continue;
  if(row.first!=t.id||t.id.empty()||t.settlement_id.empty()||!t.reported_tick||*t.reported_tick<t.accepted_tick||t.accepted_tick<0)return {false,{},common_failure("InvalidTaskCompletionEvidence",row.first,ErrorKind::internal).error};
  if(t.contract_id){const auto*c=state.contracts.find(*t.contract_id);if(!c||c->id!=*t.contract_id||c->current_task_id!=t.id||c->executor_actor_id!=actor||c->status!=ContractStatus::settled||c->settlement_id!=t.settlement_id||!c->execution_stopped||!c->restitution_refs.empty()||c->frozen_terms.agreed_quantity<=0||c->delivered_quantity!=c->frozen_terms.agreed_quantity)return {false,{},common_failure("InvalidTaskCompletionContract",t.id,ErrorKind::internal).error};}
  // Cold coverage already guarantees table key uniqueness; compare original settlement aliases without a growing scratch set.
  for(std::size_t j=0;j<state.tasks.rows().size();++j){if(j==i)continue;const auto&other=state.tasks.rows()[j].second;if(other.assignee!=actor||other.status!="success"||other.settlement_id!=t.settlement_id)continue;const auto*other_d=resolve_task_definition(state,catalog,other);if(other_d&&(other_d->template_id=="procurement"||other_d->template_id=="message"||other_d->template_id=="escort"))return {false,{},common_failure("InvalidTaskCompletionEvidence",t.id,ErrorKind::internal).error};}
  TaskCompletionEvidence evidence{t.id,d->template_id,t.settlement_id,*t.reported_tick};if(!out[0]||earlier(evidence,*out[0])){out[1]=std::move(out[0]);out[0]=std::move(evidence);}else if(!out[1]||earlier(evidence,*out[1]))out[1]=std::move(evidence);
 }return {true,std::move(out),{}};
}

}
