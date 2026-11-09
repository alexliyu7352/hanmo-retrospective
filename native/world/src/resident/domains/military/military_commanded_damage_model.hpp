// Military14冻结真实入战军官、原条件与人物资格；后续技能变化不能回填旧轮次。
#pragma once
#include "military_officer_contract.hpp"
namespace hanmo::resident {
struct MilitaryCommandedDamageRule {StableId profile="entry_commanded_damage_05_v1";};
struct MilitaryCommandedDamageUnit {
 StableId unit_id,commander_actor_id,troop_type_id,skill_id,authored_actor_id;
 MilitaryUnitCommandSource command_source;
 Integer leadership=0,experience=0,level=0,training=0;
 std::vector<StableId> causal_fact_ids;
 FieldOrder field_order,source_field_order;
};
struct MilitaryCommandedDamageState {
 Integer entry_tick=0;
 std::vector<MilitaryCommandedDamageUnit> units;
 FieldOrder field_order;
};
struct MilitaryCommandedDamageQuote {Integer attack_strength=0,defense_strength=0,base_casualties=0;};
}
