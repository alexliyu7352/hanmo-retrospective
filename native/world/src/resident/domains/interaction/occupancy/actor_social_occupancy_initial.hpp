#pragma once
#include "actor_social_occupancy.hpp"
#include "resident/domains/actor_foundation/created_origin.hpp"
namespace hanmo::resident {
struct ActorSocialOccupancyInitialCatalog {
 CatalogIdentity identity;StableId scenario_id;
 OrderedTable<ActorSocialOccupancy> authored_initial;
};
// Called solely by the original author/seed compiler, before dynamic Created
// initialization. Retains only actual initial actor occupancy, not the world.
Outcome<ActorSocialOccupancyInitialCatalog> compile_actor_social_occupancy_initial(const CommonState& original_seed,const InteractionState& original_interaction,const CatalogIdentity&,const StableId& scenario_id);
Status validate_actor_social_occupancy_layout(const CommonStore&,const InteractionState&,bool selected,const std::vector<std::pair<StableId,StableId>>& modules,const ActorSocialOccupancyInitialCatalog*);
struct ActorSocialOccupancyInitialContext {
 const CommonStore& common;const CommonCatalog& common_catalog;
 const AuthoredActorFoundationState& foundation;const ActorFoundationCatalog& actor_catalog;
 const FamilyCatalog* family_catalog;const ActorSocialOccupancyInitialCatalog& compiled;
 const CurrentMomentRead& clock;
 const BornCharacterSourceRead* born_sources=nullptr;
};
class NativeActorSocialOccupancyInitialRead final:public ActorSocialOccupancyInitialRead {
 ActorSocialOccupancyInitialContext context_;
public:explicit NativeActorSocialOccupancyInitialRead(ActorSocialOccupancyInitialContext x):context_(x){}
 bool bound_to(const CommonStore&c)const noexcept override{return &c==&context_.common;}
 Outcome<ActorSocialOccupancyInitial> read_actor_initial_social_occupancy(const StableId&)const override;
};
// New-world only. Restore must decode its selected section, never call this.
Status initialize_actor_social_occupancy_sources(CommonStore&,InteractionState&,const ActorSocialOccupancyInitialRead&,const CurrentMomentRead&,Integer physical_version=1);
}
