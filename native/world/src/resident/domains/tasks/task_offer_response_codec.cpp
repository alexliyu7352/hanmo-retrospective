// 响应使用独立v1冷域；既有tasks/interaction字段和旧profile序列保持原样。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "task_offer_response.hpp"
#include "resident/domains/interaction/interaction_codec.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "resident/storage/common_codec.hpp"
#include "task_projection.hpp"
#include "resident/domains/interaction/interaction_projection.hpp"
namespace hanmo::resident {
// 类型化遍历不存任意JSON；旧profile不调用此域，不能猜测过去已发生婉拒。
Status visit_task_offer_response_checkpoint(CodecVisitor& codec,TaskOfferResponseState& state){
    codec.field("version",state.version);if(state.version!=1&&state.version!=2)return codec.reject("UnsupportedTaskOfferResponseVersion");
    if(state.version==1&&state.runtime)return codec.reject("RuntimeOfferResponseRequiresV2");
    codec.sequence("responses",state.responses,[](auto& c,auto& row){c.field("actor_id",row.actor_id);c.field("offer_id",row.offer_id);c.field("response_tick",row.response_tick);c.field("discovered_tick",row.discovered_tick);c.object("context",[&](auto& value){return interaction_codec::visit_context(value,row.context);});c.field("source_kind",row.source.kind);c.field("source_id",row.source.id);return c.status();});
    if(state.version==2){if(codec.reading())state.runtime.emplace();if(!state.runtime)return codec.reject("MissingRuntimeOfferResponses");return visit_runtime_task_offer_responses(codec,*state.runtime);}
    return codec.status();
}
// 冷JSON只接受已注册字段；未知响应类型、版本和额外数据不能静默丢失。
Outcome<TaskOfferResponseState> decode_task_offer_response_state(const BoundaryValue& value){try{
    using namespace boundary;TaskOfferResponseState result;result.version=get_int(value,"version","task_offer_responses");if(result.version!=1&&result.version!=2)fail("UnsupportedTaskOfferResponseVersion","task_offer_responses/version");
    if(result.version==1)fields(value,{"version","responses"},{},"task_offer_responses");else fields(value,{"version","responses","runtime_responses"},{},"task_offer_responses");
    for(const auto& item:array(field(value,"responses","task_offer_responses"),"task_offer_responses/responses")){
        fields(item,{"actor_id","offer_id","response","response_tick","discovered_tick","context","source"},{},"task_offer_responses/response");if(get_string(item,"response","response")!="declined_for_now")fail("UnsupportedTaskOfferResponse","response/response");
        TaskOfferResponse row;row.actor_id=get_string(item,"actor_id","response");row.offer_id=get_string(item,"offer_id","response");row.response_tick=get_int(item,"response_tick","response");row.discovered_tick=get_int(item,"discovered_tick","response");row.context=interaction_codec::decode_context(field(item,"context","response"),"response/context");const auto& source=field(item,"source","response");fields(source,{"kind","id"},{},"response/source");row.source.kind=get_string(source,"kind","response/source");row.source.id=get_string(source,"id","response/source");result.responses.push_back(std::move(row));
    }
    if(result.version==2){auto runtime=decode_runtime_task_offer_responses(field(value,"runtime_responses","task_offer_responses"));if(!runtime.ok)return {false,{},runtime.error};result.runtime=std::move(runtime.value);}
    return {true,std::move(result),{}};
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
// 输出只包含响应和原来源引用；没有可花预算、NPC实时位置或伪造的任务终态。
// 两个显式版本复用原行字段；未选 runtime 的旧版本不隐式增加数组。
Status emit_task_offer_response_fields(ProjectionWriter& writer,const TaskOfferResponseState& state){
    if(state.version!=1&&state.version!=2)return writer.reject("UnsupportedTaskOfferResponseVersion");
    if(state.version==1&&state.runtime)return writer.reject("RuntimeOfferResponseRequiresV2");
    if(state.version==2&&!state.runtime)return writer.reject("MissingRuntimeOfferResponses");
    writer.field("version",state.version);
    writer.sequence("responses",state.responses,[](auto& out,const auto& row){return out.object("",[&](auto& w){
        w.field("actor_id",row.actor_id);w.field("offer_id",row.offer_id);w.text("response","declined_for_now");w.field("response_tick",row.response_tick);w.field("discovered_tick",row.discovered_tick);
        w.object("context",[&](auto& c){return emit_interaction_context_fields(c,row.context);});
        return w.object("source",[&](auto& c){c.field("kind",row.source.kind);return c.field("id",row.source.id);});
    });});
    if(state.version==2)return emit_runtime_task_offer_responses(writer,"runtime_responses",*state.runtime);
    return writer.status();
}
BoundaryValue encode_task_offer_response_state(const TaskOfferResponseState& state){
    BoundaryValue value;BoundaryProjectionWriter writer(value);auto checked=writer.object("",[&](auto& w){return emit_task_offer_response_fields(w,state);});
    if(checked.ok)checked=writer.finish();
    if(!checked.ok){boundary::DecodeFailure error(checked.error.original.code,checked.error.original.path);error.error=checked.error;throw error;}
    return value;
}
}
