#include "attribute_source_cut_v1.hpp"
#include "battle_injury_catalog_v1.hpp"
#include "condition_application_history.hpp"
#include "injury_attribute_reducer.hpp"
#include "resident/domains/growth/growth_permanent_contributions.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/read_stamp.hpp"
namespace hanmo::resident {
namespace {
Status roots(const CharacterConditionSources &s,
             const GrowthPermanentSources &growth, const StableId &actor) {
  if (&growth.condition != &s || s.permanent_growth != &growth.state ||
      s.permanent_growth_catalog != &growth.catalog ||
      !battle_injury_source1_selected(s.catalog.identity))
    return common_failure("CharacterAttributeSameCandidateSourcesRequired",
                          actor);
  bool growth3 = false;
  for (const auto &module : s.catalog.identity.module_versions)
    if (module.first == "growth_permanent_contributions") {
      if (growth3 || module.second != "3")
        return common_failure("CharacterAttributeGrowth3SelectionRequired",
                              actor);
      growth3 = true;
    }
  if (!growth3)
    return common_failure("CharacterAttributeGrowth3SelectionRequired", actor);
  return common_success();
}
Status records(const CharacterConditionSources &s,
               const GrowthPermanentSources &growth, const StableId &actor,
               const WorldMoment &at, std::size_t cut,
               Integer condition_revision,
               const CharacterInitialValuesV2 &initial,
               CharacterAttributeComponentVisitorV1 &visitor) {
  auto valid = validate_growth_permanent_results(growth);
  if (!valid.ok)
    return valid;
  valid = validate_battle_injury_episode_sources_v1(s);
  if (!valid.ok)
    return valid;
  valid = visitor.initial(initial);
  if (!valid.ok)
    return valid;
  for (const auto &entry : growth.state.milestones.rows()) {
    if (entry.second.actor_id != actor)
      continue;
    auto original = read_verified_attribute_milestone(growth, {entry.first});
    if (!original.ok)
      return {false, {}, original.error};
    auto position = read_actual_common_fact_position(
        s.common,
        SourceRefBuildView{"growth", "attribute_milestone", entry.first});
    if (!position.ok)
      return {false, {}, position.error};
    if (position.value >= cut)
      continue;
    auto order =
        compare_world_moments(original.value->applied_at.exact_moment, at);
    if (!order.ok)
      return {false, {}, order.error};
    if (order.value > 0)
      return common_failure("CharacterAttributeFutureMilestoneAtCut",
                            entry.first);
    valid = visitor.permanent_growth(*original.value);
    if (!valid.ok)
      return valid;
  }
  for (const auto &entry :
       s.state.character_condition_extensions.effect_contributions.rows()) {
    if (entry.second.identity.actor_id != actor)
      continue;
    auto original = read_battle_injury_effect_source_v1(s, entry.first);
    if (!original.ok)
      return {false, {}, original.error};
    const auto &receipt = *original.value.origin;
    auto position = read_actual_common_fact_position(
        s.common, SourceRefBuildView{"actor_foundation", "condition_receipt",
                                     receipt.result_id});
    if (!position.ok)
      return {false, {}, position.error};
    if (position.value >= cut)
      continue;
    if (receipt.condition_revision_after > condition_revision)
      return common_failure("CharacterAttributeInjuryPrefixMismatch",
                            entry.first);
    const auto *points = std::get_if<InjuryAttributeReduction>(
        &original.value.effect->effect_payload);
    if (!points || !std::holds_alternative<PermanentInjuryEffect>(
                       original.value.effect->retention))
      return common_failure("CharacterAttributeOriginalEffectAdapterRequired",
                            entry.first);
    valid =
        validate_original_injury_attribute_reducer(*points, s.catalog.identity);
    if (!valid.ok)
      return valid;
    // Permanent original contributions remain pinned after acute episode death.
    // Each immutable contribution identity is emitted once, never per later
    // hit.
    valid = visitor.permanent_injury(original.value);
    if (!valid.ok)
      return valid;
  }
  return common_success();
}
} // namespace
Status visit_character_base_growth_injury_before_fact_v1(
    const CharacterConditionSources &s, const GrowthPermanentSources &growth,
    const StableId &actor, const SourceRef &consumer, const WorldMoment &at,
    CharacterAttributeComponentVisitorV1 &visitor) {
  auto valid = roots(s, growth, actor);
  if (!valid.ok)
    return valid;
  auto cut = read_actual_common_fact_position(s.common, consumer);
  if (!cut.ok)
    return {false, {}, cut.error};
  auto initial = read_character_initial_values_before_fact_v2(
      s.common, s.state, s.catalog, s.family_catalog, s.shared, actor, consumer,
      at, s.actual_now, s.born_sources);
  if (!initial.ok)
    return {false, {}, initial.error};
  if (!initial.value.present || !initial.value.initial.base_attributes)
    return common_failure("CharacterAttributeOriginalActorAbsent", actor);
  auto body =
      read_character_condition_before_application_fact(s, actor, consumer, at);
  if (!body.ok)
    return {false, {}, body.error};
  return records(s, growth, actor, at, cut.value,
                 body.value.head.condition_revision, initial.value.initial,
                 visitor);
}
Status visit_current_character_base_growth_injury_v1(
    const CharacterConditionSources &s, const GrowthPermanentSources &growth,
    const CurrentMomentRead &clock, const StableId &actor,
    CharacterAttributeComponentVisitorV1 &visitor) {
  auto valid = roots(s, growth, actor);
  if (!valid.ok)
    return valid;
  const auto revision = s.common.revision();
  auto now = clock.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  auto same = compare_world_moments(now.value, s.actual_now);
  if (!same.ok)
    return {false, {}, same.error};
  if (same.value != 0)
    return common_failure("CharacterAttributeCurrentClockMismatch", actor);
  auto prefix = read_current_common_fact_prefix(s.common);
  if (!prefix.ok)
    return {false, {}, prefix.error};
  auto cut = read_actual_common_fact_prefix(s.common, prefix.value);
  if (!cut.ok)
    return {false, {}, cut.error};
  auto body = read_verified_current_character_condition_head(
      s, s.common, s.state, actor, clock);
  if (!body.ok)
    return {false, {}, body.error};
  const auto condition_revision = body.value->condition_revision;
  auto initial = read_character_initial_values_v2(
      s.common, s.state, s.catalog, s.family_catalog, s.shared, actor,
      now.value, s.born_sources);
  if (!initial.ok)
    return {false, {}, initial.error};
  if (!initial.value.base_attributes)
    return common_failure("CharacterAttributeOriginalActorAbsent", actor);
  valid = records(s, growth, actor, now.value, cut.value, condition_revision,
                  initial.value, visitor);
  if (!valid.ok)
    return valid;
  auto after = clock.read_current_moment();
  if (!after.ok)
    return {false, {}, after.error};
  same = compare_world_moments(after.value, now.value);
  if (!same.ok)
    return {false, {}, same.error};
  if (same.value != 0 || s.common.revision() != revision ||
      s.common.state().facts.size() != cut.value)
    return common_failure("CharacterAttributeCurrentSourcesChanged", actor);
  return common_success();
}
} // namespace hanmo::resident
