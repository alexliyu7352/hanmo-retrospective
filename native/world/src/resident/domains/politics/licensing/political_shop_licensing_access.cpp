// 开设资格只读取当时已存在的完整册、精确铺位许可与未解除通缉；历史开设不借今日授权。
#include "political_shop_licensing_detail.hpp"
#include "political_shop_licensing_exact2.hpp"
#include "resident/domains/commerce/private_shop_opening_provenance.hpp"
#include "resident/domains/commerce/private_shop_opening_source3.hpp"
namespace hanmo::resident::shop_law_detail {
Outcome<OwnedPrivateShopPermissionSource>
permission_owned(const PoliticalShopLicensingContext &x,
                 const PrivateShopPermissionRequest &request, Integer tick,
                 Integer sequence, const StableId &boundary) {
  const auto fail = [&](const StableId &code) {
    return Outcome<OwnedPrivateShopPermissionSource>{
        false, {}, common_failure(code, request.premises_id).error};
  };
  if (request.actor_id.empty() || request.premises_id.empty() ||
      request.place_id.empty() || !x.common.actor(request.actor_id))
    return fail("PoliticalShopPermissionRequestMismatch");
  auto alive = person_alive_before(x, request.actor_id, tick, boundary);
  if (!alive.ok)
    return {false, {}, alive.error};
  const auto *premises = x.premises.find(request.premises_id);
  if (!premises || premises->place_id != request.place_id)
    return fail("PoliticalShopPermissionRequestMismatch");
  const PoliticalShopPermit *selected = nullptr;
  const PoliticalShopClearance *source = nullptr;
  Integer latest = 0;
  for (const auto &row : x.state.permits.rows()) {
    auto n = organization_instance_sequence(row.first, "political_shop_permit");
    const auto *clearance =
        x.state.clearances.find(row.second.clearance_fact_id);
    if (!n || !clearance)
      return fail("PoliticalShopPermitHistoryUnavailable");
    if (*n < sequence && *n > latest &&
        same_request(clearance->request, request)) {
      selected = &row.second;
      source = clearance;
      latest = *n;
    }
  }
  if (!selected || !source)
    return fail("LegalPermissionUnavailable");
  const auto *court = x.catalog.courts.find(source->court_id);
  if (!court)
    return fail("PoliticalShopCourtUnavailable");
  if (source->tick > tick || selected->tick > tick ||
      tick > source->valid_through_tick)
    return fail("PoliticalShopPermissionExpired");
  auto control = court_endpoint_owned(x, *court, tick, sequence, boundary);
  if (!control.ok)
    return {false, {}, control.error};
  for (const auto &row : x.state.revocations.rows())
    if (row.second.permit_fact_id == selected->id) {
      auto n = organization_instance_sequence(row.first,
                                              "political_shop_revocation");
      if (!n)
        return fail("PoliticalShopPermitHistoryUnavailable");
      if (*n < sequence)
        return fail("PoliticalShopPermissionRevoked");
    }
  if (wanted_before(x, court->id, request.actor_id, sequence))
    return fail("PoliticalShopApplicantWanted");
  return {true,
          own_private_shop_permission_source(
              x.common.state().identity, request, selected->id, source->id,
              source->tick, tick, source->valid_through_tick),
          {}};
}
// 旧接口只在末端复制兼容；正式内部不经这份裸DTO。
Outcome<PrivateShopPermissionSource>
permission(const PoliticalShopLicensingContext &x,
           const PrivateShopPermissionRequest &request, Integer tick,
           Integer sequence, const StableId &boundary) {
  auto value = permission_owned(x, request, tick, sequence, boundary);
  if (!value.ok)
    return {false, {}, value.error};
  return {true, value.value.source(), {}};
}
} // namespace hanmo::resident::shop_law_detail
namespace hanmo::resident {
Outcome<OwnedPrivateShopPermissionSource>
PoliticalPrivateShopPermissionAccess::private_shop_opening_permission_owned(
    const PrivateShopPermissionRequest &request) const {
  auto valid = validate_political_shop_licensing(context_);
  if (!valid.ok)
    return {false, {}, valid.error};
  return shop_law_detail::permission_owned(
      context_, request, context_.common.tick(),
      context_.common.state().next_instance_id, {});
}
Outcome<OwnedPrivateShopPermissionSource> PoliticalPrivateShopPermissionAccess::
    private_shop_opening_permission_before_fact_owned(
        const PrivateShopPermissionRequest &request, const StableId &opening,
        Integer tick) const {
  auto valid = validate_political_shop_licensing(context_);
  if (!valid.ok)
    return {false, {}, valid.error};
  auto boundary = shop_law_detail::fact_sequence(context_.common, opening,
                                                 "private_shop_opened");
  if (!boundary.ok)
    return {false, {}, boundary.error};
  if (tick < 0 || tick > context_.common.tick())
    return {false,
            {},
            common_failure("PoliticalShopOpeningTickMismatch", opening).error};
  if (context_.state.version == 2) {
    auto actual = read_political_shop_permission_boundary_moment2(
        context_, {private_shop_module, private_shop_open_kind, opening});
    if (!actual.ok)
      return {false, {}, actual.error};
    const auto *row = context_.opening_sources->state.shops.find(opening);
    if (!row || row->terms.proprietor_actor_id != request.actor_id ||
        row->terms.premises_id != request.premises_id ||
        row->terms.place_id != request.place_id || row->opened_tick != tick ||
        (*actual.value).half_day_tick != tick)
      return {false,
              {},
              common_failure("PoliticalShopOpeningOriginalMismatch", opening)
                  .error};
    const auto check_exact = [&](const auto &table) -> Status {
      for (const auto &e : table.rows()) {
        if (!e.second.occurred_at)
          return common_failure("PoliticalShopExactTimeMissing", e.first);
        auto order =
            compare_world_moments(*e.second.occurred_at, *actual.value);
        if (!order.ok)
          return {false, {}, order.error};
        bool prior = shop_law_detail::before(context_.common, e.first, opening);
        if ((prior && order.value > 0) || (!prior && order.value < 0))
          return common_failure("PoliticalShopOpeningExactOrderMismatch",
                                opening);
      }
      return common_success();
    };
    for (const auto &v : {check_exact(context_.state.registries),
                          check_exact(context_.state.warrants),
                          check_exact(context_.state.releases),
                          check_exact(context_.state.clearances),
                          check_exact(context_.state.permits),
                          check_exact(context_.state.revocations)})
      if (!v.ok)
        return {false, {}, v.error};
  }
  // S5二验实际opening载荷；本读再要求所有本域头与该截面在tick和全局事实序上相容。
  auto check = [&](const auto &table, const StableId &prefix) -> Status {
    for (const auto &row : table.rows()) {
      auto n = organization_instance_sequence(row.first, prefix);
      if (!n ||
          ((*n < boundary.value) !=
           shop_law_detail::before(context_.common, row.first, opening)) ||
          (*n < boundary.value && row.second.tick > tick) ||
          (*n > boundary.value && row.second.tick < tick))
        return common_failure("PoliticalShopOpeningOrderMismatch", opening);
    }
    return common_success();
  };
  for (const auto &result :
       {check(context_.state.registries, "political_shop_registry"),
        check(context_.state.warrants, "political_wanted"),
        check(context_.state.releases, "political_wanted_release"),
        check(context_.state.clearances, "political_shop_clearance"),
        check(context_.state.permits, "political_shop_permit"),
        check(context_.state.revocations, "political_shop_revocation")})
    if (!result.ok)
      return {false, {}, result.error};
  return shop_law_detail::permission_owned(context_, request, tick,
                                           boundary.value, opening);
}
// 原公开裸ABI沿唯一full读，只用于旧调用兼容，不宣同job拥有完整。
Outcome<PrivateShopPermissionSource>
PoliticalPrivateShopPermissionAccess::private_shop_opening_permission(
    const PrivateShopPermissionRequest &request) const {
  auto value = private_shop_opening_permission_owned(request);
  if (!value.ok)
    return {false, {}, value.error};
  return {true, value.value.source(), {}};
}
Outcome<PrivateShopPermissionSource> PoliticalPrivateShopPermissionAccess::
    private_shop_opening_permission_before_fact(
        const PrivateShopPermissionRequest &request, const StableId &opening,
        Integer tick) const {
  auto value =
      private_shop_opening_permission_before_fact_owned(request, opening, tick);
  if (!value.ok)
    return {false, {}, value.error};
  return {true, value.value.source(), {}};
}
Status
PoliticalPrivateShopPermissionAccess::visit_opening_permission_provenance(
    const PrivateShopPermissionRequest &request, const StableId &opening,
    Integer tick, PrivateShopOpeningProvenanceSink &sink) const {
  // 原完整历史算法包括真实初册与六表suffix，先全部成功再同步借整个原context。
  auto permission =
      private_shop_opening_permission_before_fact_owned(request, opening, tick);
  if (!permission.ok)
    return {false, {}, permission.error};
  return sink.permission(context_, request, opening, tick);
}

} // namespace hanmo::resident
