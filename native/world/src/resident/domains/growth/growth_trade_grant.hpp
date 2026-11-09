// Immutable XP values from the actual original trade writer. No second XP balance.
#pragma once
#include "growth_capability_application.hpp"
namespace hanmo::resident {
struct GrowthTradeGrant {
  StableId action_id, actor_id, skill_id;
  SourceRef original_trade_result_ref;
  Integer original_trade_source_schema = 2;
  Integer xp_before = 0, actual_xp_gain = 0, xp_after = 0;
  // Filled once by the existing same-candidate capability writer; net zero has none.
  std::optional<GrowthCapabilityApplication> capability_application;
};
} // namespace hanmo::resident
