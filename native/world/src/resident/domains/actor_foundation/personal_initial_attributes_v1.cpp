#include "personal_initial_attributes_v1.hpp"
#include "battle_injury_catalog_v1.hpp"
#include "condition_application_history.hpp"
#include "resident/domains/growth/growth_permanent_contributions.hpp"
#include "resident/domains/personal/personal_battle_lifecycle.hpp"
#include "resident/kernel/native_personal_battle_lifecycle.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
Outcome<PersonalInitialAttributes> project(const ActorAttributes &attributes,
                                           const FrozenBodyScalars &body,
                                           const StableId &actor) {
  if (!body.alive || body.hp < 1 || body.hp > 100 || body.fatigue < 0 ||
      body.fatigue > 100 || body.injury < 0 || body.injury > 3)
    return {false,
            {},
            common_failure("PersonalInitialLivingBodyRequired", actor).error};
  return {true, {attributes, body.hp, body.fatigue, body.injury}, {}};
}
} // namespace
bool NativePersonalInitialAttributesRead::bound_to(
    const CommonStore &common, const PersonalState &personal,
    const PersonalCatalog &catalog) const noexcept {
  return &conditions_.common == &common && &personal_ == &personal &&
         &catalog_ == &catalog && conditions_.injury_personal == &personal &&
         conditions_.personal_catalog == &catalog &&
         &growth_.condition == &conditions_ &&
         conditions_.permanent_growth == &growth_.state &&
         conditions_.permanent_growth_catalog == &growth_.catalog &&
         absence_.bound_to(common, conditions_.state, conditions_.catalog);
}
Status NativePersonalInitialAttributesRead::selected() const {
  if (!bound_to(conditions_.common, personal_, catalog_) || !catalog_.enabled ||
      !battle_injury_source1_selected(conditions_.catalog.identity))
    return common_failure("PersonalInitialAttributeSourcesRequired", {},
                          ErrorKind::unsupported_capability);
  auto lifecycle =
      selected_native_personal_battle_lifecycle(conditions_.catalog.identity);
  if (!lifecycle.ok)
    return {false, {}, lifecycle.error};
  if (lifecycle.value != 1)
    return common_failure("PersonalInitialLifecycle1Required", {},
                          ErrorKind::unsupported_capability);
  auto now = clock_.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  auto same = compare_world_moments(now.value, conditions_.actual_now);
  if (!same.ok)
    return {false, {}, same.error};
  if (same.value != 0)
    return common_failure("PersonalInitialActualClockMismatch");
  return common_success();
}
Status NativePersonalInitialAttributesRead::validate_context() const {
  auto valid = selected();
  if (!valid.ok)
    return valid;
  // 空会话/空单位也必须通过原全源门，不能因逐人循环为空漏验。
  return validate_personal_battle_lifecycle_sources(
      conditions_.common, personal_, 1, conditions_.actual_now);
}
Outcome<PersonalInitialAttributes>
NativePersonalInitialAttributesRead::current(const StableId &actor) const {
  auto valid = selected();
  if (!valid.ok)
    return {false, {}, valid.error};
  auto attributes = read_current_effective_character_attributes_v1(
      conditions_, growth_, absence_, clock_, actor);
  if (!attributes.ok)
    return {false, {}, attributes.error};
  auto body = read_current_character_condition(conditions_, actor);
  if (!body.ok)
    return {false, {}, body.error};
  return project(attributes.value, body.value.body, actor);
}
Outcome<PersonalInitialAttributes>
NativePersonalInitialAttributesRead::before_start(const PersonalBattle &battle,
                                                  const StableId &actor) const {
  auto valid = selected();
  if (!valid.ok)
    return {false, {}, valid.error};
  if (personal_.battles.find(battle.id) != &battle || !battle.lifecycle ||
      !battle.units.find(actor))
    return {false,
            {},
            common_failure("PersonalInitialActualBattleRequired", actor).error};
  valid = validate_personal_battle_lifecycle_sources(
      conditions_.common, personal_, 1, conditions_.actual_now);
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto &started = battle.lifecycle->started;


  auto memory = CheckpointMemoryCharge(0);
  const SourceRef source{checkpoint_fresh_text_copy("personal"),
                         checkpoint_fresh_text_copy("personal_battle_started"),
                         checkpoint_fresh_string_copy(started.source_id)};
  auto attributes = read_effective_character_attributes_before_fact_v1(
      conditions_, growth_, absence_, actor, source, started.occurred_at);
  if (!attributes.ok)
    return {false, {}, attributes.error};
  auto body = read_character_condition_before_application_fact(
      conditions_, actor, source, started.occurred_at);
  if (!body.ok)
    return {false, {}, body.error};
  return project(attributes.value, body.value.body, actor);
}
Integer personal_initial_martial_v1(const PersonalInitialAttributes &source) {
  const Integer efficiencies[] = {10000, 9000, 7500, 6000};
  const Integer fatigue = source.fatigue >= 90   ? 6000
                          : source.fatigue >= 70 ? 7500
                          : source.fatigue >= 40 ? 9000
                                                 : 10000;
  return std::max<Integer>(
      1, source.effective.martial *
             std::min(fatigue,
                      efficiencies[std::clamp<Integer>(source.injury, 0, 3)]) /
             10000);
}
Status validate_personal_initial_attributes_sources_v1(
    const CommonStore &common, const PersonalState &personal,
    const PersonalCatalog &catalog,
    const PersonalInitialAttributesRead &source) {
  if (!source.bound_to(common, personal, catalog))
    return common_failure("PersonalInitialSameWorldSourcesRequired");
  auto valid = source.validate_context();
  if (!valid.ok)
    return valid;
  for (const auto &entry : personal.battles.rows()) {
    const auto &battle = entry.second;
    if (entry.first != battle.id || !battle.lifecycle)
      return common_failure("PersonalInitialActualBattleRequired", entry.first);
    for (const auto &row : battle.units.rows()) {
      auto before = source.before_start(battle, row.first);
      if (!before.ok)
        return {false, {}, before.error};
      if (row.first != row.second.actor_id ||
          row.second.initial_hp != before.value.hp ||
          row.second.martial != personal_initial_martial_v1(before.value))
        return common_failure("PersonalInitialAttributeSnapshotMismatch",
                              row.first);
    }
  }
  return common_success();
}
} // namespace hanmo::resident
