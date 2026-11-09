#include "private_cohort_household_origin.hpp"
#include <algorithm>
#include <new>
#include <stdexcept>
namespace hanmo::resident {
Outcome<PrivateCohortHouseholdOrigin> read_private_cohort_household_before_source(
 const CommonStore& c,const LocalEconomyState& state,const LocalEconomyCatalog& catalog,
 const StableId& id,const SourceRef& cut){try{
 auto fail=[&](const char*code){return Outcome<PrivateCohortHouseholdOrigin>{false,{},common_failure(code,id).error};};
 const auto* header=c.state().facts.find(cut.id);
 if(!valid_source_ref(cut)||!header||header->id!=cut.id||header->kind!=cut.kind||header->domain!=cut.domain||
    std::count(c.state().facts.order().begin(),c.state().facts.order().end(),cut.id)!=1)
  return fail("PrivateCohortActualAdoptionRequired");
 if(!catalog.enabled)return fail("PrivateCohortOriginalModuleRequired");
 const auto* seed=catalog.cohorts.find(id);const auto* current=state.cohorts.find(id);
 if(!seed||!current||seed->id!=id||current->id!=id||seed->household_group_id.empty())
  return fail("PrivateCohortOriginalDefinitionRequired");
 const auto* home=catalog.households.find(seed->household_group_id);
 const auto* actual=state.households.find(seed->household_group_id);
 if(!home||!actual||home->id!=seed->household_group_id||actual->id!=home->id||
    home->wallet_id.empty()||home->inventory_id.empty()||
    seed->private_wallet_id!=home->wallet_id||seed->private_inventory_id!=home->inventory_id)
  return fail("PrivateCohortOriginalHouseholdMismatch");
 // The only original split/transfer changes numbers in pre-authored cohorts.
 // No writer changes cohort household identity; reject a changed current pair
 // instead of inventing a historical membership or deriving it from today's owner.
 if(current->household_group_id!=seed->household_group_id||
    current->private_wallet_id!=seed->private_wallet_id||current->private_inventory_id!=seed->private_inventory_id||
    actual->wallet_id!=home->wallet_id||actual->inventory_id!=home->inventory_id)
  return fail("PrivateCohortUnregisteredIdentityChange");
 return {true,{id,home->id,home->wallet_id,home->inventory_id,
   {"local_economy_catalog","cohort",id},{"local_economy_catalog","household",home->id}}, {}};
 }catch(const std::bad_alloc&){return {false,{},common_failure("PrivateCohortReadExhausted").error};}
 catch(const std::length_error&){return {false,{},common_failure("PrivateCohortReadExhausted").error};}}
}
