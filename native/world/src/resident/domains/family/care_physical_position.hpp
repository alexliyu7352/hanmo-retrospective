// FH §3 的实际 after 占位或执行 before 占位深值。
// 原单位身份导入经济 owner，本值不持当前床位数、不授容量或清场权限。
#pragma once
#include "resident/domains/local_economy/economic_facility_sources.hpp"
#include "resident/kernel/world_timeline.hpp"

namespace hanmo::resident {
// 未精确定位时完整保留受影响集合；空集合不能自行证明匿名身体已经移走。
struct CareAggregatePhysicalPosition {
  StableId placement_claim_id;
  std::vector<FacilityPhysicalUnitRef> affected_unit_refs;
  ActorFoundationRuleRef position_rule_ref;
};
// 只有原实际定位来源能证明身体与单位对应，不能从 claim 分配反推本支。
struct CareExactPhysicalPosition {
  StableId placement_claim_id;
  std::vector<FacilityPhysicalUnitRef> occupied_unit_refs;
  std::vector<MomentOwnerSource> position_source_refs;
};
// FH 固定 wire Aggregate=0、ExactUnits=1，完整源合法性由原业务及经济 owner
// 核验。
using CarePhysicalPositionBasis =
    std::variant<CareAggregatePhysicalPosition, CareExactPhysicalPosition>;
} // namespace hanmo::resident
