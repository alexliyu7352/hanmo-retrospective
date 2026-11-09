#include "resident/kernel/catalog_section_observation.hpp"
#include "political_shop_registry_detail.hpp"
#include <set>
namespace hanmo::resident {
Status validate_political_shop_registry_catalog(const PoliticalShopRegistryContext& x){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 101, "PoliticalShopRegistryContext", "validate_political_shop_registry_catalog", &x);

 const auto& l=x.legal;if(!x.catalog.enabled||x.catalog.version!=1||x.state.version!=1||x.catalog.policies.empty()||x.catalog.policies.size()>4096||&l.common!=&x.control.common||l.phase!=x.control.phase)return common_failure("PoliticalShopRegistryProfileRequired");
 auto legal=validate_political_shop_licensing_catalog(l);if(!legal.ok)return legal;
 std::set<std::pair<StableId,StableId>> pairs;
 for(const auto& row:x.catalog.policies.rows()){if(row.first!=row.second.id||!pairs.emplace(row.second.court_id,row.second.organization_id).second)return common_failure("PoliticalShopRegistryPolicyMismatch",row.first);auto endpoint=shop_registry_detail::endpoint(x,row.second);if(!endpoint.ok)return {false,{},endpoint.error};}
 return common_success();
}
Status validate_political_shop_registry_successions(const PoliticalShopRegistryContext& x){
 auto checked=validate_political_shop_registry_catalog(x);if(!checked.ok)return checked;const auto& l=x.legal;
 if(x.state.acceptances.size()>65536)return common_failure("PoliticalShopRegistryHistoryLimit");
 std::map<StableId,StableId> latest;std::map<StableId,StableId> last_registration;std::set<StableId> covered;Integer last_sequence=0,last_tick=-1;StableId previous_fact;
 for(const auto& row:x.state.acceptances.rows()){const auto& a=row.second;const auto* policy=x.catalog.policies.find(a.policy_id);const auto* court=l.catalog.courts.find(a.court_id);const auto* head=l.common.state().facts.find(row.first);auto sequence=organization_instance_sequence(row.first,political_shop_registry_accept_prefix);
  if(row.first!=a.id||!policy||policy->court_id!=a.court_id||!court||!sequence||*sequence<=last_sequence||*sequence>=l.common.state().next_instance_id||a.tick<last_tick||a.tick<0||a.tick>l.common.tick()||!head||head->id!=a.id||head->kind!=political_shop_registry_accept_kind||head->domain!=political_shop_registry_succession_module||(!previous_fact.empty()&&!shop_law_detail::before(l.common,previous_fact,a.id)))return common_failure("PoliticalShopRegistryAcceptanceMismatch",row.first);
  const auto p=latest.find(a.court_id);const auto expected=p==latest.end()?std::nullopt:std::optional<StableId>{p->second};if(a.previous_acceptance_fact_id!=expected||last_registration[a.court_id]==a.registration_fact_id)return common_failure("PoliticalShopRegistryAcceptanceChainMismatch",a.id);
  auto endpoint=shop_registry_detail::endpoint(x,*policy);if(!endpoint.ok)return {false,{},endpoint.error};auto control=read_political_shop_registry_control(x.control,l.politics,l.politics_catalog,*court,a.id,a.tick);if(!control.ok)return {false,{},control.error};
  if(control.value.revision<1||control.value.registration_fact_id!=a.registration_fact_id||control.value.approval_fact_id.empty()||control.value.approval_fact_id!=a.approval_fact_id||control.value.occupation_fact_id!=a.occupation_fact_id||control.value.organization_id!=endpoint.value.organization_id)return common_failure("PoliticalShopRegistryControlMismatch",a.id);
  checked=shop_registry_detail::authority_before(x,endpoint.value,a.authority,a.grant_id,a.id,a.tick);if(!checked.ok)return checked;
  auto book=read_political_shop_registry_legal_sources(l.common,l.catalog,l.state,a.court_id,a.id,a.tick);if(!book.ok)return {false,{},book.error};if(book.value.initial_registry_fact_id!=a.initial_registry_fact_id||book.value.fact_ids!=a.retained_legal_fact_ids)return common_failure("PoliticalShopRegistryBookCoverageMismatch",a.id);
  latest[a.court_id]=a.id;last_registration[a.court_id]=a.registration_fact_id;covered.insert(a.id);last_sequence=*sequence;last_tick=a.tick;previous_fact=a.id;
 }
 for(const auto& row:l.common.state().facts.rows())if((row.second.kind==political_shop_registry_accept_kind||row.second.domain==political_shop_registry_succession_module)&&!covered.count(row.first))return common_failure("PoliticalShopRegistryPayloadRequired",row.first);
 return validate_political_shop_registry_player_bindings(l.common,x.state);
}
}
