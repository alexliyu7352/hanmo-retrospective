#include "native_character_attribute_absence.hpp"
#include "native_deferred_arcana_absence.hpp"
#include "original_catalog_identity.hpp"
#include "resident/domains/actor_foundation/battle_injury_catalog_v1.hpp"
#include "resident/domains/actor_foundation/injury_capability_map.hpp"
#include "world_bundle.hpp"
#include "world_read_stamp.hpp"

namespace hanmo::resident {
namespace {
// 只承认本代三个明确来源格式；不把旧 Condition 或未知 Growth 版本当作零修正。
Status finite_sources(const CatalogIdentity &identity) {
  if (!battle_injury_source1_selected(identity))
    return common_failure("AttributeAbsenceInjurySelectionRequired", {},
                          ErrorKind::unsupported_capability);
  std::size_t growth = 0;
  for (const auto &module : identity.module_versions)
    if (module.first == "growth_permanent_contributions") {
      if (++growth != 1 || module.second != "3")
        return common_failure("AttributeAbsenceGrowth3Required", {},
                              ErrorKind::unsupported_capability);
    }
  if (growth != 1)
    return common_failure("AttributeAbsenceGrowth3Required", {},
                          ErrorKind::unsupported_capability);
  return common_success();
}
} // namespace

NativeCharacterAttributeAbsenceRead::NativeCharacterAttributeAbsenceRead(
    const WorldBundle &world) noexcept
    : world_(world) {}

bool NativeCharacterAttributeAbsenceRead::bound_to(
    const CommonStore &common, const AuthoredActorFoundationState &state,
    const ActorFoundationCatalog &catalog) const noexcept {
  return &common == &world_.common && world_.actor_foundation &&
         &state == &*world_.actor_foundation && world_.catalog &&
         world_.catalog->actor_foundation.get() == &catalog &&
         same_original_catalog_identity(catalog.identity,
                                        world_.catalog->identity);
}

Status
NativeCharacterAttributeAbsenceRead::validate_finite_attribute_source_absence(
    const WorldMoment &actual_now) const {
  if (!world_.catalog || !world_.catalog->actor_foundation ||
      !world_.actor_foundation || !world_.catalog->growth ||
      !world_.catalog->growth_permanent || !world_.growth_permanent ||
      !world_.catalog->common || !world_.world_timeline ||
      !world_.catalog->exact_time ||
      !exact_complete_selected(*world_.catalog->exact_time))
    return common_failure("AttributeAbsenceActualWorldRootsRequired", {},
                          ErrorKind::unsupported_capability);
  const auto &catalog = *world_.catalog->actor_foundation;
  if (!bound_to(world_.common, *world_.actor_foundation, catalog) ||
      !same_original_catalog_identity(
          world_.catalog->growth_permanent->identity,
          world_.catalog->identity) ||
      catalog.original_growth.get() != world_.catalog->growth.get() ||
      !world_.catalog->growth->complete_capability_application_chain)
    return common_failure("AttributeAbsenceActualCatalogMismatch", {},
                          ErrorKind::internal);
  auto valid = finite_sources(world_.catalog->identity);
  if (!valid.ok)
    return valid;

  // 原 S0 reader 在 contact_install 等阶段读实际 adopted M，不从旧 tick
  // 拼零余量。
  WorldCurrentMomentRead clock(world_);
  auto current = clock.read_current_moment();
  if (!current.ok)
    return {false, {}, std::move(current.error)};
  auto same = compare_world_moments(current.value, actual_now);
  if (!same.ok)
    return {false, {}, std::move(same.error)};
  if (same.value != 0)
    return common_failure("AttributeAbsenceActualMomentMismatch", {},
                          ErrorKind::internal);

  // 这是原全世界反扫：目录、阶段、Action、Common 事实、S9 输出等残留一概拒绝。
  // 不因为本 World 没有 Arcana 指针而返回成功，也不在此调用 Condition/Growth
  // 全验形成环。
  valid = validate_native_deferred_arcana_absence(world_);
  if (!valid.ok)
    return valid;
  valid = validate_battle_injury_catalog_v1(catalog);
  if (!valid.ok)
    return valid;
  valid = validate_injury_capability_map(catalog);
  if (!valid.ok)
    return valid;

  // 本代作者 map 只准永久 flat，原恢复、返战、即时限制没有被安装为属性写者。
  // 当前装备/个人战目录的数值仅用于战斗，不含五属性 modifier；未来添加该来源时
  // 必须同时扩展具名原来源及此准入证明，不能继承这里的中性项。
  const auto &conditions =
      world_.actor_foundation->character_condition_extensions;
  if (!conditions.recovery_resolutions.empty() ||
      !conditions.combat_return_bindings.empty() ||
      !conditions.instant_limits.empty())
    return common_failure("AttributeAbsenceUnimplementedEffectSource", {},
                          ErrorKind::unsupported_capability);
  return common_success();
}
} // namespace hanmo::resident
