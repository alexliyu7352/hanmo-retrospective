// 仅供明确task_offer_responses/2调用；旧v1不写存在位或新增字段，不能凭尾部猜版本。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "task_runtime_offer_response.hpp"
#include "resident/domains/interaction/interaction_codec.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include <set>
#include "task_projection.hpp"
#include "resident/domains/interaction/interaction_projection.hpp"
namespace hanmo::resident {
namespace {
// 新冷子载荷拒重复字段和超长集合，不能用第二个来源字段藏换绑值。
void unique_shape(const BoundaryValue& value,std::size_t depth=0){
    if(depth>CodecVisitor::max_depth)boundary::fail("RuntimeOfferResponseTooDeep","runtime_responses");
    if(const auto* fields=value.as<BoundaryValue::Dict>()){std::set<StableId> seen;for(const auto& field:*fields){if(!seen.insert(field.first).second)boundary::fail("DuplicateRuntimeOfferResponseField",field.first);unique_shape(field.second,depth+1);}}
    if(const auto* rows=value.as<BoundaryValue::Array>()){if(rows->size()>CodecVisitor::max_items)boundary::fail("RuntimeOfferResponsesTooLarge","runtime_responses");for(const auto& row:*rows)unique_shape(row,depth+1);}
}
}
Status visit_runtime_task_offer_responses(CodecVisitor& codec,RuntimeTaskOfferResponses& state){
    return codec.sequence("runtime_responses",state.responses,[](CodecVisitor& c,RuntimeTaskOfferResponse& row){
        c.field("actor_id",row.actor_id);c.field("offer_id",row.offer_id);c.field("need_id",row.need_id);c.field("affair_id",row.affair_id);c.field("terms_version",row.terms_version);c.field("session_id",row.session_id);c.field("need_revision",row.need_revision);c.field("response_tick",row.response_tick);
        c.object("context",[&](CodecVisitor& v){return interaction_codec::visit_context(v,row.context);});
        return c.object("source",[&](CodecVisitor& v){v.field("report_id",row.source.report_id);v.field("fact_id",row.source.fact_id);v.field("source_key",row.source.source_key);v.field("observation_tick",row.source.observation_tick);v.field("learned_tick",row.source.learned_tick);return v.field("local_public",row.source.local_public);});
    });
}
Outcome<RuntimeTaskOfferResponses> decode_runtime_task_offer_responses(const BoundaryValue& value){try{
    using namespace boundary;unique_shape(value);RuntimeTaskOfferResponses result;
    for(const auto& item:array(value,"runtime_responses")){
        fields(item,{"actor_id","offer_id","need_id","affair_id","terms_version","session_id","need_revision","response_tick","context","source"},{},"runtime_response");
        RuntimeTaskOfferResponse row;row.actor_id=get_string(item,"actor_id","runtime_response");row.offer_id=get_string(item,"offer_id","runtime_response");row.need_id=get_string(item,"need_id","runtime_response");row.affair_id=get_string(item,"affair_id","runtime_response");row.terms_version=get_string(item,"terms_version","runtime_response");row.session_id=get_string(item,"session_id","runtime_response");row.need_revision=get_int(item,"need_revision","runtime_response");row.response_tick=get_int(item,"response_tick","runtime_response");row.context=interaction_codec::decode_context(field(item,"context","runtime_response"),"runtime_response/context");
        const auto& source=field(item,"source","runtime_response");fields(source,{"report_id","fact_id","source_key","observation_tick","learned_tick","local_public"},{},"runtime_response/source");
        row.source.report_id=get_string(source,"report_id","runtime_response/source");row.source.fact_id=get_string(source,"fact_id","runtime_response/source");row.source.source_key=get_string(source,"source_key","runtime_response/source");row.source.observation_tick=get_int(source,"observation_tick","runtime_response/source");row.source.learned_tick=get_int(source,"learned_tick","runtime_response/source");row.source.local_public=get_bool(source,"local_public","runtime_response/source");result.responses.push_back(std::move(row));
    }
    return {true,std::move(result),{}};
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
// 原 runtime 数组可直接嵌外层字段；同一函数亦产生原独立 JSON 数组。
Status emit_runtime_task_offer_responses(ProjectionWriter& writer,std::string_view key,const RuntimeTaskOfferResponses& state){
    return writer.sequence(key,state.responses,[](auto& out,const auto& row){return out.object("",[&](auto& w){
        w.field("actor_id",row.actor_id);w.field("offer_id",row.offer_id);w.field("need_id",row.need_id);w.field("affair_id",row.affair_id);w.field("terms_version",row.terms_version);w.field("session_id",row.session_id);w.field("need_revision",row.need_revision);w.field("response_tick",row.response_tick);
        w.object("context",[&](auto& c){return emit_interaction_context_fields(c,row.context);});
        return w.object("source",[&](auto& c){c.field("report_id",row.source.report_id);c.field("fact_id",row.source.fact_id);c.field("source_key",row.source.source_key);c.field("observation_tick",row.source.observation_tick);c.field("learned_tick",row.source.learned_tick);return c.field("local_public",row.source.local_public);});
    });});
}
BoundaryValue encode_runtime_task_offer_responses(const RuntimeTaskOfferResponses& state){
    BoundaryValue value;BoundaryProjectionWriter writer(value);auto checked=emit_runtime_task_offer_responses(writer,"",state);
    if(checked.ok)checked=writer.finish();
    if(!checked.ok){boundary::DecodeFailure error(checked.error.original.code,checked.error.original.path);error.error=checked.error;throw error;}
    return value;
}
}
