// 新独立Military15资格段候选；旧14禁止追加字段，唯一资源值继续在原BattleUnit.tactic_points。
#pragma once
#include "military_command_resources_contract.hpp"
namespace hanmo::resident {
struct MilitaryCommandResourcesRule { StableId profile="entry_tactics_recovery_05_v1"; };
struct MilitaryCommandResourcesUnit {
 StableId unit_id,commander_actor_id,authored_actor_id;
 Integer experience=0,level=0;
 std::vector<StableId> causal_fact_ids;
 FieldOrder field_order;
};
struct MilitaryCommandResourcesState {
 Integer entry_tick=0;
 std::vector<MilitaryCommandResourcesUnit> units;
 FieldOrder field_order;
};
struct MilitaryCommandResourceQuote { Integer before=0,recovered=0,available=0; };
}
