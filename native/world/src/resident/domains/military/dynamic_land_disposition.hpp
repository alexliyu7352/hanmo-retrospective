// Original F rout/withdraw consequences over the one complete E snapshot.
#pragma once
#include "dynamic_land_command_shock.hpp"
namespace hanmo::resident {
struct DynamicLandMemberDisposition {
 StableId unit_ref,member_ref,before_status,after_status,captor_unit_ref,captor_army_ref;
 SourceRef actual_effect_group;
};
struct DynamicLandActualExit {
 StableId unit_ref;SourceRef accepted_withdrawal_source,actual_exit_geometry_source,actual_effect_group;
};
struct DynamicLandDispositionMerge {
 std::vector<DynamicLandMemberDisposition> member_dispositions;
 std::vector<DynamicLandActualExit> actual_exits;
};
// Original legacy rout has no service timer: same-M defeated survivors are
// captured/deserted once from the complete group. The separately selected
// retreat profile requires its own real progress/exit source and cannot borrow
// a technical slice or manufacture a round here.
Outcome<DynamicLandDispositionMerge> merge_dynamic_land_rout_and_withdrawal(
 const MilitaryBattleCatalog&,const StableId& battlefield,
 const std::vector<DynamicLandPostureUnit>& strike_snapshot,
 const DynamicLandMergedAllocation&,const DynamicLandCommanderMerge&,
 const DynamicLandCommandShockMerge&,const std::vector<DynamicActualWithdrawalRead>&,
 const SourceRef& actual_effect_group,ExactArithmeticBudget={});
}
