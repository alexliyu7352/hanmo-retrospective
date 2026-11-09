#pragma once
#include "world_read_stamp.hpp"
#include "resident/domains/military/population_military.hpp"
#include "resident/domains/military/dynamic_land_consequence.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
namespace hanmo::resident {
// The actual effect-group source owner remains alive with the borrowed context.
// No current-moment snapshot or caller-supplied casualty projection is stored.
class NativePopulationMilitaryWorldRead final:public CheckpointAllocatedObject {
    const WorldBundle& world_;
    WorldCurrentMomentRead clock_;
    std::shared_ptr<const DynamicMilitaryLandConsequenceSources> effects_;
    PopulationMilitaryReadContext context_;
    NativePopulationMilitaryWorldRead(const WorldBundle&,
        std::shared_ptr<const DynamicMilitaryLandConsequenceSources>,Integer);
public:
    static Outcome<std::unique_ptr<NativePopulationMilitaryWorldRead>> create(const WorldBundle&,
        std::shared_ptr<const DynamicMilitaryLandConsequenceSources> actual_effect_owner);
    const PopulationMilitaryReadContext& context()const{return context_;}
};
class NativePopulationMilitaryWorldWrite final:public CheckpointAllocatedObject {
    WorldBundle& world_;
    WorldCurrentMomentRead clock_;
    std::shared_ptr<const DynamicMilitaryLandConsequenceSources> effects_;
    PopulationMilitaryWriteContext context_;
    NativePopulationMilitaryWorldWrite(WorldBundle&,
        std::shared_ptr<const DynamicMilitaryLandConsequenceSources>,Integer);
public:
    static Outcome<std::unique_ptr<NativePopulationMilitaryWorldWrite>> create(WorldBundle&,
        std::shared_ptr<const DynamicMilitaryLandConsequenceSources> actual_effect_owner);
    PopulationMilitaryWriteContext& context(){return context_;}
};
}
