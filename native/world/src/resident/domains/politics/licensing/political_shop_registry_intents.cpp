#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/politics/politics_projection_detail.hpp"
// 作者只绑定本人和原院接册政策，身份与去重先验；实际Office/Grant与批准在每次提案和提交重核。
#include "political_shop_registry_intents.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
#include "resident/domains/politics/takeover/politics_takeover_payload_guard.hpp"
#include <set>
namespace hanmo::resident::dynamic_economy_codec {
template<class A>void record(A& a,PoliticalShopRegistryIntent& v){a("id",v.id);a("actor_id",v.actor_id);a("registry_policy_id",v.registry_policy_id);}
template<class A>void record(A& a,PoliticalShopRegistryIntentCatalog& v){a("version",v.version);a("intents",v.intents);}
}
namespace hanmo::resident {namespace {
Status shape(const PoliticalShopRegistryIntentCatalog& c){
 if(c.version!=1)return common_failure("UnsupportedPoliticalShopRegistryIntentVersion");if(c.intents.empty()||c.intents.size()>64)return common_failure("PoliticalShopRegistryIntentLimit");
 for(const auto& row:c.intents.rows()){const auto& v=row.second;if(row.first!=v.id||v.id.empty()||v.id.size()>4096||v.actor_id.empty()||v.actor_id.size()>4096||v.registry_policy_id.empty()||v.registry_policy_id.size()>4096)return common_failure("InvalidPoliticalShopRegistryIntent",row.first);}return common_success();
}
}
Outcome<PoliticalShopRegistryIntentCatalog> decode_political_shop_registry_intents(const BoundaryValue& value){
 auto unique=validate_political_takeover_payload(value);if(!unique.ok)return {false,{},unique.error};try{PoliticalShopRegistryIntentCatalog out;dynamic_economy_codec::decode(value,out,"/political_shop_registry_intents");auto checked=shape(out);if(!checked.ok)return {false,{},checked.error};return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}
}
// 原目录字段借读出口，沿同一record供值与shape发射，不生成业务状态。
Status emit_political_shop_registry_intents_fields(ProjectionWriter& writer,const PoliticalShopRegistryIntentCatalog& value){
 try{return dynamic_economy_codec::emit_fields(writer,value);}
 catch(const boundary::DecodeFailure& e){return writer.reject(e.error.original.code,e.error.original.path);}
}
BoundaryValue encode_political_shop_registry_intents(const PoliticalShopRegistryIntentCatalog& value){return politics_projection_detail::encode_fields(value,emit_political_shop_registry_intents_fields);}
Status validate_political_shop_registry_intents(const PoliticalShopRegistryIntentCatalog& c,const CommonCatalog& common,const PoliticalShopRegistrySuccessionCatalog& registries){
 auto checked=shape(c);if(!checked.ok)return checked;if(!registries.enabled||registries.version!=1)return common_failure("PoliticalShopRegistryIntentProfileRequired");std::set<std::pair<StableId,StableId>> sources;
 for(const auto& row:c.intents.rows()){const auto& v=row.second;const auto* p=registries.policies.find(v.registry_policy_id);if(!p||std::find(common.actor_ids.begin(),common.actor_ids.end(),v.actor_id)==common.actor_ids.end())return common_failure("PoliticalShopRegistryIntentSourceMismatch",row.first);if(!sources.emplace(v.actor_id,p->court_id).second)return common_failure("DuplicatePoliticalShopRegistryIntent",row.first);}return common_success();
}
}
