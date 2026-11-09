// 原连续会战段导致的真实领队退出；不造旧 BattleRecord、计划 ID 或精确世界时刻。
#pragma once
#include "field_segment_settlement.hpp"
#include "resident/domains/military/military_journey_termination.hpp"
namespace hanmo::resident {
// 三根必须同一未发布 World；中央须用原 owning scope 的 alias 保活被借根。
// shared_ptr 只持有此引用束不等于延长 World 寿命，不得绑定临时副本。
struct FieldBattleActorOutcomeContext {
  const CommonStore &common;
  const MilitaryState &military;
  const FieldBattleWorldState &field;
};
// 采用前须仍持同一原旅行 Action；只给本次原伤亡回执中的实际领队出具退出原因。
Outcome<std::optional<MilitaryJourneyTermination>>
preview_field_segment_journey_termination(
    const CommonStore &, const MilitaryState &, const FieldBattleWorldState &,
    const StableId &casualty_fact_id, const StableId &action_id,
    const StableId &actor_id);
// 原人物及旅行写者完成后再读；来源仍为同一段回执，不从最新位置推导旧行动。
// 本叶不给 ActionCompletion 落账；中央必须在同一未发布 World 接原完成入口。
Outcome<std::optional<MilitaryJourneyTermination>>
query_field_segment_journey_termination(
    const CommonStore &, const MilitaryState &, const FieldBattleWorldState &,
    const StableId &casualty_fact_id, const StableId &action_id,
    const StableId &actor_id);
} // namespace hanmo::resident
