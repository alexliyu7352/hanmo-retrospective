#include "native_family_initial_sources.hpp"
#include "exact_time_profile.hpp"
#include "world_bundle.hpp"

namespace hanmo::resident {
NativeFamilyInitialSourcesLease::NativeFamilyInitialSourcesLease(
    const WorldBundle &w, OriginalCalendarRead calendar)
    : clock_(w), calendar_(std::move(calendar)),
      control_{w.common,
               w.family,
               *w.catalog->family,
               *w.catalog->common,
               w.family_legacy_enabled() ? &w.family_legacy : nullptr,
               &*w.actor_foundation,
               w.catalog->actor_foundation.get(),
               &clock_,
               w.control_continuation ? &*w.control_continuation : nullptr},
      origins_{*w.actor_foundation, *w.catalog->actor_foundation, clock_},
      initial_age_{w.common, *w.actor_foundation, *w.catalog->actor_foundation,
                   w.family, w.family_birth,      control_,
                   calendar_} {}

Outcome<std::unique_ptr<NativeFamilyInitialSourcesLease>>
NativeFamilyInitialSourcesLease::create(const WorldBundle &w) {
  if (!w.catalog)
    return {false, {}, common_failure("FamilyInitialCatalogRequired").error};
  if (!w.catalog->exact_time ||
      !exact_complete_selected(*w.catalog->exact_time))
    return {true, {}, {}};
  if (!w.catalog->common || !w.catalog->family || !w.actor_foundation ||
      !w.catalog->actor_foundation)
    return {false,
            {},
            common_failure("CompleteFamilyOriginalSourcesRequired").error};
  const auto &identity = w.catalog->actor_foundation->identity;
  if (identity.content_digest != w.common.state().metadata.content_digest ||
      w.catalog->actor_foundation->scenario_id !=
          w.common.state().metadata.scenario_id)
    return {false,
            {},
            common_failure("FamilyInitialSourceCandidateMismatch").error};
  Integer selected = 0;
  for (const auto &module : identity.module_versions)
    if (module.first == "actor_start_context") {
      ++selected;
      if (module.second != "3")
        return {
            false, {}, common_failure("FamilyOriginV2ProfileRequired").error};
    }
  if (selected != 1)
    return {false, {}, common_failure("FamilyOriginV2ProfileRequired").error};
  auto calendar = OriginalCalendarRead::admit(
      w.catalog->identity,
      {original_calendar_rule_id, 1, w.catalog->identity.content_digest});
  if (!calendar.ok)
    return {false, {}, std::move(calendar.error)};
  // 只让原 Household 初态门读 authored origin；Born 年检仍由原完整 lease 负责。
  return {
      true,
      std::unique_ptr<NativeFamilyInitialSourcesLease>(
          new NativeFamilyInitialSourcesLease(w, std::move(calendar.value))),
      {}};
}
} // namespace hanmo::resident
