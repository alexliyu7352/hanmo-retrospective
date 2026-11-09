// 原作者选择只沿真正 Complete 与原 P1 phase；不新增作者 version 或能力。
#pragma once
#include "politics_fiscal_time2.hpp"
#include "politics_time2.hpp"
#include "resident/kernel/exact_time_profile.hpp"
#include "takeover/politics_takeover_time2.hpp"
#include <string_view>
namespace hanmo::resident {
inline Outcome<Integer>
read_political_author_body_version(const BoundaryValue &scenario) {
  auto time = decode_exact_time_profile(scenario);
  if (!time.ok)
    return {false, 0, std::move(time.error)};
  if (!time.value || !exact_complete_selected(*time.value))
    return {true, 1, {}};
  const ExactPhaseSelection *political = nullptr;
  for (const auto &phase : time.value->complete->phase_providers)
    if (phase.provider == ExactProviderId::politics) {
      if (political)
        return {false, 0,
                common_failure("DuplicatePoliticalAuthorPhase").error};
      political = &phase;
    }
  if (!political)
    return {true, 1, {}};
  if (political->profile_id != "organization_politics_original_calendar" ||
      (political->version != 1 && political->version != 2))
    return {false, 0,
            common_failure("UnsupportedPoliticalAuthorPhase", {},
                           ErrorKind::unsupported_capability)
                .error};
  return {true, 2, {}};
}
// 冷存读只看锁定 module，0 是真实未选；不能从 payload 或 Complete 存在猜版本。
inline Outcome<Integer>
read_political_locked_body_version(const CatalogIdentity &identity,
                                   std::string_view module) {
  if (module != "politics" && module != "political_fiscal" &&
      module != "political_takeover")
    return {false, 0,
            common_failure("UnsupportedPoliticalPhysicalModule").error};
  Integer selected = 0;
  for (const auto &row : identity.module_versions)
    if (row.first == module) {
      if (selected || (row.second != "1" && row.second != "2"))
        return {false, 0,
                common_failure("PoliticalPhysicalModuleMismatch").error};
      selected = row.second == "1" ? 1 : 2;
    }
  return {true, selected, {}};
}
} // namespace hanmo::resident
