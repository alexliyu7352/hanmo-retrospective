#include "item_service_initial.hpp"
namespace hanmo::resident {
Outcome<InitialItemServiceState> make_initial_item_service(const CommonStore&common,
 const CommonCatalog&shared,const ItemServiceCatalog&catalog,Integer version,
 const RepairPolicyCatalog*policies,const RepairServiceRuleCatalog*rules){
 auto failure=[](const Status&s)->Outcome<InitialItemServiceState>{return {false,{},s.error};};
 if(version!=1&&version!=2&&version!=3)return failure(common_failure("UnsupportedItemInstancesVersion",{},ErrorKind::unsupported_capability));
 auto valid=validate_item_service_catalog(catalog,shared);if(!valid.ok)return failure(valid);
 if(version==3){
  if(!policies||!rules)return failure(common_failure("OriginalRepairCatalogsRequired",{},ErrorKind::unsupported_capability));
  valid=validate_repair_policy_catalog(*policies,shared);if(!valid.ok)return failure(valid);
  valid=validate_repair_service_rule_catalog(*rules,*policies);if(!valid.ok)return failure(valid);
 }else if(policies||rules)return failure(common_failure("UnselectedRepairCatalogs",{},ErrorKind::internal));
 for(const auto&row:common.state().inventories.rows())for(const auto&lot:row.second.lots)
  if(lot.item_instance_id||lot.lot_properties_ref)return failure(common_failure("InitialItemOriginalMaterializationRequired",lot.id,ErrorKind::unsupported_capability));
 // All source tables are genuinely empty until an original owner applies an
 // output. This does not replace the selected full validation/composition gate.
 return {true,{}, {}};
}
}
