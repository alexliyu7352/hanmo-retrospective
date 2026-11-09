#include "resident/domains/military/military_model.hpp"
// 条件性资源声明仅覆盖原捕获清单和该机构原有公库，不放宽普通Grant的Owner规则。
#include "politics_supply_authority.hpp"
#include "resident/domains/politics/takeover/politics_takeover_catalog.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
bool political_supply_grant_declares_resource(const CommonCatalog& common,const OrganizationCatalog& organizations,const PoliticsCatalog& politics,const MilitaryCatalog& military,const PoliticalTakeoverCatalog& takeover,bool enabled,const StableId& id,const ResourceKey& resource){
 if(!enabled||!takeover.enabled||!validate_political_takeover_catalog(common,organizations,politics,military,takeover).ok||(resource.kind!=ResourceKey::Kind::wallet&&resource.kind!=ResourceKey::Kind::inventory))return false;
 const auto* grant=organizations.grants.find(id);const auto* office=grant?organizations.offices.find(grant->office_id):nullptr;if(!grant||!office||grant->allowed_command_kinds!=std::vector<StableId>{political_supply_policy_grant}||grant->amount_limit<0||grant->scopes.size()!=1||grant->scopes.front().kind!=ScopeRef::Kind::site||std::find(office->grant_ids.begin(),office->grant_ids.end(),id)==office->grant_ids.end())return false;
 auto contains=[](const auto& rows,const auto& value){return std::find(rows.begin(),rows.end(),value)!=rows.end();};const auto& requested=resource.kind==ResourceKey::Kind::wallet?grant->wallet_ids:grant->inventory_ids;if(!contains(requested,resource.id))return false;
 for(const auto& row:takeover.rules.rows()){
  const auto& rule=row.second;if(rule.successor_office_id!=office->id||rule.site_id!=grant->scopes.front().id)continue;const auto hold=std::find_if(military.strongholds.begin(),military.strongholds.end(),[&](const auto& value){return value.id==rule.stronghold_id;});if(hold==military.strongholds.end())continue;
  auto valid=[&](const std::vector<StableId>& ids,bool inventory){std::set<StableId> unique;for(const auto& asset:ids){const auto* definition=inventory?common.required_inventories.find(asset):common.required_wallets.find(asset);if(!unique.insert(asset).second||!definition||definition->owner.kind!=Owner::Kind::organization||!organizations.organizations.find(definition->owner.id))return false;if(definition->owner.id!=office->organization_id&&!contains(inventory?hold->public_inventory_ids:hold->public_wallet_ids,asset))return false;}return true;};
  if(valid(grant->wallet_ids,false)&&valid(grant->inventory_ids,true))return true;
 }
 return false;
}
}
