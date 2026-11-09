#include "resident/domains/military/military_model.hpp"
// 不借旧产权history读口证明从未发生的所有权；单独核显式条件性职责声明。
#include "politics_takeover_authority.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
bool political_takeover_grant_declares_resource(const CommonCatalog& common,const OrganizationCatalog& organizations,const PoliticsCatalog& politics,const MilitaryCatalog& military,const PoliticalTakeoverCatalog& catalog,const StableId& grant_id,const ResourceKey& resource){
    if(!catalog.enabled||!validate_political_takeover_catalog(common,organizations,politics,military,catalog).ok)return false;
    const auto* grant=organizations.grants.find(grant_id);if(!grant||grant->allowed_command_kinds!=std::vector<StableId>{political_takeover_command_kind})return false;
    if(resource.kind!=ResourceKey::Kind::wallet&&resource.kind!=ResourceKey::Kind::inventory)return false;
    const auto exact=[](const auto& left,const auto& right){const std::set<StableId> a(left.begin(),left.end()),b(right.begin(),right.end());return a.size()==left.size()&&b.size()==right.size()&&a==b;};
    for(const auto& row:catalog.rules.rows()){
        const auto& rule=row.second;if(rule.successor_office_id!=grant->office_id||grant->scopes.size()!=1||grant->scopes.front().kind!=ScopeRef::Kind::site||grant->scopes.front().id!=rule.site_id)continue;
        const auto hold=std::find_if(military.strongholds.begin(),military.strongholds.end(),[&](const auto& value){return value.id==rule.stronghold_id;});if(hold==military.strongholds.end()||!exact(grant->wallet_ids,hold->public_wallet_ids)||!exact(grant->inventory_ids,hold->public_inventory_ids))continue;
        const auto& selected=resource.kind==ResourceKey::Kind::wallet?grant->wallet_ids:grant->inventory_ids;if(std::find(selected.begin(),selected.end(),resource.id)==selected.end())continue;
        const auto* definition=resource.kind==ResourceKey::Kind::wallet?common.required_wallets.find(resource.id):common.required_inventories.find(resource.id);
        if(definition&&definition->owner.kind==Owner::Kind::organization&&organizations.organizations.find(definition->owner.id))return true;
    }
    return false;
}
}
