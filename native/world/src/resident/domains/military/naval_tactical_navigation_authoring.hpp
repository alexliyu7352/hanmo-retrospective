// 新转向/水流规则逐作者数值读取；能力开关由原完整Naval作者入口统一核验。
#pragma once
#include "naval_tactical_navigation.hpp"
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident {
Outcome<NavalTacticalNavigationPolicy> decode_naval_tactical_navigation(const BoundaryValue&);
}
