// Original army_commander_once_v1, after complete E and commander exposure.
#pragma once
#include "dynamic_land_commander.hpp"
namespace hanmo::resident {
struct DynamicCommandShockLeaderRead {
 MilitaryCommandShockLeader original_leader;
 // A real earlier effect group, not an invented round or a current flag.
 std::optional<SourceRef> prior_shock_group;
};
struct DynamicActualWithdrawalRead {
 StableId unit_ref;SourceRef accepted_withdrawal_source,actual_exit_geometry_source;
};
struct DynamicCommandShockCause {
 StableId army_id,actor_id,unit_id,kind;SourceRef actual_effect_group;
};
struct DynamicLandCommandShockMerge {
 std::vector<DynamicCommandShockCause> actual_causes;
 std::vector<DynamicLandMoraleChange> morale_changes;
};
Outcome<DynamicLandCommandShockMerge> merge_dynamic_land_command_shock(
 const MilitaryBattleCatalog&,const std::vector<DynamicLandPostureUnit>& original_before,
 const std::vector<DynamicLandPostureUnit>& actual_strike_snapshot,
 const DynamicLandMergedAllocation& complete_allocations,const DynamicLandCommanderMerge&,
 const std::vector<DynamicCommandShockLeaderRead>& actual_leaders,
 const std::vector<DynamicActualWithdrawalRead>& actual_withdrawals,
 const SourceRef& actual_effect_group);
}
