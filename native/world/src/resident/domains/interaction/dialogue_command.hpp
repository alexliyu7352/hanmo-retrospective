#pragma once
#include "interaction_context.hpp"
namespace hanmo::resident {
// The unchanged original authored command, extracted only to break owner cycles.
struct DialogueCommand { StableId kind,offer_id,shop_id,service_id,target_actor_id,activity,skill_id; std::optional<InteractionContext> context;  std::vector<StableId> field_order;StableId place_id{}; };
}
