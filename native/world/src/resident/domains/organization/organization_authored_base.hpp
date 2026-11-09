// B1.1 原作者基地依赖：只消费 S5 已实际安装的自有设施权利，不自行制造来源。
#pragma once
#include "organization_authored_origin.hpp"

namespace hanmo::resident {
struct EconomicFacilityInstallationReadContext;
// 借同候选 Common 与不可变目录，逐源核组织 Owner 和固定治所；
// 成功只证明此初始基地子条件，不代表 active、租赁、mobile、责任或政治控制就绪。
Status validate_organization_authored_installed_base(
    const OrganizationAuthoredOriginContext &, const OrganizationRecord &,
    const LifecycleSeedPolicy &,
    const EconomicFacilityInstallationReadContext &);
} // namespace hanmo::resident
