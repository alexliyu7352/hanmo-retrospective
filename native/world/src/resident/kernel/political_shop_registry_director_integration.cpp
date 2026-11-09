#include "political_shop_registry_director_integration.hpp"
#include "political_shop_registry_integration.hpp"
#include "political_takeover_integration.hpp"
#include "world_bundle.hpp"
namespace hanmo::resident {
Status validate_political_shop_registry_director_world(const WorldBundle& w){
 const bool enabled=w.catalog->director&&w.catalog->director->shop_registry_intents;auto valid=validate_political_shop_registry_intent_modules(enabled,w.catalog->identity.module_versions);if(!valid.ok)return valid;if(!w.catalog->director)return common_success();
 if(!enabled)return validate_political_shop_registry_intent_director(*w.catalog->director,w.director,nullptr);
 auto law=PoliticalShopLicensingWorldAccess::create(w);if(!law.ok)return {false,{},law.error};if(!law.value->registry_context())return common_failure("PoliticalShopRegistryIntentProfileRequired");
 return validate_political_shop_registry_intent_director(*w.catalog->director,w.director,law.value->registry_context());
}
Outcome<std::optional<DecisionProposal>> propose_political_shop_registry_director_world(const WorldBundle& w,const KnownSubjectView& known,const DirectorPolicy& policy,const DecisionProposal& base){
 if(!w.catalog->director||!w.catalog->director->shop_registry_intents||!policy.shop_registry_intent_id)return {false,{},common_failure("PoliticalShopRegistryIntentProfileRequired",known.actor_id,ErrorKind::internal).error};
 const auto* intent=w.catalog->director->shop_registry_intents->intents.find(*policy.shop_registry_intent_id);if(!intent)return {false,{},common_failure("PoliticalShopRegistryIntentSourceMismatch",*policy.shop_registry_intent_id,ErrorKind::internal).error};if(intent->actor_id!=known.actor_id)return {true,std::nullopt,{}};
 auto law=PoliticalShopLicensingWorldAccess::create(w);if(!law.ok)return {false,{},law.error};if(!law.value->registry_context()||!w.catalog->political_takeover||!w.catalog->travel)return {false,{},common_failure("PoliticalShopRegistryIntentProfileRequired").error};
 auto takeover=PoliticalTakeoverWorldAccess::create(w,*w.catalog->political_takeover,w.political_takeover);if(!takeover.ok)return {false,{},takeover.error};
 return propose_director_shop_registry_acceptance(*law.value->registry_context(),takeover.value->responsibilities(),*w.catalog->travel,known,policy,*intent,base,w.catalog->director->candidates_per_subject);
}
}
