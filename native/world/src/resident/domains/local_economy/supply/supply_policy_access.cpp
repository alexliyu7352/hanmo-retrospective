// 全部条款按原ID提供，当前暂停仅影响新经营；旧合同和历史观察不被现库存覆盖。
#include "supply_policy_internal.hpp"
namespace hanmo::resident {
using namespace economic_supply_detail;
Outcome<EconomicSupplyPolicySource> economic_supply_detail::current(const EconomicSupplyPolicyContext& x,const EconomicSupplyPolicyRecord& r){
 EconomicSupplyPolicySource out;out.issued=terms(x,r);out.observed_tick=observed_tick(x.ledger);out.effective_tick=r.earliest_effective_tick;if(const auto* old=latest(x,r.id))out.last_observation_fact_id=old->observation.id;
 auto adopted=x.adoptions.political_supply_adoption(r.id);if(!adopted.ok)return {false,{},adopted.error};if(!adopted.value){out.unavailable_code="EconomicSupplyPolicyPending";return {true,std::move(out),{}};}
 auto valid=adoption(x,r,*adopted.value,out.observed_tick);if(!valid.ok)return {false,{},valid.error};out.approval_fact_id=adopted.value->approval_fact_id;out.effective_tick=std::max(r.earliest_effective_tick,adopted.value->effective_tick);
 if(out.observed_tick<out.effective_tick)out.unavailable_code="EconomicSupplyPolicyNotEffective";
 else if(out.observed_tick>r.through_tick)out.unavailable_code="EconomicSupplyPolicyExpired";
 else if(!adopted.value->new_contracts_allowed)out.unavailable_code=adopted.value->unavailable_code;
 else {const Owner owner{Owner::Kind::organization,r.policy.organization_id,{}};const auto* store=x.ledger.common.inventory(r.policy.inventory_id);const auto* fund=x.ledger.common.wallet(r.policy.funding_wallet_id);const auto* reward=x.ledger.common.wallet(r.policy.reward_wallet_id);if(!store||!fund||!reward)return {false,{},common_failure("EconomicSupplyExecutionAccountMissing",r.id).error};if(!owner_equal(store->owner,owner)||!owner_equal(fund->owner,owner)||!owner_equal(reward->owner,owner))out.unavailable_code="EconomicSupplyExecutionAccountChanged";else out.new_contracts_allowed=true;}
 return {true,std::move(out),{}};
}
Outcome<std::vector<StableId>> EconomicSupplyPolicyAccess::economic_supply_policy_ids()const{auto valid=validate_economic_supply_policies(context_);if(!valid.ok)return {false,{},valid.error};return {true,context_.ledger.state.policies.order(),{}};}
Outcome<std::optional<EconomicSupplyPolicySource>> EconomicSupplyPolicyAccess::economic_supply_policy(const StableId& id)const{auto valid=validate_economic_supply_policies(context_);if(!valid.ok)return {false,{},valid.error};const auto* r=context_.ledger.state.policies.find(id);if(!r)return {true,{}, {}};auto source=current(context_,*r);if(!source.ok)return {false,{},source.error};return {true,std::move(source.value),{}};}
Outcome<std::optional<EconomicSupplyObservationSource>> EconomicSupplyPolicyAccess::economic_supply_observation(const StableId& id)const{auto valid=validate_economic_supply_policies(context_);if(!valid.ok)return {false,{},valid.error};const auto* r=context_.ledger.state.observations.find(id);if(!r)return {true,{}, {}};return {true,EconomicSupplyObservationSource{context_.ledger.common.state().identity,r->policy_fact_id,r->observation},{}};}
}
