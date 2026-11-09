#include "resident/domains/growth/growth_model.hpp"
#include "growth_quiet_source.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/domains/actor_foundation/authored_origin.hpp"
#include "resident/domains/actor_foundation/character_condition_read.hpp"
#include "resident/domains/estate/estate_rest_history.hpp"
#include "resident/domains/estate/estate_title_history.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {},
                       ErrorKind kind = ErrorKind::internal) {
  throw Failure{common_failure(code, id, kind).error};
}
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
bool same(const SourceRef &a, const SourceRef &b) {
  return a.domain == b.domain && a.kind == b.kind && a.id == b.id;
}
GrowthQuietView join(ActorSocialOccupancyView social,
                     MilitaryActorActiveArmyRead army) {
  if (social.original_sources.size() != 1)
    fail("GrowthQuietSocialInitialHeadRequired", army.actor_id);
  GrowthQuietView value;
  value.quiet = !social.value.interaction_id && !social.value.party_id &&
                army.active_army_ids.empty();
  value.original_sources = std::move(social.original_sources);
  for (auto &actual : army.original_sources) {
    for (const auto &prior : value.original_sources)
      if (same(prior, actual.source_ref))
        fail("GrowthQuietDuplicateSource", actual.source_ref.id);
    value.original_sources.push_back(std::move(actual.source_ref));
  }
  return value;
}
const OriginalLongRestConditionRule &
selected(const CharacterConditionSources &body) {
  if (!body.growth_catalog ||
      !body.growth_catalog->complete_exact_command_cuts || !body.fixed_quiet ||
      !body.catalog.original_long_rest_rule)
    fail("GrowthLongRestQuietSourcesRequired", {},
         ErrorKind::unsupported_capability);
  if (!body.fixed_quiet->bound_to(body.common))
    fail("GrowthQuietSameCandidateRequired");
  const auto &rule = *body.catalog.original_long_rest_rule;
  if (rule.rule_ref.id != "original_estate_long_rest" ||
      rule.rule_ref.version != 1 ||
      rule.rule_ref.catalog_digest != body.catalog.identity.content_digest)
    fail("OriginalLongRestConditionRuleRequired");
  return rule;
}
struct OriginalQuietSources {
  CheckpointMemoryCharge memory;
  std::vector<SourceRef> values;
};
OriginalQuietSources retained(const LifeQuietServiceInputs &input) {

  if (input.actual_conflict_source_refs.size() > SIZE_MAX / sizeof(SourceRef))
    throw std::bad_alloc();

  for (const auto &row : input.actual_conflict_source_refs) {
    const auto *source = std::get_if<SourceRef>(&row);
    if (!source)
      fail("GrowthQuietOriginalSourceRequired");


  }
  OriginalQuietSources out{CheckpointMemoryCharge(0), {}};
  out.values.reserve(input.actual_conflict_source_refs.size());
  for (const auto &row : input.actual_conflict_source_refs)
    out.values.push_back(std::get<SourceRef>(row));
  return out;
}
void match(const LifeQuietServiceInputs &input, const GrowthQuietView &actual) {
  if (input.is_quiet != actual.quiet ||
      input.actual_conflict_source_refs.size() !=
          actual.original_sources.size())
    fail("GrowthQuietFrozenSourceMismatch", input.place_ref);
  for (std::size_t n = 0; n < actual.original_sources.size(); ++n) {
    const auto *ref =
        std::get_if<SourceRef>(&input.actual_conflict_source_refs[n]);
    if (!ref || !same(*ref, actual.original_sources[n]))
      fail("GrowthQuietFrozenSourceMismatch", input.place_ref);
  }
}
} // namespace
Outcome<GrowthQuietView>
NativeGrowthQuietRead::read_actual(const StableId &actor,
                                   const WorldMoment &at) const {
  try {
    if (!bound_to(common_))
      fail("GrowthQuietSameCandidateRequired", actor);
    const auto now = take(clock_.read_current_moment());
    if (take(compare_world_moments(at, now)))
      fail("GrowthQuietActualClockRequired", actor);
    return {true,
            join(take(social_.read_actual_actor_social_occupancy(actor, at)),
                 take(selected_ ? read_actual_actor_active_armies_selected(
                     common_, military_, *selected_, naval_, actor, now, {},
                     formations_, returns_) : read_actual_actor_active_armies(
                     common_, military_, *exact_, naval_, actor, now, {},
                     formations_, returns_))),
            {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Outcome<GrowthQuietView> NativeGrowthQuietRead::read_at_source_head(
    const StableId &actor, const WorldMoment &at,
    const std::vector<SourceRef> &frozen, const WorldMoment &actual_now) const {
  try {
    if (!bound_to(common_))
      fail("GrowthQuietSameCandidateRequired", actor);
    const auto now = take(clock_.read_current_moment());
    if (take(compare_world_moments(now, actual_now)) ||
        take(compare_world_moments(at, now)) > 0)
      fail("GrowthQuietActualClockRequired", actor);

    if (frozen.size() > SIZE_MAX / sizeof(SourceRef))
      throw std::bad_alloc();


    auto memory = CheckpointMemoryCharge(0);
    std::vector<SourceRef> army;
    army.reserve(frozen.size());
    std::size_t social_heads = 0;
    for (std::size_t n = 0; n < frozen.size(); ++n) {
      const auto &source = frozen[n];
      for (std::size_t p = 0; p < n; ++p)
        if (same(source, frozen[p]))
          fail("GrowthQuietDuplicateSource", source.id);
      if (source.domain == "interaction") {
        take(actor_social_occupancy_source_schema(source));
        ++social_heads;
      } else {
        take(selected_ ? read_selected_military_active_army_source(
            common_, military_, *selected_, naval_, source, now, {},
            formations_, returns_) : read_military_active_army_source(
            common_, military_, *exact_, naval_, source, now, {},
            formations_, returns_));
        army.push_back(source);
      }
    }
    if (social_heads != 1)
      fail("GrowthQuietSocialInitialHeadRequired", actor);
    return {true,
            join(take(social_.read_actor_social_occupancy_at_source_head(
                     actor, at, frozen, now)),
                 take(selected_ ? read_actor_active_armies_at_selected_source_head(
                     common_, military_, *selected_, naval_, actor, at, army, now,
                     {}, formations_, returns_) : read_actor_active_armies_at_source_head(
                     common_, military_, *exact_, naval_, actor, at, army, now,
                     {}, formations_, returns_))),
            {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Outcome<std::optional<GrowthQuietHeadChange>>
NativeGrowthQuietRead::read_next_change(const StableId &actor,
                                        const WorldMoment &from,
                                        const std::vector<SourceRef> &from_head,
                                        const WorldMoment &to,
                                        const std::vector<SourceRef> &to_head,
                                        const WorldMoment &now) const {
  try {
    if (!bound_to(common_) || take(compare_world_moments(from, to)) > 0)
      fail("GrowthQuietActualWindowRequired", actor);
    auto before = take(read_at_source_head(actor, from, from_head, now));
    auto end = take(read_at_source_head(actor, to, to_head, now));
    std::vector<SourceRef> from_army, to_army;
    const SourceRef *from_social = nullptr;
    const SourceRef *to_social = nullptr;
    for (const auto &source : from_head)
      if (source.domain == "interaction")
        from_social = &source;
      else
        from_army.push_back(source);
    for (const auto &source : to_head)
      if (source.domain == "interaction")
        to_social = &source;
      else
        to_army.push_back(source);
    if (!from_social || !to_social)
      fail("GrowthQuietSocialInitialHeadRequired", actor);
    struct First final : ActorSocialOccupancyChangeVisitor {
      const ActorSocialOccupancyTransition *value = nullptr;
      Status changed(const ActorSocialOccupancyTransition &row) override {
        if (!value)
          value = &row;
        return common_success();
      }
    } social_change;
    take(social_.visit_actor_social_occupancy_changes(
        actor, from, *from_social, to, *to_social, now, social_change));
    auto army_changes = take(selected_ ? read_actor_active_army_head_changes_selected(
        common_, military_, *selected_, naval_, actor, from, from_army, to, to_army,
        now, {}, formations_, returns_) : read_actor_active_army_head_changes(
        common_, military_, *exact_, naval_, actor, from, from_army, to, to_army,
        now, {}, formations_, returns_));
    if (!social_change.value && army_changes.empty())
      return {true, std::nullopt, {}};
    const auto *army = army_changes.empty() ? nullptr : &army_changes.front();
    bool social = bool(social_change.value);
    if (social && army) {
      const auto order = take(compare_world_moments(
          social_change.value->occurred_at, army->occurred_at));
      if (order > 0)
        social = false;
      else if (!order) {
        const auto sequence =
            take(social_.read_actor_social_occupancy_source_order(
                {"interaction", "actor_social_occupancy_changed",
                 social_change.value->id},
                1));
        if (sequence == army->actual_source.original_serial)
          fail("GrowthQuietActualSourceOrderCollision", actor);
        social = sequence < army->actual_source.original_serial;
      }
    }
    GrowthQuietHeadChange change;
    change.before = std::move(before);
    change.after.original_sources = change.before.original_sources;
    if (social) {
      const auto &actual = *social_change.value;
      change.actual_source = {"interaction", "actor_social_occupancy_changed",
                              actual.id};
      change.source_schema = 1;
      change.occurred_at = actual.occurred_at;
      change.after.original_sources.front() = change.actual_source;
      auto other = take(selected_ ? read_actor_active_armies_at_selected_source_head(
          common_, military_, *selected_, naval_, actor, change.occurred_at,
          from_army, now, {}, formations_, returns_) : read_actor_active_armies_at_source_head(
          common_, military_, *exact_, naval_, actor, change.occurred_at,
          from_army, now, {}, formations_, returns_));
      change.after.quiet = !actual.after.interaction_id &&
                           !actual.after.party_id &&
                           other.active_army_ids.empty();
    } else {
      change.actual_source = army->actual_source.source_ref;
      change.source_schema = army->actual_source.source_schema;
      change.occurred_at = army->occurred_at;
      change.after.original_sources.push_back(change.actual_source);
      auto other = take(social_.read_actor_social_occupancy_at_source_head(
          actor, change.occurred_at, from_head, now));
      change.after.quiet = !other.value.interaction_id &&
                           !other.value.party_id &&
                           army->after_active_army_ids.empty();
    }
    // The composed successor is re-read through both actual owners at this cut.
    auto actual = take(read_at_source_head(actor, change.occurred_at,
                                           change.after.original_sources, now));
    if (actual.quiet != change.after.quiet ||
        actual.original_sources.size() != change.after.original_sources.size())
      fail("GrowthQuietActualSuccessorMismatch", actor);
    for (std::size_t i = 0; i < actual.original_sources.size(); ++i)
      if (!same(actual.original_sources[i], change.after.original_sources[i]))
        fail("GrowthQuietActualSuccessorMismatch", actor);
    return {true, std::move(change), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Outcome<std::optional<GrowthQuietHeadChange>>
read_growth_long_rest_next_quiet_change(
    const CharacterConditionSources &body, const StableId &actor,
    const LifeQuietServiceInputs &from,
    const LifeQuietServiceInputs *terminal) {
  try {
    selected(body);
    take(validate_growth_long_rest_quiet(body, actor, from));
    auto retained_from = retained(from);
    GrowthQuietView target;
    WorldMoment until = body.actual_now;
    if (terminal) {
      take(validate_growth_long_rest_quiet(body, actor, *terminal));
      auto source = retained(*terminal);
      target = take(body.fixed_quiet->read_at_source_head(
          actor, terminal->observed_at, source.values, body.actual_now));
      until = terminal->observed_at;
    } else
      target = take(body.fixed_quiet->read_actual(actor, body.actual_now));
    return body.fixed_quiet->read_next_change(
        actor, from.observed_at, retained_from.values, until,
        target.original_sources, body.actual_now);
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Outcome<LifeQuietServiceInputs>
capture_growth_long_rest_quiet(const CharacterConditionSources &body,
                               const StableId &actor, const StableId &place) {
  try {
    const auto &rule = selected(body);
    if (place.empty() || !body.shared || !body.shared->places.find(place))
      fail("LongRestOriginalPlaceRequired", actor);
    auto actual = take(body.fixed_quiet->read_actual(actor, body.actual_now));
    LifeQuietServiceInputs result;
    result.place_ref = place;
    result.quiet_rule_ref = rule.rule_ref;
    result.observed_at = body.actual_now;
    result.is_quiet = actual.quiet;
    for (auto &source : actual.original_sources)
      result.actual_conflict_source_refs.emplace_back(std::move(source));
    return {true, std::move(result), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Status
refresh_growth_long_rest_quiet_projection(const CharacterConditionSources &body,
                                          const StableId &actor,
                                          LifeQuietServiceInputs &input) {
  try {
    selected(body);
    auto sources = retained(input);
    auto actual = take(body.fixed_quiet->read_at_source_head(
        actor, body.actual_now, sources.values, body.actual_now));
    match(input, actual);
    input.observed_at = body.actual_now;
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Status validate_growth_long_rest_quiet(const CharacterConditionSources &body,
                                       const StableId &actor,
                                       const LifeQuietServiceInputs &input) {
  try {
    const auto &rule = selected(body);
    if (input.place_ref.empty() || !body.shared ||
        !body.shared->places.find(input.place_ref) ||
        !actor_foundation_rule_equal(input.quiet_rule_ref, rule.rule_ref) ||
        take(compare_world_moments(input.observed_at, body.actual_now)) > 0)
      fail("GrowthQuietOriginalRuleMismatch", actor);
    auto sources = retained(input);
    match(input,
          take(body.fixed_quiet->read_at_source_head(
              actor, input.observed_at, sources.values, body.actual_now)));
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Status validate_growth_long_rest_quiet_span(
    const CharacterConditionSources &body, const StableId &actor,
    const LifeQuietServiceInputs &input, const MomentSpan &span) {
  try {
    take(validate_growth_long_rest_quiet(body, actor, input));
    take(make_moment_span(span.from, span.to));
    if (!input.is_quiet ||
        take(compare_world_moments(input.observed_at, span.from)) > 0 ||
        take(compare_world_moments(span.from, span.to)) >= 0)
      fail("GrowthQuietActualServiceWindowRequired", actor);
    auto sources = retained(input);
    // An omitted strict interior transition is rejected by each original owner.
    // At to, the frozen same-M cut excludes later changes, preserving
    // [from,to).
    auto end = take(body.fixed_quiet->read_at_source_head(
        actor, span.to, sources.values, body.actual_now));
    if (!end.quiet)
      fail("GrowthQuietNotProvidedThroughoutService", actor);
    match(input, end);
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
bool growth_quiet_input_source_shape(const SourceRef &source,
                                     Integer schema) noexcept {
  if (source.id.empty())
    return false;
  if (source.domain == "interaction")
    return source.kind == "actor_social_occupancy_changed" && schema == 1;
  if (source.domain == "military")
    return ((source.kind == "military_action" ||
             source.kind == "military_integration" ||
             source.kind == "military_return") &&
            schema == 1) ||
           (source.kind == "military_casualties" &&
            (schema == 1 || schema == 2)) ||
           (source.kind == "military_action_completion_applied" && schema == 2);
  return source.domain == "military_naval" && source.kind == "naval_capture" &&
         schema == 1;
}
namespace {
const LifeQuietServiceInputs &
quiet_inputs(const LifeFixedServiceBoundary &boundary) {
  const auto &quiet =
      boundary.input_snapshot.specific.growth_fact.original_quiet_or_rest_basis;
  if (boundary.action_kind != "long_rest" || !quiet)
    fail("GrowthLongRestQuietSourceRequired", boundary.action_ref);
  return *quiet;
}
const LifeOccupationInputChange *
quiet_cause(const LifeFixedServiceBoundary &boundary) {
  const LifeFixedInputChange *source = nullptr;
  if (const auto *change = std::get_if<LifeActualInputChange>(&boundary.cause))
    source = &change->change;
  else if (const auto *terminal =
               std::get_if<LifeOriginalTerminalApplication>(&boundary.cause))
    if (const auto *stop =
            std::get_if<LifeActualStopTrigger>(&terminal->trigger))
      source = std::get_if<LifeFixedInputChange>(&stop->source);
  const auto *occupation =
      source ? std::get_if<LifeOccupationInputChange>(source) : nullptr;
  return occupation &&
                 growth_quiet_input_source_shape(
                     occupation->original_source_ref, occupation->source_schema)
             ? occupation
             : nullptr;
}
const RestFact &rest_fact(const GrowthState &growth,
                          const LifeFixedServiceBoundary &boundary) {
  const auto *raw = growth.facts.find(boundary.action_ref);
  const auto *rest = raw ? std::get_if<RestFact>(raw) : nullptr;
  if (!rest)
    fail("GrowthLongRestOriginalResultRequired", boundary.action_ref);
  return *rest;
}
void validate_site(const CharacterConditionSources &body, const RestFact &rest,
                   const LifeQuietServiceInputs &quiet,
                   const StableId &action) {
  if (!body.rest_estates || !body.rest_estate_state)
    fail("GrowthLongRestEstateTitleReaderRequired", action,
         ErrorKind::unsupported_capability);
  const auto *estate = body.rest_estates->estates.estates.find(rest.estate_id);
  if (!estate || quiet.place_ref != estate->place_id ||
      rest.food_item_id != estate->food_item_id ||
      rest.ration_per_person != estate->ration_per_person)
    fail("GrowthLongRestOriginalEstateMismatch", action);
  NativeEstateAssetTitleRead titles(*body.rest_estates,
                                    *body.rest_estate_state);
  auto title = take(titles.read_estate_title_before_source(
      rest.estate_id, {"services", "long_rest", action}));
  if (title.legal_owner.kind != Owner::Kind::actor ||
      title.legal_owner.id != rest.actor_id ||
      quiet.site_access_source_refs.size() !=
          title.original_title_applications.size() + 1)
    fail("GrowthLongRestOriginalTitleMismatch", action);
  const auto *initial =
      std::get_if<ScenarioOriginRef>(&quiet.site_access_source_refs.front());
  if (!initial ||
      !actor_foundation_origin_equal(*initial, title.initial_origin))
    fail("GrowthLongRestOriginalTitleMismatch", action);
  for (std::size_t n = 0; n < title.original_title_applications.size(); ++n) {
    const auto *source =
        std::get_if<SourceRef>(&quiet.site_access_source_refs[n + 1]);
    if (!source || !same(*source, title.original_title_applications[n]))
      fail("GrowthLongRestOriginalTitleMismatch", action);
  }
}

} // namespace
Status validate_growth_long_rest_quiet_input_change(
    const CharacterConditionSources &body, const GrowthState &growth,
    const LifeFixedServiceBoundary &boundary,
    const LifeOccupationInputChange &source) {
  try {
    const auto &rule = selected(body);
    const auto &rest = rest_fact(growth, boundary);
    const auto *prior = boundary.previous_boundary_ref
                            ? growth.fixed_service_boundaries.find(
                                  *boundary.previous_boundary_ref)
                            : nullptr;
    if (!prior || prior->action_ref != boundary.action_ref ||
        prior->boundary_ordinal + 1 != boundary.boundary_ordinal ||
        !growth_quiet_input_source_shape(source.original_source_ref,
                                         source.source_schema))
      fail("GrowthQuietInputActualPreviousRequired", boundary.boundary_id);
    auto change = take(read_growth_long_rest_next_quiet_change(
        body, rest.actor_id, quiet_inputs(*prior), &quiet_inputs(boundary)));
    if (!change || !same(change->actual_source, source.original_source_ref) ||
        change->source_schema != source.source_schema ||
        take(compare_world_moments(change->occurred_at, boundary.occurred_at)))
      fail("GrowthQuietInputOriginalSourceMismatch", boundary.boundary_id);
    match(quiet_inputs(*prior), change->before);
    match(quiet_inputs(boundary), change->after);
    const ActorFoundationRuleRef *applied = nullptr;
    if (const auto *actual =
            std::get_if<LifeActualInputChange>(&boundary.cause))
      applied = &actual->transition_rule_ref;
    else if (const auto *terminal =
                 std::get_if<LifeOriginalTerminalApplication>(&boundary.cause))
      if (const auto *stop =
              std::get_if<LifeActualStopTrigger>(&terminal->trigger))
        applied = &stop->original_stop_rule_ref;
    if (!applied || !actor_foundation_rule_equal(*applied, rule.rule_ref) ||
        (change->after.quiet
             ? boundary.kind != LifeFixedBoundaryKind::input_rebound
             : boundary.kind != LifeFixedBoundaryKind::stopped))
      fail("GrowthQuietOriginalDispositionMismatch", boundary.boundary_id);
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Status validate_growth_long_rest_quiet_input_coverage(
    const CharacterConditionSources &body, const GrowthState &growth) {
  try {
    for (const auto &row : growth.fixed_service_boundaries.rows()) {
      const auto &boundary = row.second;
      if (boundary.action_kind != "long_rest")
        continue;
      const auto &rest = rest_fact(growth, boundary);
      const auto &quiet = quiet_inputs(boundary);
      take(validate_growth_long_rest_quiet(body, rest.actor_id, quiet));
      validate_site(body, rest, quiet, boundary.action_ref);
      if (boundary.previous_boundary_ref) {
        const auto *previous = growth.fixed_service_boundaries.find(
            *boundary.previous_boundary_ref);
        if (!previous)
          fail("GrowthQuietInputActualPreviousRequired", boundary.boundary_id);
        if (!quiet_cause(boundary)) {
          auto from = retained(quiet_inputs(*previous)), to = retained(quiet);
          if (from.values.size() != to.values.size())
            fail("GrowthQuietUnexplainedSourceChange", boundary.boundary_id);
          for (std::size_t i = 0; i < from.values.size(); ++i)
            if (!same(from.values[i], to.values[i]))
              fail("GrowthQuietUnexplainedSourceChange", boundary.boundary_id);
        }
      }
      if (boundary.kind != LifeFixedBoundaryKind::started)
        continue;
      const LifeFixedServiceBoundary *last = &boundary;
      for (const auto &candidate : growth.fixed_service_boundaries.rows())
        if (candidate.second.action_ref == boundary.action_ref &&
            candidate.second.boundary_ordinal > last->boundary_ordinal)
          last = &candidate.second;
      const bool terminal = last->kind == LifeFixedBoundaryKind::completed ||
                            last->kind == LifeFixedBoundaryKind::stopped;
      const LifeQuietServiceInputs *cursor = &quiet;
      Integer ordinal = 0;
      for (;;) {
        auto change = take(read_growth_long_rest_next_quiet_change(
            body, rest.actor_id, *cursor,
            terminal ? &quiet_inputs(*last) : nullptr));
        if (!change)
          break;
        const LifeFixedServiceBoundary *adopted = nullptr;
        for (const auto &candidate : growth.fixed_service_boundaries.rows())
          if (candidate.second.action_ref == boundary.action_ref)
            if (const auto *cause = quiet_cause(candidate.second);
                cause &&
                same(cause->original_source_ref, change->actual_source) &&
                cause->source_schema == change->source_schema) {
              if (adopted)
                fail("GrowthQuietInputAdoptionRepeated", candidate.first);
              adopted = &candidate.second;
            }
        if (!adopted || adopted->boundary_ordinal <= ordinal ||
            take(compare_world_moments(adopted->occurred_at,
                                       change->occurred_at)))
          fail("GrowthQuietInputCoverageMissing", change->actual_source.id);
        take(validate_growth_long_rest_quiet_input_change(
            body, growth, *adopted, *quiet_cause(*adopted)));
        ordinal = adopted->boundary_ordinal;
        cursor = &quiet_inputs(*adopted);
      }
    }
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}

} // namespace hanmo::resident
