#include "resident/kernel/catalog_section_observation.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/politics/politics_projection_detail.hpp"
// 继位政策只绑定作者明确的原首职及专项Grant；不会启用组织目录中同名占位字符串。
#include "political_succession_contract.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
#include "resident/domains/politics/takeover/politics_takeover_payload_guard.hpp"
#include <set>
namespace hanmo::resident::dynamic_economy_codec {
// 所有管辖及期限字段显式必填，不从当前Owner或亲缘关系推导继任资格。
template<class A>void record(A& a,PoliticalSuccessionPolicy& v){a("id",v.id);a("organization_id",v.organization_id);a("leader_office_id",v.leader_office_id);a("seat_place_id",v.seat_place_id);a("nomination_grant_id",v.nomination_grant_id);a("response_ticks",v.response_ticks);a("accession_ticks",v.accession_ticks);a("jurisdiction_id",v.jurisdiction_id);}
// 本片目录尚未公共注册；未知版本及额外字段由同一严格codec拒绝。
template<class A>void record(A& a,PoliticalSuccessionCatalog& v){a("version",v.version);a("policies",v.policies);}
}
namespace hanmo::resident {
Status validate_political_succession_catalog(const CommonStore& common,const CommonCatalog& cc,const OrganizationCatalog& org,const PoliticalSuccessionCatalog& catalog){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 109, "PoliticalSuccessionCatalog", "validate_political_succession_catalog", &catalog);

 if(catalog.version!=1||catalog.policies.empty()||catalog.policies.size()>4096)return common_failure("InvalidPoliticalSuccessionCatalog");std::set<StableId> organizations,offices;
 for(const auto& row:catalog.policies.rows()){
  const auto& p=row.second;const auto* principal=org.organizations.find(p.organization_id);const auto* office=org.offices.find(p.leader_office_id);const auto* grant=org.grants.find(p.nomination_grant_id);
  if(row.first!=p.id||p.id.empty()||p.jurisdiction_id.empty()||p.response_ticks<1||p.response_ticks>2147483647||p.accession_ticks<1||p.accession_ticks>2147483647||!organizations.insert(p.organization_id).second||!offices.insert(p.leader_office_id).second||!principal||principal->status!=OrganizationStatus::active||principal->succession_policy_id!=p.id||principal->headquarters_site!=p.seat_place_id||!cc.places.find(p.seat_place_id)||!office||office->organization_id!=p.organization_id||office->status!=OfficeStatus::active||!office->holder_actor_id||!common.actor(*office->holder_actor_id)||std::find(principal->office_ids.begin(),principal->office_ids.end(),office->id)==principal->office_ids.end())return common_failure("PoliticalSuccessionOriginalOfficeRequired",p.id);
  if(!grant||grant->office_id!=office->id||std::find(office->grant_ids.begin(),office->grant_ids.end(),grant->id)==office->grant_ids.end()||std::none_of(grant->scopes.begin(),grant->scopes.end(),[&](const ScopeRef& s){return s.kind==ScopeRef::Kind::site&&s.id==p.seat_place_id;}))return common_failure("PoliticalSuccessionNominationGrantRequired",p.id);
  for(const auto* kind:{political_succession_nominate_command,political_succession_withdraw_command})if(std::find(grant->allowed_command_kinds.begin(),grant->allowed_command_kinds.end(),kind)==grant->allowed_command_kinds.end())return common_failure("PoliticalSuccessionNominationGrantRequired",p.id);
 }return common_success();
}
// 重复JSON键先拒绝，目录解码本身不生成批准、本人接受或任何权力交割。
Outcome<PoliticalSuccessionCatalog> decode_political_succession_catalog(const BoundaryValue& root){auto unique=validate_political_takeover_payload(root);if(!unique.ok)return {false,{},unique.error};try{PoliticalSuccessionCatalog out;dynamic_economy_codec::decode(root,out,"/political_successions");if(out.version!=1)return {false,{},common_failure("UnsupportedPoliticalSuccessionVersion").error};return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
// 原目录字段借读出口，沿同一record供值与shape发射，不生成业务状态。
Status emit_political_succession_catalog_fields(ProjectionWriter& writer,const PoliticalSuccessionCatalog& value){
 try{return dynamic_economy_codec::emit_fields(writer,value);}
 catch(const boundary::DecodeFailure& e){return writer.reject(e.error.original.code,e.error.original.path);}
}
BoundaryValue encode_political_succession_catalog(const PoliticalSuccessionCatalog& value){return politics_projection_detail::encode_fields(value,emit_political_succession_catalog_fields);}
}
