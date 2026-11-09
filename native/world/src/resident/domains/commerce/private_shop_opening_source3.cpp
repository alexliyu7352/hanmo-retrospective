// 原 Shop /3 唯一时刻来自同 World 原 exact-operation clock；不会由整数 tick
// 或后生营业事实补出。
#include "private_shop_opening_source3.hpp"
#include "private_shop_operation_sources.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
namespace hanmo::resident {
Status validate_private_shop_opening_time_layout3(const PrivateShopState &s) {
  if (s.version != 1 && s.version != 2 && s.version != 3)
    return common_failure("UnsupportedPrivateShopVersion");
  for (const auto &entry : s.shops.rows()) {
    const auto &row = entry.second;
    if (bool(row.opened_at) != (s.version == 3))
      return common_failure("PrivateShopOpeningMomentSelectionMismatch",
                            entry.first);
    if (row.opened_at) {
      auto encoded = encode_world_moment(*row.opened_at);
      if (!encoded.ok)
        return {false, {}, encoded.error};
      if (row.opened_at->half_day_tick != row.opened_tick)
        return common_failure("PrivateShopOpeningMomentTickMismatch",
                              entry.first);
    }
  }
  return common_success();
}
Outcome<WorldMoment>
read_private_shop_opening_context_moment3(const PrivateShopContext &x) {
  if (x.catalog.version != 3 || x.state.version != 3 || !x.exact_operations ||
      &x.exact_operations->shops != &x)
    return {false,
            {},
            common_failure("PrivateShopOpeningExactContextRequired").error};
  const auto &op = *x.exact_operations;
  if (&op.spatial.common != &x.common || &op.body.common != &x.common ||
      &op.spatial.common_catalog != &x.common_catalog ||
      op.sources.schema_version != 1 || op.capabilities.schema_version != 1 ||
      &op.body.state != &op.spatial.foundation ||
      &op.body.catalog != &op.spatial.actors ||
      op.body.elapsed != &op.elapsed || op.body.scale != &op.scale)
    return {false,
            {},
            common_failure("PrivateShopOpeningExactContextMismatch").error};
  auto at = op.clock.read_current_moment();
  if (!at.ok)
    return at;
  auto body = compare_world_moments(at.value, op.body.actual_now);
  if (!body.ok)
    return {false, {}, body.error};
  auto spatial = op.spatial.spatial.current_moment();
  if (!spatial.ok)
    return {false, {}, spatial.error};
  auto match = compare_world_moments(at.value, spatial.value);
  if (!match.ok)
    return {false, {}, match.error};
  auto shape = encode_world_moment(at.value);
  if (!shape.ok)
    return {false, {}, shape.error};
  auto observed = private_shop_observed_tick(x);
  if (!observed.ok)
    return {false, {}, observed.error};
  if (body.value || match.value || at.value.half_day_tick != observed.value)
    return {false,
            {},
            common_failure("PrivateShopOpeningExactClockMismatch").error};
  return at;
}
Outcome<WorldMoment>
read_private_shop_opening_write_moment3(const PrivateShopContext &x) {
  auto committed = validate_private_shop_committed_read3(x);
  if (!committed.ok)
    return {false, {}, committed.error};
  if (x.phase)
    return {false,
            {},
            common_failure("PrivateShopCommandRequiresCommittedClock").error};
  return read_private_shop_opening_context_moment3(x);
}
Outcome<const WorldMoment *>
read_private_shop_opening_actual3(const PrivateShopOpeningSources3 &x,
                                  const SourceRef &source) {
  if (x.catalog.version != 3 || x.state.version != 3)
    return {false,
            {},
            common_failure("PrivateShopOpeningExactContextRequired").error};
  auto now = x.clock.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  auto clock_shape = encode_world_moment(now.value);
  if (!clock_shape.ok)
    return {false, {}, clock_shape.error};
  if (source.domain != private_shop_module ||
      source.kind != private_shop_open_kind || source.id.empty())
    return {
        false,
        {},
        common_failure("PrivateShopOpeningSourceRequired", source.id).error};
  auto actual = read_actual_common_fact_position(x.common, source);
  if (!actual.ok)
    return {false, {}, actual.error};
  const auto *row = x.state.shops.find(source.id);
  if (!row || row->id != source.id || !row->opened_at ||
      row->opened_at->half_day_tick != row->opened_tick ||
      row->opened_tick < x.state.initialized_tick ||
      !x.catalog.premises.find(row->terms.premises_id))
    return {
        false,
        {},
        common_failure("PrivateShopOpeningMomentRequired", source.id).error};
  auto shape = encode_world_moment(*row->opened_at);
  if (!shape.ok)
    return {false, {}, shape.error};
  auto order = compare_world_moments(*row->opened_at, now.value);
  if (!order.ok)
    return {false, {}, order.error};
  if (order.value > 0)
    return {false,
            {},
            common_failure("PrivateShopOpeningMomentFuture", source.id).error};
  return {true, &*row->opened_at, {}};
}
Outcome<const WorldMoment *>
read_private_shop_opening_actual3(const PrivateShopContext &x,
                                  const SourceRef &source) {
  auto actual = read_private_shop_opening_context_moment3(x);
  if (!actual.ok)
    return {false, {}, actual.error};
  return read_private_shop_opening_actual3(
      PrivateShopOpeningSources3{x.common, x.state, x.catalog,
                                 x.exact_operations->clock},
      source);
}
Status validate_private_shop_opening_sources3(const PrivateShopContext &x) {
  auto layout = validate_private_shop_opening_time_layout3(x.state);
  if (!layout.ok)
    return layout;
  if (x.state.version != 3)
    return common_success();
  auto current = read_private_shop_opening_context_moment3(x);
  if (!current.ok)
    return {false, {}, current.error};
  const WorldMoment *previous = nullptr;
  std::size_t count = 0;
  for (const auto &entry : x.common.state().facts.rows()) {
    const auto &head = entry.second;
    if (head.domain != private_shop_module ||
        head.kind != private_shop_open_kind)
      continue;
    if (head.id != entry.first)
      return common_failure("PrivateShopOpeningHeaderMismatch", entry.first);
    auto actual = read_private_shop_opening_actual3(
        PrivateShopOpeningSources3{x.common, x.state, x.catalog,
                                   x.exact_operations->clock},
        {head.domain, head.kind, head.id});
    if (!actual.ok)
      return {false, {}, actual.error};
    if (previous) {
      auto order = compare_world_moments(*previous, *actual.value);
      if (!order.ok)
        return {false, {}, order.error};
      if (order.value > 0)
        return common_failure("PrivateShopOpeningMomentOrderMismatch", head.id);
    }
    previous = actual.value;
    ++count;
  }
  if (count != x.state.shops.size())
    return common_failure("PrivateShopOpeningSourceCoverageMismatch");
  return common_success();
}
} // namespace hanmo::resident
