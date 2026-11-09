#include "resident/kernel/catalog_section_observation.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/politics/politics_projection_detail.hpp"
// 辞任规程必须明确下属辖区、真实原首职及该首职专项批准Grant，不由客户端任选审批人。
#include "political_office_resignation_contract.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
#include "resident/domains/politics/takeover/politics_takeover_payload_guard.hpp"
#include <set>
namespace hanmo::resident::dynamic_economy_codec {
template<class A>void record(A& a,PoliticalOfficeResignationPolicy& p){a("id",p.id);a("jurisdiction_id",p.jurisdiction_id);a("approval_succession_policy_id",p.approval_succession_policy_id);a("approval_grant_id",p.approval_grant_id);a("response_ticks",p.response_ticks);}
template<class A>void record(A& a,PoliticalOfficeResignationCatalog& c){a("version",c.version);a("policies",c.policies);}
}
namespace hanmo::resident {
Status validate_political_office_resignation_catalog(const CommonStore& c,const CommonCatalog& cc,const PoliticsCatalog& politics,const OrganizationCatalog& org,const PoliticalSuccessionCatalog& heads,const PoliticalOfficeResignationCatalog& catalog){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 103, "PoliticalOfficeResignationCatalog", "validate_political_office_resignation_catalog", &catalog);

 if(!politics.enabled||(catalog.version!=1&&catalog.version!=2)||catalog.policies.empty()||catalog.policies.size()>4096)return common_failure("InvalidPoliticalOfficeResignationCatalog");auto valid=validate_political_succession_catalog(c,cc,org,heads);if(!valid.ok)return valid;std::set<StableId> offices;
 for(const auto& row:catalog.policies.rows()){
  const auto& p=row.second;const auto* j=politics.jurisdictions.find(p.jurisdiction_id);const auto* head=heads.policies.find(p.approval_succession_policy_id);const auto* target=j?org.offices.find(j->office_id):nullptr;const auto* superior=head?org.offices.find(head->leader_office_id):nullptr;const auto* grant=org.grants.find(p.approval_grant_id);
  if(row.first!=p.id||p.id.empty()||p.response_ticks<1||p.response_ticks>2147483647||!j||!cc.places.find(j->seat_place_id)||j->political_organization_id!=j->controlling_organization_id||!head||head->organization_id!=j->political_organization_id||!target||target->organization_id!=head->organization_id||target->id==head->leader_office_id||target->deputy_actor_id||!offices.insert(target->id).second||!superior||superior->deputy_actor_id||superior->organization_id!=head->organization_id)return common_failure("PoliticalOfficeResignationSubordinateOfficeRequired",p.id);
  if(!grant||grant->amount_limit!=0||grant->may_delegate||!grant->wallet_ids.empty()||!grant->inventory_ids.empty()||grant->allowed_command_kinds!=std::vector<StableId>{political_office_resignation_answer_command}||grant->scopes.size()!=1||grant->valid_from<0||(grant->valid_until&&*grant->valid_until<grant->valid_from)||grant->office_id!=superior->id||std::find(superior->grant_ids.begin(),superior->grant_ids.end(),grant->id)==superior->grant_ids.end()||std::find(grant->allowed_command_kinds.begin(),grant->allowed_command_kinds.end(),political_office_resignation_answer_command)==grant->allowed_command_kinds.end()||std::none_of(grant->scopes.begin(),grant->scopes.end(),[&](const ScopeRef& scope){return scope.kind==ScopeRef::Kind::site&&scope.id==head->seat_place_id;}))return common_failure("PoliticalOfficeResignationApprovalGrantRequired",p.id);
 }return common_success();
}
Outcome<PoliticalOfficeResignationCatalog> decode_political_office_resignation_catalog(const BoundaryValue& value){auto valid=validate_political_takeover_payload(value);if(!valid.ok)return {false,{},valid.error};try{PoliticalOfficeResignationCatalog out;dynamic_economy_codec::decode(value,out,"/political_office_resignations");if((out.version!=1&&out.version!=2))return {false,{},common_failure("UnsupportedPoliticalOfficeResignationVersion").error};return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
// 原作者目录同字段直接发射，值树与无树导出不再各维护一套序列。
Status emit_political_office_resignation_catalog_fields(ProjectionWriter& writer,const PoliticalOfficeResignationCatalog& c){
 try{return dynamic_economy_codec::emit_fields(writer,c);}
 catch(const boundary::DecodeFailure& e){return writer.reject(e.error.original.code,e.error.original.path);}
}
BoundaryValue encode_political_office_resignation_catalog(const PoliticalOfficeResignationCatalog& c){return politics_projection_detail::encode_fields(c,emit_political_office_resignation_catalog_fields);}
}
