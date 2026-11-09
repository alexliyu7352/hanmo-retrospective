// 治理模块v1双冷边界同序遍历typed字段；旧任务section无新增字节或默认治理记录。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "governance_task_state.hpp"
#include "task_dynamic_codec.hpp"
namespace hanmo::resident {
namespace {
// 接受条款保留来源与版本，只保存资源/政治权威记录的ID。
template<class A>Status acceptance(A& c,GovernanceTaskAcceptance& a){
    c.field("fact_id",a.fact_id);c.field("task_id",a.task_id);c.field("contract_id",a.contract_id);c.field("authority_source_fact_id",a.authority_source_fact_id);c.field("organization_id",a.organization_id);c.field("executor_actor_id",a.executor_actor_id);c.field("jurisdiction_id",a.jurisdiction_id);c.field("site_id",a.site_id);c.field("rule_id",a.rule_id);c.field("rule_version",a.rule_version);c.field("relief_terms_id",a.relief_terms_id);c.field("reservation_id",a.reservation_id);c.field("display_name",a.display_name);dynamic_codec::enumeration(c,"kind",a.kind,0);c.field("accepted_revision",a.accepted_revision);c.field("accepted_tick",a.accepted_tick);c.field("work_ticks",a.work_ticks);return c.field("deadline_tick",a.deadline_tick);
}
// 工作事实记录原动作及完成时计划，不保存会随以后技能变化的重复结果值。
template<class A>Status work(A& c,GovernanceWorkCompletion& w){
    c.field("fact_id",w.fact_id);c.field("task_id",w.task_id);c.field("contract_id",w.contract_id);c.field("action_id",w.action_id);c.field("acceptance_fact_id",w.acceptance_fact_id);c.field("outcome_plan_id",w.outcome_plan_id);c.field("started_tick",w.started_tick);return c.field("completed_tick",w.completed_tick);
}
// 最终源保资源收据逐项引用，终止类型必须是本版已知枚举。
template<class A>Status completion(A& c,GovernanceTaskCompletion& f){
    c.field("fact_id",f.fact_id);c.field("task_id",f.task_id);c.field("contract_id",f.contract_id);c.field("action_id",f.action_id);c.field("acceptance_fact_id",f.acceptance_fact_id);c.field("work_completion_fact_id",f.work_completion_fact_id);c.field("outcome_plan_id",f.outcome_plan_id);dynamic_codec::strings(c,"resource_receipt_ids",f.resource_receipt_ids);dynamic_codec::enumeration(c,"result",f.result,2);c.field("completed_tick",f.completed_tick);return c.field("reason",f.reason);
}
// 中断只记历史已做工时，唯一剩余倒计时在终止前仍取Common Action。
template<class A>Status interruption(A& c,GovernanceWorkInterruption& f){c.field("fact_id",f.fact_id);c.field("task_id",f.task_id);c.field("contract_id",f.contract_id);c.field("action_id",f.action_id);c.field("acceptance_fact_id",f.acceptance_fact_id);c.field("reason",f.reason);c.field("stopped_tick",f.stopped_tick);c.field("completed_work_ticks",f.completed_work_ticks);return c.optional("cause_fact_id",f.cause_fact_id,[](auto& row,auto& value){return row.field("value",value);});}
// 表顺序保留，重复task ID由共同有序表解码拒绝。
template<class A>Status state(A& c,GovernanceTaskState& s){
    c.field("version",s.version);if(s.version!=1)return c.reject("UnsupportedGovernanceTaskVersion");
    return dynamic_codec::table(c,"governance_contracts",s.governance_contracts,[](auto& row,auto& value){row.object("acceptance",[&](auto& nested){return acceptance(nested,value.acceptance);});row.field("action_id",value.action_id);row.optional("work",value.work,[](auto& nested,auto& value){return work(nested,value);});row.optional("completion",value.completion,[](auto& nested,auto& value){return completion(nested,value);});row.optional("interruption",value.interruption,[](auto& nested,auto& value){return interruption(nested,value);});row.field("execution_note_code",value.execution_note_code);row.field("pending_code",value.pending_code);return row.field("last_settlement_attempt_tick",value.last_settlement_attempt_tick);});
}
// JSON入口先拒重复key，不能让first-match字段读取吞掉伪造的同名后缀。
Status unique_keys(const BoundaryValue& value,std::size_t depth=0){
    if(depth>CodecVisitor::max_depth)return common_failure("GovernancePayloadTooDeep");
    if(const auto* fields=value.as<BoundaryValue::Dict>()){std::set<StableId> seen;for(const auto& field:*fields){if(!seen.insert(field.first).second)return common_failure("DuplicateGovernanceField",field.first);auto valid=unique_keys(field.second,depth+1);if(!valid.ok)return valid;}}
    if(const auto* rows=value.as<BoundaryValue::Array>()){if(rows->size()>CodecVisitor::max_items)return common_failure("GovernancePayloadTooLarge");for(const auto& row:*rows){auto valid=unique_keys(row,depth+1);if(!valid.ok)return valid;}}return common_success();
}
}
Status visit_governance_task_checkpoint(CodecVisitor& c,GovernanceTaskState& s){return state(c,s);}
// 只写适配沿原 state visitor，DynamicProjectionArchive 不改源字段或复制整表。
Status emit_governance_task_state_fields(ProjectionWriter& writer,const GovernanceTaskState& value){
    DynamicProjectionArchive archive(writer);
    return state(archive,const_cast<GovernanceTaskState&>(value));
}
// 原 JSON 编码与无树形状出口调用同一字段表；失败保完整原错误。
BoundaryValue encode_governance_task_state(const GovernanceTaskState& value){
    BoundaryValue result;BoundaryProjectionWriter writer(result);
    auto status=writer.object("",[&](auto& fields){return emit_governance_task_state_fields(fields,value);});
    if(status.ok)status=writer.finish();
    if(!status.ok){boundary::DecodeFailure failure(status.error.original.code,status.error.original.path);failure.error=status.error;throw failure;}
    return result;
}
Outcome<GovernanceTaskState> decode_governance_task_state(const BoundaryValue& value){auto valid=unique_keys(value);if(!valid.ok)return {false,{},valid.error};GovernanceTaskState result;dynamic_codec::JsonArchive codec(value);valid=state(codec,result);if(valid.ok)valid=codec.finish();return valid.ok?Outcome<GovernanceTaskState>{true,std::move(result),{}}:Outcome<GovernanceTaskState>{false,{},valid.error};}
}
