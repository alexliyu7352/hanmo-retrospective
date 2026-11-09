#include "native_private_shop_custody_retention.hpp"
#include "native_private_shop_household_custody.hpp"
#include "exact_complete_world.hpp"
#include "resident/domains/local_economy/household_foundation_lease.hpp"
#include "world_bundle.hpp"
#include <new>
#include <stdexcept>
namespace hanmo::resident {
namespace {
// 先建可选词法读阶段，再建完整 owner；返回前反序销毁，借根不逃逸本次调用。
template <class F>
Status with_original_custody(const WorldBundle &world, bool require_selected,
                             F consume) {
  try {
    auto layout = validate_native_private_shop_household_custody_layout(world);
    if (!layout.ok)
      return layout;
    if (!world.private_shop_household_custody) {
      if (require_selected)
        return common_failure("PrivateShopCustodyRetentionSelectionRequired");
      return common_success();
    }
    auto now = world.actual_phase_moment();
    if (!now.ok)
      return {false, {}, now.error};
    std::unique_ptr<PhaseCurrentReadScope> phase;
    if (!world.has_active_read_phase()) {
      auto opened = world.enter_phase_read_context(
          "private_shop_custody_retention", "local_economy",
          {now.value, now.value}, now.value);
      if (!opened.ok)
        return {false, {}, opened.error};
      phase = std::move(opened.value);
    }
    auto owner = world.complete_exact_time_bindings();
    if (!owner.ok)
      return {false, {}, owner.error};
    if (!owner.value)
      return common_failure("PrivateShopCustodyRetentionOwnerRequired");
    const auto *household = owner.value->reads().household_foundation;
    if (!household)
      return common_failure("PrivateShopCustodyRetentionHouseholdRequired");
    auto bound = household->validate_bindings();
    if (!bound.ok)
      return bound;
    const auto *source = household->private_custody();
    if (!source || &source->state != &*world.private_shop_household_custody ||
        &source->catalog !=
            world.catalog->private_shop_household_custody.get() ||
        &source->market.operation.shops.common != &world.common ||
        &source->travel != world.catalog->travel.get())
      return common_failure("PrivateShopCustodyRetentionSameWorldRequired", {},
                            ErrorKind::internal);
    // 空行也走原全根门；不能用没有当前消费者掩盖已选择来源损坏。
    auto valid = validate_private_shop_household_custody_sources(*source);
    if (!valid.ok)
      return valid;
    return consume(*source);
  } catch (const std::bad_alloc &) {
    return common_failure("PrivateShopCustodyRetentionExhausted", {},
                          ErrorKind::internal);
  } catch (const std::length_error &) {
    return common_failure("PrivateShopCustodyRetentionExhausted", {},
                          ErrorKind::internal);
  }
}
} // namespace
Status visit_native_private_shop_custody_retention(
    const WorldBundle &world, const MomentOwnerSource &actual,
    PrivateShopHouseholdCustodyPinSink &sink) {
  return with_original_custody(
      world, true, [&](const PrivateShopHouseholdCustodyContext &source) {
        return visit_private_shop_household_custody_pins(source, actual, sink);
      });
}
Status visit_native_private_shop_custody_retention(
    const WorldBundle &world, PrivateShopHouseholdCustodyPinSink &sink) {
  return with_original_custody(
      world, false, [&](const PrivateShopHouseholdCustodyContext &source) {
        for (const auto &row : source.state.accepted.rows()) {
          const MomentOwnerSource actual{
              private_shop_household_custody_source(row.first), 1};
          auto emitted =
              visit_private_shop_household_custody_pins(source, actual, sink);
          if (!emitted.ok)
            return emitted;
        }
        return common_success();
      });
}
} // namespace hanmo::resident
