#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/politics/politics_projection_detail.hpp"
// JSON和独立typed尾共用字段表，明示实际代理签字与原唯一治理批准之间的关系。
#include "political_governance_agency_codec.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
#include "resident/domains/politics/takeover/politics_takeover_payload_guard.hpp"
namespace hanmo::resident::dynamic_economy_codec {
// agency2独占枚举，不向旧版本写入未知字段。
template<>struct EnumNames<PoliticalGovernanceAgencyAuthoredStep>{static auto rows(){return std::vector<std::pair<PoliticalGovernanceAgencyAuthoredStep,StableId>>{{PoliticalGovernanceAgencyAuthoredStep::offer,"offer"},{PoliticalGovernanceAgencyAuthoredStep::answer,"answer"},{PoliticalGovernanceAgencyAuthoredStep::issue,"issue"}};}};
template<class A>void record(A& a,PoliticalGovernanceAgencyAuthoredOrigin& v){a("intent_id",v.intent_id);a("director_policy_id",v.director_policy_id);a("actor_id",v.actor_id);a("step",v.step);}
// 精确保存PoliticalSuccessionPlayerReceipt本身的作者或永久业务字段，不补第二份Office/Task/钱粮。
template<class A>void record(A& a,PoliticalSuccessionPlayerReceipt& v){a("actor_id",v.actor_id);a("result_id",v.result_id);a("request_sequence",v.request_sequence);a("result_revision",v.result_revision);}
// 精确保存PoliticalGovernanceAgencyPolicy本身的作者或永久业务字段，不补第二份Office/Task/钱粮。
template<class A>void record(A& a,PoliticalGovernanceAgencyPolicy& v){a("id",v.id);a("jurisdiction_id",v.jurisdiction_id);a("office_id",v.office_id);a("appointment_grant_id",v.appointment_grant_id);a("commission_grant_id",v.commission_grant_id);a("commission_policy_id",v.commission_policy_id);a("public_inventory_id",v.public_inventory_id);a("response_ticks",v.response_ticks);a("mandate_ticks",v.mandate_ticks);a("maximum_issues",v.maximum_issues);}
// 精确保存PoliticalGovernanceAgencyCatalog本身的作者或永久业务字段，不补第二份Office/Task/钱粮。
template<class A>void record(A& a,PoliticalGovernanceAgencyCatalog& v){a("version",v.version);a("policies",v.policies);}
// 精确保存PoliticalGovernanceAgencyOffer本身的作者或永久业务字段，不补第二份Office/Task/钱粮。
template<class A>void record(A& a,PoliticalGovernanceAgencyOffer& v){a("id",v.id);a("policy_id",v.policy_id);a("organization_id",v.organization_id);a("office_id",v.office_id);a("issuer_actor_id",v.issuer_actor_id);a("delegate_actor_id",v.delegate_actor_id);a("appointment_grant_id",v.appointment_grant_id);a("commission_grant_id",v.commission_grant_id);a("office_epoch",v.office_epoch);a("jurisdiction_revision",v.jurisdiction_revision);a("tick",v.tick);a("response_before_tick",v.response_before_tick);a("valid_through_tick",v.valid_through_tick);a("player_receipt",v.player_receipt);}
// 精确保存PoliticalGovernanceAgencyAnswer本身的作者或永久业务字段，不补第二份Office/Task/钱粮。
template<class A>void record(A& a,PoliticalGovernanceAgencyAnswer& v){a("id",v.id);a("offer_fact_id",v.offer_fact_id);a("actor_id",v.actor_id);a("tick",v.tick);a("accept",v.accept);a("player_receipt",v.player_receipt);}
// 精确保存PoliticalGovernanceAgencyRevocation本身的作者或永久业务字段，不补第二份Office/Task/钱粮。
template<class A>void record(A& a,PoliticalGovernanceAgencyRevocation& v){a("id",v.id);a("offer_fact_id",v.offer_fact_id);a("issuer_actor_id",v.issuer_actor_id);a("office_epoch",v.office_epoch);a("tick",v.tick);a("player_receipt",v.player_receipt);}
// 精确保存PoliticalGovernanceAgencyIssue本身的作者或永久业务字段，不补第二份Office/Task/钱粮。
template<class A>void record(A& a,PoliticalGovernanceAgencyIssue& v){a("id",v.id);a("offer_fact_id",v.offer_fact_id);a("acceptance_fact_id",v.acceptance_fact_id);a("delegate_actor_id",v.delegate_actor_id);a("executor_actor_id",v.executor_actor_id);a("authority_fact_id",v.authority_fact_id);a("tick",v.tick);a("player_receipt",v.player_receipt);}
// 精确保存PoliticalGovernanceAgencyState本身的作者或永久业务字段，不补第二份Office/Task/钱粮。
template<class A>void record(A& a,PoliticalGovernanceAgencyState& v){a("version",v.version);a("offers",v.offers);a("answers",v.answers);a("revocations",v.revocations);a("issues",v.issues);if(v.version==2)a("authored_origins",v.authored_origins);}
// 精确保存PoliticalOfferGovernanceAgency本身的作者或永久业务字段，不补第二份Office/Task/钱粮。
template<class A>void record(A& a,PoliticalOfferGovernanceAgency& v){a("authority",v.authority);a("policy_id",v.policy_id);a("delegate_actor_id",v.delegate_actor_id);a("expected_jurisdiction_revision",v.expected_jurisdiction_revision);}
// 精确保存PoliticalAnswerGovernanceAgency本身的作者或永久业务字段，不补第二份Office/Task/钱粮。
template<class A>void record(A& a,PoliticalAnswerGovernanceAgency& v){a("offer_fact_id",v.offer_fact_id);a("accept",v.accept);}
// 精确保存PoliticalRevokeGovernanceAgency本身的作者或永久业务字段，不补第二份Office/Task/钱粮。
template<class A>void record(A& a,PoliticalRevokeGovernanceAgency& v){a("authority",v.authority);a("offer_fact_id",v.offer_fact_id);}
// 精确保存PoliticalIssueAgencyGovernance本身的作者或永久业务字段，不补第二份Office/Task/钱粮。
template<class A>void record(A& a,PoliticalIssueAgencyGovernance& v){a("offer_fact_id",v.offer_fact_id);a("acceptance_fact_id",v.acceptance_fact_id);a("executor_actor_id",v.executor_actor_id);}
}
namespace hanmo::resident {namespace {
// 格式只查有界形状；真实权限和本人/提交来源由完整业务冷验负责。
bool layout(const PoliticalGovernanceAgencyState& s){return (s.version==1||s.version==2)&&(s.version!=1||s.authored_origins.empty())&&s.authored_origins.size()<=4096&&s.offers.size()+s.answers.size()+s.revocations.size()+s.issues.size()<=4096;}
}
Status emit_political_governance_agency_state_fields(ProjectionWriter& writer,const PoliticalGovernanceAgencyState& s){
 try{return dynamic_economy_codec::emit_fields(writer,s);}
 catch(const boundary::DecodeFailure& e){return writer.reject(e.error.original.code,e.error.original.path);}
}
BoundaryValue encode_political_governance_agency_state(const PoliticalGovernanceAgencyState& s){return politics_projection_detail::encode_fields(s,emit_political_governance_agency_state_fields);}
Outcome<PoliticalGovernanceAgencyState> decode_political_governance_agency_state(const BoundaryValue& value){auto v=validate_political_takeover_payload(value);if(!v.ok)return {false,{},v.error};try{PoliticalGovernanceAgencyState s;dynamic_economy_codec::decode(value,s,"/political_governance_agencies");if(!layout(s))return {false,{},common_failure("InvalidPoliticalGovernanceAgencyLayout").error};return {true,std::move(s),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status visit_political_governance_agency_checkpoint(CodecVisitor& v,PoliticalGovernanceAgencyState& s){if(!v.reading()&&!layout(s))return v.reject("InvalidPoliticalGovernanceAgencyLayout");auto result=dynamic_economy_codec::checkpoint(v,political_governance_agency_domain,s);return !result.ok?result:layout(s)?common_success():v.reject("InvalidPoliticalGovernanceAgencyLayout");}
// 原目录字段借读出口，沿同一record供值与shape发射，不生成业务状态。
Status emit_political_governance_agency_catalog_fields(ProjectionWriter& writer,const PoliticalGovernanceAgencyCatalog& s){
 try{return dynamic_economy_codec::emit_fields(writer,s);}
 catch(const boundary::DecodeFailure& e){return writer.reject(e.error.original.code,e.error.original.path);}
}
BoundaryValue encode_political_governance_agency_catalog(const PoliticalGovernanceAgencyCatalog& s){return politics_projection_detail::encode_fields(s,emit_political_governance_agency_catalog_fields);}
Outcome<PoliticalGovernanceAgencyCatalog> decode_political_governance_agency_catalog(const BoundaryValue& value){auto v=validate_political_takeover_payload(value);if(!v.ok)return {false,{},v.error};try{PoliticalGovernanceAgencyCatalog s;dynamic_economy_codec::decode(value,s,"/political_governance_agencies");if((s.version!=1&&s.version!=2)||s.policies.empty()||s.policies.size()>4096)return {false,{},common_failure("InvalidPoliticalGovernanceAgencyCatalog").error};return {true,std::move(s),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
StableId political_governance_agency_command_kind(const PoliticalGovernanceAgencyCommand& command){return std::visit([](const auto& c)->StableId{using T=std::decay_t<decltype(c)>;if constexpr(std::is_same_v<T,PoliticalOfferGovernanceAgency>)return political_governance_agency_offer_command;else if constexpr(std::is_same_v<T,PoliticalAnswerGovernanceAgency>)return political_governance_agency_answer_command;else if constexpr(std::is_same_v<T,PoliticalRevokeGovernanceAgency>)return political_governance_agency_revoke_command;else return political_governance_agency_issue_command;},command);}
bool is_political_governance_agency_command(const StableId& k){return k==political_governance_agency_offer_command||k==political_governance_agency_answer_command||k==political_governance_agency_revoke_command||k==political_governance_agency_issue_command;}
BoundaryValue encode_political_governance_agency_command(const PoliticalGovernanceAgencyCommand& command){return std::visit([](const auto& c){return dynamic_economy_codec::encode(c);},command);}
Outcome<PoliticalGovernanceAgencyCommand> decode_political_governance_agency_command(const StableId& kind,const BoundaryValue& value){auto v=validate_political_takeover_payload(value);if(!v.ok)return {false,{},v.error};try{PoliticalGovernanceAgencyCommand out;
if(kind==political_governance_agency_offer_command){PoliticalOfferGovernanceAgency c;dynamic_economy_codec::decode(value,c,kind);out=std::move(c);}else if(kind==political_governance_agency_answer_command){PoliticalAnswerGovernanceAgency c;dynamic_economy_codec::decode(value,c,kind);out=std::move(c);}else if(kind==political_governance_agency_revoke_command){PoliticalRevokeGovernanceAgency c;dynamic_economy_codec::decode(value,c,kind);out=std::move(c);}else if(kind==political_governance_agency_issue_command){PoliticalIssueAgencyGovernance c;dynamic_economy_codec::decode(value,c,kind);out=std::move(c);}else return {false,{},common_failure("UnknownPoliticalGovernanceAgencyCommand",kind).error};return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
}
