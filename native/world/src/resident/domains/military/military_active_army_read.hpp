// Original rest_quiet Army condition: active membership only, no new proximity rule.
#pragma once
#include "military_roster_history.hpp"
namespace hanmo::resident {
struct NavalState;struct NavalOriginalSourceContext;struct CommonActualFactPrefix;class MilitaryReturnSourceRead;
struct MilitaryActorActiveArmyRead {
 StableId actor_id;
 WorldMoment observed_at;
 std::vector<StableId> active_army_ids;
 // Actor-projected original formation/member/lifecycle changes through this cut,
 // including joins and closures even when the final active set is empty.
 // The complete world chain is validated before this projection is selected.
 std::vector<MilitaryHistorySource> original_sources;
};
// These readers borrow fully validated original Military/Naval histories. They
// additionally close source headers, native ordinals, exact times and current
// roster/status coverage; they never author initial Armies or quiet facts.
Outcome<MilitaryActorActiveArmyRead> read_actual_actor_active_armies(
 const CommonStore&,const MilitaryState&,const MilitaryExactState&,const NavalState*,
 const StableId& actor_id,const WorldMoment& actual_now,ExactArithmeticBudget={},const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr);
// Frozen head was captured by the actual FSL acceptance writer. Later same-M
// sources are excluded by their original completion/allocation serial, while
// omitted strictly earlier sources are rejected. No caller-provided schema.
Outcome<MilitaryActorActiveArmyRead> read_actor_active_armies_at_source_head(
 const CommonStore&,const MilitaryState&,const MilitaryExactState&,const NavalState*,
 const StableId& actor_id,const WorldMoment& captured_at,const std::vector<SourceRef>& actual_head,
 const WorldMoment& actual_now,ExactArithmeticBudget={},const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr);
// Exact endpoint consumers use actual Common append position, never an ID
// allocation serial or a normalized legacy tick, as their occurrence cut.
// All source/predecessor checks remain; historical output is not compared with
// today's Army state. Genuine-M events on both sides must agree with the cut.
Outcome<MilitaryActorActiveArmyRead> read_actor_active_armies_at_actual_prefix(
 const CommonStore&,const MilitaryState&,const MilitaryExactState&,const NavalState*,
 const StableId& actor_id,const CommonActualFactPrefix&,const WorldMoment& captured_at,
 const WorldMoment& actual_now,ExactArithmeticBudget={},
 const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr,
 const NavalOriginalSourceContext* naval_sources=nullptr);
Outcome<MilitaryActorActiveArmyRead> read_actor_active_armies_at_selected_actual_prefix(const CommonStore&,const MilitaryState&,const CatalogIdentity&,const MilitaryExactState*,const NavalState*,const StableId&,const CommonActualFactPrefix&,const WorldMoment&,const WorldMoment&,ExactArithmeticBudget={},const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr,const NavalOriginalSourceContext* naval_sources=nullptr,const MilitaryFieldHistoryRead* field=nullptr);
Outcome<MilitaryHistorySource> read_military_active_army_source(
 const CommonStore&,const MilitaryState&,const MilitaryExactState&,const NavalState*,
 const SourceRef&,const WorldMoment& actual_now,ExactArithmeticBudget={},const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr);
struct MilitaryActorActiveArmyHeadChange {
 MilitaryHistorySource actual_source;WorldMoment occurred_at;
 std::vector<StableId> before_active_army_ids,after_active_army_ids;
};
// Complete actor-projected source delta (from_head,to_head]. Unrelated Army
// sources are fully validated but do not advance this actor's immutable head.
// A real join followed by a departure is returned as two changes, even at one M.
Outcome<std::vector<MilitaryActorActiveArmyHeadChange>> read_actor_active_army_head_changes(
 const CommonStore&,const MilitaryState&,const MilitaryExactState&,const NavalState*,
 const StableId& actor_id,const WorldMoment& from_at,const std::vector<SourceRef>& from_head,
 const WorldMoment& to_at,const std::vector<SourceRef>& to_head,const WorldMoment& actual_now,
 ExactArithmeticBudget={},const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr);

// 显式选型的原Army历史读口；仅增加Field真根，不改变旧引用入口或持久形状。
Outcome<MilitaryActorActiveArmyRead> read_actual_actor_active_armies_selected(
 const CommonStore&,const MilitaryState&,const MilitarySelectedHistoryRead&,const NavalState*,
 const StableId&,const WorldMoment&,ExactArithmeticBudget={},const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr);
Outcome<MilitaryActorActiveArmyRead> read_actor_active_armies_at_selected_source_head(
 const CommonStore&,const MilitaryState&,const MilitarySelectedHistoryRead&,const NavalState*,
 const StableId&,const WorldMoment&,const std::vector<SourceRef>&,const WorldMoment&,
 ExactArithmeticBudget={},const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr);
Outcome<MilitaryHistorySource> read_selected_military_active_army_source(
 const CommonStore&,const MilitaryState&,const MilitarySelectedHistoryRead&,const NavalState*,
 const SourceRef&,const WorldMoment&,ExactArithmeticBudget={},const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr);
Outcome<std::vector<MilitaryActorActiveArmyHeadChange>> read_actor_active_army_head_changes_selected(
 const CommonStore&,const MilitaryState&,const MilitarySelectedHistoryRead&,const NavalState*,
 const StableId&,const WorldMoment&,const std::vector<SourceRef>&,const WorldMoment&,
 const std::vector<SourceRef>&,const WorldMoment&,ExactArithmeticBudget={},
 const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr);

}
