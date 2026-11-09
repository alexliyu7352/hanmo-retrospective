// Fresh owning roots only. Restore must use the original paired decoder and
// full source validators, never call this function to reconstruct missing rows.
#pragma once
#include "item_service.hpp"
#include "repair_policy.hpp"
#include "resident/domains/commerce/repair_service/repair_service_rules.hpp"
namespace hanmo::resident {
struct InitialItemServiceState {ItemInstancesState instances;LotPropertiesState properties;};
// No fabricated initial unique item. Existing Common lot references require
// their actual original output/materialization producer; absent source rejects.
Outcome<InitialItemServiceState> make_initial_item_service(const CommonStore&,
 const CommonCatalog&,const ItemServiceCatalog&,Integer selected_version,
 const RepairPolicyCatalog* repair_policies=nullptr,
 const RepairServiceRuleCatalog* repair_service_rules=nullptr);
}
