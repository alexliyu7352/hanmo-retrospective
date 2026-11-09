// 独立显式段保留旧政治任命字节，直接BoundaryValue也先拒绝递归重复键。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "political_field_appointments.hpp"
#include "resident/domains/politics/politics_projection_detail.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
#include "resident/domains/politics/takeover/politics_takeover_payload_guard.hpp"
namespace hanmo::resident::dynamic_economy_codec {
template<>struct EnumNames<MilitaryFieldCommandKind>{static std::vector<std::pair<MilitaryFieldCommandKind,StableId>> rows(){return {{MilitaryFieldCommandKind::unit,"unit"},{MilitaryFieldCommandKind::army,"army"}};}};
template<>struct EnumNames<MilitaryFieldCommandSourceKind>{static std::vector<std::pair<MilitaryFieldCommandSourceKind,StableId>> rows(){return {{MilitaryFieldCommandSourceKind::unit_authored,"unit_authored"},{MilitaryFieldCommandSourceKind::army_formation,"army_formation"},{MilitaryFieldCommandSourceKind::garrison_assignment,"garrison_assignment"},{MilitaryFieldCommandSourceKind::field_assignment,"field_assignment"}};}};
// 字段顺序保持旧协议；借原authority，不在编码/形状读取中复制再回写。
struct FieldAppointmentAuthorityWire{ActingAuthority& value;};
template<class A>void record(A& a,FieldAppointmentAuthorityWire& w){auto& v=w.value;a("organization_id",v.organization_id);a("office_id",v.office_id);a("authority_epoch",v.authority_epoch);a("actor_id",v.actor_id);}
template<class A>void record(A& a,MilitaryFieldCommandSource& v){a("kind",v.kind);a("id",v.id);}
template<class A>void record(A& a,MilitaryFieldCommandDeath& v){a("actor_id",v.actor_id);a("casualty_fact_id",v.casualty_fact_id);a("battle_id",v.battle_id);a("tick",v.tick);}
template<class A>void record(A& a,MilitaryFieldCommandTarget& v){a("kind",v.kind);a("organization_id",v.organization_id);a("army_id",v.army_id);a("campaign_id",v.campaign_id);a("formation_source_id",v.formation_source_id);a("unit_id",v.unit_id);a("previous_actor_id",v.previous_actor_id);a("next_actor_id",v.next_actor_id);a("previous_source",v.previous_source);a("candidate_unit_source",v.candidate_unit_source);a("predecessor_death",v.predecessor_death);a("fleet_id",v.fleet_id);}
template<class A>void record(A& a,PoliticalFieldAppointmentPolicy& v){a("id",v.id);a("organization_id",v.organization_id);a("origin_garrison_id",v.origin_garrison_id);a("origin_site_id",v.origin_site_id);a("response_ticks",v.response_ticks);a("arrival_ticks",v.arrival_ticks);a("allow_unit",v.allow_unit);a("allow_army",v.allow_army);}
template<class A>void record(A& a,PoliticalFieldAppointmentCatalog& v){a("version",v.version);a("enabled",v.enabled);a("policies",v.policies);}
template<class A>void record(A& a,PoliticalFieldAuthorization& v){a("id",v.id);a("policy_id",v.policy_id);a("grant_id",v.grant_id);FieldAppointmentAuthorityWire w{v.authority};a("authority",w);a("target",v.target);a("tick",v.tick);a("answer_before_tick",v.answer_before_tick);}
template<class A>void record(A& a,PoliticalFieldAnswer& v){a("id",v.id);a("authorization_fact_id",v.authorization_fact_id);a("actor_id",v.actor_id);a("accepted",v.accepted);a("witness_actor_ids",v.witness_actor_ids);a("tick",v.tick);a("apply_before_tick",v.apply_before_tick);}
template<class A>void record(A& a,PoliticalFieldWithdrawal& v){a("id",v.id);a("authorization_fact_id",v.authorization_fact_id);a("witness_actor_ids",v.witness_actor_ids);FieldAppointmentAuthorityWire w{v.authority};a("authority",w);a("grant_id",v.grant_id);a("tick",v.tick);}
template<class A>void record(A& a,PoliticalFieldAppointmentState& v){a("version",v.version);a("authorizations",v.authorizations);a("answers",v.answers);a("withdrawals",v.withdrawals);}
template<class A>void record(A& a,PoliticalFieldIssueCommand& v){FieldAppointmentAuthorityWire w{v.authority};a("authority",w);a("policy_id",v.policy_id);a("target",v.target);}
template<class A>void record(A& a,PoliticalFieldAnswerCommand& v){a("authorization_fact_id",v.authorization_fact_id);a("accept",v.accept);}
template<class A>void record(A& a,PoliticalFieldWithdrawCommand& v){FieldAppointmentAuthorityWire w{v.authority};a("authority",w);a("authorization_fact_id",v.authorization_fact_id);}
}
namespace hanmo::resident {
namespace {
// 所有字段必填，未知版本不回填空状态；命令也使用同一严格解码器。
template<class T>Outcome<T> decode(const BoundaryValue& value,const StableId& path){auto safe=validate_political_takeover_payload(value);if(!safe.ok)return {false,{},safe.error};try{T result;dynamic_economy_codec::decode(value,result,path);return {true,std::move(result),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
}
StableId political_field_appointment_command_kind(const PoliticalFieldAppointmentCommand& command){return std::visit([](const auto& v)->StableId{using T=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<T,PoliticalFieldIssueCommand>)return "political_field_appointment_issue";else if constexpr(std::is_same_v<T,PoliticalFieldAnswerCommand>)return "political_field_appointment_answer";else return "political_field_appointment_withdraw";},command);}
BoundaryValue encode_political_field_appointment_command(const PoliticalFieldAppointmentCommand& v){return std::visit([](const auto& x){return dynamic_economy_codec::encode(x);},v);}
Outcome<PoliticalFieldAppointmentCommand> decode_political_field_appointment_command(const StableId& kind,const BoundaryValue& value){
    if(kind=="political_field_appointment_issue"){auto r=decode<PoliticalFieldIssueCommand>(value,"/political_field_appointment_issue");return r.ok?Outcome<PoliticalFieldAppointmentCommand>{true,std::move(r.value),{}}:Outcome<PoliticalFieldAppointmentCommand>{false,{},r.error};}
    if(kind=="political_field_appointment_answer"){auto r=decode<PoliticalFieldAnswerCommand>(value,"/political_field_appointment_answer");return r.ok?Outcome<PoliticalFieldAppointmentCommand>{true,std::move(r.value),{}}:Outcome<PoliticalFieldAppointmentCommand>{false,{},r.error};}
    if(kind=="political_field_appointment_withdraw"){auto r=decode<PoliticalFieldWithdrawCommand>(value,"/political_field_appointment_withdraw");return r.ok?Outcome<PoliticalFieldAppointmentCommand>{true,std::move(r.value),{}}:Outcome<PoliticalFieldAppointmentCommand>{false,{},r.error};}
    return {false,{},common_failure("UnknownPoliticalFieldAppointmentCommand").error};
}
// 只借原record字段表输出；版本与来源准入仍由原成对codec及业务校验负责。
Status emit_political_field_catalog_fields(ProjectionWriter& writer,const PoliticalFieldAppointmentCatalog& value){
 try{return dynamic_economy_codec::emit_fields(writer,value);}
 catch(const boundary::DecodeFailure& error){return writer.reject(error.error.original.code,error.error.original.path);}
}
BoundaryValue encode_political_field_catalog(const PoliticalFieldAppointmentCatalog& value){return politics_projection_detail::encode_fields(value,emit_political_field_catalog_fields);}
Outcome<PoliticalFieldAppointmentCatalog> decode_political_field_catalog(const BoundaryValue& v){auto r=decode<PoliticalFieldAppointmentCatalog>(v,"/political_field_appointments");if(r.ok&&r.value.version!=1)return {false,{},common_failure("UnsupportedPoliticalFieldAppointmentVersion").error};return r;}
// 只借原record字段表输出；版本与来源准入仍由原成对codec及业务校验负责。
Status emit_political_field_appointments_fields(ProjectionWriter& writer,const PoliticalFieldAppointmentState& value){
 try{return dynamic_economy_codec::emit_fields(writer,value);}
 catch(const boundary::DecodeFailure& error){return writer.reject(error.error.original.code,error.error.original.path);}
}
BoundaryValue encode_political_field_appointments(const PoliticalFieldAppointmentState& value){return politics_projection_detail::encode_fields(value,emit_political_field_appointments_fields);}
Outcome<PoliticalFieldAppointmentState> decode_political_field_appointments(const BoundaryValue& v){auto r=decode<PoliticalFieldAppointmentState>(v,"/political_field_appointments");if(r.ok&&r.value.version!=1)return {false,{},common_failure("UnsupportedPoliticalFieldAppointmentVersion").error};return r;}
Status visit_political_field_appointments_checkpoint(CodecVisitor& codec,PoliticalFieldAppointmentState& value){if(!codec.reading()&&value.version!=1)return codec.reject("UnsupportedPoliticalFieldAppointmentVersion");auto r=dynamic_economy_codec::checkpoint(codec,"political_field_appointments",value);if(!r.ok)return r;return value.version==1?common_success():codec.reject("UnsupportedPoliticalFieldAppointmentVersion");}
}
