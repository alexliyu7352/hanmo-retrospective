// source/1 terminal recovery borrows real completed service, never a period/1 credit.
#pragma once
#include "growth_recovery_progress.hpp"
namespace hanmo::resident {
struct CharacterConditionSources;struct LifeActivityRecord;struct LifeFixedServiceBoundary;
struct GrowthTerminalRecoverySourceReadV1 {
 const LifeActivityRecord* original_activity=nullptr;
 const LifeFixedServiceBoundary* original_completion=nullptr;
 Integer method_points_per_half_day=0,actual_completed_half_days=0;
};
// All pointers borrow the actual sources owner. This is a raw original-service
// proof, not a new Condition head or a substitute for full FSL input validation.
Outcome<GrowthTerminalRecoverySourceReadV1> read_growth_terminal_recovery_source_v1(
 const CharacterConditionSources&,const StableId& action_id);
}
