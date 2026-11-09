#include "resident/kernel/catalog_section_observation.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/politics/politics_projection_detail.hpp"
// 接待移交目录只声明原首职和纯接待岗位，不改旧服务、原职责、任职或任何收据的initial_actor。
#include "political_office_reception_contract.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
#include "resident/domains/politics/takeover/politics_takeover_payload_guard.hpp"
#include <set>
namespace hanmo::resident::dynamic_economy_codec {
// 四个精确来源引用，不接受隐含岗位/宽泛办公室扫描。
template<class A>void record(A& a,PoliticalOfficeReceptionPolicy& v){a("id",v.id);a("succession_policy_id",v.succession_policy_id);a("role_id",v.role_id);a("reception_grant_id",v.reception_grant_id);}
// 独立目录版本不改变任何旧访客的字段顺序。
template<class A>void record(A& a,PoliticalOfficeReceptionCatalog& v){a("version",v.version);a("policies",v.policies);}
}
namespace hanmo::resident {
// 首片拒承包/教学等带原角色主体责任的服务，且不能挪用原OrgDuty的永久角色归属。
Status validate_political_office_reception_catalog(const CommonStore& c,const CommonCatalog& common,const OrganizationCatalog& organizations,const PoliticalSuccessionCatalog& successions,const PoliticalOfficeReceptionCatalog& catalog){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 102, "PoliticalOfficeReceptionCatalog", "validate_political_office_reception_catalog", &catalog);

 auto valid=validate_political_succession_catalog(c,common,organizations,successions);if(!valid.ok)return valid;
 if(catalog.version!=1||catalog.policies.empty()||catalog.policies.size()>4096)return common_failure("InvalidPoliticalOfficeReceptionCatalog");std::set<StableId> roles;
 for(const auto& row:catalog.policies.rows()){
  const auto& p=row.second;const auto* original=successions.policies.find(p.succession_policy_id);const auto* office=original?organizations.offices.find(original->leader_office_id):nullptr;const auto* role=common.roles.find(p.role_id);const auto* grant=organizations.grants.find(p.reception_grant_id);
  if(p.id.empty()||p.id!=row.first||!original||!office||!office->holder_actor_id||!role||role->id!=p.role_id||!roles.insert(p.role_id).second||role->organization_id!=std::optional<StableId>{original->organization_id}||role->initial_actor!=*office->holder_actor_id||role->place_id!=original->seat_place_id||!role->dialogue_id||role->dialogue_id->empty()||role->services!=std::vector<StableId>{"information"})return common_failure("PoliticalOfficeReceptionOriginalRoleRequired",row.first);
  for(const auto& duty:organizations.duties.rows())if(duty.second.role_id==role->id)return common_failure("PoliticalOfficeReceptionDutyDispositionRequired",role->id);
  if(!grant||grant->id!=p.reception_grant_id||grant->office_id!=office->id||grant->allowed_command_kinds!=std::vector<StableId>{political_office_reception_command}||grant->amount_limit!=0||!grant->wallet_ids.empty()||!grant->inventory_ids.empty()||grant->may_delegate||grant->valid_from<0||(grant->valid_until&&*grant->valid_until<grant->valid_from)||std::find(office->grant_ids.begin(),office->grant_ids.end(),grant->id)==office->grant_ids.end()||grant->scopes.size()!=1||grant->scopes.front().kind!=ScopeRef::Kind::site||grant->scopes.front().id!=role->place_id)return common_failure("PoliticalOfficeReceptionSpecificGrantRequired",p.id);
 }return common_success();
}
// 重复键、未知字段和非1版本严格拒绝，不从缺字段猜测岗位用途。
Outcome<PoliticalOfficeReceptionCatalog> decode_political_office_reception_catalog(const BoundaryValue& value){auto valid=validate_political_takeover_payload(value);if(!valid.ok)return {false,{},valid.error};try{PoliticalOfficeReceptionCatalog out;dynamic_economy_codec::decode(value,out,"/political_office_receptions");if(out.version!=1)return {false,{},common_failure("UnsupportedPoliticalOfficeReceptionVersion").error};return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
// 目录只输出明确的四项引用，不存当前接待人或继位成功断言。
// 原作者目录同字段直接发射，值树与无树导出不再各维护一套序列。
Status emit_political_office_reception_catalog_fields(ProjectionWriter& writer,const PoliticalOfficeReceptionCatalog& value){
 try{return dynamic_economy_codec::emit_fields(writer,value);}
 catch(const boundary::DecodeFailure& e){return writer.reject(e.error.original.code,e.error.original.path);}
}
BoundaryValue encode_political_office_reception_catalog(const PoliticalOfficeReceptionCatalog& value){return politics_projection_detail::encode_fields(value,emit_political_office_reception_catalog_fields);}
}
