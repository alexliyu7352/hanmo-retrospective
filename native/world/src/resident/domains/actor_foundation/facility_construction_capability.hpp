// 原 construction/1 工人只借同一 normal-work reducer，不另设施工身体规则。
#pragma once
#include "authored_origin.hpp"
namespace hanmo::resident {
// 唯一 facility_construction/1 + actor_start_context/3 与原完整规则齐全才注册。
// 只建立目的绑定；施工阶段、实际工时、权限与同候选准入仍由原业务核验。
Status
register_original_facility_construction_work_purpose(ActorFoundationCatalog &);
} // namespace hanmo::resident
