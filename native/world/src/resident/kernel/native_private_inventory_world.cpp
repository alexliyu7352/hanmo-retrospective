// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
// 只组合原 Estate 与原库存产权来源，不回取完整 Complete
// 工厂或复制一套资产状态。
#include "native_private_inventory_world.hpp"
#include "exact_time_profile.hpp"
#include "world_bundle.hpp"
#include <new>
#include <stdexcept>

namespace hanmo::resident {
namespace {
Status actual_roots(const WorldBundle &w) {
  if (!w.catalog || !w.catalog->exact_time ||
      !exact_complete_selected(*w.catalog->exact_time) || !w.catalog->common ||
      !w.catalog->estate || !w.catalog->family || !w.catalog->naval ||
      !w.actor_foundation || !w.catalog->actor_foundation)
    return common_failure("PrivateInventoryOriginalWorldSourcesRequired", {},
                          ErrorKind::unsupported_capability);
  const auto &catalog = *w.catalog->actor_foundation;
  if (catalog.identity.content_digest != w.catalog->identity.content_digest ||
      catalog.identity.content_digest !=
          w.common.state().metadata.content_digest ||
      catalog.scenario_id != w.common.state().metadata.scenario_id)
    return common_failure("PrivateInventoryWorldIdentityMismatch", {},
                          ErrorKind::internal);
  Integer origins = 0;
  for (const auto &module : w.catalog->identity.module_versions)
    if (module.first == "actor_start_context") {
      if (++origins != 1 || module.second != "3")
        return common_failure("PrivateInventoryOriginalActorSourcesRequired",
                              {}, ErrorKind::unsupported_capability);
    }
  if (origins != 1)
    return common_failure("PrivateInventoryOriginalActorSourcesRequired", {},
                          ErrorKind::unsupported_capability);
  const auto now = w.actual_phase_moment();
  return now.ok ? common_success() : Status{false, {}, now.error};
}
} // namespace

NativePrivateInventoryWorldRead::NativePrivateInventoryWorldRead(
    const WorldBundle &w)
    : world_(w), clock_(w),
      estate_history_{w.common,
                      *w.catalog->estate,
                      w.family,
                      *w.actor_foundation,
                      *w.catalog->actor_foundation,
                      clock_,
                      w.estate_employment_enabled() ? &w.estate_employment
                                                    : nullptr},
      estate_titles_(estate_history_, w.estate),
      context_{w.common,
               *w.catalog->common,
               w.family,
               w.military,
               *w.catalog->estate,
               estate_titles_,
               &*w.actor_foundation,
               w.catalog->actor_foundation.get(),
               w.catalog->family.get(),
               &clock_,
               w.naval,
               *w.catalog->naval},
      read_(context_) {}

Outcome<std::unique_ptr<NativePrivateInventoryWorldRead>>
NativePrivateInventoryWorldRead::create(const WorldBundle &w) {
  auto ready = actual_roots(w);
  if (!ready.ok)
    return {false, {}, std::move(ready.error)};
  try {
    auto owner = std::unique_ptr<NativePrivateInventoryWorldRead>(
        new NativePrivateInventoryWorldRead(w));
    auto bound = owner->validate_bindings();
    if (!bound.ok)
      return {false, {}, std::move(bound.error)};
    return {true, std::move(owner), {}};
  } catch (const std::bad_alloc &) {
    return {false,
            {},
            common_failure("PrivateInventoryOwningResourceExhausted", {},
                           ErrorKind::internal)
                .error};
  } catch (const std::length_error &) {
    return {false,
            {},
            common_failure("PrivateInventoryOwningResourceExhausted", {},
                           ErrorKind::internal)
                .error};
  }
}

bool NativePrivateInventoryWorldRead::bound_to(
    const WorldBundle &w) const noexcept {
  return &world_ == &w && &context_.common == &w.common &&
         &context_.family == &w.family && &context_.military == &w.military &&
         &context_.naval == &w.naval && read_.bound_to(w.common);
}

Status NativePrivateInventoryWorldRead::validate_bindings() const {
  auto ready = actual_roots(world_);
  if (!ready.ok)
    return ready;
  const auto &w = world_;
  if (!bound_to(w) || &context_.catalog != w.catalog->common.get() ||
      &context_.estates != w.catalog->estate.get() ||
      &context_.estate_titles != &estate_titles_ ||
      context_.foundation != &*w.actor_foundation ||
      context_.foundation_catalog != w.catalog->actor_foundation.get() ||
      context_.family_catalog != w.catalog->family.get() ||
      context_.clock != &clock_ ||
      &context_.naval_catalog != w.catalog->naval.get() ||
      &estate_history_.common != &w.common ||
      &estate_history_.family != &w.family ||
      &estate_history_.foundation != &*w.actor_foundation ||
      &estate_history_.foundation_catalog !=
          w.catalog->actor_foundation.get() ||
      &estate_history_.clock != &clock_)
    return common_failure("PrivateInventorySourceCandidateMismatch", {},
                          ErrorKind::internal);
  // 这里不以构造成功代替每笔真实产权核验；消费者仍必须调用原
  // current/before-source 读口。
  return common_success();
}
} // namespace hanmo::resident
