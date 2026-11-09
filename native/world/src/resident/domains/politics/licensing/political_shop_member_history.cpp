// 法律业务先限原司法/接册事实，再复用同一成员历史重放；既有错误码与准入边界保持。
#include "political_shop_member_history.hpp"
#include "political_shop_licensing_detail.hpp"
#include "political_shop_licensing_exact2.hpp"
#include "political_shop_registry_contract.hpp"
#include "resident/domains/politics/political_member_history.hpp"
namespace hanmo::resident {
Status validate_political_shop_historical_member_duty(
    const PoliticalShopLicensingContext &x, const ActingAuthority &authority,
    Integer tick, const StableId &boundary) {
  const auto fail = [&](const StableId &id = StableId{}) {
    return common_failure("PoliticalShopMemberHistoryUnavailable",
                          id.empty() ? authority.actor_id : id);
  };
  if (!x.organization_catalog.enabled || authority.actor_id.empty() ||
      authority.organization_id.empty() ||
      !x.common.actor(authority.actor_id) || tick < 0 || tick > x.common.tick())
    return fail();
  const auto *head = x.common.state().facts.find(boundary);
  if (!head || head->id != boundary)
    return fail(boundary);
  if (head && head->domain == political_shop_licensing_module &&
      (head->kind == "political_wanted_registered" ||
       head->kind == "political_wanted_released" ||
       head->kind == political_shop_clearance_kind ||
       head->kind == "political_shop_permit_revoked")) {
    auto n = shop_law_detail::fact_sequence(x.common, boundary, head->kind);
    if (!n.ok)
      return {false, {}, n.error};
  }
  if (x.state.version == 2) {
    auto actual = read_political_shop_licensing_actual_source2(
        x, {head->domain, head->kind, head->id});
    if (!actual.ok)
      return {false, {}, actual.error};
    if (actual.value.occurred_at->half_day_tick != tick)
      return fail(boundary);
    return validate_political_historical_member_duty(
        x.common, x.organizations, x.organization_catalog, authority, tick,
        boundary, PoliticalMemberHistoryUse::shop_licensing, x.phase, x.clock,
        actual.value.occurred_at);
  }
  return validate_political_historical_member_duty(
      x.common, x.organizations, x.organization_catalog, authority, tick,
      boundary, PoliticalMemberHistoryUse::shop_licensing, x.phase);
}
} // namespace hanmo::resident
