// 公共注册消费具名typed引用；不存在记录不能被领域名或任务前缀冒充。
#include "resident/checkpoint/projection_record_archive.hpp"
#include "governance_task_payloads.hpp"
#include <set>
namespace hanmo::resident {
namespace {
// 这些kind只由任务域治理生命周期产生，不接受任意governance前缀。
bool task_fact_kind(const StableId& kind){return kind=="governance_task_accepted"||kind=="governance_work_completed"||kind=="governance_work_interrupted"||kind=="governance_task_completed"||kind=="governance_task_cancelled"||kind=="governance_task_failed";}
}
Outcome<GovernanceTaskPayloadRegistry> governance_task_payload_registry(const CommonStore& common,const TaskState& tasks,const GovernanceTaskState& state){
    auto valid=validate_governance_tasks(common,tasks,state);if(!valid.ok)return {false,{},valid.error};GovernanceTaskPayloadRegistry result;std::set<StableId> facts,actions;
    const auto add=[&](const StableId& id,const StableId& kind){facts.insert(id);result.facts.push_back({id,kind,"tasks"});};
    for(const auto& row:state.governance_contracts.rows()){
        const auto& contract=row.second;const auto& accepted=contract.acceptance;add(accepted.fact_id,"governance_task_accepted");
        if(contract.work)add(contract.work->fact_id,"governance_work_completed");
        if(contract.interruption)add(contract.interruption->fact_id,"governance_work_interrupted");
        if(contract.completion)add(contract.completion->fact_id,contract.completion->result==GovernanceTaskResult::completed?"governance_task_completed":contract.completion->result==GovernanceTaskResult::cancelled?"governance_task_cancelled":"governance_task_failed");
        if(common.action(contract.action_id)){actions.insert(contract.action_id);result.actions.push_back({contract.action_id,accepted.task_id,accepted.contract_id,accepted.fact_id});}
    }
    for(const auto& row:common.state().facts.rows())if(task_fact_kind(row.second.kind)&&!facts.count(row.first))return {false,{},common_failure("OrphanGovernanceFact",row.first).error};
    for(const auto& row:common.state().actions.rows())if(row.second.kind=="governance_appease"&&!actions.count(row.first))return {false,{},common_failure("OrphanGovernanceAction",row.first).error};
    return {true,std::move(result),{}};
}
// 同一来源只生成审计边界字段，任务运行不读取这些JSON对象。
// 原事实头和 Action 链接逐行借读；同一输出表保留原键与字段顺序。
Status emit_governance_task_payloads_fields(ProjectionWriter& writer,const GovernanceTaskPayloadRegistry& registry){
    writer.object("facts",[&](auto& facts){
        for(const auto& fact:registry.facts){
            facts.object(fact.id,[&](auto& row){row.field("kind",fact.kind);return row.field("domain",fact.domain);});
            if(!facts.status().ok)return facts.status();
        }
        return facts.status();
    });
    return writer.object("action_params",[&](auto& actions){
        for(const auto& action:registry.actions){
            actions.object(action.action_id,[&](auto& row){row.field("task_id",action.task_id);row.field("contract_id",action.contract_id);return row.field("acceptance_fact_id",action.acceptance_fact_id);});
            if(!actions.status().ok)return actions.status();
        }
        return actions.status();
    });
}
// 原 JSON 编码与无树形状出口调用同一字段表；失败保完整原错误。
BoundaryValue encode_governance_task_payloads(const GovernanceTaskPayloadRegistry& value){
    BoundaryValue result;BoundaryProjectionWriter writer(result);
    auto status=writer.object("",[&](auto& fields){return emit_governance_task_payloads_fields(fields,value);});
    if(status.ok)status=writer.finish();
    if(!status.ok){boundary::DecodeFailure failure(status.error.original.code,status.error.original.path);failure.error=status.error;throw failure;}
    return result;
}
Status validate_governance_task_cold_payloads(const BoundaryValue& source,const GovernanceTaskPayloadRegistry& registry){try{
    const auto expected=encode_governance_task_payloads(registry);
    for(const auto& row:boundary::object(boundary::field(expected,"facts","governance_payloads"),"facts")){
        const auto& actual=boundary::field(boundary::field(source,"facts","state"),row.first,"facts");if(!compare_boundary(actual,row.second).exact())return common_failure("GovernanceFactPayloadMismatch",row.first);
    }
    for(const auto& row:boundary::object(boundary::field(expected,"action_params","governance_payloads"),"action_params")){
        const auto& actual=boundary::field(boundary::field(boundary::field(source,"actions","state"),row.first,"actions"),"params",row.first);if(!compare_boundary(actual,row.second).exact())return common_failure("GovernanceActionPayloadMismatch",row.first);
    }
    return common_success();
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
}
