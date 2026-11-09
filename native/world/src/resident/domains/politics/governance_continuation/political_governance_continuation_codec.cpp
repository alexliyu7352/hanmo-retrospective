// 独立新段逐字段保存原责任引用与实际结果绑定；旧治理、Task和经济字节不增加字段。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "political_governance_continuation.hpp"
#include "resident/domains/politics/politics_projection_detail.hpp"
#include "resident/domains/politics/takeover/politics_takeover_payload_guard.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
namespace hanmo::resident::dynamic_economy_codec {
template<class A>void record(A& a,PoliticalGovernanceContinuation& v){a("id",v.id);a("task_id",v.task_id);a("acceptance_fact_id",v.acceptance_fact_id);a("authority_fact_id",v.authority_fact_id);a("contract_id",v.contract_id);a("executor_actor_id",v.executor_actor_id);a("jurisdiction_id",v.jurisdiction_id);a("site_id",v.site_id);a("original_organization_id",v.original_organization_id);a("public_wallet_id",v.public_wallet_id);a("public_inventory_id",v.public_inventory_id);a("registration_fact_id",v.registration_fact_id);a("approval_fact_id",v.approval_fact_id);a("decision_fact_id",v.decision_fact_id);a("occupation_fact_id",v.occupation_fact_id);a("grant_id",v.grant_id);a("authority",v.authority);a("tick",v.tick);}
template<class A>void record(A& a,GovernanceContinuationPlayerReceipt& v){a("source_fact_id",v.source_fact_id);a("actor_id",v.actor_id);a("result_id",v.result_id);a("tick",v.tick);a("request_sequence",v.request_sequence);a("result_revision",v.result_revision);}
template<class A>void record(A& a,PoliticalGovernanceContinuationState& v){a("version",v.version);a("continuations",v.continuations);a("player_receipts",v.player_receipts);}
template<class A>void record(A& a,PoliticalContinueGovernance& v){a("authority",v.authority);a("task_id",v.task_id);a("registration_fact_id",v.registration_fact_id);a("approval_fact_id",v.approval_fact_id);a("decision_fact_id",v.decision_fact_id);}
}
namespace hanmo::resident {
// 只借原record字段表输出；版本与来源准入仍由原成对codec及业务校验负责。
Status emit_political_governance_continuations_fields(ProjectionWriter& writer,const PoliticalGovernanceContinuationState& value){
 try{return dynamic_economy_codec::emit_fields(writer,value);}
 catch(const boundary::DecodeFailure& error){return writer.reject(error.error.original.code,error.error.original.path);}
}
BoundaryValue encode_political_governance_continuations(const PoliticalGovernanceContinuationState& value){return politics_projection_detail::encode_fields(value,emit_political_governance_continuations_fields);}
Outcome<PoliticalGovernanceContinuationState> decode_political_governance_continuations(const BoundaryValue& value){auto valid=validate_political_takeover_payload(value);if(!valid.ok)return {false,{},valid.error};try{PoliticalGovernanceContinuationState out;dynamic_economy_codec::decode(value,out,"/political_governance_continuations");if(out.version!=1)return {false,{},common_failure("UnsupportedGovernanceContinuationsVersion").error};return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status visit_political_governance_continuations_checkpoint(CodecVisitor& visitor,PoliticalGovernanceContinuationState& v){if(!visitor.reading()&&v.version!=1)return visitor.reject("UnsupportedGovernanceContinuationsVersion");auto valid=dynamic_economy_codec::checkpoint(visitor,political_governance_continuation_module,v);return !valid.ok?valid:v.version==1?common_success():visitor.reject("UnsupportedGovernanceContinuationsVersion");}
BoundaryValue encode_political_continue_governance_command(const PoliticalContinueGovernance& v){return dynamic_economy_codec::encode(v);}
Outcome<PoliticalContinueGovernance> decode_political_continue_governance_command(const BoundaryValue& value){auto valid=validate_political_takeover_payload(value);if(!valid.ok)return {false,{},valid.error};try{PoliticalContinueGovernance out;dynamic_economy_codec::decode(value,out,"/political_continue_governance");for(const auto& id:{out.task_id,out.registration_fact_id,out.approval_fact_id,out.decision_fact_id,out.authority.organization_id,out.authority.office_id,out.authority.actor_id})if(id.empty()||id.size()>4096)return {false,{},common_failure("InvalidGovernanceContinuationCommand").error};if(out.authority.authority_epoch<0)return {false,{},common_failure("InvalidGovernanceContinuationCommand").error};return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
}
