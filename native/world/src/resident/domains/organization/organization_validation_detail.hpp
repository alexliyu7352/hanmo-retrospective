// 完整校验与旧入口共用原身体规则；仅两支实现内部调用，不对外授准入能力。
#pragma once
#include "organization_complete_validation.hpp"
namespace hanmo::resident::organization_detail {
// complete 为真实上下文时，动态主体由其 lifecycle
// 来源闭包验证；旧入口传空保原规则。
Status validate_organization_body(
    const CommonStore &, const OrganizationState &, const OrganizationCatalog &,
    const LocalEconomyState &, const DynamicResultRead *, const ActivityRead *,
    const OrganizationResourceHistoryRead &,
    const OrganizationProspectiveGrantRead &,
    const OrganizationAuthoritySources *,
    const OrganizationCompleteValidationContext *);
} // namespace hanmo::resident::organization_detail
