#include "resident/kernel/catalog_section_observation.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/politics/politics_projection_detail.hpp"
// 目录只声明后来任期可选择的生前交接，不生成候选、双方同意或Office变更。
#include "political_term_handover_contract.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
#include "resident/domains/politics/takeover/politics_takeover_payload_guard.hpp"
#include <set>
namespace hanmo::resident::dynamic_economy_codec {
// 原任期制度、专项Grant及有限期逐字段冻结，未知条款直接拒绝。
template<class A>void record(A& a,PoliticalTermHandoverPolicy& v){a("id",v.id);a("successor_round_policy_id",v.successor_round_policy_id);a("handover_grant_id",v.handover_grant_id);a("response_ticks",v.response_ticks);}
// 新目录使用独立版本，任何旧域访客不因它增空尾。
template<class A>void record(A& a,PoliticalTermHandoverCatalog& v){a("version",v.version);a("policies",v.policies);}
}
namespace hanmo::resident {
// 全部原制度先验后只接受同一原首职的专门零金额授权，副手和真实现任仍留业务点重核。
Status validate_political_term_handover_catalog(const CommonStore& c,const CommonCatalog& common,const OrganizationCatalog& organizations,const PoliticalSuccessionCatalog& original,const PoliticalSuccessorRoundCatalog& rounds,const PoliticalTermHandoverCatalog& catalog){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 112, "PoliticalTermHandoverCatalog", "validate_political_term_handover_catalog", &catalog);

 auto valid=validate_political_successor_round_catalog(c,common,organizations,original,rounds);if(!valid.ok)return valid;
 if(catalog.version!=1||catalog.policies.empty()||catalog.policies.size()>4096)return common_failure("InvalidPoliticalTermHandoverCatalog");std::set<StableId> unique;
 for(const auto& row:catalog.policies.rows()){
  const auto& p=row.second;const auto* round=rounds.policies.find(p.successor_round_policy_id);const auto* old=round?original.policies.find(round->succession_policy_id):nullptr;const auto* office=old?organizations.offices.find(old->leader_office_id):nullptr;const auto* grant=organizations.grants.find(p.handover_grant_id);
  if(p.id!=row.first||p.id.empty()||!round||!old||!office||!unique.insert(round->id).second||p.response_ticks<1||p.response_ticks>2147483647)return common_failure("PoliticalTermHandoverOriginalPolicyRequired",row.first);
  if(!grant||grant->id!=p.handover_grant_id||grant->id==old->nomination_grant_id||grant->id==round->nomination_grant_id||grant->office_id!=office->id||grant->allowed_command_kinds.size()!=2||grant->amount_limit!=0||!grant->wallet_ids.empty()||!grant->inventory_ids.empty()||std::find(office->grant_ids.begin(),office->grant_ids.end(),grant->id)==office->grant_ids.end()||std::none_of(grant->scopes.begin(),grant->scopes.end(),[&](const ScopeRef& s){return s.kind==ScopeRef::Kind::site&&s.id==old->seat_place_id;}))return common_failure("PoliticalTermHandoverSpecificGrantRequired",p.id);
  for(const auto* kind:{political_term_handover_offer_command,political_term_handover_withdraw_command})if(std::find(grant->allowed_command_kinds.begin(),grant->allowed_command_kinds.end(),kind)==grant->allowed_command_kinds.end())return common_failure("PoliticalTermHandoverSpecificGrantRequired",p.id);
 }return common_success();
}
// 重复键与未知字段沿原严格解码口，坏目录不产生任何部分任职状态。
Outcome<PoliticalTermHandoverCatalog> decode_political_term_handover_catalog(const BoundaryValue& value){auto valid=validate_political_takeover_payload(value);if(!valid.ok)return {false,{},valid.error};try{PoliticalTermHandoverCatalog out;dynamic_economy_codec::decode(value,out,"/political_term_handovers");if(out.version!=1)return {false,{},common_failure("UnsupportedPoliticalTermHandoverVersion").error};return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
// 仅输出作者条款，不包含实际让职或承诺来源。
// 原作者目录同字段直接发射，值树与无树导出不再各维护一套序列。
Status emit_political_term_handover_catalog_fields(ProjectionWriter& writer,const PoliticalTermHandoverCatalog& value){
 try{return dynamic_economy_codec::emit_fields(writer,value);}
 catch(const boundary::DecodeFailure& e){return writer.reject(e.error.original.code,e.error.original.path);}
}
BoundaryValue encode_political_term_handover_catalog(const PoliticalTermHandoverCatalog& value){return politics_projection_detail::encode_fields(value,emit_political_term_handover_catalog_fields);}
}
