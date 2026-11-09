// 读对象拥有其全部政治/经济借用；不将临时context存入World或Director状态。
#include "political_supply_renewal_integration.hpp"
#include "economic_supply_policy_integration.hpp"
#include "world_bundle.hpp"
namespace hanmo::resident {
Status validate_political_supply_renewal_world(const WorldBundle& w){
    const bool enabled=w.catalog->director&&w.catalog->director->supply_renewals;
    auto layout=validate_political_supply_renewal_modules(enabled,w.catalog->identity.module_versions);if(!layout.ok)return layout;
    if(!w.catalog->director)return common_success();
    if(!enabled)return validate_political_supply_renewal_director(*w.catalog->director,w.director,nullptr,nullptr);
    auto access=EconomicSupplyWorldAccess::create(w);if(!access.ok)return {false,{},access.error};
    return validate_political_supply_renewal_director(*w.catalog->director,w.director,&access.value->political_context(),&access.value->context());
}
Outcome<std::optional<DecisionProposal>> propose_political_supply_renewal_world(const WorldBundle& w,
    const KnownSubjectView& known,const DirectorPolicy& policy,const DecisionProposal& base){
    if(!w.catalog->director||!w.catalog->director->supply_renewals||!policy.supply_renewal_intent_id)
        return {false,{},common_failure("PoliticalSupplyRenewalDirectorProfileRequired",known.actor_id,ErrorKind::internal).error};
    const auto* intent=w.catalog->director->supply_renewals->intents.find(*policy.supply_renewal_intent_id);
    if(!intent)return {false,{},common_failure("PoliticalSupplyRenewalIntentMismatch",*policy.supply_renewal_intent_id,ErrorKind::internal).error};
    auto access=EconomicSupplyWorldAccess::create(w);if(!access.ok)return {false,{},access.error};
    return propose_director_supply_renewal(access.value->political_context(),access.value->context(),known,policy,*intent,base,w.catalog->director->candidates_per_subject);
}
}
