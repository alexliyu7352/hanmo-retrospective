#include "resident/domains/interaction/interaction_projection.hpp"
#include "growth_state_projection.hpp"
#include "growth_study_teaching_requests_codec.hpp"
#include "resident/domains/interaction/interaction_codec.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include <set>
namespace hanmo::resident {
// 仅显式所选条目构成本局目录，未用旧定义不被自动激活。
Outcome<GrowthStudyTeachingRequestCatalog> decode_growth_study_teaching_catalog(const BoundaryValue& definitions,const BoundaryValue& config,const CommonCatalog& common,const GrowthCatalog& growth,const InteractionCatalog& interaction){try{
    using namespace boundary;const StableId path="study_teaching_requests";
    fields(config,{"profile","request_ids"},{},path);
    if(get_string(config,"profile",path)!="authored_study_teaching_requests_v1")unsupported("UnsupportedStudyTeachingProfile",path+"/profile");
    const auto& selected=array(field(config,"request_ids",path),path+"/request_ids");
    if(selected.empty()||selected.size()>4096)fail("InvalidStudyTeachingCatalogSize",path+"/request_ids");
    object(definitions,path+"/definitions");std::set<StableId> seen;GrowthStudyTeachingRequestCatalog catalog;catalog.enabled=true;
    for(const auto& item:selected){
        const auto key=string(item,path+"/request_ids");if(key.empty()||key.size()>256)fail("InvalidStudyTeachingRequest",path+"/request_ids");if(!seen.insert(key).second)fail("DuplicateStudyTeachingRequest",key);
        const auto row_path=path+"/definitions/"+key;const auto& value=field(definitions,key,path+"/definitions");
        fields(value,{"id","teacher_actor_id","student_actor_id","skill_id","course_role_id","dialogue_id","node_id","duration","payment_policy","tariff"},{},row_path);
        GrowthStudyTeachingRequestDefinition row;row.id=get_string(value,"id",row_path);row.teacher_actor_id=get_string(value,"teacher_actor_id",row_path);row.student_actor_id=get_string(value,"student_actor_id",row_path);row.skill_id=get_string(value,"skill_id",row_path);row.course_role_id=get_string(value,"course_role_id",row_path);row.dialogue_id=get_string(value,"dialogue_id",row_path);row.node_id=get_string(value,"node_id",row_path);row.duration=get_int(value,"duration",row_path);row.payment_policy=get_string(value,"payment_policy",row_path);row.tariff=get_string(value,"tariff",row_path);
        if(!catalog.definitions.insert(key,std::move(row)))fail("DuplicateStudyTeachingRequest",key);
    }
    const auto valid=validate_growth_study_teaching_requests(catalog,common,growth,interaction);if(!valid.ok)return {false,{},valid.error};return {true,std::move(catalog),{}};
}catch(const boundary::DecodeFailure& failure){return {false,{},failure.error};}}

// 这里恢复永久声明的形状；外层必须核实际历史课程、原支付条款和人物。
Outcome<GrowthStudyTeachingAgreement> decode_growth_study_teaching_agreement(const BoundaryValue& value){try{
    using namespace boundary;const StableId path="study_teaching_agreement";
    fields(value,{"request_id","teacher_actor_id","student_actor_id","skill_id","course_role_id","dialogue_id","node_id","node_kind","session_id","payment_policy","tariff","request_context","duration","accepted_tick","accepted_revision"},{},path);
    GrowthStudyTeachingAgreement row;row.request_id=get_string(value,"request_id",path);row.teacher_actor_id=get_string(value,"teacher_actor_id",path);row.student_actor_id=get_string(value,"student_actor_id",path);row.skill_id=get_string(value,"skill_id",path);row.course_role_id=get_string(value,"course_role_id",path);row.dialogue_id=get_string(value,"dialogue_id",path);row.node_id=get_string(value,"node_id",path);row.node_kind=get_string(value,"node_kind",path);row.session_id=get_string(value,"session_id",path);row.payment_policy=get_string(value,"payment_policy",path);row.tariff=get_string(value,"tariff",path);row.request_context=interaction_codec::decode_context(field(value,"request_context",path),path+"/request_context");row.duration=get_int(value,"duration",path);row.accepted_tick=get_int(value,"accepted_tick",path);row.accepted_revision=get_int(value,"accepted_revision",path);row.field_order=order(value);return {true,std::move(row),{}};
}catch(const boundary::DecodeFailure& failure){return {false,{},failure.error};}}
Status emit_growth_study_teaching_agreement_fields(ProjectionWriter& out,const GrowthStudyTeachingAgreement& row){out.field("request_id",row.request_id);out.field("teacher_actor_id",row.teacher_actor_id);out.field("student_actor_id",row.student_actor_id);out.field("skill_id",row.skill_id);out.field("course_role_id",row.course_role_id);out.field("dialogue_id",row.dialogue_id);out.field("node_id",row.node_id);out.field("node_kind",row.node_kind);out.field("session_id",row.session_id);out.field("payment_policy",row.payment_policy);out.field("tariff",row.tariff);out.object("request_context",[&](auto& w){return emit_interaction_context_fields(w,row.request_context);});out.field("duration",row.duration);out.field("accepted_tick",row.accepted_tick);out.field("accepted_revision",row.accepted_revision);return out.order_fields(row.field_order);}
BoundaryValue encode_growth_study_teaching_agreement(const GrowthStudyTeachingAgreement& row){BoundaryValue value;BoundaryProjectionWriter out(value);auto status=out.object("",[&](auto& w){return emit_growth_study_teaching_agreement_fields(w,row);});if(status.ok)status=out.finish();if(!status.ok){boundary::DecodeFailure failure(status.error.original.code,status.error.original.path);failure.error=status.error;throw failure;}return value;}
// 仅新外层版本调用；旧Growth银行不写任何存在位。
Status visit_growth_study_teaching_agreement(CodecVisitor& c,GrowthStudyTeachingAgreement& row){
    Integer version=1;c.field("version",version);if(version!=1)return c.reject("UnsupportedStudyTeachingAgreementVersion");
    c.field("request_id",row.request_id);c.field("teacher_actor_id",row.teacher_actor_id);c.field("student_actor_id",row.student_actor_id);c.field("skill_id",row.skill_id);c.field("course_role_id",row.course_role_id);c.field("dialogue_id",row.dialogue_id);c.field("node_id",row.node_id);c.field("node_kind",row.node_kind);c.field("session_id",row.session_id);c.field("payment_policy",row.payment_policy);c.field("tariff",row.tariff);
    c.object("request_context",[&](CodecVisitor& v){return interaction_codec::visit_context(v,row.request_context);});c.field("duration",row.duration);c.field("accepted_tick",row.accepted_tick);c.field("accepted_revision",row.accepted_revision);return common_checkpoint::strings(c,"field_order",row.field_order);
}
}
