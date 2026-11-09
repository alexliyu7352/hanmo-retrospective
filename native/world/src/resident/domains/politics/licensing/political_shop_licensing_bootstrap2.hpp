// 仅新局真实零M的完整初册；不提供普通权限读的省略开关。
#pragma once
#include "political_shop_licensing.hpp"
namespace hanmo::resident {
Status initialize_political_shop_licensing_bootstrap2_in_candidate(
    CommonStore &, PoliticalShopLicensingState &,
    const PoliticalShopLicensingContext &, const OrganizationAuthorityClock &,
    const CurrentMomentRead &);
namespace shop_law_bootstrap_detail {
// 只允许原registry/无authority作者seed；两钟来自同World真实拥有图。
Outcome<std::optional<WorldMoment>>
read_initial_moment(const PoliticalShopLicensingContext &,
                    const OrganizationAuthorityClock &,
                    const CurrentMomentRead &);
Status validate_initial_sources(const PoliticalShopLicensingContext &,
                                const OrganizationAuthorityClock &,
                                const CurrentMomentRead &);
} // namespace shop_law_bootstrap_detail
} // namespace hanmo::resident
