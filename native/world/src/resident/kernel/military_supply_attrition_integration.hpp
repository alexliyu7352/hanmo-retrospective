// Military9在原供给后消费同一真实Supply，人口及经济转移仍属于原完整World事务。
#pragma once
#include "resident/domains/military/military_supply_attrition_rules.hpp"
namespace hanmo::resident {
struct WorldBundle;
Status step_military_supply_attrition_world(WorldBundle&,const DynamicStepInput&);
// 军事原史之后、战斗或舟战初值回放之前核匿名逃散源，不能挪到发布之后。
Status validate_military_supply_attrition_world(const WorldBundle&);
}
