#include "resident/kernel/catalog_section_observation.hpp"
#include "native_military_battle_lifecycle.hpp"
#include "native_military_occupation.hpp"
#include "world_bundle.hpp"
#include "resident/domains/military/military_battle_lifecycle.hpp"
namespace hanmo::resident {
Outcome<Integer> selected_native_military_battle_lifecycle(const CatalogIdentity& id){
 Integer selected=0;for(const auto& row:id.module_versions)if(row.first=="military_battle_lifecycle"){
  if(selected||row.second!="1")return {false,0,common_failure("MilitaryBattleLifecycleModuleMismatch",row.first).error};selected=1;
 }return {true,selected,{}};
}
Status append_native_military_battle_lifecycle_module(const BoundaryValue& scenario,CatalogIdentity& id){try{
 auto old=selected_native_military_battle_lifecycle(id);if(!old.ok)return {false,{},std::move(old.error)};if(old.value)return common_failure("DuplicateMilitaryBattleLifecycleModule");
 const BoundaryValue* found=nullptr;for(const auto& row:boundary::object(scenario,"scenario"))if(row.first=="military_battle_lifecycle"){if(found)return common_failure("DuplicateMilitaryBattleLifecycleSelection");found=&row.second;}
 if(!found)return common_success();boundary::fields(*found,{"profile"},{},"military_battle_lifecycle");
 if(boundary::object(*found,"military_battle_lifecycle").size()!=1||boundary::get_string_ref(*found,"profile","military_battle_lifecycle")!="original_military_battle_lifecycle_v1")return common_failure("UnsupportedMilitaryBattleLifecycleSelection",{},ErrorKind::unsupported_capability);
 id.module_versions.emplace_back("military_battle_lifecycle","1");return common_success();
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status validate_native_military_battle_lifecycle_catalog(const PreparedCatalog& catalog){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 143, "PreparedCatalog", "validate_native_military_battle_lifecycle_catalog", &catalog);

 auto selected=selected_native_military_battle_lifecycle(catalog.identity);if(!selected.ok)return {false,{},std::move(selected.error)};if(!selected.value)return common_success();
 if(!catalog.common||!catalog.military||!catalog.exact_time)return common_failure("MilitaryBattleLifecycleExactSourcesRequired",{},ErrorKind::unsupported_capability);
 return validate_native_military_occupation_catalog(catalog);
}
Status validate_native_military_battle_lifecycle_layout(const WorldBundle& world){
 if(!world.catalog)return common_failure("MilitaryBattleLifecycleCatalogRequired");auto valid=validate_native_military_battle_lifecycle_catalog(*world.catalog);if(!valid.ok)return valid;
 auto selected=selected_native_military_battle_lifecycle(world.catalog->identity);if(!selected.ok)return {false,{},std::move(selected.error)};
 if(selected.value&&!world.world_timeline)return common_failure("MilitaryBattleLifecycleTimelineRequired",{},ErrorKind::unsupported_capability);
 valid=validate_military_battle_lifecycle_layout(world.military,selected.value);if(!valid.ok)return valid;
 if(!selected.value)for(const auto& item:world.common.state().facts.rows())if(item.second.domain=="military"&&(item.second.kind=="military_battle_started"||item.second.kind=="military_battle_closed"))return common_failure("UnexpectedMilitaryBattleLifecycleSource",item.first);
 return common_success();
}
Status validate_native_military_battle_lifecycle_sources(const WorldBundle& world){
 auto valid=validate_native_military_battle_lifecycle_layout(world);if(!valid.ok)return valid;auto selected=selected_native_military_battle_lifecycle(world.catalog->identity);if(!selected.ok)return {false,{},std::move(selected.error)};if(!selected.value)return common_success();
 auto now=world.actual_phase_moment();if(!now.ok)return {false,{},std::move(now.error)};
 return validate_military_battle_lifecycle_sources(world.common,world.military,selected.value,now.value);
}
}
