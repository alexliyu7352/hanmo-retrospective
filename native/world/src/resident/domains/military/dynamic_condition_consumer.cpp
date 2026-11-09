#include "dynamic_condition_consumer.hpp"
#include "resident/domains/growth/growth_permanent_contributions.hpp"
#include "resident/domains/actor_foundation/character_condition_read.hpp"
#include "resident/domains/growth/growth_model.hpp"
#include "resident/domains/family/care_life_condition.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include <iterator>
namespace hanmo::resident {using namespace movement_detail;
// 消费真实军旅人物后果；缺少照护写者时保留不支持能力错误，其他失败沿原守卫传播。
Outcome<DynamicMilitaryConditionApplication>
apply_dynamic_military_named_conditions_in_candidate(
    CommonStore &common, AuthoredActorFoundationState &foundation,
    FamilyDeathState &deaths, const ActorFoundationCatalog &catalog,
    const MilitaryState &military, const MilitaryExactState &exact,
    const FamilyLifeCauseSources4 &life, const SourceRef &ref,
    const CurrentReadStampRead &reads, ReadStampAuthority &stamps,
    const DomainReadVersionSource &versions,
    const CharacterConditionSources &conditions,
    GrowthCapabilityRevisionWrite *capability,
    CareLifeConditionWrite *care_life) {
  return guard<DynamicMilitaryConditionApplication>([&] {
    if (ref.domain != "military" || ref.kind != "military_casualties")
      fail("DynamicMilitaryCasualtySourceRequired", ref.id);
    const auto *actual = exact.dynamic_casualty_facts.find(ref.id);
    const auto *head = common.state().facts.find(ref.id);
    if (!actual || actual->id != ref.id || !head ||
        head->domain != ref.domain || head->kind != ref.kind)
      fail("DynamicMilitaryCasualtySourceRequired", ref.id);
    const auto now = take(reads.read_current_moment());
    if (!same_moment(actual->effective_at, now))
      fail("DynamicMilitaryConditionActualPhaseRequired", ref.id);
    if (&conditions.common != &common || &conditions.state != &foundation ||
        &conditions.catalog != &catalog || conditions.military != &military ||
        conditions.military_exact != &exact || &life.military != &military ||
        life.military_exact != &exact ||
        conditions.military_formations != life.military_formations ||
        conditions.military_returns != life.military_returns ||
        !same_moment(conditions.actual_now, now))
      fail("MilitaryConditionSameCandidateSourcesRequired", ref.id);
    const bool capability_required =
        conditions.growth_catalog &&
        conditions.growth_catalog->complete_capability_application_chain;
    if (capability_required &&
        (!capability || !conditions.permanent_growth ||
         !capability->bound_to(common, *conditions.permanent_growth)))
      fail("MilitaryConditionCapabilityWriterRequired", ref.id);
    if (capability && !capability_required)
      fail("MilitaryConditionCapabilitySelectionMismatch", ref.id);
    const bool care_required = care_life_condition_consumer_selected(catalog);
    if (care_required &&
        (!care_life || !care_life->bound_to(common, foundation, deaths)))
      throw movement_detail::Failure{
          common_failure("MilitaryConditionCareLifeWriterRequired", ref.id,
                         ErrorKind::unsupported_capability)
              .error};
    if (care_life && !care_required)
      fail("MilitaryConditionCareLifeSelectionMismatch", ref.id);
    if (!common.actor_condition_capture_identity())
      fail("MilitaryConditionEnclosingCaptureRequired", ref.id);
    const MilitaryCasualtyHistoryContext history{common, now,
                                                 conditions.military_formations,
                                                 conditions.military_returns};
    DynamicMilitaryConditionApplication out;
    for (std::size_t n = 0; n < actual->actor_changes.size(); ++n) {
      const auto source = take(read_military_actor_consequence(
          military, exact, ref, 2, Integer(n), {}, &history));
      if (!same_moment(source.occurred_at, now))
        fail("DynamicMilitaryConditionActualPhaseRequired", ref.id);
      // 捕获下一个真实人物之前使旧租约失效。
      // 原写者失败时再次失效，由调用方丢弃整个候选。
      ReadStampWriteBoundary boundary(stamps);
      const auto stamp = take(reads.capture(
          {{ResourceKey::Kind::actor, source.actor_id}}, {}, {ref.id}));
      if (!stamp)
        fail("MilitaryConditionActualReadStampRequired", ref.id);
      if (source.status == "wounded") {
        auto applied = take(apply_verified_military_wound(
            common, foundation, catalog, military, exact, ref,
            source.source_ordinal, *stamp, stamps, versions,
            conditions.military_formations, conditions.military_returns));
        if (capability)
          for (const auto &write : applied.actual_writes)
            take(capability->record_actual_condition_change(write));
        out.condition_receipts.push_back(applied.receipt_ref);
        out.actual_writes.insert(
            out.actual_writes.end(),
            std::make_move_iterator(applied.actual_writes.begin()),
            std::make_move_iterator(applied.actual_writes.end()));
      } else if (source.status == "dead") {
        LifeCauseRef4 cause =
            MilitaryDeathCause4{source.casualty_id, source.battle_id,
                                source.actor_id, source.occurred_at};
        auto applied = take(apply_verified_life_termination(
            common, foundation, deaths, catalog, life, cause, *stamp, stamps,
            versions));
        for (const auto &write : applied.actual_writes) {
          if (capability)
            take(capability->record_actual_condition_change(write));
          if (care_life)
            take(care_life->consume_actual_life_condition(write));
        }
        out.condition_receipts.push_back(applied.receipt_ref);
        out.death_refs.push_back(applied.death_ref);
        out.actual_writes.insert(
            out.actual_writes.end(),
            std::make_move_iterator(applied.actual_writes.begin()),
            std::make_move_iterator(applied.actual_writes.end()));
      } else if (source.status == "captured" || source.status == "deserted") {
        auto applied = take(apply_verified_military_availability(
            common, foundation, conditions, ref, source.source_ordinal, *stamp,
            stamps, versions));
        out.availability_transition_ids.push_back(applied.transition_id);
        out.availability_writes.insert(
            out.availability_writes.end(),
            std::make_move_iterator(applied.actual_writes.begin()),
            std::make_move_iterator(applied.actual_writes.end()));
      } else
        fail("UnknownMilitaryNamedConsequence", source.actor_id);
    }
    return out;
  });
}
}
