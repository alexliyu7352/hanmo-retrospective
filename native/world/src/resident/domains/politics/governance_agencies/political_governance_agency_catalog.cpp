#include "resident/kernel/catalog_section_observation.hpp"
// 独立代理目录必须有额外专项Grant，并把原安民模板及公钱粮冻结到同一原辖区。
#include "political_governance_agency_contract.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include <set>
namespace hanmo::resident {namespace {
// 不拼接两个原财权授权；列出的一个Grant必须独自覆盖原模板最大承诺及两容器。
template<class T> bool has(const std::vector<T>& values,const T& value){return std::find(values.begin(),values.end(),value)!=values.end();}
bool site(const Grant& g,const StableId& id){return std::any_of(g.scopes.begin(),g.scopes.end(),[&](const ScopeRef& s){return s.kind==ScopeRef::Kind::site&&s.id==id;});}
bool dates(const Grant& g){return g.valid_from>=0&&(!g.valid_until||*g.valid_until>=g.valid_from);}
}
Status validate_political_governance_agency_catalog(const CommonStore& common,const PoliticsCatalog& politics,const PoliticsGovernanceCatalog& governance,const OrganizationCatalog& organizations,const PoliticalGovernanceAgencyCatalog& catalog){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 97, "PoliticalGovernanceAgencyCatalog", "validate_political_governance_agency_catalog", &catalog);

 if(!politics.enabled||!governance.enabled||!organizations.enabled||(catalog.version!=1&&catalog.version!=2)||catalog.policies.empty()||catalog.policies.size()>4096)return common_failure("PoliticalGovernanceAgencyProfileRequired");
 for(const auto& row:catalog.policies.rows()){
  const auto& p=row.second;const auto* j=politics.jurisdictions.find(p.jurisdiction_id);const auto* office=organizations.offices.find(p.office_id);const auto* appointment=organizations.grants.find(p.appointment_grant_id);const auto* commission=organizations.grants.find(p.commission_grant_id);const auto* plan=governance.commission_policies.find(p.commission_policy_id);const auto* rule=plan?governance.appeasement_rules.find(plan->rule_id):nullptr;
  if(row.first!=p.id||p.id.empty()||!j||!office||!appointment||!commission||!plan||!rule||p.response_ticks<1||p.mandate_ticks<p.response_ticks||p.mandate_ticks>4096||p.maximum_issues<1||p.maximum_issues>4096)return common_failure("InvalidPoliticalGovernanceAgencyPolicy",row.first);
  if(j->revision!=0||j->political_organization_id!=j->controlling_organization_id||j->office_id!=office->id||office->organization_id!=j->political_organization_id||office->status!=OfficeStatus::active||!office->holder_actor_id||!common.actor(*office->holder_actor_id)||office->deputy_actor_id)return common_failure("PoliticalGovernanceAgencyOriginalOfficeRequired",row.first);
  auto valid=validate_appeasement_rule(*rule);if(!valid.ok)return valid;
  if(!has(office->grant_ids,p.appointment_grant_id)||!has(office->grant_ids,p.commission_grant_id)||appointment->id==commission->id||appointment->office_id!=office->id||commission->office_id!=office->id)return common_failure("PoliticalGovernanceAgencySpecificGrantRequired",row.first);
  const std::vector<StableId> commands{political_governance_agency_offer_command,political_governance_agency_revoke_command};
  if(appointment->allowed_command_kinds!=commands||appointment->amount_limit!=0||!appointment->wallet_ids.empty()||!appointment->inventory_ids.empty()||appointment->may_delegate||appointment->scopes.size()!=1||!site(*appointment,j->seat_place_id)||!dates(*appointment))return common_failure("PoliticalGovernanceAgencySpecificGrantRequired",row.first);
  if(!dates(*commission)||!commission->may_delegate||!has(commission->allowed_command_kinds,StableId{"political_commission_governance"})||!site(*commission,j->seat_place_id)||commission->amount_limit<rule->cash_amount||!has(commission->wallet_ids,j->public_wallet_id)||!has(commission->inventory_ids,p.public_inventory_id)||!has(j->public_inventory_ids,p.public_inventory_id))return common_failure("PoliticalGovernanceAgencyCommissionGrantIncomplete",row.first);
 }
 return common_success();
}
// 只接受四类真实本段来源；原治理authority保留自己的原kind/domain，不借此口解码。
std::optional<Integer> political_governance_agency_fact_sequence(const CommonStore& common,const StableId& id,const StableId& kind){StableId prefix;if(kind=="political_governance_agency_offered")prefix="political_governance_agency_offer";else if(kind=="political_governance_agency_accepted"||kind=="political_governance_agency_declined")prefix="political_governance_agency_answer";else if(kind=="political_governance_agency_revoked")prefix="political_governance_agency_revocation";else if(kind=="political_governance_agency_issued")prefix="political_governance_agency_issue";else return {};const auto* h=common.state().facts.find(id);const auto n=organization_instance_sequence(id,prefix);return h&&h->id==id&&h->kind==kind&&h->domain==political_governance_agency_domain&&n&&*n<common.state().next_instance_id?n:std::nullopt;}
}
