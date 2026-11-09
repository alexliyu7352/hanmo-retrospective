#pragma once
#include "resident/model/common_model.hpp"
namespace hanmo::resident {
// Original InteractionContext, moved unchanged so original source records can
// use the same type without including their owning InteractionState recursively.
struct InteractionContext { StableId place_id,provider_actor_id,role_id; std::optional<StableId> kind,encounter_id; std::optional<StableId> input_error;  std::vector<StableId> field_order; };
}
