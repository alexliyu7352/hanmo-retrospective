#include "resident/domains/interaction/interaction_projection.hpp"
#include "growth_state_projection.hpp"
#include "growth_teaching_requests_codec.hpp"
#include "resident/domains/interaction/interaction_codec.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include <set>
namespace hanmo::resident {
// 显式选择的请求才成为本局契约；未选的旧作者定义不被偷偷启用或升级。
Outcome<GrowthTeachingRequestCatalog> decode_growth_teaching_request_catalog(const BoundaryValue& definitions,const BoundaryValue& config,const CommonCatalog& common,const InteractionCatalog& interaction){try{
    using namespace boundary;const StableId path="teaching_requests";
    fields(config,{"profile","request_ids"},{},path);
    if(get_string(config,"profile",path)!="authored_teaching_requests_v1")unsupported("UnsupportedTeachingRequestsProfile",path+"/profile");
    const auto& selected=array(field(config,"request_ids",path),path+"/request_ids");
    if(selected.empty()||selected.size()>4096)fail("InvalidTeachingRequestCatalogSize",path+"/request_ids");
    object(definitions,path+"/definitions");std::set<StableId> seen;GrowthTeachingRequestCatalog catalog;catalog.enabled=true;
    for(const auto& item:selected){
        const auto key=string(item,path+"/request_ids");
        if(key.empty()||key.size()>256)fail("InvalidTeachingRequestDefinition",path+"/request_ids");
        if(!seen.insert(key).second)fail("DuplicateTeachingRequestDefinition",key);
        const auto row_path=path+"/definitions/"+key;const auto& value=field(definitions,key,path+"/definitions");
        fields(value,{"id","teacher_actor_id","student_actor_id","skill_id","dialogue_id","node_id"},{},row_path);
        GrowthTeachingRequestDefinition row{get_string(value,"id",row_path),get_string(value,"teacher_actor_id",row_path),get_string(value,"student_actor_id",row_path),get_string(value,"skill_id",row_path),get_string(value,"dialogue_id",row_path),get_string(value,"node_id",row_path)};
        if(!catalog.definitions.insert(key,std::move(row)))fail("DuplicateTeachingRequestDefinition",key);
    }
    const auto valid=validate_growth_teaching_request_catalog(catalog,common,interaction);if(!valid.ok)return {false,{},valid.error};return {true,std::move(catalog),{}};
}catch(const boundary::DecodeFailure& failure){return {false,{},failure.error};}}

// 这里只恢复永久来源；外层世界还须核请求、人物、训练事实与历史时刻的同源绑定。
Outcome<GrowthTeachingAgreement> decode_growth_teaching_agreement(const BoundaryValue& value){try{
    using namespace boundary;const StableId path="teaching_agreement";
    fields(value,{"request_id","teacher_actor_id","student_actor_id","skill_id","dialogue_id","node_id","node_kind","session_id","context","accepted_tick","accepted_revision"},{},path);
    GrowthTeachingAgreement row;row.request_id=get_string(value,"request_id",path);row.teacher_actor_id=get_string(value,"teacher_actor_id",path);row.student_actor_id=get_string(value,"student_actor_id",path);row.skill_id=get_string(value,"skill_id",path);row.dialogue_id=get_string(value,"dialogue_id",path);row.node_id=get_string(value,"node_id",path);row.node_kind=get_string(value,"node_kind",path);row.session_id=get_string(value,"session_id",path);
    row.context=interaction_codec::decode_context(field(value,"context",path),path+"/context");row.accepted_tick=get_int(value,"accepted_tick",path);row.accepted_revision=get_int(value,"accepted_revision",path);row.field_order=order(value);return {true,std::move(row),{}};
}catch(const boundary::DecodeFailure& failure){return {false,{},failure.error};}}

Status emit_growth_teaching_agreement_fields(ProjectionWriter& out,const GrowthTeachingAgreement& row){out.field("request_id",row.request_id);out.field("teacher_actor_id",row.teacher_actor_id);out.field("student_actor_id",row.student_actor_id);out.field("skill_id",row.skill_id);out.field("dialogue_id",row.dialogue_id);out.field("node_id",row.node_id);out.field("node_kind",row.node_kind);out.field("session_id",row.session_id);out.object("context",[&](auto& w){return emit_interaction_context_fields(w,row.context);});out.field("accepted_tick",row.accepted_tick);out.field("accepted_revision",row.accepted_revision);return out.order_fields(row.field_order);}
BoundaryValue encode_growth_teaching_agreement(const GrowthTeachingAgreement& row){BoundaryValue value;BoundaryProjectionWriter out(value);auto status=out.object("",[&](auto& w){return emit_growth_teaching_agreement_fields(w,row);});if(status.ok)status=out.finish();if(!status.ok){boundary::DecodeFailure failure(status.error.original.code,status.error.original.path);failure.error=status.error;throw failure;}return value;}

// 仅被新的显式外层版本调用；此处没有临时execution_epoch或旧profile的optional存在位。
Status visit_growth_teaching_agreement(CodecVisitor& c,GrowthTeachingAgreement& row){
    Integer version=1;c.field("version",version);if(version!=1)return c.reject("UnsupportedTeachingAgreementVersion");
    c.field("request_id",row.request_id);c.field("teacher_actor_id",row.teacher_actor_id);c.field("student_actor_id",row.student_actor_id);c.field("skill_id",row.skill_id);c.field("dialogue_id",row.dialogue_id);c.field("node_id",row.node_id);c.field("node_kind",row.node_kind);c.field("session_id",row.session_id);
    c.object("context",[&](CodecVisitor& v){return interaction_codec::visit_context(v,row.context);});c.field("accepted_tick",row.accepted_tick);c.field("accepted_revision",row.accepted_revision);return common_checkpoint::strings(c,"field_order",row.field_order);
}
}
