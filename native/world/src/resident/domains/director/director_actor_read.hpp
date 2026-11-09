// Borrow the original Actor route from either selected Director layout. No
// shadow registry/schedule/plan is built for the complete subject2 layout.
#pragma once
#include "director_model.hpp"
namespace hanmo::resident {
class ExactResultTimeRead;class ExactResultReadStamp;class CurrentMomentRead;
struct DirectorActorRegistrationView {Integer epoch=0;bool obligations_only=false;const StableId* policy_id=nullptr;DirectorLifecycle lifecycle=DirectorLifecycle::retired;};
Status validate_director_actor_plan_fields(const CommonStore&,const DirectorCatalog&,const DirectorPolicy&,const FinitePlan&,const GrowthLifeRead*);
std::optional<DirectorActorRegistrationView> director_actor_registration(const DirectorState&,const StableId&);
const ReviewSchedule* director_actor_schedule(const DirectorState&,const StableId&);
Outcome<const FinitePlan*> read_director_actor_plan(const DirectorState&,const StableId&);
// Complete macro review reads only the actor's actual KnownReport receipts.
Outcome<KnownSubjectView> read_known_director_actor2(const CommonStore&,const SocialState&,const StableId&,const InteractionState&,const ExactResultTimeRead&,const ExactResultReadStamp&,const CurrentMomentRead&);
}
