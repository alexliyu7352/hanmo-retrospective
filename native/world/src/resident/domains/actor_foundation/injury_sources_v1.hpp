// 独立
// character-injury-sources/1。只持逐笔原来源和真实前后像，不持当前身体/点数余额。
#pragma once
#include "model.hpp"
#include "resident/domains/growth/growth_recovery_progress.hpp"
#include "resident/kernel/source_ref.hpp"
namespace hanmo::resident {
// 真原来源：field 每段 {military,military_casualties,casualty:*} 或
// personal {personal,personal_battle,battle:*}。参与者由应用行 actor_id 指定。
// schema1 指各原领域/1，不把 field 冒作 dynamic_military/2 casualty。
struct CharacterBattleReceiptSourceV1 {
  SourceRef original_receipt;
  Integer source_schema = 1;
};
struct CharacterTreatmentActionSourceV1 {
  StableId action_id;
  Integer completion_ordinal = 1;
};
// tag0/1/2 是本独立格式唯一有限联合；不是旧 Condition 原联合的暗扩。
using CharacterInjuryOriginalSourceV1 =
    std::variant<CharacterBattleReceiptSourceV1, OriginalRestProvidedCause,
                 CharacterTreatmentActionSourceV1>;
struct CharacterInjuryApplicationV1 {
  StableId actor_id;
  CharacterInjuryOriginalSourceV1 original_source;
  // Existing credit fields retain the actual offered/used amount; battle
  // applications carry zero credit. This is immutable per-source evidence.
  GrowthRecoveryCredit progress;
};
struct CharacterInjurySourcesStateV1 {
  // key 为同一实际 ConditionReceiptV2.result_id；没有第二分配器或时钟。
  OrderedTable<CharacterInjuryApplicationV1> applications;
};
} // namespace hanmo::resident

namespace hanmo::resident {
class CommonStore;
struct PersonalState;
struct PersonalCatalog;
struct PersonalFact;
struct MilitaryState;
struct MilitaryCasualtyFact;
struct FieldBattleWorldState;
class FieldSegmentApplicationRead;
// Borrowed original result, valid only while the supplied owning roots remain
// unchanged. This leaf never authenticates its own Condition/application row.
struct CharacterBattleInjuryReadV1 {
  const WorldMoment *completed_at = nullptr;
  Integer incoming_injury =
      0; // 0: actual participant body write, no injury/points change
  const PersonalFact *personal_fact = nullptr;
  const MilitaryCasualtyFact *field_casualties = nullptr;
  // 只读本段 wounded；dead 仍由原生命 writer 独立拥有。
};
Outcome<CharacterBattleInjuryReadV1> read_personal_battle_injury_source_v1(
    const CommonStore &, const PersonalState &, const PersonalCatalog &,
    Integer selected_lifecycle, const CharacterBattleReceiptSourceV1 &,
    const StableId &actor_id, const WorldMoment &actual_now);
Outcome<CharacterBattleInjuryReadV1> read_field_battle_injury_source_v1(
    const CommonStore &, const MilitaryState &, const FieldBattleWorldState &,
    const CharacterBattleReceiptSourceV1 &, const StableId &actor_id,
    const WorldMoment &actual_now,
    const FieldSegmentApplicationRead *actual_clock = nullptr);
} // namespace hanmo::resident
