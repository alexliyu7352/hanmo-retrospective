// 冷接册先核原实际占领/批准历史；办理时两个reader自持于同栈，失败由原整候选撤回。
#include "political_shop_registry_integration.hpp"
#include "political_takeover_integration.hpp"
#include "world_bundle.hpp"
namespace hanmo::resident {
Status validate_political_shop_registry_world(const WorldBundle& w){
 auto layout=validate_political_shop_registry_succession_layout(w.common,w.catalog->identity.module_versions,w.catalog->political_shop_registry_successions.get(),w.political_shop_registry_successions_seed_complete?&w.political_shop_registry_successions:nullptr);if(!layout.ok)return layout;
 if(!w.political_shop_registry_successions_enabled())return w.political_shop_registry_successions.version==1&&w.political_shop_registry_successions.acceptances.empty()?common_success():common_failure("UnexpectedPoliticalShopRegistrySuccessionState");
 auto law=PoliticalShopLicensingWorldAccess::create(w);if(!law.ok)return {false,{},law.error};if(!law.value->registry_context())return common_failure("PoliticalShopRegistrySuccessionProfileRequired");
 return validate_political_shop_registry_receipts(*law.value->registry_context(),[](const CommitResult& r){return r.details&&std::holds_alternative<Done>(r.details->data);});
}
Outcome<PoliticalShopRegistryAcceptanceQuote> prepare_political_shop_registry_world(const WorldBundle& w,const StableId& actor,const PoliticalAcceptShopRegistry& command){
 auto guard=command_common_guard(w,actor,political_shop_registry_accept_command);if(!guard.ok)return {false,{},guard.error};
 auto law=PoliticalShopLicensingWorldAccess::create(w);if(!law.ok)return {false,{},law.error};if(!law.value->registry_context())return {false,{},common_failure("PoliticalShopRegistrySuccessionProfileRequired").error};
 auto takeover=PoliticalTakeoverWorldAccess::create(w,*w.catalog->political_takeover,w.political_takeover);if(!takeover.ok)return {false,{},takeover.error};
 return preview_political_shop_registry_acceptance(*law.value->registry_context(),takeover.value->responsibilities(),actor,command);
}
Outcome<StableId> commit_political_shop_registry_world(WorldBundle& w,const StableId& actor,const PoliticalAcceptShopRegistry& command,const PoliticalShopRegistryAcceptanceQuote& quote){
 auto guard=command_common_guard(w,actor,political_shop_registry_accept_command);if(!guard.ok)return {false,{},guard.error};
 auto law=PoliticalShopLicensingWorldAccess::create(w);if(!law.ok)return {false,{},law.error};if(!law.value->registry_context())return {false,{},common_failure("PoliticalShopRegistrySuccessionProfileRequired").error};
 auto takeover=PoliticalTakeoverWorldAccess::create(w,*w.catalog->political_takeover,w.political_takeover);if(!takeover.ok)return {false,{},takeover.error};
 return apply_political_shop_registry_acceptance_in_candidate(w.common,w.political_shop_registry_successions,*law.value->registry_context(),takeover.value->responsibilities(),actor,command,quote);
}
Outcome<BoundaryValue> query_political_shop_registry_world(const WorldBundle& w,const StableId& place,Integer cursor){
 auto law=PoliticalShopLicensingWorldAccess::create(w);if(!law.ok)return {false,{},law.error};if(!law.value->registry_context())return {false,{},common_failure("PoliticalShopRegistrySuccessionProfileRequired").error};
 auto takeover=PoliticalTakeoverWorldAccess::create(w,*w.catalog->political_takeover,w.political_takeover);if(!takeover.ok)return {false,{},takeover.error};
 return query_political_shop_registry_successions(*law.value->registry_context(),takeover.value->responsibilities(),place,cursor);
}
}
