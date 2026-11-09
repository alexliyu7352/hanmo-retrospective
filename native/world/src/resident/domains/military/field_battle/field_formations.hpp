// 会战编队只从原实际成军来源与当下名册派生；不接受作者预种的 Army 或兵员余额。
#pragma once
#include "field_session.hpp"
namespace hanmo::resident {
class CommonStore;
class CheckpointMemoryCharge;
struct MilitaryState;
struct MilitaryCatalog;
class MilitaryFormationSourceRead;
// 保留原五类名单与真实军官；空军团、来源重复、人员重复或未知兵种均拒绝。
Outcome<std::vector<FieldFormation>> read_field_army_formations(
    const CommonStore &, const MilitaryState &, const MilitaryCatalog &,
    const StableId &army_id, Integer side, const StableId &player_actor_id,
    const MilitaryFormationSourceRead *exact_formations = nullptr);
// 精确成军只消费外层原 S0 时刻；不得用 Common.tick 的零分数倒查本 tick 成军。
Outcome<std::vector<FieldFormation>>
read_field_army_formations(const CommonStore &, const MilitaryState &,
                           const MilitaryCatalog &, const StableId &army_id,
                           Integer side, const StableId &player_actor_id,
                           const WorldMoment &actual_now,
                           const MilitaryFormationSourceRead *exact_formations);
class FieldSegmentApplicationRead;
struct FieldBattleWorldState;
// Only the original same-World active S0 reader can cross Common's stable tick.
// The older moment-only overload keeps its existing stable-tick restriction.
Outcome<std::vector<FieldFormation>>
read_field_army_formations_at_original_clock(
    const CommonStore &, const MilitaryState &, const MilitaryCatalog &,
    const FieldBattleWorldState &, const StableId &army_id, Integer side,
    const StableId &player_actor_id, const FieldSegmentApplicationRead &,
    const MilitaryFormationSourceRead &,
    CheckpointMemoryCharge *retained_output = nullptr);
// The actual next queued unit alone. Previously admitted units may already
// have lost their officer; they are not re-admitted or re-frozen by this read.
Outcome<FieldFormation> read_field_reinforcement_formation_at_original_clock(
    const CommonStore &, const MilitaryState &, const MilitaryCatalog &,
    const FieldBattleWorldState &, const StableId &army_id,
    const StableId &formation_id, Integer side, const StableId &player_actor_id,
    const FieldSegmentApplicationRead &, const MilitaryFormationSourceRead &,
    CheckpointMemoryCharge &retained_output);
} // namespace hanmo::resident
