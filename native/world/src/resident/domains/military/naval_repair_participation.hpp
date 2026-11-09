#pragma once
#include "naval_repair_original_history.hpp"
#include "resident/domains/travel/actor_spatial_source_membership.hpp"
#include "resident/domains/actor_foundation/character_availability_read.hpp"
#include "resident/domains/interaction/occupancy/actor_social_occupancy.hpp"
namespace hanmo::resident {
struct ExactNavalRepairParticipationContext {
 const ExactNavalRepairHistoryContext& history;
 const CharacterConditionSources& bodies;
 const ActorSpatialHeadSourceReadContext& spatial;
 const NativeActorSocialOccupancyRead& social;
 const MilitarySpatialHandoffState* military_handoffs=nullptr;
};
// Original alive/available, physical stationary position and Party gates only.
// These do not replace the separate Army/ship/load proof, retained Action
// occupation or complete battle-entry exclusion of the original profession.
Status validate_exact_naval_repair_people_at_acceptance(const ExactNavalRepairParticipationContext&,const NewNavalRepairTimeSource2&);
Status validate_exact_naval_repair_people_service_span(const ExactNavalRepairParticipationContext&,const NewNavalRepairTimeSource2&,const MomentSpan&);
// Trusted current-M check immediately before the genuine completion writes.
Status validate_exact_naval_repair_people_at_current_completion(const ExactNavalRepairParticipationContext&,const NewNavalRepairTimeSource2&);
Status validate_exact_naval_repair_people_at_success(const ExactNavalRepairParticipationContext&,const NewNavalRepairTimeSource2&);
}
