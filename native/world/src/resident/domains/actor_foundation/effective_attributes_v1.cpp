#include "effective_attributes_v1.hpp"
#include "battle_injury_catalog_v1.hpp"
#include "character_condition_read.hpp"
#include "injury_attribute_reducer.hpp"
#include "injury_capability_map.hpp"
#include "injury_grade_effects_v3.hpp"
#include "condition_application_history.hpp"
#include "resident/domains/growth/growth_permanent_contributions.hpp"
namespace hanmo::resident {
namespace {
Integer ActorAttributes::*field(const StableId &id) {
  if (id == "administration")
    return &ActorAttributes::administration;
  if (id == "charisma")
    return &ActorAttributes::charisma;
  if (id == "intellect")
    return &ActorAttributes::intellect;
  if (id == "leadership")
    return &ActorAttributes::leadership;
  if (id == "martial")
    return &ActorAttributes::martial;
  return nullptr;
}
class Components final : public CharacterAttributeComponentVisitorV1 {
public:
  ActorAttributes base{}, growth{}, losses{};
  bool initial_seen = false;
  Status initial(const CharacterInitialValuesV2 &original) override {
    if (initial_seen || !original.base_attributes || !original.origin)
      return common_failure("EffectiveAttributesActualOriginRequired");
    base = *original.base_attributes;
    initial_seen = true;
    return common_success();
  }
  Status permanent_growth(const AttributeMilestone &original) override {
    auto member = field(original.attribute_id);
    if (!member || original.delta != 1 || growth.*member >= 5)
      return common_failure("EffectiveAttributesOriginalMilestoneInvalid",
                            original.milestone_result_id);
    growth.*member += original.delta;
    return common_success();
  }
  Status
  permanent_injury(const VerifiedBattleInjuryEffectV1 &original) override {
    const auto *reduction = original.effect
                                ? std::get_if<InjuryAttributeReduction>(
                                      &original.effect->effect_payload)
                                : nullptr;
    if (!reduction || !std::holds_alternative<PermanentInjuryEffect>(
                          original.effect->retention))
      return common_failure(
          "EffectiveAttributesOriginalPermanentEffectRequired");
    auto member = field(reduction->attribute_id);
    if (!member || reduction->amount < 0)
      return common_failure("EffectiveAttributesOriginalReductionInvalid");
    // The admitted positive source total is base<=100 + growth<=5; other
    // positive/negative modifiers have been PROVEN uninstalled, not omitted.
    // All losses above 105 are mathematically equivalent for the final clamp.
    // Saturating this nonnegative loss classification avoids Integer overflow
    // without clamping a running attribute or losing possible cancellation.
    const auto room = Integer{105} - losses.*member;
    losses.*member += reduction->amount >= room ? room : reduction->amount;
    return common_success();
  }
};
} // namespace
namespace {
Status admit(const CharacterConditionSources &s,
             const CharacterAttributeAbsentSourcesReadV1 &absence,
             const StableId &actor) {
  if (!absence.bound_to(s.common, s.state, s.catalog))
    return {false,
            {},
            common_failure("EffectiveAttributesSameWorldAbsenceRequired", actor)
                .error};
  auto admitted =
      absence.validate_finite_attribute_source_absence(s.actual_now);
  if (!admitted.ok)
    return {false, {}, admitted.error};
  admitted = validate_battle_injury_catalog_v1(s.catalog);
  if (!admitted.ok)
    return {false, {}, admitted.error};
  admitted = validate_injury_capability_map(s.catalog);
  if (!admitted.ok)
    return {false, {}, admitted.error};
  const auto &condition = s.state.character_condition_extensions;
  if (!condition.recovery_resolutions.empty() ||
      !condition.combat_return_bindings.empty() ||
      !condition.instant_limits.empty())
    return {false,
            {},
            common_failure("EffectiveAttributesOtherOriginalSourcesRequired",
                           actor, ErrorKind::unsupported_capability)
                .error};
  return common_success();
}
} // namespace
Outcome<ActorAttributes> read_effective_character_attributes_before_fact_v1(
    const CharacterConditionSources &s, const GrowthPermanentSources &growth,
    const CharacterAttributeAbsentSourcesReadV1 &absence, const StableId &actor,
    const SourceRef &consumer, const WorldMoment &at) {
  auto admitted = admit(s, absence, actor);
  if (!admitted.ok)
    return {false, {}, admitted.error};
  Components values;
  auto read = visit_character_base_growth_injury_before_fact_v1(
      s, growth, actor, consumer, at, values);
  if (!read.ok)
    return {false, {}, read.error};
  if (!values.initial_seen)
    return {
        false,
        {},
        common_failure("EffectiveAttributesActualOriginRequired", actor).error};
  // Zero terms are justified by the explicit finite absence read above and the
  // actual map/roots, never by null optional providers or an empty query
  // result.
  const ActorAttributes absent{};
  ActorAttributes recoverable{};
  if (injury_grade_effects_v3_selected(s.catalog)) {
    auto body = read_character_condition_before_application_fact(s, actor,
                                                                 consumer, at);
    if (!body.ok)
      return {false, {}, body.error};
    auto loss = original_current_injury_losses_v3(body.value.body.injury);
    if (!loss.ok)
      return {false, {}, loss.error};
    recoverable = loss.value;
  }
  return evaluate_original_attribute_points(values.base, values.growth, absent,
                                            values.losses, recoverable);
}
Outcome<ActorAttributes> read_current_effective_character_attributes_v1(
    const CharacterConditionSources &s, const GrowthPermanentSources &growth,
    const CharacterAttributeAbsentSourcesReadV1 &absence,
    const CurrentMomentRead &clock, const StableId &actor) {
  auto admitted = admit(s, absence, actor);
  if (!admitted.ok)
    return {false, {}, admitted.error};
  Components values;
  auto read = visit_current_character_base_growth_injury_v1(s, growth, clock,
                                                            actor, values);
  if (!read.ok)
    return {false, {}, read.error};
  if (!values.initial_seen)
    return {
        false,
        {},
        common_failure("EffectiveAttributesActualOriginRequired", actor).error};
  const ActorAttributes absent{};
  ActorAttributes recoverable{};
  if (injury_grade_effects_v3_selected(s.catalog)) {
    auto body = read_current_character_condition(s, actor);
    if (!body.ok)
      return {false, {}, body.error};
    auto loss = original_current_injury_losses_v3(body.value.body.injury);
    if (!loss.ok)
      return {false, {}, loss.error};
    recoverable = loss.value;
  }
  return evaluate_original_attribute_points(values.base, values.growth, absent,
                                            values.losses, recoverable);
}
} // namespace hanmo::resident
