#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/politics/politics_projection_detail.hpp"
// 同一访问表用于JSON和独立二进制尾；未知版本和额外字段不得降为原Office操作。
#include "political_successor_round_codec.hpp"
#include "resident/domains/politics/takeover/politics_takeover_payload_guard.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
namespace hanmo::resident::dynamic_economy_codec {
// 实际提交尾绑定原Common结果；不可由事实的相邻编号推算。
template<class A> void record(A& a,PoliticalSuccessionPlayerReceipt& v){a("actor_id",v.actor_id);a("result_id",v.result_id);a("request_sequence",v.request_sequence);a("result_revision",v.result_revision);}
// 原首任、Office和专项Grant冻结为生前签发来源。
template<class A> void record(A& a,PoliticalSuccessorRoundNomination& v){a("id",v.id);a("policy_id",v.policy_id);a("organization_id",v.organization_id);a("office_id",v.office_id);a("issuer_actor_id",v.issuer_actor_id);a("candidate_actor_id",v.candidate_actor_id);a("grant_id",v.grant_id);a("head_source_fact_id",v.head_source_fact_id);a("head_office_change_fact_id",v.head_office_change_fact_id);a("office_epoch",v.office_epoch);a("issued_tick",v.issued_tick);a("response_until",v.response_until);a("player_receipt",v.player_receipt);}
// 本人后撤回保留原接受，不能删除已发生的意愿史。
template<class A> void record(A& a,PoliticalSuccessorRoundAnswer& v){a("id",v.id);a("nomination_fact_id",v.nomination_fact_id);a("actor_id",v.actor_id);a("previous_acceptance_fact_id",v.previous_acceptance_fact_id);a("tick",v.tick);a("accepted",v.accepted);a("player_receipt",v.player_receipt);}
// 原首任撤回仍冻结原署权和epoch。
template<class A> void record(A& a,PoliticalSuccessorRoundWithdrawal& v){a("id",v.id);a("nomination_fact_id",v.nomination_fact_id);a("actor_id",v.actor_id);a("grant_id",v.grant_id);a("office_epoch",v.office_epoch);a("tick",v.tick);a("player_receipt",v.player_receipt);}
// 正式就任只引用实际通知、死亡与原组织任职事实，不复制公库或玩家控制。
template<class A> void record(A& a,PoliticalSuccessorRoundAccession& v){a("id",v.id);a("acceptance_fact_id",v.acceptance_fact_id);a("actor_id",v.actor_id);a("death_fact_id",v.death_fact_id);a("notice_binding_fact_id",v.notice_binding_fact_id);a("office_change_fact_id",v.office_change_fact_id);a("tick",v.tick);a("installed_office_epoch",v.installed_office_epoch);a("player_receipt",v.player_receipt);}
// 两分账共享一个显式版本尾，无新profile时不调用此访问器。
template<class A> void record(A& a,PoliticalSuccessorRoundState& v){a("version",v.version);a("nominations",v.nominations);a("answers",v.answers);a("withdrawals",v.withdrawals);a("accessions",v.accessions);}
// 本人只能选择已有业务源，所有时间、金额和Office变更由实际领域writer确定。
template<class A> void record(A& a,PoliticalNominateNextSuccessor& v){a("authority",v.authority);a("policy_id",v.policy_id);a("candidate_actor_id",v.candidate_actor_id);a("expected_head_source_fact_id",v.expected_head_source_fact_id);a("expected_office_epoch",v.expected_office_epoch);}
template<class A> void record(A& a,PoliticalAnswerNextSuccession& v){a("nomination_fact_id",v.nomination_fact_id);a("accept",v.accept);}
template<class A> void record(A& a,PoliticalWithdrawNextSuccession& v){a("authority",v.authority);a("nomination_fact_id",v.nomination_fact_id);}
template<class A> void record(A& a,PoliticalAccedeNextSuccession& v){a("acceptance_fact_id",v.acceptance_fact_id);a("death_fact_id",v.death_fact_id);a("death_notice_binding_fact_id",v.death_notice_binding_fact_id);a("expected_office_epoch",v.expected_office_epoch);}
}
namespace hanmo::resident {namespace {
// 格式门只负责有界形状；原生命、任职与实际结果必须随后完整冷验。
bool layout(const PoliticalSuccessorRoundState& s){return s.version==1&&s.nominations.size()<=4096&&s.answers.size()<=8192&&s.withdrawals.size()<=4096&&s.accessions.size()<=4096;}
}
Status emit_political_successor_round_state_fields(ProjectionWriter& writer,const PoliticalSuccessorRoundState& s){
 try{return dynamic_economy_codec::emit_fields(writer,s);}
 catch(const boundary::DecodeFailure& e){return writer.reject(e.error.original.code,e.error.original.path);}
}
BoundaryValue encode_political_successor_round_state(const PoliticalSuccessorRoundState& s){return politics_projection_detail::encode_fields(s,emit_political_successor_round_state_fields);}
Outcome<PoliticalSuccessorRoundState> decode_political_successor_round_state(const BoundaryValue& value){auto unique=validate_political_takeover_payload(value);if(!unique.ok)return {false,{},unique.error};try{PoliticalSuccessorRoundState s;dynamic_economy_codec::decode(value,s,"/political_successor_rounds");if(!layout(s))return {false,{},common_failure("InvalidPoliticalSuccessorRoundLayout").error};return {true,std::move(s),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status visit_political_successor_round_checkpoint(CodecVisitor& visitor,PoliticalSuccessorRoundState& state){if(!visitor.reading()&&!layout(state))return visitor.reject("InvalidPoliticalSuccessorRoundLayout");auto status=dynamic_economy_codec::checkpoint(visitor,political_successor_rounds_domain,state);return !status.ok?status:(layout(state)?common_success():visitor.reject("InvalidPoliticalSuccessorRoundLayout"));}
StableId political_successor_round_command_kind(const PoliticalSuccessorRoundCommand& c){return std::visit([](const auto& v)->StableId{using T=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<T,PoliticalNominateNextSuccessor>)return political_successor_round_nominate_command;else if constexpr(std::is_same_v<T,PoliticalAnswerNextSuccession>)return political_successor_round_answer_command;else if constexpr(std::is_same_v<T,PoliticalWithdrawNextSuccession>)return political_successor_round_withdraw_command;else return political_successor_round_accede_command;},c);}
bool is_political_successor_round_command(const StableId& k){return k==political_successor_round_nominate_command||k==political_successor_round_answer_command||k==political_successor_round_withdraw_command||k==political_successor_round_accede_command;}
BoundaryValue encode_political_successor_round_command(const PoliticalSuccessorRoundCommand& c){return std::visit([](const auto& v){return dynamic_economy_codec::encode(v);},c);}
Outcome<PoliticalSuccessorRoundCommand> decode_political_successor_round_command(const StableId& kind,const BoundaryValue& value){auto unique=validate_political_takeover_payload(value);if(!unique.ok)return {false,{},unique.error};try{PoliticalSuccessorRoundCommand command;
 if(kind==political_successor_round_nominate_command){PoliticalNominateNextSuccessor v;dynamic_economy_codec::decode(value,v,kind);command=std::move(v);}else if(kind==political_successor_round_answer_command){PoliticalAnswerNextSuccession v;dynamic_economy_codec::decode(value,v,kind);command=std::move(v);}else if(kind==political_successor_round_withdraw_command){PoliticalWithdrawNextSuccession v;dynamic_economy_codec::decode(value,v,kind);command=std::move(v);}else if(kind==political_successor_round_accede_command){PoliticalAccedeNextSuccession v;dynamic_economy_codec::decode(value,v,kind);command=std::move(v);}else return {false,{},common_failure("UnknownPoliticalSuccessorRoundCommand",kind).error};
 return {true,std::move(command),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
}
