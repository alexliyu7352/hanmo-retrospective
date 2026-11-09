// 垫付和偿还载荷共用同一严格字段序；数字金额和原事实引用不从今日余额重造。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "task_personal_advance.hpp"
#include "task_dynamic_codec.hpp"
#include "resident/domains/tasks/task_dynamic_projection.hpp"
#include "resident/domains/tasks/task_projection.hpp"
namespace hanmo::resident {
namespace {
template<class A>Status policy(A& c,TaskPersonalAdvancePolicy& p){c.field("id",p.id);c.field("template_id",p.template_id);c.field("task_template_id",p.task_template_id);c.field("version",p.version);c.field("maximum_cash",p.maximum_cash);return c.field("repayment_policy",p.repayment_policy);}
template<class A>Status terms(A& c,TaskPersonalAdvanceTerms& t){
    c.field("contract_id",t.contract_id);c.field("task_id",t.task_id);c.field("claim_id",t.claim_id);c.field("terms_version",t.terms_version);
    c.object("policy",[&](auto& nested){return policy(nested,t.policy);});c.field("lender_actor_id",t.lender_actor_id);c.field("personal_wallet_id",t.personal_wallet_id);c.field("escrow_wallet_id",t.escrow_wallet_id);c.field("repayment_wallet_id",t.repayment_wallet_id);
    c.object("original_issuer",[&](auto& nested){return dynamic_codec::owner(nested,t.original_issuer);});return c.field("frozen_tick",t.frozen_tick);
}
template<class A>Status payment(A& c,TaskPersonalAdvancePayment& p){c.field("fact_id",p.fact_id);c.field("contract_id",p.contract_id);c.field("task_id",p.task_id);c.field("lender_actor_id",p.lender_actor_id);c.field("from_wallet_id",p.from_wallet_id);c.field("to_wallet_id",p.to_wallet_id);c.field("amount",p.amount);return c.field("tick",p.tick);}
template<class A>Status repayment(A& c,TaskPersonalAdvanceRepayment& p){
    c.field("fact_id",p.fact_id);c.field("contract_id",p.contract_id);c.field("task_id",p.task_id);c.field("lender_actor_id",p.lender_actor_id);c.field("to_wallet_id",p.to_wallet_id);c.field("escrow_wallet_id",p.escrow_wallet_id);c.field("issuer_wallet_id",p.issuer_wallet_id);
    dynamic_codec::strings(c,"payment_fact_ids",p.payment_fact_ids);c.field("amount",p.amount);c.field("from_escrow",p.from_escrow);c.field("from_issuer",p.from_issuer);return c.field("tick",p.tick);
}
template<class A>Status state(A& c,TaskPersonalAdvanceState& s){
    c.field("version",s.version);if(s.version!=1)return c.reject("UnsupportedTaskAdvanceVersion");
    dynamic_codec::table(c,"terms",s.terms,[](auto& row,auto& value){return terms(row,value);});
    dynamic_codec::table(c,"payments",s.payments,[](auto& row,auto& value){return payment(row,value);});
    return dynamic_codec::table(c,"repayments",s.repayments,[](auto& row,auto& value){return repayment(row,value);});
}
// first-match读取前拒重复字段与超限容器；坏档不得以重复后缀藏另一笔付款。
Status unique_keys(const BoundaryValue& value,std::size_t depth=0){
    if(depth>CodecVisitor::max_depth)return common_failure("TaskAdvancePayloadTooDeep");
    if(const auto* fields=value.as<BoundaryValue::Dict>()){
        if(fields->size()>CodecVisitor::max_items)return common_failure("TaskAdvancePayloadTooLarge");
        std::set<StableId> seen;for(const auto& field:*fields){if(!seen.insert(field.first).second)return common_failure("DuplicateTaskAdvanceField",field.first);auto valid=unique_keys(field.second,depth+1);if(!valid.ok)return valid;}
    }
    if(const auto* rows=value.as<BoundaryValue::Array>()){if(rows->size()>CodecVisitor::max_items)return common_failure("TaskAdvancePayloadTooLarge");for(const auto& row:*rows){auto valid=unique_keys(row,depth+1);if(!valid.ok)return valid;}}
    return common_success();
}
}
Status visit_task_personal_advance_checkpoint(CodecVisitor& codec,TaskPersonalAdvanceState& value){return state(codec,value);}
// 原整段字段表同时驱动 JSON 与有界 shape；这里只借只写引用，不复制派生索引。
Status emit_task_personal_advances_fields(ProjectionWriter& writer,const TaskPersonalAdvanceState& value){
    DynamicProjectionArchive archive(writer);return state(archive,const_cast<TaskPersonalAdvanceState&>(value));
}
BoundaryValue encode_task_personal_advance_state(const TaskPersonalAdvanceState& value){
    BoundaryValue result;BoundaryProjectionWriter writer(result);
    auto checked=writer.object("",[&](auto& child){return emit_task_personal_advances_fields(child,value);});
    if(checked.ok)checked=writer.finish();
    if(!checked.ok){boundary::DecodeFailure error(checked.error.original.code,checked.error.original.path);error.error=checked.error;throw error;}
    return result;
}
Outcome<TaskPersonalAdvanceState> decode_task_personal_advance_state(const BoundaryValue& value){
    auto valid=unique_keys(value);if(!valid.ok)return {false,{},valid.error};TaskPersonalAdvanceState result;dynamic_codec::JsonArchive codec(value);valid=state(codec,result);if(valid.ok)valid=codec.finish();
    return valid.ok?Outcome<TaskPersonalAdvanceState>{true,std::move(result),{}}:Outcome<TaskPersonalAdvanceState>{false,{},valid.error};
}
}
