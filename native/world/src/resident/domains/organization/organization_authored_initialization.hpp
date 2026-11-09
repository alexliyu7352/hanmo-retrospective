// 两阶段作者初始化：仅完成同一候选中已材料化的原身体，不复制安装第二根。
#pragma once
#include "organization_authored_origin.hpp"
namespace hanmo::resident {
// 前段必须唯一调用原 make_initial_organization；此段借实际安装源和真实 clock。
// 任一失败必须丢弃 Common/身体/runtime/设施等整个候选，绝不提前发布部分根。
Status finish_authored_organization_runtime_in_candidate(
    CommonStore &, OrganizationState &, OrganizationRuntime &,
    const OrganizationCatalog &, const OrganizationLifecycleCatalog &,
    const LocalEconomyCatalog &, const LocalEconomyState &,
    const EconomicObligationSourcesState &,
    const OrganizationAuthoredOriginSourceRead &,
    const OrganizationAuthorityClock &);
} // namespace hanmo::resident
