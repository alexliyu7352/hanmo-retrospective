// 原初始自有设施使用权窄读口；不证明今日租赁、转属或任职授权。
#pragma once
#include "economic_facility_sources.hpp"

namespace hanmo::resident {
struct EconomicFacilityInstallationReadContext;
// 借值仅在调用方绑定的同一只读 job/root 存活且未写入期间有效，禁止保存。
// SourceRef 与期望 Owner 只是核验目标；权利必须反读原安装证据、锁行与政策。
Outcome<const PhysicalServiceFacilityExtension *>
read_economic_facility_initial_owned_usage(
    const EconomicFacilityInstallationReadContext &, const SourceRef &,
    const Owner &expected_owner);
} // namespace hanmo::resident
