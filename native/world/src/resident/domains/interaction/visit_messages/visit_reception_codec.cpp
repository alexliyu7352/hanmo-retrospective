// 新段严格访问，不把可重建索引或临时世界epoch保存为跨档许可。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "visit_reception_state.hpp"
#include "resident/domains/tasks/task_dynamic_codec.hpp"
#include "resident/domains/tasks/task_dynamic_projection.hpp"
#include "resident/domains/interaction/interaction_source_projection.hpp"
namespace hanmo::resident {namespace {
template<class A>Status claim(A& a,VisitReceptionClaim& v){a.field("fact_id",v.fact_id);a.field("intent_id",v.intent_id);a.field("policy_id",v.policy_id);a.field("session_id",v.session_id);a.field("actor_id",v.actor_id);a.field("provider_actor_id",v.provider_actor_id);a.field("role_id",v.role_id);a.field("dialogue_id",v.dialogue_id);a.field("entry_node_id",v.entry_node_id);a.field("place_id",v.place_id);return a.field("tick",v.tick);}
template<class A>Status state(A& a,VisitReceptionState& v){a.field("version",v.version);if(v.version!=1)return a.reject("UnsupportedVisitReceptionVersion");return dynamic_codec::table(a,"claims",v.claims,[](auto& child,auto& row){return claim(child,row);});}
// 直接typed JSON边界也先拒绝递归重复字段，不依赖文本parser代守门。
Status unique(const BoundaryValue& value,std::size_t depth=0){
    if(depth>CodecVisitor::max_depth)return common_failure("VisitReceptionTooDeep");
    if(const auto* fields=value.as<BoundaryValue::Dict>()){if(fields->size()>CodecVisitor::max_items)return common_failure("VisitReceptionTooLarge");std::set<StableId> seen;for(const auto& row:*fields){if(!seen.insert(row.first).second)return common_failure("DuplicateVisitReceptionField",row.first);auto r=unique(row.second,depth+1);if(!r.ok)return r;}}
    if(const auto* rows=value.as<BoundaryValue::Array>()){if(rows->size()>CodecVisitor::max_items)return common_failure("VisitReceptionTooLarge");for(const auto& row:*rows){auto r=unique(row,depth+1);if(!r.ok)return r;}}return common_success();
}
}
void rebuild_visit_reception_indexes(VisitReceptionState& state){state.by_session.clear();for(const auto& row:state.claims.rows())state.by_session.emplace(row.second.session_id,row.first);}
Status visit_visit_reception_checkpoint(CodecVisitor& codec,VisitReceptionState& value){auto result=state(codec,value);if(result.ok&&codec.reading())rebuild_visit_reception_indexes(value);return result;}
// 原整段字段表同时驱动 JSON 与有界 shape；这里只借只写引用，不复制派生索引。
Status emit_visit_reception_claims_fields(ProjectionWriter& writer,const VisitReceptionState& value){
    DynamicProjectionArchive archive(writer);return state(archive,const_cast<VisitReceptionState&>(value));
}
BoundaryValue encode_visit_reception_claims(const VisitReceptionState& value){
    BoundaryValue result;BoundaryProjectionWriter writer(result);
    auto checked=writer.object("",[&](auto& child){return emit_visit_reception_claims_fields(child,value);});
    if(checked.ok)checked=writer.finish();
    if(!checked.ok){boundary::DecodeFailure error(checked.error.original.code,checked.error.original.path);error.error=checked.error;throw error;}
    return result;
}
Outcome<VisitReceptionState> decode_visit_reception_claims(const BoundaryValue& value){auto result=unique(value);if(!result.ok)return {false,{},result.error};VisitReceptionState output;dynamic_codec::JsonArchive archive(value);result=state(archive,output);if(result.ok)result=archive.finish();if(!result.ok)return {false,{},result.error};rebuild_visit_reception_indexes(output);return {true,std::move(output),{}};}
}
