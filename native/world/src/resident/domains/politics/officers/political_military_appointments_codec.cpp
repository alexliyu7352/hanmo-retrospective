// 独立显式段保留旧政治任命字节，直接BoundaryValue也先拒绝递归重复键。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "political_military_appointments.hpp"
#include "resident/domains/politics/politics_projection_detail.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
#include "resident/domains/politics/takeover/politics_takeover_payload_guard.hpp"
namespace hanmo::resident::dynamic_economy_codec {
template<>struct EnumNames<MilitaryUnitCommandSourceKind>{static std::vector<std::pair<MilitaryUnitCommandSourceKind,StableId>> rows(){return {{MilitaryUnitCommandSourceKind::authored,"authored"},{MilitaryUnitCommandSourceKind::army_formation,"army_formation"},{MilitaryUnitCommandSourceKind::officer_assignment,"officer_assignment"},{MilitaryUnitCommandSourceKind::field_assignment,"field_assignment"}};}};
// 字段顺序保持旧协议；借原authority，不在编码/形状读取中复制再回写。
struct AppointmentAuthorityWire{ActingAuthority& value;};
template<class A>void record(A& a,AppointmentAuthorityWire& w){auto& v=w.value;a("organization_id",v.organization_id);a("office_id",v.office_id);a("authority_epoch",v.authority_epoch);a("actor_id",v.actor_id);}
template<class A>void record(A& a,MilitaryUnitCommandSource& v){a("kind",v.kind);a("id",v.id);}
template<class A>void record(A& a,MilitaryOfficerAssignmentTarget& v){a("organization_id",v.organization_id);a("garrison_id",v.garrison_id);a("unit_id",v.unit_id);a("previous_actor_id",v.previous_actor_id);a("next_actor_id",v.next_actor_id);a("previous_source",v.previous_source);}
template<class A>void record(A& a,PoliticalMilitaryAppointmentPolicy& v){a("id",v.id);a("organization_id",v.organization_id);a("garrison_id",v.garrison_id);a("site_id",v.site_id);a("response_ticks",v.response_ticks);a("arrival_ticks",v.arrival_ticks);}
template<class A>void record(A& a,PoliticalMilitaryAppointmentCatalog& v){a("version",v.version);a("enabled",v.enabled);a("policies",v.policies);}
template<class A>void record(A& a,PoliticalMilitaryAuthorization& v){a("id",v.id);a("policy_id",v.policy_id);a("grant_id",v.grant_id);a("meeting_place_id",v.meeting_place_id);AppointmentAuthorityWire w{v.authority};a("authority",w);a("target",v.target);a("tick",v.tick);a("answer_before_tick",v.answer_before_tick);}
template<class A>void record(A& a,PoliticalMilitaryAnswer& v){a("id",v.id);a("authorization_fact_id",v.authorization_fact_id);a("actor_id",v.actor_id);a("accepted",v.accepted);a("witness_actor_ids",v.witness_actor_ids);a("tick",v.tick);a("apply_before_tick",v.apply_before_tick);}
template<class A>void record(A& a,PoliticalMilitaryWithdrawal& v){a("id",v.id);a("authorization_fact_id",v.authorization_fact_id);a("witness_actor_ids",v.witness_actor_ids);AppointmentAuthorityWire w{v.authority};a("authority",w);a("grant_id",v.grant_id);a("tick",v.tick);}
template<class A>void record(A& a,PoliticalMilitaryAppointmentState& v){a("version",v.version);a("authorizations",v.authorizations);a("answers",v.answers);a("withdrawals",v.withdrawals);}
template<class A>void record(A& a,PoliticalMilitaryIssueCommand& v){AppointmentAuthorityWire w{v.authority};a("authority",w);a("policy_id",v.policy_id);a("target",v.target);}
template<class A>void record(A& a,PoliticalMilitaryAnswerCommand& v){a("authorization_fact_id",v.authorization_fact_id);a("accept",v.accept);}
template<class A>void record(A& a,PoliticalMilitaryWithdrawCommand& v){AppointmentAuthorityWire w{v.authority};a("authority",w);a("authorization_fact_id",v.authorization_fact_id);}
}
namespace hanmo::resident {
namespace {
// 所有字段必填，未知版本不回填空状态；命令也使用同一严格解码器。
template<class T>Outcome<T> decode(const BoundaryValue& value,const StableId& path){auto safe=validate_political_takeover_payload(value);if(!safe.ok)return {false,{},safe.error};try{T result;dynamic_economy_codec::decode(value,result,path);return {true,std::move(result),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
}
StableId political_military_appointment_command_kind(const PoliticalMilitaryAppointmentCommand& command){return std::visit([](const auto& v)->StableId{using T=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<T,PoliticalMilitaryIssueCommand>)return "political_military_officer_issue";else if constexpr(std::is_same_v<T,PoliticalMilitaryAnswerCommand>)return "political_military_officer_answer";else return "political_military_officer_withdraw";},command);}
BoundaryValue encode_political_military_appointment_command(const PoliticalMilitaryAppointmentCommand& v){return std::visit([](const auto& x){return dynamic_economy_codec::encode(x);},v);}
Outcome<PoliticalMilitaryAppointmentCommand> decode_political_military_appointment_command(const StableId& kind,const BoundaryValue& value){
    if(kind=="political_military_officer_issue"){auto r=decode<PoliticalMilitaryIssueCommand>(value,"/political_military_officer_issue");return r.ok?Outcome<PoliticalMilitaryAppointmentCommand>{true,std::move(r.value),{}}:Outcome<PoliticalMilitaryAppointmentCommand>{false,{},r.error};}
    if(kind=="political_military_officer_answer"){auto r=decode<PoliticalMilitaryAnswerCommand>(value,"/political_military_officer_answer");return r.ok?Outcome<PoliticalMilitaryAppointmentCommand>{true,std::move(r.value),{}}:Outcome<PoliticalMilitaryAppointmentCommand>{false,{},r.error};}
    if(kind=="political_military_officer_withdraw"){auto r=decode<PoliticalMilitaryWithdrawCommand>(value,"/political_military_officer_withdraw");return r.ok?Outcome<PoliticalMilitaryAppointmentCommand>{true,std::move(r.value),{}}:Outcome<PoliticalMilitaryAppointmentCommand>{false,{},r.error};}
    return {false,{},common_failure("UnknownPoliticalMilitaryAppointmentCommand").error};
}
// 只借原record字段表输出；版本与来源准入仍由原成对codec及业务校验负责。
Status emit_political_military_appointment_catalog_fields(ProjectionWriter& writer,const PoliticalMilitaryAppointmentCatalog& value){
 try{return dynamic_economy_codec::emit_fields(writer,value);}
 catch(const boundary::DecodeFailure& error){return writer.reject(error.error.original.code,error.error.original.path);}
}
BoundaryValue encode_political_military_appointment_catalog(const PoliticalMilitaryAppointmentCatalog& value){return politics_projection_detail::encode_fields(value,emit_political_military_appointment_catalog_fields);}
Outcome<PoliticalMilitaryAppointmentCatalog> decode_political_military_appointment_catalog(const BoundaryValue& v){auto r=decode<PoliticalMilitaryAppointmentCatalog>(v,"/political_military_appointments");if(r.ok&&r.value.version!=1)return {false,{},common_failure("UnsupportedPoliticalMilitaryAppointmentVersion").error};return r;}
// 只借原record字段表输出；版本与来源准入仍由原成对codec及业务校验负责。
Status emit_political_military_appointments_fields(ProjectionWriter& writer,const PoliticalMilitaryAppointmentState& value){
 try{return dynamic_economy_codec::emit_fields(writer,value);}
 catch(const boundary::DecodeFailure& error){return writer.reject(error.error.original.code,error.error.original.path);}
}
BoundaryValue encode_political_military_appointments(const PoliticalMilitaryAppointmentState& value){return politics_projection_detail::encode_fields(value,emit_political_military_appointments_fields);}
// 旧入口不接收新来源；显式配对重载才允许field_assignment，旧0..2编码保持逐字节。
Outcome<PoliticalMilitaryAppointmentState> decode_political_military_appointments(const BoundaryValue& v){return decode_political_military_appointments(v,false);}
Outcome<PoliticalMilitaryAppointmentState> decode_political_military_appointments(const BoundaryValue& v,bool field_commands){auto r=decode<PoliticalMilitaryAppointmentState>(v,"/political_military_appointments");if(!r.ok)return r;if(r.value.version!=1)return {false,{},common_failure("UnsupportedPoliticalMilitaryAppointmentVersion").error};for(const auto& row:r.value.authorizations.rows()){const auto kind=static_cast<Integer>(row.second.target.previous_source.kind);if(kind<0||kind>(field_commands?3:2))return {false,{},common_failure("PoliticalMilitaryCommandSourceVersionRequired",row.first).error};}return r;}
Status visit_political_military_appointments_checkpoint(CodecVisitor& codec,PoliticalMilitaryAppointmentState& value){return visit_political_military_appointments_checkpoint(codec,value,false);}
Status visit_political_military_appointments_checkpoint(CodecVisitor& codec,PoliticalMilitaryAppointmentState& value,bool field_commands){
 auto shape=[&](){if(value.version!=1)return codec.reject("UnsupportedPoliticalMilitaryAppointmentVersion");for(const auto& row:value.authorizations.rows()){const auto kind=static_cast<Integer>(row.second.target.previous_source.kind);if(kind<0||kind>(field_commands?3:2))return codec.reject("PoliticalMilitaryCommandSourceVersionRequired");}return common_success();};
 if(!codec.reading()){auto valid=shape();if(!valid.ok)return valid;}auto r=dynamic_economy_codec::checkpoint(codec,"political_military_appointments",value);if(!r.ok)return r;return shape();
}
}
