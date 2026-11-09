// 注册前反查完整政治/Task来源；JSON参数不能自造一项使者权源或第二份履约账。
#include "resident/checkpoint/projection_record_archive.hpp"
#include "envoy_task_payloads.hpp"
#include <set>
namespace hanmo::resident {
Outcome<EnvoyTaskPayloadRegistry> envoy_task_payload_registry(const CommonStore& common,const TaskState& tasks,const EnvoyTaskState& envoy,const CommonCatalog& catalog,const DiplomaticMissionRead& missions,const DiplomacyPresentationRead& presentations,const DiplomaticEnvoyReportRead* reports){
    auto valid=reports?validate_envoy_tasks_with_reports(common,tasks,envoy,catalog,missions,presentations,*reports):validate_envoy_tasks(common,tasks,envoy,catalog,missions,presentations);if(!valid.ok)return {false,{},valid.error};EnvoyTaskPayloadRegistry result;std::set<StableId> ids;
    auto add=[&](const StableId& id,const StableId& kind){ids.insert(id);result.facts.push_back({id,kind,"tasks"});};
    for(const auto& row:envoy.envoy_contracts.rows()){const auto& g=row.second;const auto& a=g.acceptance;add(a.fact_id,"envoy_task_accepted");for(const auto& stop:g.interruptions)add(stop.fact_id,"envoy_handover_interrupted");if(g.delivery)add(g.delivery->fact_id,"envoy_task_delivered");if(g.completion)add(g.completion->fact_id,g.completion->result==EnvoyTaskResult::completed?"envoy_task_completed":g.completion->result==EnvoyTaskResult::cancelled?"envoy_task_cancelled":"envoy_task_failed");if(g.action)result.actions.push_back({g.action->action_id,g.action->kind==EnvoyTaskActionKind::deliver?"envoy_deliver":"envoy_report",row.first,a.contract_id,a.fact_id});}
    for(const auto& row:common.state().facts.rows()){const auto& kind=row.second.kind;if((kind=="envoy_task_accepted"||kind=="envoy_handover_interrupted"||kind=="envoy_task_delivered"||kind=="envoy_task_completed"||kind=="envoy_task_cancelled"||kind=="envoy_task_failed")&&!ids.count(row.first))return {false,{},common_failure("OrphanEnvoyFact",row.first).error};}
    return {true,std::move(result),{}};
}
// Common只有kind/domain及明确Task链接；工时、政治条款和接触对象不复制为可执行脚本。
// 原事实头和 Action 链接逐行借读；同一输出表保留原键与字段顺序。
Status emit_envoy_task_payloads_fields(ProjectionWriter& writer,const EnvoyTaskPayloadRegistry& registry){
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
BoundaryValue encode_envoy_task_payloads(const EnvoyTaskPayloadRegistry& value){
    BoundaryValue result;BoundaryProjectionWriter writer(result);
    auto status=writer.object("",[&](auto& fields){return emit_envoy_task_payloads_fields(fields,value);});
    if(status.ok)status=writer.finish();
    if(!status.ok){boundary::DecodeFailure failure(status.error.original.code,status.error.original.path);failure.error=status.error;throw failure;}
    return result;
}
Status validate_envoy_task_cold_payloads(const BoundaryValue& source,const EnvoyTaskPayloadRegistry& registry){try{
    auto expected=encode_envoy_task_payloads(registry);
    for(const auto& row:boundary::object(boundary::field(expected,"facts","envoy_payloads"),"facts")){const auto& actual=boundary::field(boundary::field(source,"facts","state"),row.first,"facts");if(!compare_boundary(actual,row.second).exact())return common_failure("EnvoyFactPayloadMismatch",row.first);}
    for(const auto& row:boundary::object(boundary::field(expected,"action_params","envoy_payloads"),"action_params")){const auto& actual=boundary::field(boundary::field(boundary::field(source,"actions","state"),row.first,"actions"),"params",row.first);if(!compare_boundary(actual,row.second).exact())return common_failure("EnvoyActionPayloadMismatch",row.first);}
    return common_success();
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
}
