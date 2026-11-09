#include "native_director_wake_world.hpp"
#include "exact_complete_world.hpp"
#include "world_bundle.hpp"
namespace hanmo::resident {
namespace {
enum class Boundary { original_seed, successful_sources, decoded_state };
Status apply_boundary(WorldBundle& world,Boundary boundary){
    if(!world.catalog)return common_failure("DirectorWorldCatalogRequired",{},ErrorKind::internal);
    if(!world.catalog->exact_time||!exact_complete_selected(*world.catalog->exact_time))
        return common_success();
    if(!world.catalog->director||!world.catalog->director->subjects2)
        return common_failure("CompleteDirectorOriginalCatalogRequired",{},ErrorKind::unsupported_capability);
    if(!world.director.subject2||!world.world_timeline)
        return common_failure("CompleteDirectorSelectedStateMismatch",{},ErrorKind::internal);
    auto now=world.actual_phase_moment();if(!now.ok)return {false,{},std::move(now.error)};
    std::unique_ptr<PhaseCurrentReadScope> phase;
    if(!world.has_active_read_phase()){
        const char* name=boundary==Boundary::original_seed?"director_original_seed":
            boundary==Boundary::decoded_state?"director_decoded_wake_cache":"director_actual_source_wakes";
        auto entered=world.enter_phase_read_context(name,"director",{now.value,now.value},now.value);
        if(!entered.ok)return {false,{},std::move(entered.error)};
        phase=std::move(entered.value);
    }
    // The factory contains only the two compiled function pointers. Do not keep
    // an earlier body/source lease across the original Director writers below;
    // each actual writer asks that factory for its own fresh same-world lease.
    auto owners=world.complete_exact_time_bindings();if(!owners.ok)return {false,{},std::move(owners.error)};
    if(!owners.value)return common_failure("CompleteDirectorOwningLeaseRequired",{},ErrorKind::unsupported_capability);
    const auto factory=owners.value->reads().director;
    owners.value.reset();
    if(!factory.read||!factory.write)
        return common_failure("CompleteDirectorOriginalFactoryRequired",{},ErrorKind::unsupported_capability);
    switch(boundary){
    case Boundary::original_seed:return initialize_complete_director_in_candidate(world,factory);
    case Boundary::successful_sources:return collect_complete_director_wakes_in_candidate(world,factory);
    case Boundary::decoded_state:return restore_complete_director_wake_cache_in_candidate(world,factory);
    }
    return common_failure("InvalidDirectorSourceBoundary",{},ErrorKind::internal);
}
}
Status initialize_native_complete_director_in_candidate(WorldBundle& world){return apply_boundary(world,Boundary::original_seed);}
Status collect_native_complete_director_wakes_in_candidate(WorldBundle& world){return apply_boundary(world,Boundary::successful_sources);}
Status restore_native_complete_director_wake_cache_in_candidate(WorldBundle& world){return apply_boundary(world,Boundary::decoded_state);}
}
