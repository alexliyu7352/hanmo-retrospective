// 原05攻门的筑城加成独立显式选择；仍由原围城拥有占位、材料、工期与最终节点变化。
#pragma once
#include "military_officer_contract.hpp"
#include "military_siege_engineering_contract.hpp"
namespace hanmo::resident {
struct MilitarySiegeEngineeringRule {StableId profile="commander_fortification_05_v1";};
struct MilitarySiegeEngineeringPolicy {MilitarySiegeEngineeringRule rule;std::vector<StableId> topology_ids;};
// 每半日准备固定实际部队指挥源和当时经验；不是第二份经验或兵员账。
struct MilitarySiegeEngineeringUnit {
 StableId unit_id,army_id,commander_actor_id,authored_actor_id;
 MilitaryUnitCommandSource command_source;
 Integer fit_count=0,experience=0,level=0;
 std::vector<StableId> causal_fact_ids;
 FieldOrder field_order,source_field_order;
};
struct MilitarySiegeEngineeringPreparation {
 StableId id,siege_id,topology_id,previous_preparation_id;
 Integer from_tick=0,to_tick=0,round=0;
 std::vector<MilitarySiegeEngineeringUnit> units;
 FieldOrder field_order;
};
struct MilitarySiegeEngineeringState {
 std::vector<MilitarySiegeEngineeringPreparation> preparations;
 FieldOrder field_order;
};
// 纯形状不证明人物资格或军籍，World早军史与晚Growth来源必须分别核验。
Status validate_military_siege_engineering_rule(const MilitarySiegeEngineeringRule&);
Status validate_military_siege_engineering_preparation_shape(const MilitarySiegeEngineeringPreparation&);
// 返回进入旧城墙/节点防护计算前的原始伤害；器械仅从原已验实际use传入，不自动生成。
Outcome<Integer> military_siege_engineering_assault_base(const MilitarySiegeEngineeringRule&,Integer effective_soldiers,Integer fortification_level,bool engine_used);
}
