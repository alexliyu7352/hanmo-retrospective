// 历史grant可保留已合法转出的公库引用；该读口只供稳定态校验，不授予当前使用权。
#pragma once
#include "organization_model.hpp"
#include "office_lifecycle.hpp"
#include <functional>
namespace hanmo::resident {
using OrganizationResourceHistoryRead = std::function<bool(const ResourceKey&,const StableId&)>;
// 条件性目录范围单独验证，不能冒充真实历史Owner或当前支出授权。
using OrganizationProspectiveGrantRead = std::function<bool(const Grant&,const ResourceKey&)>;
// 中央必须证明原产权、真实转移链及当前产权；没有证明时沿用严格资源归属检查。
Status validate_organization(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const LocalEconomyState&,const DynamicResultRead*,const ActivityRead*,const OrganizationResourceHistoryRead&);
Status validate_organization(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const LocalEconomyState&,const DynamicResultRead*,const ActivityRead*,const OrganizationResourceHistoryRead&,const OrganizationProspectiveGrantRead&);
// 新权源profile必须提供真实历史转任/死亡reader，不能只核Common头存在就降级接收。
struct OrganizationAuthoritySources {const ReadStamp& stamp;const OfficeTransitionSourceRead& transitions;const VerifiedDeathRead& deaths;};
Status validate_organization(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const LocalEconomyState&,const DynamicResultRead*,const ActivityRead*,const OrganizationResourceHistoryRead&,const OrganizationProspectiveGrantRead&,const OrganizationAuthoritySources*);
}
