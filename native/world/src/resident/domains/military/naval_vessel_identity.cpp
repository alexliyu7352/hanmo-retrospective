#include "naval_vessel_identity.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
Status validate_original_naval_vessel_identity(const NavalVesselRecord& actual,
 const NavalVesselDefinition& original){
 if(actual.id!=original.id||actual.hull_id!=original.hull_id||
    actual.cargo_inventory_id!=original.cargo_inventory_id||
    actual.origin_source!="author:"+original.id)
  return common_failure("NavalAuthorSourceMismatch",original.id);
 return common_success();
}
Outcome<const NavalVesselDefinition*> read_original_naval_vessel_identity(
 const NavalAssetState& state,const NavalAssetCatalog& catalog,const StableId& id){
 const NavalVesselRecord* actual=nullptr;const NavalVesselDefinition* original=nullptr;
 for(const auto& row:state.vessels)if(row.id==id){
  if(actual)return {false,nullptr,common_failure("NavalAuthorSourceMismatch",id).error};
  actual=&row;
 }
 for(const auto& row:catalog.vessels)if(row.id==id){
  if(original)return {false,nullptr,common_failure("NavalAuthorSourceMismatch",id).error};
  original=&row;
 }
 if(id.empty()||!actual||!original)
  return {false,nullptr,common_failure("NavalAuthorSourceRequired",id).error};
 auto valid=validate_original_naval_vessel_identity(*actual,*original);
 if(!valid.ok)return {false,nullptr,std::move(valid.error)};
 return {true,original,{}};
}
}
