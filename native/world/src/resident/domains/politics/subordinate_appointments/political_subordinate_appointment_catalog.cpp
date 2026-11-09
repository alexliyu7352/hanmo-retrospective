#include "resident/kernel/catalog_section_observation.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/politics/politics_projection_detail.hpp"
// 首职专项任官许可只准填这一个原下属Office；不要求上级自己掌握下级的日常公库/任职权限。
#include "political_subordinate_appointment_contract.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
#include "resident/domains/politics/takeover/politics_takeover_payload_guard.hpp"
#include <set>
namespace hanmo::resident::dynamic_economy_codec {
template<class A>void record(A& a,PoliticalSubordinateAppointmentPolicy& p){a("id",p.id);a("jurisdiction_id",p.jurisdiction_id);a("head_succession_policy_id",p.head_succession_policy_id);a("appointment_grant_id",p.appointment_grant_id);a("appointment_policy_id",p.appointment_policy_id);}
template<class A>void record(A& a,PoliticalSubordinateAppointmentCatalog& c){a("version",c.version);a("policies",c.policies);}
}
namespace hanmo::resident {
Status validate_political_subordinate_appointment_catalog(const CommonStore& c,const CommonCatalog& common,const PoliticsCatalog& politics,const OrganizationCatalog& org,const PoliticalSuccessionCatalog& heads,const PoliticalSubordinateAppointmentCatalog& catalog){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 108, "PoliticalSubordinateAppointmentCatalog", "validate_political_subordinate_appointment_catalog", &catalog);

 if(!politics.enabled||catalog.version!=1||catalog.policies.empty()||catalog.policies.size()>4096)return common_failure("InvalidPoliticalSubordinateAppointmentCatalog");auto checked=validate_political_succession_catalog(c,common,org,heads);if(!checked.ok)return checked;std::set<StableId> offices,grants;
 for(const auto& row:catalog.policies.rows()){
  const auto& p=row.second;const auto* j=politics.jurisdictions.find(p.jurisdiction_id);const auto* head=heads.policies.find(p.head_succession_policy_id);const auto* target=j?org.offices.find(j->office_id):nullptr;const auto* leader=head?org.offices.find(head->leader_office_id):nullptr;const auto* g=org.grants.find(p.appointment_grant_id);const auto* terms=politics.appointment_policies.find(p.appointment_policy_id);
  if(row.first!=p.id||p.id.empty()||!j||j->revision!=0||j->political_organization_id!=j->controlling_organization_id||!common.places.find(j->seat_place_id)||!head||head->organization_id!=j->political_organization_id||head->jurisdiction_id==j->id||!target||target->organization_id!=head->organization_id||target->id==head->leader_office_id||target->deputy_actor_id||!offices.insert(target->id).second||!leader||leader->deputy_actor_id||leader->organization_id!=head->organization_id)return common_failure("PoliticalSubordinateOriginalOfficeRequired",p.id);
  if(!terms||terms->response_ticks<1||terms->response_ticks>2147483647||terms->arrival_ticks<1||terms->arrival_ticks>2147483647||terms->minimum_administration<1||terms->minimum_merit<0)return common_failure("PoliticalSubordinateAppointmentTermsRequired",p.id);
  if(!g||g->office_id!=leader->id||!grants.insert(g->id).second||std::find(leader->grant_ids.begin(),leader->grant_ids.end(),g->id)==leader->grant_ids.end()||g->amount_limit!=0||g->may_delegate||!g->wallet_ids.empty()||!g->inventory_ids.empty()||g->valid_from<0||(g->valid_until&&*g->valid_until<g->valid_from)||g->scopes.size()!=1||g->scopes.front().kind!=ScopeRef::Kind::site||g->scopes.front().id!=head->seat_place_id||g->allowed_command_kinds!=std::vector<StableId>{political_subordinate_offer_command,political_subordinate_withdraw_command})return common_failure("PoliticalSubordinateAppointmentGrantRequired",p.id);
  // 原下属Grant保持其原办公室及范围；新填空许可不会替换、延长或新增其中任何一份。
  for(const auto& id:target->grant_ids){const auto* original=org.grants.find(id);if(!original||original->office_id!=target->id)return common_failure("PoliticalSubordinateOriginalGrantRequired",id);}
 }
 return common_success();
}
Outcome<PoliticalSubordinateAppointmentCatalog> decode_political_subordinate_appointment_catalog(const BoundaryValue& value){auto valid=validate_political_takeover_payload(value);if(!valid.ok)return {false,{},valid.error};try{PoliticalSubordinateAppointmentCatalog out;dynamic_economy_codec::decode(value,out,"/political_subordinate_appointments");if(out.version!=1)return {false,{},common_failure("UnsupportedPoliticalSubordinateAppointmentVersion").error};return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
// 原作者目录同字段直接发射，值树与无树导出不再各维护一套序列。
Status emit_political_subordinate_appointment_catalog_fields(ProjectionWriter& writer,const PoliticalSubordinateAppointmentCatalog& c){
 try{return dynamic_economy_codec::emit_fields(writer,c);}
 catch(const boundary::DecodeFailure& e){return writer.reject(e.error.original.code,e.error.original.path);}
}
BoundaryValue encode_political_subordinate_appointment_catalog(const PoliticalSubordinateAppointmentCatalog& c){return politics_projection_detail::encode_fields(c,emit_political_subordinate_appointment_catalog_fields);}
}
