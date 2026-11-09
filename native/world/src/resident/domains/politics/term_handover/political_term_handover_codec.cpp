#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/politics/politics_projection_detail.hpp"
// 同一严格字段表访问JSON及独立二进制段；收到任意旧政治命令或额外payload字段均拒绝。
#include "political_term_handover_codec.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
#include "resident/domains/politics/takeover/politics_takeover_payload_guard.hpp"
namespace hanmo::resident::dynamic_economy_codec {
// 该结构只复用既有实际提交值形状；每个新模块仍在自己的独立尾保存本身的永久来源。
template<class A>void record(A& a,PoliticalSuccessionPlayerReceipt& v){a("actor_id",v.actor_id);a("result_id",v.result_id);a("request_sequence",v.request_sequence);a("result_revision",v.result_revision);}
// 本任首职的有期限offer是让职意愿，不能在持久化时伪成空职或实际就任。
template<class A>void record(A& a,PoliticalTermHandoverOffer& v){a("id",v.id);a("policy_id",v.policy_id);a("organization_id",v.organization_id);a("office_id",v.office_id);a("issuer_actor_id",v.issuer_actor_id);a("candidate_actor_id",v.candidate_actor_id);a("grant_id",v.grant_id);a("head_source_fact_id",v.head_source_fact_id);a("head_office_change_fact_id",v.head_office_change_fact_id);a("office_epoch",v.office_epoch);a("issued_tick",v.issued_tick);a("response_until",v.response_until);a("player_receipt",v.player_receipt);}
// 接受保存唯一原Office结果；拒绝相同字段必须为空/零，由完整本域史验证。
template<class A>void record(A& a,PoliticalTermHandoverAnswer& v){a("id",v.id);a("offer_fact_id",v.offer_fact_id);a("actor_id",v.actor_id);a("office_change_fact_id",v.office_change_fact_id);a("tick",v.tick);a("expected_office_epoch",v.expected_office_epoch);a("installed_office_epoch",v.installed_office_epoch);a("accepted",v.accepted);a("player_receipt",v.player_receipt);}
// 撤回冻结当时本任首职权限，不从今日新任职反推。
template<class A>void record(A& a,PoliticalTermHandoverWithdrawal& v){a("id",v.id);a("offer_fact_id",v.offer_fact_id);a("actor_id",v.actor_id);a("grant_id",v.grant_id);a("office_epoch",v.office_epoch);a("tick",v.tick);a("player_receipt",v.player_receipt);}
template<class A>void record(A& a,PoliticalTermHandoverState& v){a("version",v.version);a("offers",v.offers);a("answers",v.answers);a("withdrawals",v.withdrawals);}
// 命令仅包含本人选择和期望任职版本，tick/期限/Office结果不得由客户端传入。
template<class A>void record(A& a,PoliticalOfferTermHandover& v){a("authority",v.authority);a("policy_id",v.policy_id);a("candidate_actor_id",v.candidate_actor_id);a("expected_head_source_fact_id",v.expected_head_source_fact_id);a("expected_office_epoch",v.expected_office_epoch);}
template<class A>void record(A& a,PoliticalAnswerTermHandover& v){a("offer_fact_id",v.offer_fact_id);a("accept",v.accept);a("expected_office_epoch",v.expected_office_epoch);}
template<class A>void record(A& a,PoliticalWithdrawTermHandover& v){a("authority",v.authority);a("offer_fact_id",v.offer_fact_id);}
}
namespace hanmo::resident {namespace {
// 格式门只验证有界形状；真实签署、控制史与结果绑定仍由完整业务冷验负责。
bool layout(const PoliticalTermHandoverState& s){return s.version==1&&s.offers.size()<=4096&&s.answers.size()<=4096&&s.withdrawals.size()<=4096;}
}
Status emit_political_term_handover_state_fields(ProjectionWriter& writer,const PoliticalTermHandoverState& s){
 try{return dynamic_economy_codec::emit_fields(writer,s);}
 catch(const boundary::DecodeFailure& e){return writer.reject(e.error.original.code,e.error.original.path);}
}
BoundaryValue encode_political_term_handover_state(const PoliticalTermHandoverState& s){return politics_projection_detail::encode_fields(s,emit_political_term_handover_state_fields);}
Outcome<PoliticalTermHandoverState> decode_political_term_handover_state(const BoundaryValue& value){auto unique=validate_political_takeover_payload(value);if(!unique.ok)return {false,{},unique.error};try{PoliticalTermHandoverState s;dynamic_economy_codec::decode(value,s,"/political_term_handovers");if(!layout(s))return {false,{},common_failure("InvalidPoliticalTermHandoverLayout").error};return {true,std::move(s),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status visit_political_term_handover_checkpoint(CodecVisitor& v,PoliticalTermHandoverState& s){if(!v.reading()&&!layout(s))return v.reject("InvalidPoliticalTermHandoverLayout");auto status=dynamic_economy_codec::checkpoint(v,political_term_handover_domain,s);return !status.ok?status:(layout(s)?common_success():v.reject("InvalidPoliticalTermHandoverLayout"));}
StableId political_term_handover_command_kind(const PoliticalTermHandoverCommand& c){return std::visit([](const auto& v)->StableId{using T=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<T,PoliticalOfferTermHandover>)return political_term_handover_offer_command;else if constexpr(std::is_same_v<T,PoliticalAnswerTermHandover>)return political_term_handover_answer_command;else return political_term_handover_withdraw_command;},c);}
bool is_political_term_handover_command(const StableId& kind){return kind==political_term_handover_offer_command||kind==political_term_handover_answer_command||kind==political_term_handover_withdraw_command;}
BoundaryValue encode_political_term_handover_command(const PoliticalTermHandoverCommand& c){return std::visit([](const auto& v){return dynamic_economy_codec::encode(v);},c);}
Outcome<PoliticalTermHandoverCommand> decode_political_term_handover_command(const StableId& kind,const BoundaryValue& value){auto unique=validate_political_takeover_payload(value);if(!unique.ok)return {false,{},unique.error};try{PoliticalTermHandoverCommand command;if(kind==political_term_handover_offer_command){PoliticalOfferTermHandover c;dynamic_economy_codec::decode(value,c,kind);command=std::move(c);}else if(kind==political_term_handover_answer_command){PoliticalAnswerTermHandover c;dynamic_economy_codec::decode(value,c,kind);command=std::move(c);}else if(kind==political_term_handover_withdraw_command){PoliticalWithdrawTermHandover c;dynamic_economy_codec::decode(value,c,kind);command=std::move(c);}else return {false,{},common_failure("UnknownPoliticalTermHandoverCommand",kind).error};return {true,std::move(command),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
}
