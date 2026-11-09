// 只读本人所辖原部队的有限供给来源，不暴露全军日志或旁人的私人伤势。
#pragma once
#include "military_supply_attrition_rules.hpp"
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident {
Outcome<BoundaryValue> military_supply_attrition_page(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId& observer);
}
