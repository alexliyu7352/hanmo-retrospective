// 军级失帅震动只作用于该真实军团；同侧友军不是同一指挥链，不据controller猜总帅。
#pragma once
#include "military_field_command_contract.hpp"
namespace hanmo::resident {
struct MilitaryCommandShockRule {StableId profile="army_commander_once_v1";Integer morale_loss=10;};
inline constexpr const char* military_command_shock_module="military_command_shock";
inline constexpr std::uint32_t military_command_shock_checkpoint_feature=33554432u;
// unit_id为空表示本军统帅未在本次实际入战部中；远方统帅不得借同军自动成为损失源。
struct MilitaryCommandShockLeader {
 StableId army_id,actor_id,unit_id;MilitaryFieldCommandSource command_source;FieldOrder field_order,source_field_order;
};
// cause只记录本场首次真实轮次变化；人物总死亡仍由原MilitaryCasualtyFact独占。
struct MilitaryCommandShockCause {
 StableId army_id,actor_id,unit_id,kind;Integer round=0;FieldOrder field_order;
};
struct MilitaryCommandShockState {
 std::vector<MilitaryCommandShockLeader> leaders;
 std::vector<MilitaryCommandShockCause> causes;
 FieldOrder field_order;
};
}
