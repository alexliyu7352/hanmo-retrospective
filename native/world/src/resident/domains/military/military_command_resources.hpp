// 原05资源常数是固定规则；报价不写、每部真实自身行动仅应用一次，外层原round负责重放。
#pragma once
#include "battle_model.hpp"
#include "military_command_resources_model.hpp"
namespace hanmo::resident {
Status validate_military_command_resources_rule(const MilitaryCommandResourcesRule&);
Status validate_military_command_resources_shape(const MilitaryBattleRecord&,const MilitaryCommandResourcesState&);
Outcome<MilitaryCommandResourceQuote> quote_military_command_resource_recovery(Integer before,Integer tactics_level);
Outcome<MilitaryCommandResourceQuote> query_military_command_resources(const MilitaryBattleRecord&,const MilitaryCommandResourcesState&,const StableId& unit_id);
}
