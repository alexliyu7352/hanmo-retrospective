// 原作者责任的有限源组合；只核定义及真实安装来源，不授当前权力或替代完整起点。
#pragma once
#include "organization_authored_origin.hpp"
namespace hanmo::resident {
// PreparedCatalog 必须是绑定本次 Common/组织/经济的原目录；只借其原角色种子，
// 不用今日 role.holder 倒填初始职责。未知责任种类、缺真实 commitment/activity
// 源拒绝。
Status validate_organization_authored_responsibilities(
    const OrganizationAuthoredOriginContext &, const PreparedCatalog &,
    const OrganizationRecord &, const LifecycleSeedPolicy &);
} // namespace hanmo::resident
