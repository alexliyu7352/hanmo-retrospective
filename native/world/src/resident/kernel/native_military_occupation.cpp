#include "resident/kernel/catalog_section_observation.hpp"
#include "native_military_occupation.hpp"
#include "exact_time_profile.hpp"
#include "world_bundle.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/domains/military/occupation_time.hpp"
namespace hanmo::resident {
namespace {
Outcome<Integer> selection(const BoundaryValue& scenario){try{
    using namespace boundary;const auto* dynamic=scenario.find("dynamic_world");
    if(!dynamic)return {true,1,{}};
    const BoundaryValue* selected=nullptr;
    for(const auto& row:object(*dynamic,"scenario/dynamic_world"))if(row.first=="military_occupation"){
        if(selected)fail("DuplicateMilitaryOccupationSelection","scenario/dynamic_world");selected=&row.second;
    }
    if(!selected)return {true,1,{}};
    fields(*selected,{"version"},{},"military_occupation");
    if(object(*selected,"military_occupation").size()!=1||get_int(*selected,"version","military_occupation")!=2)
        unsupported("UnsupportedMilitaryOccupationVersion","military_occupation/version");
    auto exact=decode_exact_time_profile(scenario);if(!exact.ok)return {false,0,std::move(exact.error)};
    if(!exact.value||!exact_complete_selected(*exact.value))
        unsupported("MilitaryOccupationCompleteProfileRequired","military_occupation");
    return {true,2,{}};
}catch(const boundary::DecodeFailure& e){return {false,0,e.error};}}
Status append(CatalogIdentity& identity,Integer selected){
    for(const auto& row:identity.module_versions)if(row.first=="military_occupation")
        return common_failure("DuplicateMilitaryOccupationMarker");
    if(selected==2)identity.module_versions.emplace_back("military_occupation","2");
    return common_success();
}
}
Status append_native_military_occupation_module(const BoundaryValue& scenario,CatalogIdentity& identity){
    auto selected=selection(scenario);if(!selected.ok)return {false,{},std::move(selected.error)};
    return append(identity,selected.value);
}
Status prepare_native_military_occupation_selection(const BoundaryValue& scenario,PreparedCatalog& catalog){
    if(catalog.selected_military_occupation_schema!=1)return common_failure("MilitaryOccupationAlreadyPrepared");
    auto selected=selection(scenario);if(!selected.ok)return {false,{},std::move(selected.error)};
    auto added=append(catalog.identity,selected.value);if(!added.ok)return added;
    catalog.selected_military_occupation_schema=selected.value;return common_success();
}
Outcome<Integer> selected_military_occupation_schema(const CatalogIdentity& identity){
    Integer selected=1;
    for(const auto& row:identity.module_versions)if(row.first=="military_occupation"){
        if(selected!=1||row.second!="2")return {false,0,common_failure("MilitaryOccupationProfileMismatch").error};
        selected=2;
    }
    return {true,selected,{}};
}
Status validate_native_military_occupation_catalog(const PreparedCatalog& catalog){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 144, "PreparedCatalog", "validate_native_military_occupation_catalog", &catalog);

    auto selected=selected_military_occupation_schema(catalog.identity);if(!selected.ok)return {false,{},std::move(selected.error)};
    if(selected.value!=catalog.selected_military_occupation_schema)
        return common_failure("MilitaryOccupationProfileMismatch");
    if(selected.value==2&&(!catalog.exact_time||!exact_complete_selected(*catalog.exact_time)||!catalog.military))
        return common_failure("MilitaryOccupationCompleteProfileRequired");
    return common_success();
}
Outcome<std::uint32_t> military_checkpoint_base_version(std::uint32_t physical,Integer selected){
    if(physical<64)return {false,0,common_failure("MilitarySupplyCauseCheckpointVersionRequired").error};
    physical-=64;
    if(selected==1&&physical>=3&&physical<=17)return {true,physical,{}};
    if(selected==2&&physical>=35&&physical<=49)return {true,physical-32,{}};
    return {false,0,common_failure("MilitaryOccupationCheckpointProfileMismatch").error};
}
Status validate_native_military_occupation_selection(const WorldBundle& world){
    if(!world.catalog)return common_failure("MilitaryOccupationCatalogRequired");
    auto selected=validate_native_military_occupation_catalog(*world.catalog);if(!selected.ok)return selected;
    if(world.catalog->selected_military_occupation_schema==1){
        for(const auto& row:world.military.occupation_facts)if(row.exact_time)
            return common_failure("UnexpectedMilitaryOccupationExactSource",row.id);
        return common_success();
    }
    auto now=world.actual_phase_moment();if(!now.ok)return {false,{},std::move(now.error)};
    return validate_military_occupation_times(world.common,world.military,*world.catalog->military,
        world.catalog->selected_military_occupation_schema,now.value);
}
}
