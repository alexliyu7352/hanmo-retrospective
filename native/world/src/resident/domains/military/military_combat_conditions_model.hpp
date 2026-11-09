// Military13只将原部队疲劳四档接入真实攻击；训练数值保存为同一条件来源，不借此改旧攻守候选。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct MilitaryCombatConditionRule {StableId profile="entry_fatigue_03_v1";};
// 无原条件字段的单位保持明确空值和中性效率；不得补出伪造的0级训练/疲劳史。
struct MilitaryCombatConditionSnapshot {
 StableId unit_id,source_garrison_id,last_condition_fact_id;
 std::optional<Integer> training,fatigue;
 Integer efficiency_bp=10000;
 FieldOrder field_order;
};
struct MilitaryCombatConditionState {
 Integer entry_tick=0;
 std::vector<MilitaryCombatConditionSnapshot> units;
 FieldOrder field_order;
};
}
