// 新延期独立段统一JSON与typed字段顺序；旧Tasks不添默认字段，派生索引不持久化。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "task_extension_state.hpp"
#include "resident/domains/tasks/task_dynamic_codec.hpp"
#include "resident/domains/tasks/task_dynamic_projection.hpp"
#include "resident/domains/tasks/task_projection.hpp"
namespace hanmo::resident {namespace {
template<class A>Status fact(A& a,TaskExtensionFact& f){a.field("fact_id",f.fact_id);a.field("policy_id",f.policy_id);a.field("task_id",f.task_id);a.field("offer_id",f.offer_id);a.field("actor_id",f.actor_id);a.field("issuer_actor_id",f.issuer_actor_id);a.field("session_id",f.session_id);a.field("place_id",f.place_id);a.field("role_id",f.role_id);a.field("dialogue_id",f.dialogue_id);a.field("node_id",f.node_id);a.field("accepted_tick",f.accepted_tick);a.field("granted_tick",f.granted_tick);a.field("original_deadline_tick",f.original_deadline_tick);return a.field("deadline_tick",f.deadline_tick);}
template<class A>Status state(A& a,TaskExtensionState& s){a.field("version",s.version);if(s.version!=1)return a.reject("UnsupportedTaskExtensionVersion");return dynamic_codec::table(a,"facts",s.facts,[](auto& c,auto& f){return fact(c,f);});}
// JSON任意层重键直接拒绝；不能由后一个同名字段覆盖真实同意或索引来源。
Status unique(const BoundaryValue& value,std::size_t depth=0){if(depth>CodecVisitor::max_depth)return common_failure("TaskExtensionTooDeep");if(const auto* dict=value.as<BoundaryValue::Dict>()){if(dict->size()>CodecVisitor::max_items)return common_failure("TaskExtensionTooLarge");std::set<StableId> keys;for(const auto& entry:*dict){if(!keys.insert(entry.first).second)return common_failure("DuplicateTaskExtensionField",entry.first);auto valid=unique(entry.second,depth+1);if(!valid.ok)return valid;}}if(const auto* array=value.as<BoundaryValue::Array>()){if(array->size()>CodecVisitor::max_items)return common_failure("TaskExtensionTooLarge");for(const auto& entry:*array){auto valid=unique(entry,depth+1);if(!valid.ok)return valid;}}return common_success();}
}
void rebuild_task_extension_index(TaskExtensionState& s){s.by_task.clear();for(const auto& row:s.facts.rows())s.by_task.emplace(row.second.task_id,row.first);}
Outcome<TaskExtensionState> decode_task_extensions(const BoundaryValue& input){auto valid=unique(input);if(!valid.ok)return {false,{},valid.error};TaskExtensionState result;dynamic_codec::JsonArchive archive(input);valid=state(archive,result);if(valid.ok)valid=archive.finish();if(!valid.ok)return {false,{},valid.error};rebuild_task_extension_index(result);return {true,std::move(result),{}};}
// 原整段字段表同时驱动 JSON 与有界 shape；这里只借只写引用，不复制派生索引。
Status emit_task_extensions_fields(ProjectionWriter& writer,const TaskExtensionState& value){
    DynamicProjectionArchive archive(writer);return state(archive,const_cast<TaskExtensionState&>(value));
}
BoundaryValue encode_task_extensions(const TaskExtensionState& value){
    BoundaryValue result;BoundaryProjectionWriter writer(result);
    auto checked=writer.object("",[&](auto& child){return emit_task_extensions_fields(child,value);});
    if(checked.ok)checked=writer.finish();
    if(!checked.ok){boundary::DecodeFailure error(checked.error.original.code,checked.error.original.path);error.error=checked.error;throw error;}
    return result;
}
Status visit_task_extensions_checkpoint(CodecVisitor& archive,TaskExtensionState& s){auto valid=state(archive,s);if(valid.ok&&archive.reading())rebuild_task_extension_index(s);return valid;}
}
