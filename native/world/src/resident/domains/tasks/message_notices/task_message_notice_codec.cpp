// 新通知绑定段显式追加；来源字段完全借S4 codec，不把任意JSON内容升级为死亡证明。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "task_message_notice_state.hpp"
#include "resident/domains/tasks/task_dynamic_codec.hpp"
#include "resident/domains/tasks/task_dynamic_projection.hpp"
#include "resident/domains/tasks/task_projection.hpp"
namespace hanmo::resident { namespace {
Status source(CodecVisitor& c,WitnessedDeathNoticeSource& s){return c.object("source",[&](auto& child){return visit_witnessed_death_notice_source(child,s);});}
Status source(dynamic_codec::JsonArchive& c,WitnessedDeathNoticeSource& s){BoundaryValue::Dict fields;if(!c.reading())fields=*encode_witnessed_death_notice_source(s).as<BoundaryValue::Dict>();auto result=c.field("source",fields);if(result.ok&&c.reading()){auto decoded=decode_witnessed_death_notice_source(BoundaryValue{std::move(fields)});if(!decoded.ok)return c.reject(decoded.error.original.code);s=std::move(decoded.value);}return result;}
// 原 S4 source 同字段出口，整个 source object 只委托一次，不复制其字段或构 Boundary 树。
Status source(DynamicProjectionArchive& c,WitnessedDeathNoticeSource& value){return c.object("source",[&](auto& child){return child.foreign_record(value,decode_witnessed_death_notice_source,emit_witnessed_death_notice_source_fields);});}
template<class A>Status binding(A& c,TaskMessageNoticeBinding& b){c.field("fact_id",b.fact_id);c.field("task_id",b.task_id);c.field("offer_id",b.offer_id);c.field("accept_action_id",b.accept_action_id);c.field("carrier_actor_id",b.carrier_actor_id);c.field("source_lot_id",b.source_lot_id);c.field("carrier_inventory_id",b.carrier_inventory_id);c.field("tick",b.tick);return source(c,b.source);}
template<class A>Status state(A& c,TaskMessageNoticeState& s){c.field("version",s.version);if(s.version!=1)return c.reject("UnsupportedTaskMessageNoticeVersion");return dynamic_codec::table(c,"bindings",s.bindings,[](auto& child,auto& value){return binding(child,value);});}
// first-match读取前拒绝所有层次重键；来源自己的严格字段门继续执行。
Status unique(const BoundaryValue& v,std::size_t depth=0){if(depth>CodecVisitor::max_depth)return common_failure("TaskMessageNoticeTooDeep");if(const auto* fields=v.as<BoundaryValue::Dict>()){if(fields->size()>CodecVisitor::max_items)return common_failure("TaskMessageNoticeTooLarge");std::set<StableId> seen;for(const auto& p:*fields){if(!seen.insert(p.first).second)return common_failure("DuplicateTaskMessageNoticeField",p.first);auto r=unique(p.second,depth+1);if(!r.ok)return r;}}if(const auto* rows=v.as<BoundaryValue::Array>()){if(rows->size()>CodecVisitor::max_items)return common_failure("TaskMessageNoticeTooLarge");for(const auto& row:*rows){auto r=unique(row,depth+1);if(!r.ok)return r;}}return common_success();}
}
void rebuild_task_message_notice_index(TaskMessageNoticeState& s){s.task_to_binding.clear();for(const auto& row:s.bindings.rows())s.task_to_binding[row.second.task_id]=row.first;}
Status visit_task_message_notices_checkpoint(CodecVisitor& c,TaskMessageNoticeState& s){auto result=state(c,s);if(result.ok&&c.reading())rebuild_task_message_notice_index(s);return result;}
Status emit_task_message_notices_fields(ProjectionWriter& writer,const TaskMessageNoticeState& value){DynamicProjectionArchive archive(writer);return state(archive,const_cast<TaskMessageNoticeState&>(value));}
BoundaryValue encode_task_message_notices(const TaskMessageNoticeState& value){BoundaryValue result;BoundaryProjectionWriter writer(result);auto checked=writer.object("",[&](auto& child){return emit_task_message_notices_fields(child,value);});if(checked.ok)checked=writer.finish();if(!checked.ok){boundary::DecodeFailure error(checked.error.original.code,checked.error.original.path);error.error=checked.error;throw error;}return result;}
Outcome<TaskMessageNoticeState> decode_task_message_notices(const BoundaryValue& v){auto r=unique(v);if(!r.ok)return {false,{},r.error};TaskMessageNoticeState s;dynamic_codec::JsonArchive c(v);r=state(c,s);if(r.ok)r=c.finish();if(!r.ok)return {false,{},r.error};rebuild_task_message_notice_index(s);return {true,std::move(s),{}};}
}
