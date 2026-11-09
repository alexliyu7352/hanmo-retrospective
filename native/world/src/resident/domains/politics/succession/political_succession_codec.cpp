#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/politics/politics_projection_detail.hpp"
// 同一访问表用于JSON和独立二进制尾；未知版本和额外字段不得降为原Office操作。
#include "political_succession_codec.hpp"
#include "resident/domains/politics/takeover/politics_takeover_payload_guard.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
namespace hanmo::resident::dynamic_economy_codec {
// 实际提交尾绑定原Common结果；不可由事实的相邻编号推算。
template<class A> void record(A& a,PoliticalSuccessionPlayerReceipt& v){a("actor_id",v.actor_id);a("result_id",v.result_id);a("request_sequence",v.request_sequence);a("result_revision",v.result_revision);}
// 原首任、Office和专项Grant冻结为生前签发来源。
template<class A> void record(A& a,PoliticalSuccessionNomination& v){a("id",v.id);a("policy_id",v.policy_id);a("organization_id",v.organization_id);a("office_id",v.office_id);a("issuer_actor_id",v.issuer_actor_id);a("candidate_actor_id",v.candidate_actor_id);a("grant_id",v.grant_id);a("office_epoch",v.office_epoch);a("issued_tick",v.issued_tick);a("response_until",v.response_until);a("player_receipt",v.player_receipt);}
// 本人后撤回保留原接受，不能删除已发生的意愿史。
template<class A> void record(A& a,PoliticalSuccessionAnswer& v){a("id",v.id);a("nomination_fact_id",v.nomination_fact_id);a("actor_id",v.actor_id);a("previous_acceptance_fact_id",v.previous_acceptance_fact_id);a("tick",v.tick);a("accepted",v.accepted);a("player_receipt",v.player_receipt);}
// 原首任撤回仍冻结原署权和epoch。
template<class A> void record(A& a,PoliticalSuccessionWithdrawal& v){a("id",v.id);a("nomination_fact_id",v.nomination_fact_id);a("actor_id",v.actor_id);a("grant_id",v.grant_id);a("office_epoch",v.office_epoch);a("tick",v.tick);a("player_receipt",v.player_receipt);}
// 正式就任只引用实际通知、死亡与原组织任职事实，不复制公库或玩家控制。
template<class A> void record(A& a,PoliticalSuccessionAccession& v){a("id",v.id);a("acceptance_fact_id",v.acceptance_fact_id);a("actor_id",v.actor_id);a("death_fact_id",v.death_fact_id);a("notice_binding_fact_id",v.notice_binding_fact_id);a("office_change_fact_id",v.office_change_fact_id);a("tick",v.tick);a("installed_office_epoch",v.installed_office_epoch);a("player_receipt",v.player_receipt);}
// 两分账共享一个显式版本尾，无新profile时不调用此访问器。
template<class A> void record(A& a,PoliticalSuccessionNominationState& v){a("version",v.version);a("nominations",v.nominations);a("answers",v.answers);a("withdrawals",v.withdrawals);}
template<class A> void record(A& a,PoliticalSuccessionAccessionState& v){a("version",v.version);a("accessions",v.accessions);}
template<class A> void record(A& a,PoliticalSuccessionState& v){a("version",v.version);a("nominations",v.nominations);a("accessions",v.accessions);}
// 本人只能选择已有业务源，所有时间、金额和Office变更由实际领域writer确定。
template<class A> void record(A& a,PoliticalNominateSuccessor& v){a("authority",v.authority);a("policy_id",v.policy_id);a("candidate_actor_id",v.candidate_actor_id);a("expected_office_epoch",v.expected_office_epoch);}
template<class A> void record(A& a,PoliticalAnswerSuccession& v){a("nomination_fact_id",v.nomination_fact_id);a("accept",v.accept);}
template<class A> void record(A& a,PoliticalWithdrawSuccession& v){a("authority",v.authority);a("nomination_fact_id",v.nomination_fact_id);}
template<class A> void record(A& a,PoliticalAccedeSuccession& v){a("acceptance_fact_id",v.acceptance_fact_id);a("death_fact_id",v.death_fact_id);a("death_notice_binding_fact_id",v.death_notice_binding_fact_id);a("expected_office_epoch",v.expected_office_epoch);}
}
namespace hanmo::resident {namespace {
// 格式门只负责有界形状；原生命、任职与实际结果必须随后完整冷验。
bool layout(const PoliticalSuccessionState& s){return s.version==1&&s.nominations.version==1&&s.accessions.version==1&&s.nominations.nominations.size()<=4096&&s.nominations.answers.size()<=8192&&s.nominations.withdrawals.size()<=4096&&s.accessions.accessions.size()<=4096;}
}
Status emit_political_succession_state_fields(ProjectionWriter& writer,const PoliticalSuccessionState& s){
 try{return dynamic_economy_codec::emit_fields(writer,s);}
 catch(const boundary::DecodeFailure& e){return writer.reject(e.error.original.code,e.error.original.path);}
}
BoundaryValue encode_political_succession_state(const PoliticalSuccessionState& s){return politics_projection_detail::encode_fields(s,emit_political_succession_state_fields);}
Outcome<PoliticalSuccessionState> decode_political_succession_state(const BoundaryValue& value){auto unique=validate_political_takeover_payload(value);if(!unique.ok)return {false,{},unique.error};try{PoliticalSuccessionState s;dynamic_economy_codec::decode(value,s,"/political_successions");if(!layout(s))return {false,{},common_failure("InvalidPoliticalSuccessionLayout").error};return {true,std::move(s),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status visit_political_succession_checkpoint(CodecVisitor& visitor,PoliticalSuccessionState& state){if(!visitor.reading()&&!layout(state))return visitor.reject("InvalidPoliticalSuccessionLayout");auto status=dynamic_economy_codec::checkpoint(visitor,political_succession_domain,state);return !status.ok?status:(layout(state)?common_success():visitor.reject("InvalidPoliticalSuccessionLayout"));}
StableId political_succession_command_kind(const PoliticalSuccessionCommand& c){return std::visit([](const auto& v)->StableId{using T=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<T,PoliticalNominateSuccessor>)return political_succession_nominate_command;else if constexpr(std::is_same_v<T,PoliticalAnswerSuccession>)return political_succession_answer_command;else if constexpr(std::is_same_v<T,PoliticalWithdrawSuccession>)return political_succession_withdraw_command;else return political_succession_accede_command;},c);}
bool is_political_succession_command(const StableId& k){return k==political_succession_nominate_command||k==political_succession_answer_command||k==political_succession_withdraw_command||k==political_succession_accede_command;}
BoundaryValue encode_political_succession_command(const PoliticalSuccessionCommand& c){return std::visit([](const auto& v){return dynamic_economy_codec::encode(v);},c);}
Outcome<PoliticalSuccessionCommand> decode_political_succession_command(const StableId& kind,const BoundaryValue& value){auto unique=validate_political_takeover_payload(value);if(!unique.ok)return {false,{},unique.error};try{PoliticalSuccessionCommand command;
 if(kind==political_succession_nominate_command){PoliticalNominateSuccessor v;dynamic_economy_codec::decode(value,v,kind);command=std::move(v);}else if(kind==political_succession_answer_command){PoliticalAnswerSuccession v;dynamic_economy_codec::decode(value,v,kind);command=std::move(v);}else if(kind==political_succession_withdraw_command){PoliticalWithdrawSuccession v;dynamic_economy_codec::decode(value,v,kind);command=std::move(v);}else if(kind==political_succession_accede_command){PoliticalAccedeSuccession v;dynamic_economy_codec::decode(value,v,kind);command=std::move(v);}else return {false,{},common_failure("UnknownPoliticalSuccessionCommand",kind).error};
 return {true,std::move(command),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
}
