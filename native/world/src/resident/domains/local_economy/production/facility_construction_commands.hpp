// E01 原 PlanFacilityCapacity 四项输入；不接自由时刻、产权或已完工结果。
#pragma once
#include "facility_construction_model.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
namespace hanmo::resident {
inline constexpr const char *plan_facility_capacity_command =
    "plan_facility_capacity";
struct PlanFacilityCapacity {
  StableId asset_ref;
  EconomicRuleRef upgrade_spec;
  std::vector<StableId> workers;
  std::vector<CargoSelection> materials;
};
// 真实四成员默认形状，未知成员继续传播，不以零代替。

Outcome<PlanFacilityCapacity>
decode_plan_facility_capacity_command(const BoundaryValue &);
Outcome<BoundaryValue>
encode_plan_facility_capacity_command(const PlanFacilityCapacity &);
} // namespace hanmo::resident
