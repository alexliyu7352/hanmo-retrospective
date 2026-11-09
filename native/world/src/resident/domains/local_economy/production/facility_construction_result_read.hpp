// 原施工结果只反读真实已供工；不是当前权利或专业应用的放行口。
#pragma once
#include "facility_construction_service.hpp"

namespace hanmo::resident {
// 原期间的身份、时刻和工量必须与同一个 S0 Fixed /2 结果逐字段相同。
// 借用结果与 context 同 job、同根寿命；不得保存到持久根。
Outcome<const ConstructionPeriodResult *>
read_facility_construction_period_service_evidence(
    const FacilityConstructionServiceContext &, const MomentOwnerSource &);
// 首个有限阶段只接受条款 H 个不同原期间依 ordinal 连续覆盖，阶段在最后
// 一个真实期之后同 Moment 发布。此口验证工/料来源，不代替 S6 权利窗口、
// 完工重新核准或后续应用；不得单凭它给设施增容量。
Outcome<const ConstructionStageResult *>
read_facility_construction_stage_service_evidence(
    const FacilityConstructionServiceContext &, const MomentOwnerSource &);
} // namespace hanmo::resident
