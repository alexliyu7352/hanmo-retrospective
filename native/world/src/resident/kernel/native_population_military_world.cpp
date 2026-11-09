#include "native_population_military_world.hpp"
#include "world_bundle.hpp"
namespace hanmo::resident {
namespace {
Outcome<Integer> selected(const WorldBundle& world,const DynamicMilitaryLandConsequenceSources* effects){
    if(!world.catalog||!world.population_military_enabled()||!world.catalog->exact_time||
       !exact_complete_selected(*world.catalog->exact_time)||!world.military_exact||!effects)
        return {false,0,common_failure("PopulationMilitaryExactOwningSourcesRequired").error};
    auto version=selected_population_military_version(*world.catalog->population_military,world.siege_integration_enabled());
    if(!version.ok)return version;
    if (version.value != 4)
      return {false, 0,
              common_failure("PopulationMilitaryExactProfileRequired").error};
    std::size_t markers=0;
    for(const auto& row:world.catalog->identity.module_versions)if(row.first=="population_military"){
        ++markers;
        if (row.second != "4")
          return {
              false, 0,
              common_failure("PopulationMilitaryExactSelectionMismatch").error};
    }
    if(markers!=1)return {false,0,common_failure("PopulationMilitaryExactSelectionMismatch").error};
    bool transport_selected=false;
    for(const auto& row:world.catalog->identity.module_versions)if(row.first=="civilian_transport_population"){
        if(transport_selected||row.second!="2")
            return {false,0,common_failure("PopulationTransportSelectedVersionRequired").error};
        transport_selected=true;
    }
    if(transport_selected!=bool(world.population_transport))
        return {false,0,common_failure("PopulationTransportSelectedStatePairMismatch").error};
    return version;
}
}
NativePopulationMilitaryWorldRead::NativePopulationMilitaryWorldRead(const WorldBundle& world,
    std::shared_ptr<const DynamicMilitaryLandConsequenceSources> effects,Integer version)
    :world_(world),clock_(world),effects_(std::move(effects)),context_(world.population_read()){
    context_.selected_version=version;context_.military_exact=&*world.military_exact;
    context_.actual_clock=&clock_;context_.dynamic_land_sources=effects_.get();
    context_.actual_integration=world.siege_integration_enabled();
    context_.population_transport=world.population_transport?&*world.population_transport:nullptr;
}
Outcome<std::unique_ptr<NativePopulationMilitaryWorldRead>> NativePopulationMilitaryWorldRead::create(
    const WorldBundle& world,std::shared_ptr<const DynamicMilitaryLandConsequenceSources> effects){
    auto version=selected(world,effects.get());if(!version.ok)return {false,{},version.error};
    return {true,std::unique_ptr<NativePopulationMilitaryWorldRead>(
        new NativePopulationMilitaryWorldRead(world,std::move(effects),version.value)),{}};
}
NativePopulationMilitaryWorldWrite::NativePopulationMilitaryWorldWrite(WorldBundle& world,
    std::shared_ptr<const DynamicMilitaryLandConsequenceSources> effects,Integer version)
    :world_(world),clock_(world),effects_(std::move(effects)),context_(world.population_write()){
    context_.selected_version=version;context_.military_exact=&*world.military_exact;
    context_.actual_clock=&clock_;context_.dynamic_land_sources=effects_.get();
    context_.actual_integration=world.siege_integration_enabled();
    context_.population_transport=world.population_transport?&*world.population_transport:nullptr;
}
Outcome<std::unique_ptr<NativePopulationMilitaryWorldWrite>> NativePopulationMilitaryWorldWrite::create(
    WorldBundle& world,std::shared_ptr<const DynamicMilitaryLandConsequenceSources> effects){
    auto version=selected(world,effects.get());if(!version.ok)return {false,{},version.error};
    return {true,std::unique_ptr<NativePopulationMilitaryWorldWrite>(
        new NativePopulationMilitaryWorldWrite(world,std::move(effects),version.value)),{}};
}
}
