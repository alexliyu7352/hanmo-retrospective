#include "field_death_consumer.hpp"
#include "resident/domains/actor_foundation/life_condition.hpp"
#include "resident/domains/actor_foundation/model.hpp"
#include "resident/domains/family/care_life_condition.hpp"
#include "resident/domains/growth/growth_permanent_contributions.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
namespace {
// 原写者错误原样传出；调用者不得发布只完成名册而未完成生命后果的候选。
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
void take(Status value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
}
} // namespace
Outcome<std::vector<LifeConditionApplication>>
apply_field_segment_deaths_in_candidate(
    CommonStore &common, AuthoredActorFoundationState &foundation,
    FamilyDeathState &deaths, const ActorFoundationCatalog &catalog,
    const MilitaryState &military, const FieldBattleWorldState &field,
    const FamilyLifeCauseSources4 &life, const SourceRef &segment,
    const FieldSegmentApplicationRead &clock, const CurrentReadStampRead &reads,
    ReadStampAuthority &stamps, const DomainReadVersionSource &versions,
    const GrowthPermanentState *growth,
    GrowthCapabilityRevisionWrite *capability, CareLifeConditionWrite *care,
    const CharacterConditionSources *episode_sources) {
  try {
    if (!clock.bound_to(common, military, field) ||
        &life.military != &military || life.field_battles != &field ||
        !common.actor_condition_capture_identity())
      fail("FieldDeathSameCandidateRequired", segment.id);
    const auto now = take(reads.read_current_moment());
    const auto applied_at = take(read_field_segment_application_moment(
        common, military, field, segment, clock));
    if (take(compare_world_moments(now, applied_at)) != 0)
      fail("FieldDeathActualApplicationRequired", segment.id);
    const auto source = take(read_field_segment_settlement_source(
        common, military, field, segment, &clock));
    bool capability_selected = false;
    for (const auto &[name, version] : catalog.identity.module_versions)
      if (name == "growth_permanent_contributions" && version == "3")
        capability_selected = true;
    if (capability_selected &&
        (!capability || !growth || !capability->bound_to(common, *growth)))
      fail("FieldDeathCapabilityWriterRequired", segment.id);
    if (capability && !capability_selected)
      fail("FieldDeathCapabilitySelectionMismatch", segment.id);
    const bool care_selected = care_life_condition_consumer_selected(catalog);
    if (care_selected && (!care || !care->bound_to(common, foundation, deaths)))
      fail("FieldDeathCareWriterRequired", segment.id);
    if (care && !care_selected)
      fail("FieldDeathCareSelectionMismatch", segment.id);
    std::vector<LifeConditionApplication> out;
    for (const auto &change : source.casualties->actor_changes) {
      if (change.status != "dead")
        continue;
      const SourceRef casualty{"military", "military_casualties",
                               source.casualties->id};
      const auto fatal = take(read_field_segment_fatal_source(
          common, military, field, casualty, change.actor_id, now, &clock));
      ReadStampWriteBoundary boundary(stamps);
      auto stamp = take(reads.capture(
          {{ResourceKey::Kind::actor, change.actor_id}},
          {{"actor_foundation", "condition_head", change.actor_id}},
          {casualty.id}));
      if (!stamp)
        fail("FieldDeathActualReadStampRequired", change.actor_id);
      LifeCauseRef4 cause =
          MilitaryDeathCause4{casualty.id, fatal.source.session->battle_id,
                              change.actor_id, applied_at};
      auto applied = take(apply_verified_life_termination(
          common, foundation, deaths, catalog, life, cause, *stamp, stamps,
          versions, episode_sources));
      for (const auto &write : applied.actual_writes) {
        if (capability_selected)
          take(capability->record_actual_condition_change(write));
        if (care_selected)
          take(care->consume_actual_life_condition(write));
      }
      out.push_back(std::move(applied));
    }
    return {true, std::move(out), {}};
  } catch (Failure &error) {
    return {false, {}, std::move(error.error)};
  }
}
} // namespace hanmo::resident
