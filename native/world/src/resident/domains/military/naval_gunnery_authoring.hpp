// 舰载武器只由显式Naval6策略作者提供，未知字段/船型由准入原门拒绝。
#pragma once
#include "naval_gunnery_model.hpp"
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident {
Outcome<NavalGunneryPolicy> decode_naval_gunnery_policy(const BoundaryValue&);
}
