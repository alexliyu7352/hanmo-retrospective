#pragma once
#include "resident/domains/military/military_refs.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
// Owned by the actual candidate apply scope or retained exact lease. The
// CurrentMomentRead is the same candidate clock, not an occupation-selected
// source and not a reader manufactured from the legacy Integer tick argument.
struct MilitaryBattleLifecycleWriteContext {
 const CommonStore& common;const MilitaryState& state;const CurrentMomentRead& clock;
 ExactArithmeticBudget arithmetic{};
};
class MilitaryFormationSourceRead;class MilitaryReturnSourceRead;struct MilitaryExactState;
struct MilitaryBattleOccupancyRead1 {
 bool occupied=false;const MilitaryBattleRecord* battle=nullptr;
 const MilitaryBattleLifecycleEvent1* actual_start=nullptr;
};
// Retained result borrow. The source-local occurrence/association proof does
// not replace the original full Battle/recovery/authority validators.
Outcome<MilitaryBattleOccupancyRead1> read_military_battle_occupied_before(
 const CommonStore&,const MilitaryState&,const MilitaryExactState&,Integer selected,
 const StableId& actor,const CommonActualFactPrefix&,const WorldMoment& at,const WorldMoment& now,
 const MilitaryFormationSourceRead* formations,const MilitaryReturnSourceRead* returns,ExactArithmeticBudget={});
struct MilitaryFieldHistoryRead;
Outcome<MilitaryBattleOccupancyRead1> read_selected_military_battle_occupied_before(const CommonStore&,const MilitaryState&,const CatalogIdentity&,const MilitaryExactState*,Integer,const StableId&,const CommonActualFactPrefix&,const WorldMoment&,const WorldMoment&,const MilitaryFormationSourceRead*,const MilitaryReturnSourceRead*,ExactArithmeticBudget={},const MilitaryFieldHistoryRead* field=nullptr);
Status validate_military_battle_lifecycle_layout(const MilitaryState&,Integer selected);
Status validate_military_battle_lifecycle_sources(const CommonStore&,const MilitaryState&,Integer selected,const WorldMoment& now,ExactArithmeticBudget={});
Status validate_military_battle_lifecycle_write_binding(const CommonWrite&,const MilitaryState&,const MilitaryAccess&);
Status publish_military_battle_lifecycle_start_in_candidate(CommonWrite&,MilitaryState&,const MilitaryAccess&,const StableId& battle_id);
Status validate_military_battle_resolved_lifecycle(const MilitaryBattleRecord&,const MilitaryBattleRecord&,Integer selected);
// Called only after the original producer has actually selected phase=closed.
// It does not decide victory, automatic closure, release authority, or recovery.
Status publish_military_battle_lifecycle_close_in_candidate(CommonWrite&,MilitaryState&,const MilitaryAccess&,MilitaryBattleRecord&);
}
