#include "population_spatial_history.hpp"
#include "../../storage/common_store.hpp"
#include "movement_exact_detail.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/read_stamp.hpp"
#include <algorithm>
namespace hanmo::resident {
using namespace movement_detail;
namespace {
bool overlaps(const MomentSpan &a, const MomentSpan &c,
              ExactArithmeticBudget b) {
  return take(compare_world_moments(a.from, c.to, b)) < 0 &&
         take(compare_world_moments(c.from, a.to, b)) < 0;
}
void head(const CommonStore &common, const SourceRef &ref) {
  const auto *found = common.state().facts.find(ref.id);
  if (!found || found->domain != ref.domain || found->kind != ref.kind)
    fail("PopulationLocationOriginalHeadRequired", ref.id);
}
} // namespace
Outcome<PopulationStationaryLocationSpan>
read_population_stationary_location_span(
    const CommonStore &common, const SpatialCarrierState &carriers,
    const MovementExactState &movement,
    const TransportRegistrationState &registrations,
    const PopulationTransportState &population,
    const CivilianPopulationRef &population_ref, const SourceRef &ref,
    const MomentSpan &span, ExactArithmeticBudget b) {
  return guard<PopulationStationaryLocationSpan>([&] {
    take(make_moment_span(span.from, span.to, b));
    if (ref.domain != "travel")
      fail("PopulationLocationOriginalOwnerRequired", ref.id);
    const PopulationTransportBinding *binding = nullptr;
    const TransportUnitReleaseSource *release = nullptr;
    if (ref.kind == "population_transport_position") {
      binding = population.transport_bindings.find(ref.id);
      if (!binding)
        fail("PopulationTransportLocationBindingRequired", ref.id);
      for (const auto &row : registrations.releases.rows())
        if (std::find(row.second.population_binding_ids.begin(),
                      row.second.population_binding_ids.end(),
                      binding->binding_id) !=
            row.second.population_binding_ids.end()) {
          if (release)
            fail("DuplicatePopulationTransportRelease", ref.id);
          release = &row.second;
        }
    } else if (ref.kind == "transport_release") {
      release = registrations.releases.find(ref.id);
      if (!release)
        fail("PopulationTransportReleaseRequired", ref.id);
      for (const auto &id : release->population_binding_ids) {
        const auto *candidate = population.transport_bindings.find(id);
        if (candidate &&
            candidate->transport_cohort_id == population_ref.cohort_id) {
          if (binding &&
              binding->transport_unit_id != candidate->transport_unit_id)
            fail("PopulationReleaseCarrierMismatch", ref.id);
          binding = candidate;
        }
      }
    } else
      fail("UnsupportedPopulationLocationSource", ref.kind);
    if (!binding || binding->transport_cohort_id != population_ref.cohort_id ||
        (population_ref.transport_binding_id &&
         *population_ref.transport_binding_id != binding->binding_id))
      fail("PopulationTransportLocationBindingRequired", ref.id);
    const auto *registration =
        binding->registration_source_ref.domain == "travel" &&
                binding->registration_source_ref.kind ==
                    "transport_registration"
            ? registrations.registrations.find(
                  binding->registration_source_ref.id)
            : nullptr;
    if (!registration ||
        registration->transport_unit_id != binding->transport_unit_id)
      fail("PopulationLocationRegistrationRequired", ref.id);
    head(common, binding->registration_source_ref);
    PopulationStationaryLocationSpan out{
        ref,
        span,
        registration->departure_anchor,
        {{binding->registration_source_ref, 1}}};
    if (ref.kind == "transport_release") {
      head(common, ref);
      if (take(compare_world_moments(span.from, release->occurred_at, b)) < 0)
        fail("PopulationLocationBeforeRelease", ref.id);
      out.actual_anchor = release->actual_anchor;
      out.original_sources = {{ref, 1}};
      // 后续重新materialize同cohort改变唯一location源，旧release不能覆盖它。
      for (const auto &row : population.transport_bindings.rows())
        if (row.second.binding_id != binding->binding_id &&
            row.second.transport_cohort_id == binding->transport_cohort_id) {
          const auto *later = registrations.registrations.find(
              row.second.registration_source_ref.id);
          if (later &&
              take(compare_world_moments(later->occurred_at,
                                         release->occurred_at, b)) > 0 &&
              take(compare_world_moments(later->occurred_at, span.to, b)) < 0)
            fail("PopulationLocationSourceSuperseded", ref.id);
        }
      return out;
    }
    if (take(compare_world_moments(span.from, registration->occurred_at, b)) <
            0 ||
        (release &&
         take(compare_world_moments(span.to, release->occurred_at, b)) > 0))
      fail("PopulationLocationOutsideBindingLifetime", ref.id);
    WorldMoment latest = registration->occurred_at;
    Integer priority = 0;
    auto adopt = [&](const WorldMoment &at, Integer rank,
                     const MovementAnchor &anchor, const SourceRef &source) {
      if (take(compare_world_moments(at, span.from, b)) > 0)
        return;
      const auto order = take(compare_world_moments(at, latest, b));
      if (order > 0 || (order == 0 && rank > priority)) {
        latest = at;
        priority = rank;
        out.actual_anchor = anchor;
        out.original_sources = {{binding->registration_source_ref, 1},
                                {source, 1}};
      }
    };
    for (const auto &row : movement.units.rows()) {
      const auto &unit = row.second;
      const auto *execution =
          std::get_if<GroupMovementExecution>(&unit.execution);
      const auto *carrier =
          execution ? std::get_if<TransportUnitCarrierRef>(&execution->carrier)
                    : nullptr;
      if (!carrier || carrier->transport_unit_ref.transport_unit_id !=
                          binding->transport_unit_id)
        continue;
      const auto *journey = carriers.journeys.find(execution->journey_id);
      if (!journey ||
          journey->composition_revision != carrier->composition_revision)
        fail("PopulationLocationJourneyRequired", row.first);
      for (const auto &source : unit.effort_basis_refs) {
        const auto *basis = movement.exertion_bases.find(source.id);
        if (!basis || basis->unit_ref != unit.id)
          fail("PopulationLocationMotionBasisRequired", source.id);
        if (take(compare_world_moments(basis->opened_at,
                                       registration->occurred_at, b)) < 0)
          continue;
        MovementPrefixState point;
        point.basis = basis->actual_basis;
        point.cursor = basis->opened_cursor;
        point.lifecycle = MovementActive{};
        adopt(basis->opened_at, 3, take(read_movement_position(point, b)),
              source);
        for (const auto &work : unit.activity_spans)
          if (work.basis_ref == basis_id(basis->actual_basis)) {
            if (overlaps(work.span, span, b))
              fail("PopulationLocationNotStationary", binding->binding_id);
            point.cursor = work.cursor_to;
            adopt(work.span.to, 2, take(read_movement_position(point, b)),
                  source);
          }
      }
      if (const auto *close = movement.unit_closes.find(unit.id + ":close"))
        adopt(close->occurred_at, 4, close->actual_anchor,
              {"travel", "movement_unit_close", close->id});
    }
    // 当前Battle委托需要原S2历史handoff；不能借旧World最后锚伪装仍可值守。
    const auto *current = movement.spatial_bindings.find(
        "transport_unit:" + binding->transport_unit_id);
    if (current &&
        std::holds_alternative<BattleMovementWriter>(current->writer))
      fail("PopulationBattleLocationHistoryRequired", binding->binding_id);
    return out;
  });
}
namespace {
// 两个公开入口只改变实际 cut 的取得方式；原运输/移动/释放演算只此一份。
struct PopulationLocationResolution {
  MovementAnchor actual_anchor;
  std::vector<MomentOwnerSource> original_sources;
};
Outcome<PopulationLocationResolution> resolve_population_location(
    const CommonStore &common, const SpatialCarrierState &carriers,
    const MovementExactState &movement,
    const TransportRegistrationState &registrations,
    const PopulationTransportState &population,
    const CivilianPopulationRef &population_ref, const SourceRef &ref,
    const WorldMoment &at, std::size_t cut, ExactArithmeticBudget b) {
  return guard<PopulationLocationResolution>([&] {
    const auto &order = common.state().facts.order();
    auto ordinal = [&](const SourceRef &source) {
      head(common, source);
      const auto found = std::find(order.begin(), order.end(), source.id);
      if (found == order.end())
        fail("PopulationLocationCommonOrderRequired", source.id);
      return std::size_t(found - order.begin());
    };
    auto visible = [&](const WorldMoment &occurred, const SourceRef &source) {
      const auto serial = ordinal(source);
      const auto cmp = take(compare_world_moments(occurred, at, b));
      if (cmp < 0 && serial >= cut)
        fail("PopulationLocationCausalTimeMismatch", source.id);
      return cmp < 0 || (cmp == 0 && serial < cut);
    };
    if (ref.domain != "travel")
      fail("PopulationLocationOriginalOwnerRequired", ref.id);
    const PopulationTransportBinding *binding = nullptr;
    const TransportUnitReleaseSource *release = nullptr;
    if (ref.kind == "population_transport_position")
      binding = population.transport_bindings.find(ref.id);
    else if (ref.kind == "transport_release") {
      release = registrations.releases.find(ref.id);
      if (!release)
        fail("PopulationTransportReleaseRequired", ref.id);
      for (const auto &id : release->population_binding_ids) {
        const auto *candidate = population.transport_bindings.find(id);
        if (candidate &&
            candidate->transport_cohort_id == population_ref.cohort_id) {
          if (binding)
            fail("PopulationReleaseBindingAmbiguous", ref.id);
          binding = candidate;
        }
      }
    } else
      fail("UnsupportedPopulationLocationSource", ref.kind);
    if (!binding || binding->transport_cohort_id != population_ref.cohort_id ||
        (population_ref.transport_binding_id &&
         *population_ref.transport_binding_id != binding->binding_id))
      fail("PopulationTransportLocationBindingRequired", ref.id);
    const auto &registration_ref = binding->registration_source_ref;
    const auto *registration =
        registration_ref.domain == "travel" &&
                registration_ref.kind == "transport_registration"
            ? registrations.registrations.find(registration_ref.id)
            : nullptr;
    if (!registration ||
        registration->transport_unit_id != binding->transport_unit_id ||
        !visible(registration->occurred_at, registration_ref))
      fail("PopulationLocationBeforeRegistration", ref.id);
    if (!release)
      for (const auto &row : registrations.releases.rows())
        if (std::find(row.second.population_binding_ids.begin(),
                      row.second.population_binding_ids.end(),
                      binding->binding_id) !=
            row.second.population_binding_ids.end()) {
          if (release)
            fail("DuplicatePopulationTransportRelease", ref.id);
          release = &row.second;
        }
    const SourceRef release_ref =
        release ? SourceRef{"travel", "transport_release", release->id}
                : SourceRef{};
    const bool released = release && visible(release->occurred_at, release_ref);
    if ((ref.kind == "transport_release") != released)
      fail("PopulationLocationBindingLifetimeMismatch", ref.id);
    PopulationLocationResolution out{registration->departure_anchor,
                                     {{registration_ref, 1}}};
    const auto registration_serial = ordinal(registration_ref);
    for (const auto &row : population.transport_bindings.rows())
      if (row.second.binding_id != binding->binding_id &&
          row.second.transport_cohort_id == binding->transport_cohort_id) {
        const auto &next_ref = row.second.registration_source_ref;
        const auto *next = next_ref.domain == "travel" &&
                                   next_ref.kind == "transport_registration"
                               ? registrations.registrations.find(next_ref.id)
                               : nullptr;
        if (!next)
          fail("PopulationLocationRegistrationRequired", next_ref.id);
        if (ordinal(next_ref) > registration_serial &&
            visible(next->occurred_at, next_ref))
          fail("PopulationLocationSourceSuperseded", ref.id);
      }
    if (released) {
      out.actual_anchor = release->actual_anchor;
      out.original_sources = {{release_ref, 1}};
      return out;
    }
    WorldMoment latest = registration->occurred_at;
    std::size_t latest_serial = registration_serial;
    auto adopt = [&](const WorldMoment &occurred, const SourceRef &source,
                     const MovementAnchor &anchor) {
      if (!visible(occurred, source))
        return;
      const auto serial = ordinal(source);
      const auto cmp = take(compare_world_moments(occurred, latest, b));
      if (cmp > 0 || (cmp == 0 && serial > latest_serial)) {
        latest = occurred;
        latest_serial = serial;
        out.actual_anchor = anchor;
        out.original_sources = {{registration_ref, 1}, {source, 1}};
      }
    };
    for (const auto &row : movement.units.rows()) {
      const auto &unit = row.second;
      const auto *execution =
          std::get_if<GroupMovementExecution>(&unit.execution);
      const auto *carrier =
          execution ? std::get_if<TransportUnitCarrierRef>(&execution->carrier)
                    : nullptr;
      if (!carrier || carrier->transport_unit_ref.transport_unit_id !=
                          binding->transport_unit_id)
        continue;
      const auto *journey = carriers.journeys.find(execution->journey_id);
      if (!journey ||
          journey->composition_revision != carrier->composition_revision)
        fail("PopulationLocationJourneyRequired", row.first);
      for (const auto &source : unit.effort_basis_refs) {
        const auto *basis = movement.exertion_bases.find(source.id);
        if (source.domain != "travel" ||
            source.kind != "travel_exertion_basis" || !basis ||
            basis->unit_ref != unit.id ||
            execution_key(basis->execution) != execution_key(unit.execution))
          fail("PopulationLocationMotionBasisRequired", source.id);
        if (take(compare_world_moments(basis->opened_at,
                                       registration->occurred_at, b)) < 0 ||
            !visible(basis->opened_at, source))
          continue;
        MovementPrefixState point;
        point.basis = basis->actual_basis;
        point.cursor = basis->opened_cursor;
        point.lifecycle = MovementActive{};
        adopt(basis->opened_at, source, take(read_movement_position(point, b)));
        for (const auto &work : unit.activity_spans)
          if (work.basis_ref == basis_id(basis->actual_basis)) {
            if (take(compare_world_moments(work.span.from, work.span.to, b)) >=
                    0 ||
                compare(work.cursor_to,
                        add(work.cursor_from,
                            mul(work.rate,
                                take(elapsed_world_duration(work.span.from,
                                                            work.span.to, b))
                                    .value,
                                b),
                            b),
                        b))
              fail("PopulationMotionActivityMismatch", unit.id);
            if (take(compare_world_moments(work.span.from, at, b)) > 0)
              continue;
            const auto actual =
                take(compare_world_moments(work.span.to, at, b)) < 0
                    ? work.span.to
                    : at;
            point.cursor =
                add(work.cursor_from,
                    mul(work.rate,
                        take(elapsed_world_duration(work.span.from, actual, b))
                            .value,
                        b),
                    b);
            adopt(actual, source, take(read_movement_position(point, b)));
          }
      }
      if (const auto *close = movement.unit_closes.find(unit.id + ":close")) {
        if (execution_key(close->execution) != execution_key(unit.execution))
          fail("PopulationLocationCloseMismatch", close->id);
        adopt(close->occurred_at, {"travel", "movement_unit_close", close->id},
              close->actual_anchor);
      }
    }
    const auto *current = movement.spatial_bindings.find(
        "transport_unit:" + binding->transport_unit_id);
    if (current &&
        std::holds_alternative<BattleMovementWriter>(current->writer))
      fail("PopulationBattleLocationHistoryRequired", binding->binding_id);
    return out;
  });
}

} // namespace
Outcome<PopulationHistoricalLocationPoint> read_population_location_at_source(
    const CommonStore &common, const SpatialCarrierState &carriers,
    const MovementExactState &movement,
    const TransportRegistrationState &registrations,
    const PopulationTransportState &population,
    const CivilianPopulationRef &population_ref, const SourceRef &ref,
    const WorldMoment &at, const SourceRef &adoption, ExactArithmeticBudget b) {
  return guard<PopulationHistoricalLocationPoint>([&] {
    take(encode_world_moment(at, b));
    take(validate_movement_exact_values(movement, b));
    head(common, adoption);
    const auto &order = common.state().facts.order();
    const auto found = std::find(order.begin(), order.end(), adoption.id);
    if (found == order.end())
      fail("PopulationLocationCommonOrderRequired", adoption.id);
    auto actual = take(resolve_population_location(
        common, carriers, movement, registrations, population, population_ref,
        ref, at, std::size_t(found - order.begin()), b));
    return PopulationHistoricalLocationPoint{
        population_ref,
        ref,
        adoption,
        at,
        std::move(actual.actual_anchor),
        std::move(actual.original_sources)};
  });
}
Outcome<MovementAnchor> read_population_location_at_actual_fact_prefix(
    const CommonStore &common, const SpatialCarrierState &carriers,
    const MovementExactState &movement,
    const TransportRegistrationState &registrations,
    const PopulationTransportState &population,
    const CivilianPopulationRef &population_ref, const SourceRef &ref,
    const WorldMoment &at, const CommonActualFactPrefix &prefix,
    const CurrentMomentRead &clock, ExactArithmeticBudget b) {
  return guard<MovementAnchor>([&] {
    take(encode_world_moment(at, b));
    take(validate_movement_exact_values(movement, b));
    const auto cut = take(read_actual_common_fact_prefix(common, prefix));
    if (cut != common.state().facts.order().size())
      fail("PopulationLocationCurrentPrefixRequired", ref.id);
    if (take(compare_world_moments(at, take(clock.read_current_moment()), b)))
      fail("PopulationLocationCurrentMomentRequired", ref.id);
    auto actual = take(resolve_population_location(
        common, carriers, movement, registrations, population, population_ref,
        ref, at, cut, b));
    return std::move(actual.actual_anchor);
  });
}

} // namespace hanmo::resident
