#include "resident/kernel/catalog_section_observation.hpp"
#include "native_personal_battle_lifecycle.hpp"
#include "world_bundle.hpp"
#include "resident/domains/personal/personal_battle_lifecycle.hpp"
namespace hanmo::resident {
Outcome<Integer> selected_native_personal_battle_lifecycle(const CatalogIdentity& id){
 Integer selected=0;for(const auto& row:id.module_versions)if(row.first=="personal_battle_lifecycle"){
  if(selected||row.second!="1")return {false,0,common_failure("PersonalBattleLifecycleModuleMismatch",row.first).error};selected=1;
 }return {true,selected,{}};
}
Status append_native_personal_battle_lifecycle_module(const BoundaryValue& scenario,CatalogIdentity& id){try{
 auto old=selected_native_personal_battle_lifecycle(id);if(!old.ok)return {false,{},std::move(old.error)};if(old.value)return common_failure("DuplicatePersonalBattleLifecycleModule");
 const BoundaryValue* found=nullptr;for(const auto& row:boundary::object(scenario,"scenario"))if(row.first=="personal_battle_lifecycle"){if(found)return common_failure("DuplicatePersonalBattleLifecycleSelection");found=&row.second;}
 if(!found)return common_success();boundary::fields(*found,{"profile"},{},"personal_battle_lifecycle");
 if(boundary::object(*found,"personal_battle_lifecycle").size()!=1||boundary::get_string_ref(*found,"profile","personal_battle_lifecycle")!="original_personal_battle_lifecycle_v1")return common_failure("UnsupportedPersonalBattleLifecycleSelection",{},ErrorKind::unsupported_capability);
 id.module_versions.emplace_back("personal_battle_lifecycle","1");return common_success();
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status validate_native_personal_battle_lifecycle_catalog(const PreparedCatalog& catalog){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 148, "PreparedCatalog", "validate_native_personal_battle_lifecycle_catalog", &catalog);

 auto selected=selected_native_personal_battle_lifecycle(catalog.identity);if(!selected.ok)return {false,{},std::move(selected.error)};if(!selected.value)return common_success();
 Integer physical=0;for(const auto& row:catalog.identity.module_versions)if(row.first=="personal"){if(++physical!=1||row.second!="1")return common_failure("PersonalBattleLifecyclePersonal1Required");}
 if(physical!=1||!catalog.common||!catalog.personal||!catalog.interaction||!catalog.exact_time)return common_failure("PersonalBattleLifecycleExactSourcesRequired",{},ErrorKind::unsupported_capability);
 return common_success();
}
Status validate_native_personal_battle_lifecycle_layout(const WorldBundle& world){
 if(!world.catalog)return common_failure("PersonalBattleLifecycleCatalogRequired");auto valid=validate_native_personal_battle_lifecycle_catalog(*world.catalog);if(!valid.ok)return valid;
 auto selected=selected_native_personal_battle_lifecycle(world.catalog->identity);if(!selected.ok)return {false,{},std::move(selected.error)};
 if(selected.value&&!world.world_timeline)return common_failure("PersonalBattleLifecycleTimelineRequired",{},ErrorKind::unsupported_capability);
 valid=validate_personal_battle_lifecycle_layout(world.personal,selected.value);if(!valid.ok)return valid;
 if(!selected.value)for(const auto& item:world.common.state().facts.rows())if(item.second.domain=="personal"&&(item.second.kind=="personal_battle_started"||item.second.kind=="personal_battle_closed"))return common_failure("UnexpectedPersonalBattleLifecycleSource",item.first);
 return common_success();
}
Status validate_native_personal_battle_lifecycle_sources(const WorldBundle& world){
 auto valid=validate_native_personal_battle_lifecycle_layout(world);if(!valid.ok)return valid;auto selected=selected_native_personal_battle_lifecycle(world.catalog->identity);if(!selected.ok)return {false,{},std::move(selected.error)};if(!selected.value)return common_success();
 auto now=world.actual_phase_moment();if(!now.ok)return {false,{},std::move(now.error)};
 return validate_personal_battle_lifecycle_sources(world.common,world.personal,selected.value,now.value);
}
}
