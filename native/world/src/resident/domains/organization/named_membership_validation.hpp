// 具名来源与工资的跨域验证；由中央在完整候选上调用。
#pragma once
#include "organization_model.hpp"
namespace hanmo::resident {
class OrganizationAuthorityClock;
// 旧profile只允许旧目录来源，新profile逐项证明邀请、本人决定和唯一episode。
Status validate_named_membership(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const LocalEconomyState&,const LocalEconomyCatalog&,const OrganizationAuthorityClock* actual_clock=nullptr);
} // 冷恢复从不补写缺失来源或重放付款。
