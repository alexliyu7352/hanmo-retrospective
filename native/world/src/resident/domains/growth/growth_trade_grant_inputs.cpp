// True trade grant adoption; values are captured by the original writer, not inferred.
#include "growth_trade_grant_inputs.hpp"
#include "growth_trade_grant_read.hpp"
#include "resident/kernel/source_ref.hpp"
#include "growth_fixed_spatial_inputs.hpp"
#include "growth_internal.hpp"
#include "growth_quiet_source.hpp"
#include "resident/domains/actor_foundation/character_capability.hpp"
#include <algorithm>
#include <charconv>
#include <string_view>
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
const GrowthTradeGrant *changed_skill(const GrowthTradeGrant &grant,
                                          const StableId &skill) {
  return grant.skill_id == skill && grant.actual_xp_gain > 0 ? &grant : nullptr;
}
bool reads(const LifeFixedServiceBoundary &boundary,
           const GrowthTradeGrant &grant) {
  for (const auto &actor : boundary.input_snapshot.actors)
    if (actor.actor_id == grant.actor_id)
      for (const auto &skill : actor.skill_qualification_views)
        if (changed_skill(grant, skill.skill_id))
          return true;
  return false;
}
const GrowthTradeGrantRef *adoption(const LifeFixedServiceBoundary &boundary) {
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
      return std::get_if<GrowthTradeGrantRef>(&growth->original_result_ref);
  return nullptr;
}
// Same original allocated-ID contract as growth_terminal_publication.cpp.
bool allocated_publication(const CommonState &common, const StableId &id) {
  constexpr std::string_view prefix = "growth_terminal:";
  if (id.size() <= prefix.size() || id.compare(0, prefix.size(), prefix) != 0)
    return false;
  const char *first = id.data() + prefix.size();
  if (*first < '1' || *first > '9') return false;
  Integer value = 0;
  const auto parsed = std::from_chars(first, id.data() + id.size(), value);
  return parsed.ec == std::errc{} && parsed.ptr == id.data() + id.size() &&
         value > 0 && value < common.next_instance_id;
}
std::size_t position(const CommonStore &common, const StableId &id) {
  const auto &order = common.state().facts.order();
  const auto found = std::find(order.begin(), order.end(), id);
  if (found == order.end())
    fail("GrowthTradeGrantOriginalCutRequired", id);
  return static_cast<std::size_t>(found - order.begin());
}
GrowthTradeGrantView read(const CharacterConditionSources &body,
                            const GrowthState &growth,
                            const GrowthTradeGrantRef &ref) {
  if (body.growth != &growth || !body.growth_catalog || !body.shared)
    fail("GrowthTradeGrantSourcesRequired", ref.action_id);
  return take(read_growth_trade_grant_v1(body.common, growth,
      *body.growth_catalog, *body.shared, ref, body.actual_now));
}
} // namespace
Outcome<const GrowthTerminalPublication *> read_growth_trade_grant_publication_v1(
    const CharacterConditionSources &body, const GrowthState &growth,
    const GrowthTradeGrantRef &ref) {
  try {
    const auto actual = read(body, growth, ref);
    const GrowthTerminalPublication *found = nullptr;
    for (const auto &row : growth.terminal_publications.rows()) {
      const auto &value = row.second;
      if (value.action_id != ref.action_id) continue;
      const auto *header = body.common.state().facts.find(row.first);
      if (found || !allocated_publication(body.common.state(), row.first) ||
          row.first != value.publication_id || !header ||
          header->id != row.first || header->domain != "growth" ||
          header->kind != "growth_result_terminal" ||
          value.original_result_ref.kind != LifeFixedOutcomeRef::Kind::life_activity ||
          value.original_result_ref.id != ref.action_id ||
          value.original_result_ref.period_ordinal ||
          !value.original_result_ref.secondary_ref.empty() ||
          value.terminal_source_schema != actual.grant->original_trade_source_schema ||
          !same_source_ref(value.terminal_source_ref, actual.grant->original_trade_result_ref) ||
          take(compare_world_moments(value.occurred_at, actual.terminal->ended_at)))
        fail("GrowthTradeGrantPublicationMismatch", ref.action_id);
      if (position(body.common, ref.action_id) >= position(body.common, row.first))
        fail("GrowthTerminalPublicationOrderMismatch", row.first);
      found = &value;
    }
    if (!found) fail("GrowthTradeGrantPublicationRequired", ref.action_id);
    return {true, found, {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Status adopt_growth_trade_grant_inputs_in_candidate(
    CommonStore &common, GrowthState &growth,
    const CharacterConditionSources &body, ActionElapsedStore &elapsed,
    const GrowthTradeGrantRef &ref) {
  try {
    if (&body.common != &common || body.growth != &growth ||
        body.elapsed != &elapsed || !body.growth_catalog ||
        !body.growth_catalog->complete_exact_period_sources || !body.shared ||
        !body.scale || !body.action_registrations)
      fail("GrowthTradeGrantInputSourcesRequired", ref.action_id);
    const auto source = read(body, growth, ref);
    const auto &grant = *source.grant;
    const auto &applied_at = source.terminal->ended_at;
    const auto *publication = take(read_growth_trade_grant_publication_v1(body, growth, ref));
    if (!grant.actual_xp_gain) return common_success();
    if (!grant.capability_application)
      fail("GrowthTradeGrantApplicationRequired", ref.action_id);
    for (const auto &action_row : growth.actions.rows()) {
      const auto &prior = last(growth, action_row.first);
      if (prior.kind == LifeFixedBoundaryKind::completed ||
          prior.kind == LifeFixedBoundaryKind::stopped)
        fail("GrowthTradeGrantTerminalActionStillActive", action_row.first);
      if (!reads(prior, grant))
        continue;
      bool already = false;
      for (const auto &row : growth.fixed_service_boundaries.rows())
        if (row.second.action_ref == action_row.first)
          if (const auto *used = adoption(row.second);
              used && used->action_id == ref.action_id) {
            if (already)
              fail("GrowthTradeGrantDuplicateInputAdoption", ref.action_id);
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
          applied_at, start->occurred_at));
      if (after_start < 0 ||
          (after_start == 0 && position(common, publication->publication_id) <
                                   position(common, start->action_ref)))
        continue;
      if (take(compare_world_moments(applied_at,
                                     body.actual_now)))
        fail("GrowthTradeGrantUnadoptedEarlierSource", ref.action_id);
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
          fail("GrowthTradeGrantActualBodyRequired", actor.actor_id);
        const auto *required =
            take(read_growth_fixed_required_skill(body, growth, next, actor));
        for (auto &frame : actor.capability_frames) {
          if (!same(*current, frame.condition_head_snapshot))
            fail("GrowthTradeGrantUnadoptedConditionChange", actor.actor_id);
          std::optional<StableId> purpose_skill;
          if (frame.purpose_ref.id == "study" ||
              frame.purpose_ref.id == "provide_teaching") {
            if (!required)
              fail("GrowthTradeGrantSkillMaskMismatch", actor.actor_id);
            purpose_skill = *required;
          }
          frame = take(freeze_character_capability(
              take(read_character_capability_at_head(
                  body, *current, frame.purpose_ref, purpose_skill,
                  body.actual_now))));
          if (actor.actor_id == grant.actor_id)
            used_rule = frame.result.rule_ref;
        }
        for (auto &value : actor.skill_qualification_views) {
          value.source_identity = common.state().identity;
          value.observed_at = body.actual_now;
          if (actor.actor_id == grant.actor_id)
            if (const auto *net = changed_skill(grant, value.skill_id)) {
              if (value.minimum_xp != net->xp_before)
                fail("GrowthTradeGrantInputBeforeMismatch", actor.actor_id);
              value.minimum_xp = net->xp_after;
              value.minimum_level = growth_skill_level(net->xp_after);
              value.text_memory.grow(0);
              value.contribution_refs.emplace_back(
                  VerifiedLegacyTradeGrantRef{ref.action_id});
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
        fail("GrowthTradeGrantActualReadInputRequired", action_row.first);
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
validate_growth_trade_grant_input_coverage(const CharacterConditionSources &body,
                                        const GrowthState &growth) {
  try {
    if (body.growth != &growth)
      fail("GrowthTradeGrantSameCandidateRequired");
    for (const auto &row : growth.trade_grants.rows()) {
      const auto source = read(body, growth, {row.first});
      const auto &grant = *source.grant;
      const auto &applied_at = source.terminal->ended_at;
      const auto *publication = take(read_growth_trade_grant_publication_v1(body, growth, {row.first}));
      for (const auto &initial : growth.fixed_service_boundaries.rows()) {
        const auto &start = initial.second;
        if (start.kind != LifeFixedBoundaryKind::started ||
            !reads(start, grant))
          continue;
        const auto &end = last(growth, start.action_ref);
        const bool terminal = end.kind == LifeFixedBoundaryKind::completed ||
                              end.kind == LifeFixedBoundaryKind::stopped;
        const auto from = take(compare_world_moments(
                       applied_at, start.occurred_at)),
                   to = take(compare_world_moments(
                       applied_at,
                       terminal ? end.occurred_at : body.actual_now));
        if (from < 0 || to > 0)
          continue;
        const auto applied = position(body.common, publication->publication_id);
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
                ref && ref->action_id == grant.action_id)
              ++count;
        if (count != 1)
          fail("GrowthTradeGrantInputCoverageMismatch", grant.action_id);
      }
    }
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
} // namespace hanmo::resident
