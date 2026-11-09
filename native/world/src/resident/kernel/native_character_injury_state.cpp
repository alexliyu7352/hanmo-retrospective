#include "native_character_injury_state.hpp"
#include "exact_time_profile.hpp"
// clang-format off
#include "world_bundle.hpp"
// clang-format on
#include "resident/domains/actor_foundation/condition_application_fact.hpp"
#include "resident/domains/actor_foundation/injury_sources_v1_state.hpp"
#include "resident/domains/personal/personal_model.hpp"
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
namespace {
// 每个必要键都只允许锁定原版本且恰好出现一次。
bool original_module(const CatalogIdentity &identity, std::string_view key,
                     std::string_view version) {
  std::size_t count = 0;
  for (const auto &row : identity.module_versions)
    if (row.first == key) {
      if (row.second != version)
        return false;
      ++count;
    }
  return count == 1;
}
} // namespace
Outcome<Integer> selected_native_character_injury_source_version(
    const CatalogIdentity &identity) {
  auto choice = condition_application_facts_selected(
      identity, ConditionApplicationHistoryOwner::injury_sources);
  if (!choice.ok)
    return {false, 0, std::move(choice.error)};
  if (!choice.value)
    return {true, 0, {}};
  if (!original_module(identity, "world_timeline", "2") ||
      !original_module(identity, "action_elapsed", "2") ||
      !original_module(identity, "actor_start_context", "3") ||
      !original_module(identity, "growth_fixed_service_time2", "4") ||
      !original_module(identity, "growth_permanent_contributions", "3"))
    return {false, 0,
            common_failure("CharacterInjuryCompleteGrowth4Required",
                           "character-injury-sources",
                           ErrorKind::unsupported_capability)
                .error};
  if (!original_module(identity, "field_battle", "1"))
    return {false, 0,
            common_failure("CharacterInjuryOriginalField1Required",
                           "character-injury-sources",
                           ErrorKind::unsupported_capability)
                .error};
  if (!original_module(identity, "personal", "1") ||
      !original_module(identity, "personal_battle_lifecycle", "1"))
    return {false, 0,
            common_failure("CharacterInjuryPersonalOriginalSelectionRequired",
                           "character-injury-sources",
                           ErrorKind::unsupported_capability)
                .error};
  return {true, 1, {}};
}
Status append_native_character_injury_module(const BoundaryValue &scenario,
                                             CatalogIdentity &identity,
                                             const ExactTimeProfile *exact,
                                             const PersonalCatalog &personal) {
  try {
    const BoundaryValue *block = nullptr;
    for (const auto &row : boundary::object(scenario, "scenario"))
      if (row.first == "character_injury_sources") {
        if (block)
          return common_failure("DuplicateCharacterInjuryAuthorSelection");
        block = &row.second;
      }
    for (const auto &row : identity.module_versions)
      if (row.first == "character-injury-sources")
        return common_failure("DuplicateCharacterInjuryModuleSelection");
    if (!block) {
      auto selected = selected_native_character_injury_source_version(identity);
      return selected.ok ? common_success()
                         : Status{false, {}, std::move(selected.error)};
    }
    boundary::fields(*block, {"version"}, {}, "character_injury_sources");
    if (boundary::object(*block, "character_injury_sources").size() != 1 ||
        boundary::get_int(*block, "version", "character_injury_sources") != 1)
      return common_failure("UnsupportedCharacterInjuryAuthorVersion",
                            "character_injury_sources",
                            ErrorKind::unsupported_capability);
    if (!exact || !exact_complete_selected(*exact))
      return common_failure("CharacterInjuryCompleteGrowth4Required");
    auto phase = read_complete_growth_phase_version(*exact);
    if (!phase.ok)
      return {false, {}, std::move(phase.error)};
    if (phase.value != 4 ||
        !original_module(identity, "growth_permanent_contributions", "3") ||
        !original_module(identity, "character_condition_extensions", "7"))
      return common_failure("CharacterInjuryCompleteGrowth4Required");
    if (!original_module(identity, "field_battle", "1"))
      return common_failure("CharacterInjuryOriginalField1Required");
    // source1 的原 Personal 全覆盖门即使空新局也要求真实启用及明选生命周期。
    if (!personal.enabled || !original_module(identity, "personal", "1") ||
        !original_module(identity, "personal_battle_lifecycle", "1"))
      return common_failure("CharacterInjuryPersonalOriginalSelectionRequired",
                            "character-injury-sources",
                            ErrorKind::unsupported_capability);
    // 只有明确新块才替换原 condition7 身份；旧输入和旧档绝不自动升级。
    for (auto &row : identity.module_versions)
      if (row.first == "character_condition_extensions")
        row.second = "8";
    identity.module_versions.emplace_back("character-injury-sources", "1");
    auto selected = selected_native_character_injury_source_version(identity);
    return selected.ok ? common_success()
                       : Status{false, {}, std::move(selected.error)};
  } catch (const boundary::DecodeFailure &failure) {
    return {false, {}, failure.error};
  }
}
Status validate_native_character_injury_layout(const WorldBundle &world) {
  if (!world.catalog)
    return common_failure("CharacterInjuryOriginalCatalogRequired");
  auto selected =
      selected_native_character_injury_source_version(world.catalog->identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  auto shape = validate_character_injury_sources_v1_layout(
      world.catalog->identity, world.character_injury_sources
                                   ? &*world.character_injury_sources
                                   : nullptr);
  if (!shape.ok)
    return shape;
  if (selected.value &&
      (!world.catalog->personal || !world.catalog->personal->enabled))
    return common_failure("CharacterInjuryPersonalOriginalSelectionRequired",
                          "character-injury-sources",
                          ErrorKind::unsupported_capability);
  if (selected.value &&
      (!world.actor_foundation || !world.catalog->actor_foundation ||
       !world.field_battle || !world.catalog->field_battle ||
       !world.catalog->growth ||
       !world.catalog->growth->complete_capability_application_chain ||
       !world.catalog->exact_time ||
       !exact_complete_selected(*world.catalog->exact_time)))
    return common_failure("CharacterInjuryOriginalFoundationRequired");
  return common_success();
}
} // namespace hanmo::resident
