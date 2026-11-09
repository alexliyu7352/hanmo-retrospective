// 只解码本情景显式工法，不从包能力并集替旧舟战开启修缮。
#pragma once
#include "naval_repair_model.hpp"
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident {
Outcome<NavalRepairPolicy> decode_naval_repair_policy(const BoundaryValue&);
// Naval4公营服务须具名作者组织和公库，不能从当前产权自动建立收费方。
Outcome<NavalRepairChargePolicy> decode_naval_repair_charge_policy(const BoundaryValue&);
}
