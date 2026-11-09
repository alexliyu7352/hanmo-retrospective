#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/politics/politics_projection_detail.hpp"
// 同一严格字段表访问JSON及独立二进制段；收到任意旧政治命令或额外payload字段均拒绝。
#include "political_head_handover_codec.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
#include "resident/domains/politics/takeover/politics_takeover_payload_guard.hpp"
namespace hanmo::resident::dynamic_economy_codec {
// 该结构只复用既有实际提交值形状；每个新模块仍在自己的独立尾保存本身的永久来源。
template<class A>void record(A& a,PoliticalSuccessionPlayerReceipt& v){a("actor_id",v.actor_id);a("result_id",v.result_id);a("request_sequence",v.request_sequence);a("result_revision",v.result_revision);}
// 原首任的有期限offer是让职意愿，不能在持久化时伪成空职或实际就任。
template<class A>void record(A& a,PoliticalHeadHandoverOffer& v){a("id",v.id);a("policy_id",v.policy_id);a("organization_id",v.organization_id);a("office_id",v.office_id);a("issuer_actor_id",v.issuer_actor_id);a("candidate_actor_id",v.candidate_actor_id);a("grant_id",v.grant_id);a("office_epoch",v.office_epoch);a("issued_tick",v.issued_tick);a("response_until",v.response_until);a("player_receipt",v.player_receipt);}
// 接受保存唯一原Office结果；拒绝相同字段必须为空/零，由完整本域史验证。
template<class A>void record(A& a,PoliticalHeadHandoverAnswer& v){a("id",v.id);a("offer_fact_id",v.offer_fact_id);a("actor_id",v.actor_id);a("office_change_fact_id",v.office_change_fact_id);a("tick",v.tick);a("expected_office_epoch",v.expected_office_epoch);a("installed_office_epoch",v.installed_office_epoch);a("accepted",v.accepted);a("player_receipt",v.player_receipt);}
// 撤回冻结当时原首任权限，不从今日新任职反推。
template<class A>void record(A& a,PoliticalHeadHandoverWithdrawal& v){a("id",v.id);a("offer_fact_id",v.offer_fact_id);a("actor_id",v.actor_id);a("grant_id",v.grant_id);a("office_epoch",v.office_epoch);a("tick",v.tick);a("player_receipt",v.player_receipt);}
template<class A>void record(A& a,PoliticalHeadHandoverState& v){a("version",v.version);a("offers",v.offers);a("answers",v.answers);a("withdrawals",v.withdrawals);}
// 命令仅包含本人选择和期望任职版本，tick/期限/Office结果不得由客户端传入。
template<class A>void record(A& a,PoliticalOfferHeadHandover& v){a("authority",v.authority);a("policy_id",v.policy_id);a("candidate_actor_id",v.candidate_actor_id);a("expected_office_epoch",v.expected_office_epoch);}
template<class A>void record(A& a,PoliticalAnswerHeadHandover& v){a("offer_fact_id",v.offer_fact_id);a("accept",v.accept);a("expected_office_epoch",v.expected_office_epoch);}
template<class A>void record(A& a,PoliticalWithdrawHeadHandover& v){a("authority",v.authority);a("offer_fact_id",v.offer_fact_id);}
}
namespace hanmo::resident {namespace {
// 格式门只验证有界形状；真实签署、控制史与结果绑定仍由完整业务冷验负责。
bool layout(const PoliticalHeadHandoverState& s){if(s.version!=1||s.offers.size()>4096||s.answers.size()>4096||s.withdrawals.size()>4096)return false;for(const auto& r:s.offers.rows())if(r.second.exact_issue)return false;for(const auto& r:s.answers.rows())if(r.second.exact_issue)return false;for(const auto& r:s.withdrawals.rows())if(r.second.exact_issue)return false;return true;}
}
Status emit_political_head_handover_state_fields(ProjectionWriter& writer,const PoliticalHeadHandoverState& s){
 if(!layout(s))return writer.reject("InvalidPoliticalHeadHandoverLayout");
 try{return dynamic_economy_codec::emit_fields(writer,s);}
 catch(const boundary::DecodeFailure& e){return writer.reject(e.error.original.code,e.error.original.path);}
}
BoundaryValue encode_political_head_handover_state(const PoliticalHeadHandoverState& s){return politics_projection_detail::encode_fields(s,emit_political_head_handover_state_fields);}
Outcome<PoliticalHeadHandoverState> decode_political_head_handover_state(const BoundaryValue& value){auto unique=validate_political_takeover_payload(value);if(!unique.ok)return {false,{},unique.error};try{PoliticalHeadHandoverState s;dynamic_economy_codec::decode(value,s,"/political_head_handovers");if(!layout(s))return {false,{},common_failure("InvalidPoliticalHeadHandoverLayout").error};return {true,std::move(s),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status visit_political_head_handover_checkpoint(CodecVisitor& v,PoliticalHeadHandoverState& s){if(!v.reading()&&!layout(s))return v.reject("InvalidPoliticalHeadHandoverLayout");auto status=dynamic_economy_codec::checkpoint(v,political_head_handover_domain,s);return !status.ok?status:(layout(s)?common_success():v.reject("InvalidPoliticalHeadHandoverLayout"));}
StableId political_head_handover_command_kind(const PoliticalHeadHandoverCommand& c){return std::visit([](const auto& v)->StableId{using T=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<T,PoliticalOfferHeadHandover>)return political_head_handover_offer_command;else if constexpr(std::is_same_v<T,PoliticalAnswerHeadHandover>)return political_head_handover_answer_command;else return political_head_handover_withdraw_command;},c);}
bool is_political_head_handover_command(const StableId& kind){return kind==political_head_handover_offer_command||kind==political_head_handover_answer_command||kind==political_head_handover_withdraw_command;}
BoundaryValue encode_political_head_handover_command(const PoliticalHeadHandoverCommand& c){return std::visit([](const auto& v){return dynamic_economy_codec::encode(v);},c);}
Outcome<PoliticalHeadHandoverCommand> decode_political_head_handover_command(const StableId& kind,const BoundaryValue& value){auto unique=validate_political_takeover_payload(value);if(!unique.ok)return {false,{},unique.error};try{PoliticalHeadHandoverCommand command;if(kind==political_head_handover_offer_command){PoliticalOfferHeadHandover c;dynamic_economy_codec::decode(value,c,kind);command=std::move(c);}else if(kind==political_head_handover_answer_command){PoliticalAnswerHeadHandover c;dynamic_economy_codec::decode(value,c,kind);command=std::move(c);}else if(kind==political_head_handover_withdraw_command){PoliticalWithdrawHeadHandover c;dynamic_economy_codec::decode(value,c,kind);command=std::move(c);}else return {false,{},common_failure("UnknownPoliticalHeadHandoverCommand",kind).error};return {true,std::move(command),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
}
