#include "resident/kernel/catalog_section_observation.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/politics/politics_projection_detail.hpp"
// 生前交接作者目录严格指向同组织原首职；无专项Grant或有限期限时保持不可办理。
#include "political_head_handover_contract.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
#include "resident/domains/politics/takeover/politics_takeover_payload_guard.hpp"
#include <set>
namespace hanmo::resident::dynamic_economy_codec {
// 全部新条款明确书写，不从原死亡继位期限或Family关系推导让职意愿。
template<class A>void record(A& a,PoliticalHeadHandoverPolicy& v){a("id",v.id);a("succession_policy_id",v.succession_policy_id);a("handover_grant_id",v.handover_grant_id);a("response_ticks",v.response_ticks);}
// 独立目录1不修改既有政治访客；未知字段和重复键均由严格原codec拒绝。
template<class A>void record(A& a,PoliticalHeadHandoverCatalog& v){a("version",v.version);a("policies",v.policies);}
}
namespace hanmo::resident {
Status validate_political_head_handover_catalog(const CommonStore& common,const CommonCatalog& cc,const OrganizationCatalog& org,const PoliticalSuccessionCatalog& succession,const PoliticalHeadHandoverCatalog& catalog){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 99, "PoliticalHeadHandoverCatalog", "validate_political_head_handover_catalog", &catalog);

 auto valid=validate_political_succession_catalog(common,cc,org,succession);if(!valid.ok)return valid;
 if(catalog.version!=1||catalog.policies.empty()||catalog.policies.size()>4096)return common_failure("InvalidPoliticalHeadHandoverCatalog");std::set<StableId> selected;
 for(const auto& row:catalog.policies.rows()){
  const auto& p=row.second;const auto* original=succession.policies.find(p.succession_policy_id);const auto* g=org.grants.find(p.handover_grant_id);
  if(row.first!=p.id||p.id.empty()||!original||p.response_ticks<1||p.response_ticks>2147483647||!selected.insert(p.succession_policy_id).second)return common_failure("PoliticalHeadHandoverOriginalPolicyRequired",row.first);
  const auto* office=org.offices.find(original->leader_office_id);
  if(!office||!g||g->id!=p.handover_grant_id||g->id==original->nomination_grant_id||g->office_id!=office->id||g->amount_limit!=0||!g->wallet_ids.empty()||!g->inventory_ids.empty()||std::find(office->grant_ids.begin(),office->grant_ids.end(),g->id)==office->grant_ids.end()||std::none_of(g->scopes.begin(),g->scopes.end(),[&](const ScopeRef& s){return s.kind==ScopeRef::Kind::site&&s.id==original->seat_place_id;}))return common_failure("PoliticalHeadHandoverSpecificGrantRequired",p.id);
  for(const auto* kind:{political_head_handover_offer_command,political_head_handover_withdraw_command})if(std::find(g->allowed_command_kinds.begin(),g->allowed_command_kinds.end(),kind)==g->allowed_command_kinds.end())return common_failure("PoliticalHeadHandoverSpecificGrantRequired",p.id);
 }
 return common_success();
}
// 目录只声明未来可办理条款，不生成已让职、已接受或任何原Office变更。
Outcome<PoliticalHeadHandoverCatalog> decode_political_head_handover_catalog(const BoundaryValue& root){auto unique=validate_political_takeover_payload(root);if(!unique.ok)return {false,{},unique.error};try{PoliticalHeadHandoverCatalog out;dynamic_economy_codec::decode(root,out,"/political_head_handovers");if(out.version!=1)return {false,{},common_failure("UnsupportedPoliticalHeadHandoverVersion").error};return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
// 原作者目录同字段直接发射，值树与无树导出不再各维护一套序列。
Status emit_political_head_handover_catalog_fields(ProjectionWriter& writer,const PoliticalHeadHandoverCatalog& value){
 try{return dynamic_economy_codec::emit_fields(writer,value);}
 catch(const boundary::DecodeFailure& e){return writer.reject(e.error.original.code,e.error.original.path);}
}
BoundaryValue encode_political_head_handover_catalog(const PoliticalHeadHandoverCatalog& value){return politics_projection_detail::encode_fields(value,emit_political_head_handover_catalog_fields);}
}
