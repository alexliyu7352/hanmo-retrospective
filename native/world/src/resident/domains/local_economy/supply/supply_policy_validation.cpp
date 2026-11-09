// 政治签权与采用的完整历史前置；纯本账由独立ledger校验以免责任枚举递归。
#include "supply_policy_internal.hpp"
namespace hanmo::resident {
using namespace economic_supply_detail;
Status validate_economic_supply_policies(const EconomicSupplyPolicyContext& x){
 auto valid=validate_economic_supply_policy_ledger(x.ledger);if(!valid.ok)return valid;
 // 只核不可变岗位资格，当前在任人由原Task会面守卫核，不倒填历史人事。
 if(x.ledger.catalog.policies.empty()||x.ledger.catalog.policies.size()>4096)return common_failure("InvalidEconomicSupplyPolicyCatalogSize");
 for(const auto& row:x.ledger.catalog.policies.rows()){const auto& policy=row.second;const auto* task=x.tasks.templates.find(policy.template_id);if(!task||task->template_id!="procurement"||task->accept_ticks<=0)return common_failure("UnknownDynamicTaskTemplate",policy.template_id);
  const auto* source=x.ledger.economy_catalog.supply_sources.find(policy.original_source_key);if(!source)return common_failure("InvalidEconomicSupplyCatalog",row.first);
  for(const auto& role:std::vector<std::pair<StableId,StableId>>{{policy.accept_role,"accept_offer"},{policy.report_role,"report_offer"}}){const auto* definition=x.ledger.common_catalog.roles.find(role.first);if(!definition||definition->organization_id!=std::optional<StableId>{policy.organization_id}||definition->place_id!=source->site_id||std::find(definition->services.begin(),definition->services.end(),role.second)==definition->services.end())return common_failure("InvalidEconomicSupplyPolicyRole",role.first);}
 }
 for(const auto& row:x.ledger.state.policies.rows()){const auto& r=row.second;auto m=mandate(r.registration_fact_id,r.original_source_key,r.policy);if(!m.ok)return {false,{},m.error};auto original=x.authority.political_supply_policy_authority_before_issue(r.authority,m.value,r.grant_id,r.id,r.issued_tick);if(!original.ok)return {false,{},original.error};valid=proof(x,original.value,m.value,r.authority,r.grant_id,r.issued_tick);if(!valid.ok)return valid;}
 for(const auto& row:x.ledger.state.observations.rows()){const auto& o=row.second;const auto& r=*x.ledger.state.policies.find(o.policy_fact_id);auto approved=x.adoptions.political_supply_adoption_before_observation(r.id,o.observation.id,o.observation.tick);if(!approved.ok)return {false,{},approved.error};if(!approved.value)return common_failure("EconomicSupplyObservationUnapproved",row.first);valid=adoption(x,r,*approved.value,o.observation.tick);if(!valid.ok)return valid;if(!approved.value->new_contracts_allowed||std::max(r.earliest_effective_tick,approved.value->effective_tick)>o.observation.tick||!before(x.ledger.common,approved.value->approval_fact_id,o.observation.id))return common_failure("EconomicSupplyObservationNotPermitted",row.first);}
 return common_success();
}
Outcome<EconomicSupplyPolicyState> make_initial_economic_supply_policies(const CommonStore& common){if(common.tick()<0)return {false,{},common_failure("InvalidEconomicSupplyClock").error};EconomicSupplyPolicyState state;state.initialized_tick=state.last_observation_tick=common.tick();return {true,std::move(state),{}};}
}
