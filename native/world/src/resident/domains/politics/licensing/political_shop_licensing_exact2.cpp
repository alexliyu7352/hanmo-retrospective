#include "political_shop_licensing_exact2.hpp"
#include "resident/domains/politics/takeover/politics_takeover_state.hpp"
#include "political_shop_licensing_bootstrap2.hpp"
#include "resident/domains/actor_foundation/character_availability_read.hpp"
#include "resident/domains/commerce/private_shop_opening_source3.hpp"
#include "resident/domains/organization/office_authority.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/original_world_read_boundary.hpp"
#include <limits>
namespace hanmo::resident {
namespace {
template <class T>
Status table_time(const OrderedTable<T> &rows, Integer version) {
  for (const auto &entry : rows.rows()) {
    const auto &r = entry.second;
    if (bool(r.occurred_at) != (version == 2))
      return common_failure("PoliticalShopExactTimeSelectionMismatch",
                            entry.first);
    if (r.occurred_at) {
      auto shape = encode_world_moment(*r.occurred_at);
      if (!shape.ok)
        return {false, {}, shape.error};
      if (r.occurred_at->half_day_tick != r.tick)
        return common_failure("PoliticalShopExactTickMismatch", entry.first);
    }
  }
  return common_success();
}
} // namespace
Status validate_political_shop_licensing_time_layout(
    const PoliticalShopLicensingState &s) {
  if (s.version != 1 && s.version != 2)
    return common_failure("UnsupportedPoliticalShopLicensingVersion");
  for (const auto &v :
       {table_time(s.registries, s.version), table_time(s.warrants, s.version),
        table_time(s.releases, s.version), table_time(s.clearances, s.version),
        table_time(s.permits, s.version), table_time(s.revocations, s.version)})
    if (!v.ok)
      return v;
  return common_success();
}
namespace shop_law_bootstrap_detail {
Outcome<std::optional<WorldMoment>>
read_initial_moment(const PoliticalShopLicensingContext &x,
                    const OrganizationAuthorityClock &authority,
                    const CurrentMomentRead &clock) {
  const auto denied =
      [](const char *code) -> Outcome<std::optional<WorldMoment>> {
    return {false, {}, common_failure(code).error};
  };
  if (x.state.version != 2 || x.common.tick() != 0 || x.phase ||
      x.original_sink || x.registry_successions || x.character_sources ||
      x.opening_sources || !x.organization_catalog.exact_fact_clock_enabled ||
      (x.clock && x.clock != &authority) ||
      (x.moment_clock && x.moment_clock != &clock) ||
      !x.state.releases.empty() || !x.state.clearances.empty() ||
      !x.state.permits.empty() || !x.state.revocations.empty())
    return denied("PoliticalShopBootstrapSourcesRequired");
  if (x.takeover && (&x.takeover->common != &x.common || x.takeover->phase ||
                     !x.takeover->state.registrations.empty()))
    return denied("PoliticalShopBootstrapControlHistoryExists");
  for (const auto &row : x.state.warrants.rows())
    if (row.second.authority || !row.second.grant_id.empty())
      return denied("PoliticalShopBootstrapAuthorityHistoryExists");
  auto a = authority.read_organization_authority_now();
  if (!a.ok)
    return {false, {}, a.error};
  auto b = clock.read_current_moment();
  if (!b.ok)
    return {false, {}, b.error};
  auto ashape = encode_world_moment(a.value);
  if (!ashape.ok)
    return {false, {}, ashape.error};
  auto bshape = encode_world_moment(b.value);
  if (!bshape.ok)
    return {false, {}, bshape.error};
  auto same = compare_world_moments(a.value, b.value);
  if (!same.ok)
    return {false, {}, same.error};
  auto zero = compare_world_moments(a.value, WorldMoment{});
  if (!zero.ok)
    return {false, {}, zero.error};
  if (same.value || zero.value)
    return denied("PoliticalShopBootstrapActualZeroRequired");
  return {true, std::move(a.value), {}};
}
} // namespace shop_law_bootstrap_detail
Outcome<std::optional<WorldMoment>> read_political_shop_licensing_actual_now2(
    const PoliticalShopLicensingContext &x) {
  if (x.state.version == 1)
    return {true, std::nullopt, {}};
  if (x.state.version != 2 || !x.clock ||
      !x.organization_catalog.exact_fact_clock_enabled ||
      x.registry_successions)
    return {
        false, {}, common_failure("PoliticalShopExactSourcesRequired").error};
  auto now = x.clock->read_organization_authority_now();
  if (!now.ok)
    return {false, {}, now.error};
  if (!x.character_sources || !x.moment_clock ||
      &x.character_sources->common != &x.common ||
      x.character_sources->shared != &x.common_catalog)
    return {false,
            {},
            common_failure("PoliticalShopExactLifeSourcesRequired").error};
  auto actual = x.moment_clock->read_current_moment();
  if (!actual.ok)
    return {false, {}, actual.error};
  auto clocks = compare_world_moments(now.value, actual.value);
  if (!clocks.ok)
    return {false, {}, clocks.error};
  auto bodies =
      compare_world_moments(now.value, x.character_sources->actual_now);
  if (!bodies.ok)
    return {false, {}, bodies.error};
  if (clocks.value || bodies.value)
    return {false, {}, common_failure("PoliticalShopExactClockMismatch").error};
  auto valid = encode_world_moment(now.value);
  if (!valid.ok)
    return {false, {}, valid.error};
  if (!x.read_boundary2 ||
      !x.read_boundary2->bound_to(x.common, *x.moment_clock))
    return {false,
            {},
            common_failure("PoliticalShopActualReadBoundaryRequired").error};
  auto observed = x.read_boundary2->read_observed_moment();
  if (!observed.ok)
    return {false, {}, observed.error};
  auto same = compare_world_moments(now.value, observed.value);
  if (!same.ok)
    return {false, {}, same.error};
  if (same.value)
    return {false,
            {},
            common_failure("PoliticalShopActualReadBoundaryMismatch").error};
  return {true, std::move(now.value), {}};
}
Outcome<PoliticalShopLicensingActualSource2>
read_political_shop_licensing_actual_source2(
    const PoliticalShopLicensingContext &x, const SourceRef &source) {
  const auto fail = [&](const char *code) {
    return Outcome<PoliticalShopLicensingActualSource2>{
        false, {}, common_failure(code, source.id).error};
  };
  if (x.state.version != 2 || source.domain != political_shop_licensing_module)
    return fail("PoliticalShopExactSourceUnsupported");
  auto at = read_political_shop_licensing_actual_now2(x);
  if (!at.ok)
    return {false, {}, at.error};
  auto position = read_actual_common_fact_position(x.common, source);
  if (!position.ok)
    return {false, {}, position.error};
  const auto *head = x.common.state().facts.find(source.id);
  auto header =
      validate_political_shop_licensing_fact_header(x.common, x.state, *head);
  if (!header.ok)
    return {false, {}, header.error};
  const WorldMoment *occurred = nullptr;
  Integer tick = -1;
  const auto get = [&](const auto &table) {
    if (const auto *row = table.find(source.id)) {
      if (row->occurred_at)
        occurred = &*row->occurred_at;
      tick = row->tick;
    }
  };
  get(x.state.registries);
  get(x.state.warrants);
  get(x.state.releases);
  get(x.state.clearances);
  get(x.state.permits);
  get(x.state.revocations);
  if (!occurred || occurred->half_day_tick != tick)
    return fail("PoliticalShopExactTimeMissing");
  const auto *seed = x.state.warrants.find(source.id);
  if (x.state.registries.find(source.id) || (seed && !seed->authority)) {
    auto initial = compare_world_moments(*occurred, WorldMoment{});
    if (!initial.ok)
      return {false, {}, initial.error};
    if (initial.value)
      return fail("PoliticalShopExactInitialBoundaryRequired");
  }
  auto cmp = compare_world_moments(*occurred, *at.value);
  if (!cmp.ok)
    return {false, {}, cmp.error};
  if (cmp.value > 0)
    return fail("PoliticalShopExactSourceFuture");
  return {true, {head, occurred}, {}};
}
Outcome<WorldMoment>
NativeShopLicensingAuthoritySourceRead2::read_organization_authority_now()
    const {
  if (!x_.clock)
    return {false, {}, common_failure("PoliticalShopExactClockRequired").error};
  return x_.clock->read_organization_authority_now();
}
Outcome<ActualShopLicensingAuthoritySourceView>
NativeShopLicensingAuthoritySourceRead2::read_actual_shop_authority2(
    const MomentOwnerSource &ref) const {
  if (ref.source_schema != 2)
    return {false,
            {},
            common_failure("PoliticalShopExactAuthoritySchemaRequired").error};
  auto raw = read_political_shop_licensing_actual_source2(x_, ref.source_ref);
  if (!raw.ok)
    return {false, {}, raw.error};
  ActualShopLicensingAuthoritySourceView out;
  out.actual_head = raw.value.head;
  out.occurred_at = raw.value.occurred_at;
  const auto &id = ref.source_ref.id;
  if (const auto *row = x_.state.warrants.find(id)) {
    if (row->authority)
      out.actor = &*row->authority;
    out.grant_id = &row->grant_id;
  } else if (const auto *row = x_.state.releases.find(id)) {
    out.actor = &row->authority;
    out.grant_id = &row->grant_id;
  } else if (const auto *row = x_.state.clearances.find(id)) {
    out.actor = &row->authority;
    out.grant_id = &row->grant_id;
  } else if (const auto *row = x_.state.revocations.find(id)) {
    out.actor = &row->authority;
    out.grant_id = &row->grant_id;
  }
  if (!out.actor || !out.grant_id || out.grant_id->empty())
    return {false,
            {},
            common_failure("PoliticalShopActualAuthorityRequired", id).error};
  return {true, out, {}};
}
Outcome<const WorldMoment *> read_political_shop_permission_boundary_moment2(
    const PoliticalShopLicensingContext &x, const SourceRef &actual) {
  if (actual.domain == political_shop_licensing_module) {
    auto v = read_political_shop_licensing_actual_source2(x, actual);
    return {v.ok, v.ok ? v.value.occurred_at : nullptr, v.error};
  }
  if (actual.domain != private_shop_module ||
      actual.kind != private_shop_open_kind || x.state.version != 2 ||
      !x.opening_sources)
    return {
        false, nullptr,
        common_failure("PoliticalShopOpeningExactLifeSourceRequired", actual.id)
            .error};
  const auto &shop = *x.opening_sources;
  if (&shop.common != &x.common || &shop.catalog.premises != &x.premises ||
      &shop.clock != x.moment_clock)
    return {false, nullptr,
            common_failure("PoliticalShopOpeningExactWorldMismatch", actual.id)
                .error};
  auto current = read_political_shop_licensing_actual_now2(x);
  if (!current.ok)
    return {false, nullptr, current.error};
  return read_private_shop_opening_actual3(shop, actual);
}
Outcome<std::optional<WorldMoment>> read_political_shop_licensing_write_moment(
    const PoliticalShopLicensingContext &x) {
  if (x.state.version == 2 && x.phase)
    return {false,
            {},
            common_failure("PoliticalShopCommandRequiresCommittedClock").error};
  auto now = read_political_shop_licensing_actual_now2(x);
  if (!now.ok || x.state.version == 1)
    return now;
  // 原命令只能在真实已提交边界写；没有phase指针不等于已提交。
  auto committed = x.read_boundary2->read_committed_moment();
  if (!committed.ok)
    return {false, {}, committed.error};
  auto same = compare_world_moments(*now.value, committed.value);
  if (!same.ok)
    return {false, {}, same.error};
  if (same.value)
    return {false,
            {},
            common_failure("PoliticalShopCommandRequiresCommittedClock").error};
  return now;
}
} // namespace hanmo::resident
