#include "naval_repair_resource_recipe.hpp"
namespace hanmo::resident {
void append_original_naval_repair_material_reservation(ResourceRecipe& recipe,
 const NavalRepairTerms& terms,const StableId& action){
 for(const auto& item:terms.materials)
  recipe.operations.push_back(ReserveLots{terms.inventory_id,item.first,item.second,{terms.owner},action});
}
void append_original_naval_repair_material_terminal(ResourceRecipe& recipe,
 const NavalRepairTerms& terms,const StableId& action,bool success){
 if(success)for(const auto& item:terms.materials)
  recipe.operations.push_back(ConsumeLots{terms.inventory_id,item.first,item.second,{terms.owner},action});
 recipe.operations.push_back(ReleaseActionResources{action});
}
}
