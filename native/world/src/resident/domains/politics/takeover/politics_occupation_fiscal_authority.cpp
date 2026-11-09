// 作者只声明该辖区明确接任Office的新税政公库范围；运行仍核实际Owner和现任权限。
#include "politics_occupation_fiscal.hpp"
#include "politics_takeover_catalog.hpp"
#include <set>
namespace hanmo::resident {
bool occupation_fiscal_grant_declares_resource(const CommonCatalog& common,const OrganizationCatalog& organizations,const PoliticsCatalog& politics,const MilitaryCatalog& military,const PoliticalTakeoverCatalog& takeover,const OccupationFiscalCatalog& fiscal,const StableId& id,const ResourceKey& resource){
    if(!fiscal.enabled||fiscal.version!=1||!takeover.enabled||!validate_political_takeover_catalog(common,organizations,politics,military,takeover).ok)return false;
    if(resource.kind!=ResourceKey::Kind::wallet&&resource.kind!=ResourceKey::Kind::inventory)return false;
    const auto* grant=organizations.grants.find(id);if(!grant||grant->allowed_command_kinds!=std::vector<StableId>{political_occupation_fiscal_command_kind}||grant->wallet_ids.size()!=1||grant->inventory_ids.empty()||grant->scopes.size()!=1||grant->scopes.front().kind!=ScopeRef::Kind::site)return false;
    const auto* office=organizations.offices.find(grant->office_id);if(!office||std::find(office->grant_ids.begin(),office->grant_ids.end(),grant->id)==office->grant_ids.end())return false;
    for(const auto& row:takeover.rules.rows()){
        const auto& rule=row.second;const auto* jurisdiction=politics.jurisdictions.find(rule.jurisdiction_id);if(!jurisdiction||rule.successor_office_id!=grant->office_id||rule.site_id!=grant->scopes.front().id||grant->wallet_ids.front()!=jurisdiction->public_wallet_id)continue;
        std::set<StableId> inventory_ids;bool valid=true;for(const auto& inventory:grant->inventory_ids)if(!inventory_ids.insert(inventory).second||std::find(jurisdiction->public_inventory_ids.begin(),jurisdiction->public_inventory_ids.end(),inventory)==jurisdiction->public_inventory_ids.end())valid=false;if(!valid)continue;
        const auto& selected=resource.kind==ResourceKey::Kind::wallet?grant->wallet_ids:grant->inventory_ids;if(std::find(selected.begin(),selected.end(),resource.id)==selected.end())continue;
        const auto* definition=resource.kind==ResourceKey::Kind::wallet?common.required_wallets.find(resource.id):common.required_inventories.find(resource.id);if(definition&&definition->owner.kind==Owner::Kind::organization&&organizations.organizations.find(definition->owner.id))return true;
    }
    return false;
}
}
