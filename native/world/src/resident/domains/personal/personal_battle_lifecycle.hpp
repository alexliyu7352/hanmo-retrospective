#pragma once
#include "personal_model.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
// Stack-bound by the actual World apply entry. Never returned inside an access
// object whose local clock/context has already been destroyed.
struct PersonalBattleLifecycleWriteContext {
 CommonStore& common;PersonalState& state;const CurrentMomentRead& clock;
};
struct PersonalBattleOccupancyRead1 {
 bool occupied=false;
 const PersonalBattle* battle=nullptr;
 const PersonalBattleLifecycleEvent1* actual_start=nullptr;
};
Status validate_personal_battle_lifecycle_layout(const PersonalState&,Integer selected);
Status validate_personal_battle_lifecycle_sources(const CommonStore&,const PersonalState&,Integer selected,const WorldMoment& now,ExactArithmeticBudget={});
Outcome<PersonalBattleOccupancyRead1> read_personal_battle_occupied_before(
 const CommonStore&,const PersonalState&,Integer selected,const StableId& actor,
 const CommonActualFactPrefix&,const WorldMoment& at,const WorldMoment& now,ExactArithmeticBudget={});
Status validate_personal_battle_lifecycle_write_binding(const CommonStore&,const PersonalState&,const PersonalAccess&);
Outcome<std::optional<PersonalBattleLifecycle1>> prepare_personal_battle_lifecycle_start_in_candidate(CommonStore&,PersonalState&,const PersonalAccess&);
Status publish_personal_battle_lifecycle_start_in_candidate(CommonStore&,PersonalState&,const PersonalAccess&,const StableId& battle_id);
Status validate_personal_battle_resolved_lifecycle(const PersonalBattle&,const PersonalBattle&,Integer selected);
Status record_personal_battle_result_moment_in_candidate(CommonStore&,PersonalState&,const PersonalAccess&,PersonalBattle&);
Status close_personal_battle_lifecycle_in_candidate(CommonStore&,PersonalState&,const PersonalAccess&,PersonalBattle&);
}
