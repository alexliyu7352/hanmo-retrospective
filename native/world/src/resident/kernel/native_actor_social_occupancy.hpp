// One original Interaction owner, bound to this WorldBundle for its lifetime.
#pragma once
#include "world_read_stamp.hpp"
#include "resident/domains/interaction/occupancy/actor_social_occupancy_initial.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
namespace hanmo::resident {
struct WorldBundle;
Outcome<Integer> selected_native_actor_social_occupancy_version(const CatalogIdentity&);
Status append_native_actor_social_occupancy_module(const BoundaryValue& scenario,CatalogIdentity&);
Status prepare_native_actor_social_occupancy_selection(const BoundaryValue& scenario,PreparedCatalog&);
// The original seed producer supplies only its deterministic occupancy projection;
// this does not run a world with an invented RNG seed or inspect restored state.
Outcome<ActorSocialOccupancyInitialCatalog> compile_native_actor_social_occupancy_seed(
    const BoundaryValue& data,const BoundaryValue& original_initial,
    const CatalogIdentity&,const StableId& scenario_id);
class NativeActorSocialOccupancyWorld final:public ActorSocialOccupancyWrite,public CheckpointAllocatedObject {
    WorldBundle& world_;
    WorldCurrentMomentRead clock_;
    NativeActorSocialOccupancyInitialRead initial_;
    NativeActorSocialOccupancyRead read_;
    NativeActorSocialOccupancyWrite write_;
public:
    // Called only after the selected original Foundation and compiled seed exist.
    explicit NativeActorSocialOccupancyWorld(WorldBundle&);
    const NativeActorSocialOccupancyInitialRead& initial()const noexcept{return initial_;}
    const NativeActorSocialOccupancyRead& read()const noexcept{return read_;}
    ActorSocialOccupancyWrite& write()noexcept{return *this;}
    bool bound_to(const CommonStore&,const InteractionState&)const noexcept override;
    Outcome<ActorSocialOccupancy> capture(const CommonStore&,const StableId&)const override;
    Status record(const StableId&,const ActorSocialOccupancy&,ActorSocialOccupancyCause,const StableId&,const StableId&,const std::optional<SourceRef>&)override;
    Status record_party_member(const StableId&,const ActorSocialOccupancy&,ActorSocialOccupancyCause,const StableId&,const StableId&,const PartyMembershipAcceptance&)override;
};
Status validate_native_actor_social_occupancy(const WorldBundle&);
}
