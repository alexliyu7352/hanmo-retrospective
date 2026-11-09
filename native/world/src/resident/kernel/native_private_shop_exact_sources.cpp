#include "native_private_shop_exact_sources.hpp"
#include "exact_complete_world.hpp"
#include "native_character_family_world.hpp"
#include "native_complete_world_core.hpp"
#include "resident/domains/family/life_age.hpp"
#include "world_bundle.hpp"

namespace hanmo::resident {
namespace {
Status selected(const WorldBundle &w) {
  if (!w.catalog || !w.catalog->private_shops ||
      w.catalog->private_shops->version != 3 || w.private_shops.version != 3 ||
      !w.catalog->political_shop_licensing ||
      w.catalog->political_shop_licensing->version != 1 ||
      w.political_shop_licensing.version != 2 || !w.catalog->common ||
      !w.catalog->family || !w.catalog->actor_foundation ||
      !w.actor_foundation || !w.action_elapsed || !w.catalog->exact_time ||
      !exact_complete_selected(*w.catalog->exact_time))
    return common_failure("NativePrivateShopExactOriginalRootsRequired", {},
                          ErrorKind::unsupported_capability);
  Integer shop = 0, law = 0;
  for (const auto &module : w.catalog->identity.module_versions) {
    if (module.first == "private_shops") {
      if (shop || module.second != "3")
        return common_failure("NativePrivateShopExactSelectionMismatch");
      shop = 3;
    } else if (module.first == "political_shop_licensing") {
      if (law || module.second != "2")
        return common_failure("NativePrivateShopExactSelectionMismatch");
      law = 2;
    }
  }
  if (shop != 3 || law != 2 || w.political_shop_registry_successions_enabled())
    return common_failure("NativePrivateShopExactSelectionMismatch");
  return common_success();
}
} // namespace

NativePrivateShopExactSources::NativePrivateShopExactSources(
    const NativeCompleteWorldCoreSources &core, CharacterConditionSources body)
    : core_(core), body_(std::move(body)),
      authority_clock_(core.characters.early.world),
      read_boundary_(core.characters.early.world, core.characters.early.clock),
      opening_{core.characters.early.world.common,
               core.characters.early.world.private_shops,
               *core.characters.early.world.catalog->private_shops,
               core.characters.early.clock},
      age_{core.characters.family.ages(), core.characters.early.clock} {}

Outcome<std::unique_ptr<NativePrivateShopExactSources>>
NativePrivateShopExactSources::create(
    const NativeCompleteWorldCoreSources &core) {
  const auto &w = core.characters.early.world;
  auto checked = selected(w);
  if (!checked.ok)
    return {false, {}, std::move(checked.error)};
  checked = core.characters.family.validate_bindings();
  if (!checked.ok)
    return {false, {}, std::move(checked.error)};
  auto body = core.characters.family.body().sources();
  if (!body.ok)
    return {false, {}, std::move(body.error)};
  auto out = std::unique_ptr<NativePrivateShopExactSources>(
      new NativePrivateShopExactSources(core, std::move(body.value)));
  checked = out->validate_bindings();
  if (!checked.ok)
    return {false, {}, std::move(checked.error)};
  return {true, std::move(out), {}};
}

bool NativePrivateShopExactSources::bound_to(
    const WorldBundle &w) const noexcept {
  return &core_.characters.early.world == &w;
}

Status NativePrivateShopExactSources::validate_bindings() const {
  const auto &w = core_.characters.early.world;
  auto checked = selected(w);
  if (!checked.ok)
    return checked;
  const auto &ages = core_.characters.family.ages();
  if (&body_.common != &w.common || &body_.state != &*w.actor_foundation ||
      &body_.catalog != w.catalog->actor_foundation.get() ||
      body_.shared != w.catalog->common.get() ||
      body_.elapsed != &*w.action_elapsed || &opening_.common != &w.common ||
      &opening_.state != &w.private_shops ||
      &opening_.catalog != w.catalog->private_shops.get() ||
      &opening_.clock != &core_.characters.early.clock ||
      &age_.clock != &opening_.clock || &age_.basis != &ages ||
      &ages.common != &w.common || &ages.foundation != &*w.actor_foundation ||
      &ages.foundation_catalog != w.catalog->actor_foundation.get() ||
      &ages.family != &w.family || &ages.births != &w.family_birth ||
      &ages.control.common != &w.common || &ages.control.family != &w.family ||
      &ages.control.catalog != w.catalog->family.get() ||
      &ages.control.shared != w.catalog->common.get())
    return common_failure("NativePrivateShopExactSameWorldRequired", {},
                          ErrorKind::internal);
  auto now = read_boundary_.read_observed_moment();
  if (!now.ok)
    return {false, {}, std::move(now.error)};
  auto actual = opening_.clock.read_current_moment();
  if (!actual.ok)
    return {false, {}, std::move(actual.error)};
  auto authority = authority_clock_.read_organization_authority_now();
  if (!authority.ok)
    return {false, {}, std::move(authority.error)};
  const WorldMoment *observations[] = {&actual.value, &authority.value,
                                       &body_.actual_now};
  for (const auto *at : observations) {
    auto same = compare_world_moments(now.value, *at);
    if (!same.ok)
      return {false, {}, std::move(same.error)};
    if (same.value)
      return common_failure("NativePrivateShopExactMomentMismatch", {},
                            ErrorKind::internal);
  }
  // 不选 AgeState，不在构造时读尚未绑定的完整 Shop/许可/家户/P10。
  return common_success();
}
} // namespace hanmo::resident
