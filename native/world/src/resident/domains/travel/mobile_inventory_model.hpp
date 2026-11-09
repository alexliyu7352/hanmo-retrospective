// S1 authored physical container capability. These are existing Common
// inventories, never inventory creation, owner grants, or human carrying limits.
#pragma once
#include "resident/kernel/world_timeline.hpp"
#include "resident/kernel/source_ref.hpp"
#include "resident/model/common_model.hpp"
namespace hanmo::resident {
struct MobileInventoryDefinition {
    StableId id,inventory_id,initial_site_id;
    Integer version=0,container_maximum_weight=0;
    WorldMoment initialized_at;
};
struct MobileInventoryAdoptionView {
    StableId inventory_id;
    std::optional<StableId> vehicle_ref;
    MomentOwnerSource original_container_source,spatial_source,capacity_source;
    WorldMoment adopted_at;
};
}
