#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/politics/politics_projection_detail.hpp"
// 外交冷状态复用现有严格字段访问器；事实、权源、条款版本与接触快照逐项保存。
#include "diplomacy_offers.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
#include "diplomacy_v2_codec_detail.hpp"
namespace hanmo::resident::dynamic_economy_codec {
template<>struct EnumNames<DiplomaticOfferStatus>{static std::vector<std::pair<DiplomaticOfferStatus,StableId>> rows(){return {{DiplomaticOfferStatus::awaiting_delivery,"awaiting_delivery"},{DiplomaticOfferStatus::presented,"presented"},{DiplomaticOfferStatus::accepted_pending_ratification,"accepted_pending_ratification"},{DiplomaticOfferStatus::refused,"refused"},{DiplomaticOfferStatus::withdrawn,"withdrawn"},{DiplomaticOfferStatus::expired,"expired"},{DiplomaticOfferStatus::invalidated,"invalidated"}};}};
template<>struct EnumNames<DiplomaticReplyKind>{static std::vector<std::pair<DiplomaticReplyKind,StableId>> rows(){return {{DiplomaticReplyKind::accept,"accept"},{DiplomaticReplyKind::refuse,"refuse"}};}};
// 独立wire避免重复定义其他领域已持有的ActingAuthority访问器。
struct DiplomaticAuthorityWire {ActingAuthority& value;};
template<class A>void record(A& a,DiplomaticAuthorityWire& wire){auto& v=wire.value;a("organization_id",v.organization_id);a("office_id",v.office_id);a("authority_epoch",v.authority_epoch);a("actor_id",v.actor_id);}
// 提议保持原使者和独占版本，不能用解码默认值恢复已经过期的窗口。
template<class A>void record(A& a,DiplomaticOffer& v){a("id",v.id);a("policy_id",v.policy_id);a("terms_id",v.terms_id);a("issuer_grant_id",v.issuer_grant_id);a("envoy_actor_id",v.envoy_actor_id);DiplomaticAuthorityWire authority{v.issuer};a("issuer",authority);a("issued_tick",v.issued_tick);a("expires_tick",v.expires_tick);a("terms_revision",v.terms_revision);a("revision",v.revision);a("status",v.status);a("last_fact_id",v.last_fact_id);a("presentation_fact_id",v.presentation_fact_id);a("mission_authority_fact_id",v.mission_authority_fact_id);}
// 所有事实参与者与因果链必须明确，不从今日官职补造昨日证人。
template<class A>void record(A& a,DiplomaticOfferFact& v){a("id",v.id);a("offer_id",v.offer_id);a("kind",v.kind);a("actor_id",v.actor_id);a("terms_id",v.terms_id);a("tick",v.tick);a("terms_revision",v.terms_revision);a("offer_revision",v.offer_revision);a("cause_fact_ids",v.cause_fact_ids);a("witness_actor_ids",v.witness_actor_ids);}
// 交接绑定来自S3真实Action完成，不能由玩家传入四个字符串冒造。
template<class A>void record(A& a,DiplomaticPresentationTaskBinding& v){a("mission_authority_fact_id",v.mission_authority_fact_id);a("task_id",v.task_id);a("contract_id",v.contract_id);a("action_id",v.action_id);a("acceptance_fact_id",v.acceptance_fact_id);}
// 只存接触发生时所需的稳定字段，不把执行epoch当成永久接触许可。
template<class A>void record(A& a,DiplomaticPresentation& v){a("id",v.id);a("offer_id",v.offer_id);a("proposal_fact_id",v.proposal_fact_id);a("terms_id",v.terms_id);a("envoy_actor_id",v.envoy_actor_id);a("representative_grant_id",v.representative_grant_id);DiplomaticAuthorityWire authority{v.representative};a("representative",authority);a("tick",v.tick);a("terms_revision",v.terms_revision);a("offer_revision",v.offer_revision);a("session_actor_id",v.session_actor_id);a("session_id",v.session_id);a("place_id",v.place_id);a("provider_actor_id",v.provider_actor_id);a("role_id",v.role_id);a("context_kind",v.context_kind);a("task_binding",v.task_binding);}
// 回复保留实际回复者、专项权源和亲历接触；接受不写任何生效状态。
template<class A>void record(A& a,DiplomaticReply& v){a("id",v.id);a("offer_id",v.offer_id);a("presentation_fact_id",v.presentation_fact_id);a("terms_id",v.terms_id);a("grant_id",v.grant_id);DiplomaticAuthorityWire authority{v.authority};a("authority",authority);a("decision",v.decision);a("tick",v.tick);a("terms_revision",v.terms_revision);a("offer_revision",v.offer_revision);a("session_actor_id",v.session_actor_id);a("session_id",v.session_id);a("place_id",v.place_id);a("provider_actor_id",v.provider_actor_id);a("role_id",v.role_id);a("context_kind",v.context_kind);}
// source_identity是只读访问器现取的候选标签，不进入历史责任持久字节。
template<class A>void record(A& a,DiplomaticMissionApproval& v){a("mission_authority_fact_id",v.mission_authority_fact_id);a("proposal_id",v.proposal_id);a("proposal_fact_id",v.proposal_fact_id);a("policy_id",v.policy_id);a("rule_name_key",v.rule_name_key);a("terms_revision",v.terms_revision);a("issuer_organization_id",v.issuer_organization_id);a("target_organization_id",v.target_organization_id);a("envoy_actor_id",v.envoy_actor_id);a("issuer_office_id",v.issuer_office_id);a("issuer_actor_id",v.issuer_actor_id);a("issuer_grant_id",v.issuer_grant_id);a("issuer_authority_epoch",v.issuer_authority_epoch);a("target_office_id",v.target_office_id);a("delivery_site_id",v.delivery_site_id);a("report_office_id",v.report_office_id);a("report_site_id",v.report_site_id);a("continuity_policy_id",v.continuity_policy_id);a("issued_tick",v.issued_tick);a("acceptance_expires_tick",v.acceptance_expires_tick);a("delivery_expires_tick",v.delivery_expires_tick);a("delivery_work_ticks",v.delivery_work_ticks);a("report_work_ticks",v.report_work_ticks);a("return_report_required",v.return_report_required);}
// 单一state拥有提议与呈递事实，旧政治状态没有追加隐藏字段。
template<class A>void record(A& a,DiplomacyState& v){a("version",v.version);a("offers",v.offers);a("facts",v.facts);a("presentations",v.presentations);a("replies",v.replies);a("missions",v.missions);if(v.version==2){if(politics_projection_detail::reading(a)&&!v.v2)v.v2.emplace();if(!v.v2)boundary::fail("DiplomacyVersionMismatch","/diplomacy");a("v2",*v.v2);}else if(politics_projection_detail::reading(a))v.v2.reset();}
}
namespace hanmo::resident {
// 解码只恢复字节语义，完整历史真实性另交唯一领域validator。
Outcome<DiplomacyState> decode_diplomacy_state(const BoundaryValue& value){try{DiplomacyState out;dynamic_economy_codec::decode(value,out,"/diplomacy");if((out.version!=1&&out.version!=2)||(out.version==2&&(!out.v2||out.v2->version!=2)))return {false,{},common_failure("DiplomacyVersionMismatch").error};return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
// 导出保持原字段与字符串枚举，读取不会派遣或接受提议。
Status emit_diplomacy_state_fields(ProjectionWriter& writer,const DiplomacyState& state){
 try{if((state.version!=1&&state.version!=2)||(state.version==2)!=state.v2.has_value()||(state.v2&&state.v2->version!=2))boundary::fail("DiplomacyVersionMismatch","/diplomacy");return dynamic_economy_codec::emit_fields(writer,state);}
 catch(const boundary::DecodeFailure& e){return writer.reject(e.error.original.code,e.error.original.path);}
}
BoundaryValue encode_diplomacy_state(const DiplomacyState& state){return politics_projection_detail::encode_fields(state,emit_diplomacy_state_fields);}
// 二进制与JSON复用同一字段集合，缺段及未知枚举不能静默补齐。
Status visit_diplomacy_checkpoint(CodecVisitor& codec,DiplomacyState& state){if(!codec.reading()&&((state.version!=1&&state.version!=2)||(state.version==2)!=state.v2.has_value()||(state.v2&&state.v2->version!=2)))return codec.reject("DiplomacyVersionMismatch");auto result=dynamic_economy_codec::checkpoint(codec,"diplomacy",state);if(!result.ok)return result;return (state.version==1||(state.version==2&&state.v2&&state.v2->version==2))?common_success():codec.reject("DiplomacyVersionMismatch");}
}
