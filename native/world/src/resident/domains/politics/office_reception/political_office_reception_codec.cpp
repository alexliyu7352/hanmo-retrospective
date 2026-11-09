#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/politics/politics_projection_detail.hpp"
// JSON和独立二进制尾共用精确字段；客户端不能提供新接待人、事实编号或历史任职结果。
#include "political_office_reception_codec.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
#include "resident/domains/politics/takeover/politics_takeover_payload_guard.hpp"
namespace hanmo::resident::dynamic_economy_codec {
// 仅实际Common提交尾可写入此值，空值仅供writer与绑定之间的同候选窗口。
template<class A>void record(A& a,PoliticalSuccessionPlayerReceipt& v){a("actor_id",v.actor_id);a("result_id",v.result_id);a("request_sequence",v.request_sequence);a("result_revision",v.result_revision);}
// 原任职、前岗位与本次实际来源一并冻结，当前持有人不替代任何旧文书主体。
template<class A>void record(A& a,PoliticalOfficeReceptionAssignment& v){a("id",v.id);a("policy_id",v.policy_id);a("organization_id",v.organization_id);a("office_id",v.office_id);a("role_id",v.role_id);a("actor_id",v.actor_id);a("previous_actor_id",v.previous_actor_id);a("previous_assignment_fact_id",v.previous_assignment_fact_id);a("office_change_fact_id",v.office_change_fact_id);a("grant_id",v.grant_id);a("office_epoch",v.office_epoch);a("tick",v.tick);a("player_receipt",v.player_receipt);}
// 本段不向旧任何state尾追加字段。
template<class A>void record(A& a,PoliticalOfficeReceptionState& v){a("version",v.version);a("assignments",v.assignments);}
// 本人任职身份与期望前岗位是唯一客户端输入。
template<class A>void record(A& a,PoliticalAssumeOfficeReception& v){a("authority",v.authority);a("policy_id",v.policy_id);a("expected_role_actor_id",v.expected_role_actor_id);a("expected_assignment_fact_id",v.expected_assignment_fact_id);}
}
namespace hanmo::resident {namespace {
// 格式只核版本与有界条数，许可及永久结果必须另走完整冷门。
bool layout(const PoliticalOfficeReceptionState& s){return s.version==1&&s.assignments.size()<=4096;}
}
Status emit_political_office_reception_state_fields(ProjectionWriter& writer,const PoliticalOfficeReceptionState& s){
 try{return dynamic_economy_codec::emit_fields(writer,s);}
 catch(const boundary::DecodeFailure& e){return writer.reject(e.error.original.code,e.error.original.path);}
}
BoundaryValue encode_political_office_reception_state(const PoliticalOfficeReceptionState& s){return politics_projection_detail::encode_fields(s,emit_political_office_reception_state_fields);}
Outcome<PoliticalOfficeReceptionState> decode_political_office_reception_state(const BoundaryValue& value){auto unique=validate_political_takeover_payload(value);if(!unique.ok)return {false,{},unique.error};try{PoliticalOfficeReceptionState s;dynamic_economy_codec::decode(value,s,"/political_office_receptions");if(!layout(s))return {false,{},common_failure("InvalidPoliticalOfficeReceptionLayout").error};return {true,std::move(s),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status visit_political_office_reception_checkpoint(CodecVisitor& v,PoliticalOfficeReceptionState& s){if(!v.reading()&&!layout(s))return v.reject("InvalidPoliticalOfficeReceptionLayout");auto valid=dynamic_economy_codec::checkpoint(v,political_office_reception_domain,s);return !valid.ok?valid:layout(s)?common_success():v.reject("InvalidPoliticalOfficeReceptionLayout");}
BoundaryValue encode_political_office_reception_command(const PoliticalAssumeOfficeReception& c){return dynamic_economy_codec::encode(c);}
Outcome<PoliticalAssumeOfficeReception> decode_political_office_reception_command(const BoundaryValue& value){auto unique=validate_political_takeover_payload(value);if(!unique.ok)return {false,{},unique.error};try{PoliticalAssumeOfficeReception out;dynamic_economy_codec::decode(value,out,political_office_reception_command);return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
// 当前公共variant只有一个具名命令；其它kind仍严格拒绝。
StableId political_office_reception_command_kind(const PoliticalOfficeReceptionCommand&){return political_office_reception_command;}
bool is_political_office_reception_command(const StableId& kind){return kind==political_office_reception_command;}
Outcome<PoliticalOfficeReceptionCommand> decode_political_office_reception_command(const StableId& kind,const BoundaryValue& value){if(!is_political_office_reception_command(kind))return {false,{},common_failure("UnknownPoliticalOfficeReceptionCommand",kind).error};return decode_political_office_reception_command(value);}

}
