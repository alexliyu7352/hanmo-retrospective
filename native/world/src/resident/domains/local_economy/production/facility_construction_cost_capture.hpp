// 开工成本只能来自同候选 Common 真执行；本口不授予资源使用或付款权。
#pragma once
#include "facility_construction_model.hpp"
#include "resident/storage/resource_execution_draft.hpp"

namespace hanmo::resident {
class CommonStore;
// 只借当前 recipe 的真实 before/after/ordered flows，对照专业实际成本行。
// 原 writer 必须先核当时产权/受托与费用权，再执行资源；缺 capture 不允许
// 用 caller 自填已耗、今天库存差或初始资产 Owner 补造本结果。
Status validate_facility_construction_start_cost_capture(
    const CommonStore &, const ConstructionTerms &,
    const ConstructionStartCost &, const ResourceExecutionDraft &,
    const CurrentMomentRead &);
} // namespace hanmo::resident
