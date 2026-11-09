#include "growth_practice_inputs.hpp"
#include "growth_fixed_spatial_inputs.hpp"
#include "growth_internal.hpp"
#include "growth_quiet_source.hpp"
#include "resident/domains/actor_foundation/character_capability.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {
namespace {
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
bool same(const ActorConditionHead &a, const ActorConditionHead &b) {
  return a.actor_id == b.actor_id &&
         a.condition_revision == b.condition_revision &&
         a.initial_condition_seed_ref.seed_key ==
             b.initial_condition_seed_ref.seed_key &&
         bool(a.latest_receipt_ref) == bool(b.latest_receipt_ref) &&
         (!a.latest_receipt_ref ||
          a.latest_receipt_ref->result_id == b.latest_receipt_ref->result_id);
}
const LifeFixedServiceBoundary &last(const GrowthState &growth,
                                     const StableId &action) {
  const LifeFixedServiceBoundary *found = nullptr;
  for (const auto &row : growth.fixed_service_boundaries.rows())
    if (row.second.action_ref == action &&
        (!found || row.second.boundary_ordinal > found->boundary_ordinal))
      found = &row.second;
  if (!found)
    fail("GrowthFixedOriginalStartRequired", action);
  return *found;
}
const ActionTimeRegistration &
registration(const CharacterConditionSources &body, const StableId &kind) {
  const ActionTimeRegistration *found = nullptr;
  for (const auto &row : *body.action_registrations)
    if (row.owner_domain == "growth" && row.action_kind == kind) {
      if (found)
        fail("DuplicateGrowthFixedRegistration", kind);
      found = &row;
    }
  if (!found)
    fail("GrowthFixedRegistrationMissing", kind);
  return *found;
}
const PracticeSkillReceipt *changed_skill(const PracticeAwardReceipt &award,
                                          const StableId &skill) {
  for (const auto &row : award.skill_rows)
    if (row.skill_id == skill && row.actual_xp_gain)
      return &row;
  return nullptr;
}
bool reads(const LifeFixedServiceBoundary &boundary,
           const PracticeAwardReceipt &award) {
  for (const auto &actor : boundary.input_snapshot.actors)
    if (actor.actor_id == award.actor_id)
      for (const auto &skill : actor.skill_qualification_views)
        if (changed_skill(award, skill.skill_id))
          return true;
  return false;
}
const PracticeAwardRef *adoption(const LifeFixedServiceBoundary &boundary) {
  const LifeFixedInputChange *change = nullptr;
  if (const auto *input = std::get_if<LifeActualInputChange>(&boundary.cause))
    change = &input->change;
  else if (const auto *terminal =
               std::get_if<LifeOriginalTerminalApplication>(&boundary.cause))
    if (const auto *stop =
            std::get_if<LifeActualStopTrigger>(&terminal->trigger))
      change = std::get_if<LifeFixedInputChange>(&stop->source);
  if (change)
    if (const auto *growth = std::get_if<LifeGrowthInputChange>(change))
      return std::get_if<PracticeAwardRef>(&growth->original_result_ref);
  return nullptr;
}
std::size_t position(const CommonStore &common, const StableId &id) {
  const auto &order = common.state().facts.order();
  const auto found = std::find(order.begin(), order.end(), id);
  if (found == order.end())
    fail("GrowthPracticeOriginalCutRequired", id);
  return static_cast<std::size_t>(found - order.begin());
}
const PracticeAwardReceipt &read(const CharacterConditionSources &body,
                                 const PracticeAwardRef &ref) {
  if (!body.permanent_growth || !body.permanent_growth_catalog ||
      !body.work_practice)
    fail("GrowthPracticeInputSourcesRequired", ref.award_id);
  GrowthPermanentSources sources{body, *body.permanent_growth_catalog,
                                 *body.permanent_growth, *body.work_practice};
  return *take(read_verified_practice_award(sources, ref));
}
} // namespace
Status adopt_growth_practice_inputs_in_candidate(
    CommonStore &common, GrowthState &growth,
    const CharacterConditionSources &body, ActionElapsedStore &elapsed,
    const PracticeAwardRef &ref) {
  try {
    if (&body.common != &common || body.growth != &growth ||
        body.elapsed != &elapsed || !body.growth_catalog ||
        !body.growth_catalog->complete_exact_period_sources || !body.shared ||
        !body.scale || !body.action_registrations)
      fail("GrowthPracticeInputSourcesRequired", ref.award_id);
    const auto &award = read(body, ref);
    for (const auto &action_row : growth.actions.rows()) {
      const auto &prior = last(growth, action_row.first);
      if (prior.kind == LifeFixedBoundaryKind::completed ||
          prior.kind == LifeFixedBoundaryKind::stopped)
        fail("GrowthPracticeTerminalActionStillActive", action_row.first);
      if (!reads(prior, award))
        continue;
      bool already = false;
      for (const auto &row : growth.fixed_service_boundaries.rows())
        if (row.second.action_ref == action_row.first)
          if (const auto *used = adoption(row.second);
              used && used->award_id == ref.award_id) {
            if (already)
              fail("GrowthPracticeDuplicateInputAdoption", ref.award_id);
            already = true;
          }
      if (already)
        continue;
      const LifeFixedServiceBoundary *start = nullptr;
      for (const auto &row : growth.fixed_service_boundaries.rows())
        if (row.second.action_ref == action_row.first &&
            row.second.kind == LifeFixedBoundaryKind::started)
          start = &row.second;
      if (!start)
        fail("GrowthFixedOriginalStartRequired", action_row.first);
      const auto after_start = take(compare_world_moments(
          award.applied_at.exact_moment, start->occurred_at));
      if (after_start < 0 ||
          (after_start == 0 && position(common, award.original_fact_ref.id) <
                                   position(common, start->action_ref)))
        continue;
      if (take(compare_world_moments(award.applied_at.exact_moment,
                                     body.actual_now)))
        fail("GrowthPracticeUnadoptedEarlierSource", ref.award_id);
      if (prior.boundary_ordinal == std::numeric_limits<Integer>::max())
        fail("GrowthFixedOrdinalOverflow", action_row.first);
      LifeFixedServiceBoundary next;
      next.action_ref = prior.action_ref;
      next.action_kind = prior.action_kind;
      next.owner_ref = prior.owner_ref;
      next.previous_boundary_ref = prior.boundary_id;
      next.boundary_ordinal = prior.boundary_ordinal + 1;
      next.kind = LifeFixedBoundaryKind::input_rebound;
      next.occurred_at = body.actual_now;
      next.input_snapshot = prior.input_snapshot;
      ActorFoundationRuleRef used_rule;
      bool changed = false;
      for (auto &actor : next.input_snapshot.actors) {
        const auto *current = body.state.character_condition_extensions
                                  .actor_condition_heads.find(actor.actor_id);
        if (!current || actor.capability_frames.empty())
          fail("GrowthPracticeActualBodyRequired", actor.actor_id);
        const auto *required =
            take(read_growth_fixed_required_skill(body, growth, next, actor));
        for (auto &frame : actor.capability_frames) {
          if (!same(*current, frame.condition_head_snapshot))
            fail("GrowthPracticeUnadoptedConditionChange", actor.actor_id);
          std::optional<StableId> purpose_skill;
          if (frame.purpose_ref.id == "study" ||
              frame.purpose_ref.id == "provide_teaching") {
            if (!required)
              fail("GrowthPracticeSkillMaskMismatch", actor.actor_id);
            purpose_skill = *required;
          }
          frame = take(freeze_character_capability(
              take(read_character_capability_at_head(
                  body, *current, frame.purpose_ref, purpose_skill,
                  body.actual_now))));
          if (actor.actor_id == award.actor_id)
            used_rule = frame.result.rule_ref;
        }
        for (auto &value : actor.skill_qualification_views) {
          value.source_identity = common.state().identity;
          value.observed_at = body.actual_now;
          if (actor.actor_id == award.actor_id)
            if (const auto *net = changed_skill(award, value.skill_id)) {
              if (value.minimum_xp != net->xp_before)
                fail("GrowthPracticeInputBeforeMismatch", actor.actor_id);
              value.minimum_xp = net->xp_after;
              value.minimum_level = growth_skill_level(net->xp_after);
              value.text_memory.grow(0);
              value.contribution_refs.emplace_back(
                  PracticeAwardSkillContributionRef{ref.award_id,
                                                    value.skill_id});
              changed = true;
            }
        }
      }
      if (!body.fixed_spatial)
        fail("GrowthFixedSpatialSourcesRequired", action_row.first);
      take(refresh_growth_fixed_spatial_projection(body, *body.fixed_spatial,
                                                   next));
      if (next.action_kind == "long_rest") {
        const auto *raw = growth.facts.find(next.action_ref);
        const auto *rest = raw ? std::get_if<RestFact>(raw) : nullptr;
        auto &quiet = next.input_snapshot.specific.growth_fact
                          .original_quiet_or_rest_basis;
        if (!rest || !quiet)
          fail("GrowthLongRestQuietSourceRequired", next.action_ref);
        take(refresh_growth_long_rest_quiet_projection(body, rest->actor_id,
                                                       *quiet));
      }
      if (!changed || used_rule.id.empty())
        fail("GrowthPracticeActualReadInputRequired", action_row.first);
      next.cause = LifeActualInputChange{LifeGrowthInputChange{ref},
                                         std::move(used_rule)};
      next.boundary_id = take(common.next_id("growth_fixed_service_boundary"));
      const auto id = next.boundary_id;
      for (auto &actor : next.input_snapshot.actors)
        for (auto &value : actor.skill_qualification_views)
          value.actual_boundary_ref = {
              {"growth", "growth_fixed_service_boundary", id}, 1};
      const auto &reg = registration(body, next.action_kind);
      if (!growth.fixed_service_boundaries.insert(id, std::move(next)))
        fail("DuplicateGrowthFixedBoundary", id);
      take(adopt_growth_fixed_boundary_in_candidate(
          common, growth, *body.growth_catalog, *body.shared, elapsed,
          *body.scale, body.catalog.identity, reg, id, body.actual_now, {},
          body.interaction, body.interaction_catalog));
    }
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Status
validate_growth_practice_input_coverage(const CharacterConditionSources &body,
                                        const GrowthState &growth) {
  try {
    if (body.growth != &growth)
      fail("GrowthPracticeSameCandidateRequired");
    if (!body.permanent_growth)
      return common_success();
    for (const auto &row : body.permanent_growth->practice_awards.rows()) {
      const auto &award = read(body, {row.first});
      for (const auto &initial : growth.fixed_service_boundaries.rows()) {
        const auto &start = initial.second;
        if (start.kind != LifeFixedBoundaryKind::started ||
            !reads(start, award))
          continue;
        const auto &end = last(growth, start.action_ref);
        const bool terminal = end.kind == LifeFixedBoundaryKind::completed ||
                              end.kind == LifeFixedBoundaryKind::stopped;
        const auto from = take(compare_world_moments(
                       award.applied_at.exact_moment, start.occurred_at)),
                   to = take(compare_world_moments(
                       award.applied_at.exact_moment,
                       terminal ? end.occurred_at : body.actual_now));
        if (from < 0 || to > 0)
          continue;
        const auto applied = position(body.common, award.original_fact_ref.id);
        if (from == 0 && applied < position(body.common, start.action_ref))
          continue;
        if (terminal && to == 0) {
          const GrowthTerminalPublication *published = nullptr;
          for (const auto &source : growth.terminal_publications.rows())
            if (source.second.action_id == start.action_ref) {
              if (published)
                fail("GrowthFixedTerminalPublicationAmbiguous",
                     start.action_ref);
              published = &source.second;
            }
          if (!published)
            fail("GrowthFixedTerminalPublicationRequired", start.action_ref);
          if (applied > position(body.common, published->publication_id))
            continue;
        }
        std::size_t count = 0;
        for (const auto &boundary : growth.fixed_service_boundaries.rows())
          if (boundary.second.action_ref == start.action_ref)
            if (const auto *ref = adoption(boundary.second);
                ref && ref->award_id == award.award_id)
              ++count;
        if (count != 1)
          fail("GrowthPracticeInputCoverageMismatch", award.award_id);
      }
    }
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
} // namespace hanmo::resident
