#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/politics/politics_projection_detail.hpp"
// 三张只追加的真实来源表和永久提交字段共用JSON/二进制codec；旧格式不写本段空尾。
#include "political_office_resignation_codec.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
#include "resident/domains/politics/takeover/politics_takeover_payload_guard.hpp"
namespace hanmo::resident::dynamic_economy_codec {
template<class A>void record(A& a,PoliticalOfficeResignationAuthoredOrigin& v){a("intent_id",v.intent_id);a("director_policy_id",v.director_policy_id);a("actor_id",v.actor_id);a("request_fact_id",v.request_fact_id);a("reason",v.reason);}
template<class A>void record(A& a,PoliticalSuccessionPlayerReceipt& v){a("actor_id",v.actor_id);a("result_id",v.result_id);a("request_sequence",v.request_sequence);a("result_revision",v.result_revision);}
template<class A>void record(A& a,PoliticalOfficeResignationRequest& v){a("id",v.id);a("policy_id",v.policy_id);a("organization_id",v.organization_id);a("office_id",v.office_id);a("actor_id",v.actor_id);a("approver_office_id",v.approver_office_id);a("approver_actor_id",v.approver_actor_id);a("office_epoch",v.office_epoch);a("approver_epoch",v.approver_epoch);a("jurisdiction_revision",v.jurisdiction_revision);a("tick",v.tick);a("response_until",v.response_until);a("player_receipt",v.player_receipt);}
template<class A>void record(A& a,PoliticalOfficeResignationAnswer& v){a("id",v.id);a("request_fact_id",v.request_fact_id);a("actor_id",v.actor_id);a("grant_id",v.grant_id);a("office_change_fact_id",v.office_change_fact_id);a("approver_epoch",v.approver_epoch);a("vacant_office_epoch",v.vacant_office_epoch);a("tick",v.tick);a("accepted",v.accepted);a("player_receipt",v.player_receipt);}
template<class A>void record(A& a,PoliticalOfficeResignationWithdrawal& v){a("id",v.id);a("request_fact_id",v.request_fact_id);a("actor_id",v.actor_id);a("tick",v.tick);a("player_receipt",v.player_receipt);}
template<class A>void record(A& a,PoliticalOfficeResignationState& v){a("version",v.version);a("requests",v.requests);a("answers",v.answers);a("withdrawals",v.withdrawals);if(v.version==2)a("authored_origins",v.authored_origins);}
template<class A>void record(A& a,PoliticalRequestOfficeResignation& v){a("policy_id",v.policy_id);a("expected_office_epoch",v.expected_office_epoch);a("expected_jurisdiction_revision",v.expected_jurisdiction_revision);}
template<class A>void record(A& a,PoliticalAnswerOfficeResignation& v){a("authority",v.authority);a("request_fact_id",v.request_fact_id);a("accept",v.accept);}
template<class A>void record(A& a,PoliticalWithdrawOfficeResignation& v){a("request_fact_id",v.request_fact_id);}
}
namespace hanmo::resident {namespace {
// 格式门只验证有界形状；真实签署、控制史与结果绑定仍由完整业务冷验负责。
bool layout(const PoliticalOfficeResignationState& s){return (s.version==1||s.version==2)&&(s.version!=1||s.authored_origins.empty())&&s.authored_origins.size()<=4096&&s.requests.size()+s.answers.size()+s.withdrawals.size()<=4096;}
}
Status emit_political_office_resignation_state_fields(ProjectionWriter& writer,const PoliticalOfficeResignationState& s){
 try{return dynamic_economy_codec::emit_fields(writer,s);}
 catch(const boundary::DecodeFailure& e){return writer.reject(e.error.original.code,e.error.original.path);}
}
BoundaryValue encode_political_office_resignation_state(const PoliticalOfficeResignationState& s){return politics_projection_detail::encode_fields(s,emit_political_office_resignation_state_fields);}
Outcome<PoliticalOfficeResignationState> decode_political_office_resignation_state(const BoundaryValue& value){auto unique=validate_political_takeover_payload(value);if(!unique.ok)return {false,{},unique.error};try{PoliticalOfficeResignationState s;dynamic_economy_codec::decode(value,s,"/political_office_resignations");if(!layout(s))return {false,{},common_failure("InvalidPoliticalOfficeResignationLayout").error};return {true,std::move(s),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status visit_political_office_resignation_checkpoint(CodecVisitor& v,PoliticalOfficeResignationState& s){if(!v.reading()&&!layout(s))return v.reject("InvalidPoliticalOfficeResignationLayout");auto status=dynamic_economy_codec::checkpoint(v,political_office_resignation_module,s);return !status.ok?status:(layout(s)?common_success():v.reject("InvalidPoliticalOfficeResignationLayout"));}
StableId political_office_resignation_command_kind(const PoliticalOfficeResignationCommand& c){return std::visit([](const auto& v)->StableId{using T=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<T,PoliticalRequestOfficeResignation>)return political_office_resignation_request_command;else if constexpr(std::is_same_v<T,PoliticalAnswerOfficeResignation>)return political_office_resignation_answer_command;else return political_office_resignation_withdraw_command;},c);}
bool is_political_office_resignation_command(const StableId& kind){return kind==political_office_resignation_request_command||kind==political_office_resignation_answer_command||kind==political_office_resignation_withdraw_command;}
BoundaryValue encode_political_office_resignation_command(const PoliticalOfficeResignationCommand& c){return std::visit([](const auto& v){return dynamic_economy_codec::encode(v);},c);}
Outcome<PoliticalOfficeResignationCommand> decode_political_office_resignation_command(const StableId& kind,const BoundaryValue& value){auto unique=validate_political_takeover_payload(value);if(!unique.ok)return {false,{},unique.error};try{PoliticalOfficeResignationCommand command;if(kind==political_office_resignation_request_command){PoliticalRequestOfficeResignation c;dynamic_economy_codec::decode(value,c,kind);command=std::move(c);}else if(kind==political_office_resignation_answer_command){PoliticalAnswerOfficeResignation c;dynamic_economy_codec::decode(value,c,kind);command=std::move(c);}else if(kind==political_office_resignation_withdraw_command){PoliticalWithdrawOfficeResignation c;dynamic_economy_codec::decode(value,c,kind);command=std::move(c);}else return {false,{},common_failure("UnknownPoliticalOfficeResignationCommand",kind).error};return {true,std::move(command),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
}
