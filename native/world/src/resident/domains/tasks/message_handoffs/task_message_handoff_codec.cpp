// 新文书交接段与旧Task字段完全分离；JSON/二进制同序严格访问，未知/重复字段明确拒绝。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "task_message_handoff_state.hpp"
#include "resident/domains/tasks/task_dynamic_codec.hpp"
#include "resident/domains/tasks/task_dynamic_projection.hpp"
#include "resident/domains/tasks/task_projection.hpp"
namespace hanmo::resident {
namespace {
template<class A>Status receipt(A& c,TaskMessageHandoff& value){
    c.field("fact_id",value.fact_id);c.field("task_id",value.task_id);c.field("offer_id",value.offer_id);c.field("source_action_id",value.source_action_id);
    c.object("original_issuer",[&](auto& nested){return dynamic_codec::owner(nested,value.original_issuer);});
    c.field("carrier_actor_id",value.carrier_actor_id);c.field("recipient_actor_id",value.recipient_actor_id);c.field("item_id",value.item_id);c.field("source_lot_id",value.source_lot_id);c.field("recipient_lot_id",value.recipient_lot_id);
    c.field("from_inventory_id",value.from_inventory_id);c.field("to_inventory_id",value.to_inventory_id);c.field("place_id",value.place_id);c.field("role_id",value.role_id);return c.field("tick",value.tick);
}
// 原v1 visitor到receipts结束；v2保持原返报尾，v3仅在显式私人接触pair下解释空岗位实交。旧1/2字节不变。
template<class A>Status report(A& c,TaskMessageReport& value){
    c.field("fact_id",value.fact_id);c.field("task_id",value.task_id);c.field("handoff_fact_id",value.handoff_fact_id);c.field("source_action_id",value.source_action_id);c.field("settlement_id",value.settlement_id);
    c.field("carrier_actor_id",value.carrier_actor_id);c.field("report_actor_id",value.report_actor_id);c.field("place_id",value.place_id);c.field("role_id",value.role_id);return c.field("tick",value.tick);
}
template<class A>Status state(A& c,TaskMessageHandoffState& value){
    c.field("version",value.version);if(value.version!=1&&value.version!=2&&value.version!=3&&value.version!=4)return c.reject("UnsupportedTaskMessageHandoffVersion");
    auto r=dynamic_codec::table(c,"receipts",value.receipts,[](auto& nested,auto& value){return receipt(nested,value);});if(!r.ok)return r;
    if(value.version==1){if(!value.reports.empty()||!value.reports_observed_by.empty())return c.reject("TaskMessageReportVersionRequired");return r;}
    return dynamic_codec::table(c,"reports",value.reports,[](auto& nested,auto& value){return report(nested,value);});
}
// 在first-match读取之前核每个层次的字段唯一，避免伪收件人藏在第二个同名键。
Status unique(const BoundaryValue& value,std::size_t depth=0){
    if(depth>CodecVisitor::max_depth)return common_failure("TaskMessageHandoffTooDeep");
    if(const auto* fields=value.as<BoundaryValue::Dict>()){if(fields->size()>CodecVisitor::max_items)return common_failure("TaskMessageHandoffTooLarge");std::set<StableId> seen;for(const auto& field:*fields){if(!seen.insert(field.first).second)return common_failure("DuplicateTaskMessageHandoffField",field.first);auto r=unique(field.second,depth+1);if(!r.ok)return r;}}
    if(const auto* rows=value.as<BoundaryValue::Array>()){if(rows->size()>CodecVisitor::max_items)return common_failure("TaskMessageHandoffTooLarge");for(const auto& row:*rows){auto r=unique(row,depth+1);if(!r.ok)return r;}}return common_success();
}
}
void rebuild_task_message_handoff_index(TaskMessageHandoffState& value){value.reports_observed_by.clear();for(const auto& row:value.reports.rows()){const auto& report=row.second;value.reports_observed_by[report.carrier_actor_id].push_back(row.first);if(report.report_actor_id!=report.carrier_actor_id)value.reports_observed_by[report.report_actor_id].push_back(row.first);}
    value.observed_by.clear();for(const auto& row:value.receipts.rows()){const auto& receipt=row.second;value.observed_by[receipt.carrier_actor_id].push_back(row.first);if(receipt.recipient_actor_id!=receipt.carrier_actor_id)value.observed_by[receipt.recipient_actor_id].push_back(row.first);}}
Status visit_task_message_handoffs_checkpoint(CodecVisitor& codec,TaskMessageHandoffState& value){auto r=state(codec,value);if(r.ok&&codec.reading())rebuild_task_message_handoff_index(value);return r;}
// 原整段字段表同时驱动 JSON 与有界 shape；这里只借只写引用，不复制派生索引。
Status emit_task_message_handoffs_fields(ProjectionWriter& writer,const TaskMessageHandoffState& value){
    DynamicProjectionArchive archive(writer);return state(archive,const_cast<TaskMessageHandoffState&>(value));
}
BoundaryValue encode_task_message_handoffs(const TaskMessageHandoffState& value){
    BoundaryValue result;BoundaryProjectionWriter writer(result);
    auto checked=writer.object("",[&](auto& child){return emit_task_message_handoffs_fields(child,value);});
    if(checked.ok)checked=writer.finish();
    if(!checked.ok){boundary::DecodeFailure error(checked.error.original.code,checked.error.original.path);error.error=checked.error;throw error;}
    return result;
}
Outcome<TaskMessageHandoffState> decode_task_message_handoffs(const BoundaryValue& value){auto r=unique(value);if(!r.ok)return {false,{},r.error};TaskMessageHandoffState out;dynamic_codec::JsonArchive archive(value);r=state(archive,out);if(r.ok)r=archive.finish();if(!r.ok)return {false,{},r.error};rebuild_task_message_handoff_index(out);return {true,std::move(out),{}};}
}
