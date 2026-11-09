// 每笔支付的角色、原容器与政治来源均显式持久化；未知或重复字段不能被宽松读取吞掉。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "task_public_account_state.hpp"
#include "task_dynamic_codec.hpp"
#include "resident/domains/tasks/task_dynamic_projection.hpp"
#include "resident/domains/tasks/task_projection.hpp"
namespace hanmo::resident {
namespace {
// JSON与二进制严格共序，枚举只覆盖已实现的动态合同三种现金腿。
template<class A>Status leg(A& c,TaskContractPaymentLeg& value){
    dynamic_codec::enumeration(c,"purpose",value.purpose,2);c.field("from_wallet_id",value.from_wallet_id);c.field("to_wallet_id",value.to_wallet_id);return c.field("amount",value.amount);
}
// 保存的是实际ResourceRecipe收据，读取不能从当前余额反造历史。
template<class A>Status payment(A& c,TaskContractPaymentSource& value){
    c.field("fact_id",value.fact_id);c.field("responsibility_id",value.responsibility_id);c.field("task_id",value.task_id);c.field("registration_fact_id",value.registration_fact_id);c.field("decision_fact_id",value.decision_fact_id);c.field("tick",value.tick);
    return c.sequence("legs",value.legs,[](auto& nested,auto& item){return leg(nested,item);});
}
// 静态履责没有动态合同ID；原Action与静态Task同ID责任独立保存，不借任意字符串升级来源。
template<class A>Status static_payment(A& c,TaskStaticPaymentSource& value){
    c.field("fact_id",value.fact_id);c.field("task_id",value.task_id);c.field("source_action_id",value.source_action_id);
    c.field("registration_fact_id",value.registration_fact_id);c.field("decision_fact_id",value.decision_fact_id);
    dynamic_codec::enumeration(c,"stage",value.stage,3);c.field("tick",value.tick);
    return c.sequence("legs",value.legs,[](auto& nested,auto& item){dynamic_codec::enumeration(nested,"purpose",item.purpose,3);nested.field("from_wallet_id",item.from_wallet_id);nested.field("to_wallet_id",item.to_wallet_id);return nested.field("amount",item.amount);});
}
// 独立版本段要求完整payments表，空表也不能被缺段代替。
template<class A>Status state(A& c,TaskPublicAccountExecutionState& value){
    c.field("version",value.version);if(value.version!=1&&value.version!=2)return c.reject("UnsupportedTaskTakeoverExecutionVersion");
    auto valid=dynamic_codec::table(c,"payments",value.payments,[](auto& nested,auto& row){return payment(nested,row);});if(!valid.ok)return valid;
    if(value.version==2)return dynamic_codec::table(c,"static_payments",value.static_payments,[](auto& nested,auto& row){return static_payment(nested,row);});
    if(!value.static_payments.empty())return c.reject("UnexpectedTaskStaticPaymentDomain");return c.status();
}
// 在first-match字段访问前拒绝重复键及超限嵌套。
Status unique_keys(const BoundaryValue& value,std::size_t depth=0){
    if(depth>CodecVisitor::max_depth)return common_failure("TaskPaymentPayloadTooDeep");
    if(const auto* fields=value.as<BoundaryValue::Dict>()){
        if(fields->size()>CodecVisitor::max_items)return common_failure("TaskPaymentPayloadTooLarge");
        std::set<StableId> seen;for(const auto& field:*fields){if(!seen.insert(field.first).second)return common_failure("DuplicateTaskPaymentField",field.first);auto valid=unique_keys(field.second,depth+1);if(!valid.ok)return valid;}
    }
    if(const auto* rows=value.as<BoundaryValue::Array>()){
        if(rows->size()>CodecVisitor::max_items)return common_failure("TaskPaymentPayloadTooLarge");
        for(const auto& row:*rows){auto valid=unique_keys(row,depth+1);if(!valid.ok)return valid;}
    }
    return common_success();
}
}
Status visit_task_public_account_execution_checkpoint(CodecVisitor& codec,TaskPublicAccountExecutionState& value){return state(codec,value);}
// 原整段字段表同时驱动 JSON 与有界 shape；这里只借只写引用，不复制派生索引。
Status emit_task_public_account_execution_fields(ProjectionWriter& writer,const TaskPublicAccountExecutionState& value){
    DynamicProjectionArchive archive(writer);return state(archive,const_cast<TaskPublicAccountExecutionState&>(value));
}
BoundaryValue encode_task_public_account_execution_state(const TaskPublicAccountExecutionState& value){
    BoundaryValue result;BoundaryProjectionWriter writer(result);
    auto checked=writer.object("",[&](auto& child){return emit_task_public_account_execution_fields(child,value);});
    if(checked.ok)checked=writer.finish();
    if(!checked.ok){boundary::DecodeFailure error(checked.error.original.code,checked.error.original.path);error.error=checked.error;throw error;}
    return result;
}
Outcome<TaskPublicAccountExecutionState> decode_task_public_account_execution_state(const BoundaryValue& value){
    auto valid=unique_keys(value);if(!valid.ok)return {false,{},valid.error};TaskPublicAccountExecutionState result;dynamic_codec::JsonArchive codec(value);valid=state(codec,result);if(valid.ok)valid=codec.finish();
    return valid.ok?Outcome<TaskPublicAccountExecutionState>{true,std::move(result),{}}:Outcome<TaskPublicAccountExecutionState>{false,{},valid.error};
}
}
