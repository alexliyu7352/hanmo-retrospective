#include "movement_boundary_source.hpp"
#include "movement_exact_detail.hpp"
#include "resident/domains/military/field_battle/field_exact_contact_source.hpp"
#include "resident/domains/military/field_battle/field_segment_settlement.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/storage/common_store.hpp"
#include "spatial_carrier.hpp"
namespace hanmo::resident {
using namespace movement_detail;
namespace {
SourceRef anchor_source(const MovementAnchor &a) {
  return std::visit(
      [](const auto &v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, TerrainContinuousAnchor>)
          return v.actual_prefix_close_source;
        else
          return v.actual_source;
      },
      a);
}
void bind_anchor(MovementAnchor &a, const SourceRef &s) {
  std::visit(
      [&](auto &v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, TerrainContinuousAnchor>)
          v.actual_prefix_close_source = s;
        else
          v.actual_source = s;
      },
      a);
}
bool same_prediction(const MovementBoundaryContact &a,
                     const MovementBoundaryContact &z,
                     ExactArithmeticBudget b) {
  const auto *am =
      std::get_if<RunningMovementMap>(&a.motion_source.original_mapping);
  const auto *zm =
      std::get_if<RunningMovementMap>(&z.motion_source.original_mapping);
  return a.id.empty() && z.id.empty() && a.prefix_id == z.prefix_id &&
         same_source(a.original_field_source, z.original_field_source) &&
         a.field_hash == z.field_hash &&
         same_moment(a.prediction_span.from, z.prediction_span.from, b) &&
         same_moment(a.prediction_span.to, z.prediction_span.to, b) &&
         compare(a.entry_fraction, z.entry_fraction, b) == 0 &&
         same_moment(a.occurred_at, z.occurred_at, b) &&
         a.motion_source.unit_ref == z.motion_source.unit_ref &&
         a.motion_source.basis_ref == z.motion_source.basis_ref && am && zm &&
         same_moment(am->world_from, zm->world_from, b) &&
         compare(am->cursor_at_from, zm->cursor_at_from, b) == 0 &&
         compare(am->rate, zm->rate, b) == 0 &&
         compare(am->budget_rate, zm->budget_rate, b) == 0 &&
         same_source(am->rate_source, zm->rate_source) &&
         same_source(anchor_source(a.actual_anchor),
                     anchor_source(z.actual_anchor)) &&
         take(same_spatial_anchor(a.actual_anchor, z.actual_anchor, b));
}
} // namespace
Outcome<MovementBoundarySourceRead> read_movement_boundary_contact_source(
    const FieldExactContactReadContext &x, const FieldBattleCatalog &catalog,
    const SourceRef &source, ExactArithmeticBudget b) {
  return guard<MovementBoundarySourceRead>([&] {
    if (source.domain != "travel" ||
        source.kind != "movement_boundary_contact" ||
        !x.clock.bound_to(x.common, x.military, x.field))
      fail("MovementBoundaryOriginalSourceRequired", source.id);
    const auto *row = x.movement.boundary_contacts.find(source.id);
    if (!row || row->id != source.id ||
        !same_source(anchor_source(row->actual_anchor), source))
      fail("MovementBoundaryOriginalSourceRequired", source.id);
    take(read_actual_common_fact_position(x.common, source));
    const auto *prefix = x.movement.prefixes.find(row->prefix_id);
    const auto *unit = x.movement.units.find(row->motion_source.unit_ref);
    const TravelExertionBasis *basis = nullptr;
    if (unit)
      for (const auto &ref : unit->effort_basis_refs) {
        const auto *entry = x.movement.exertion_bases.find(ref.id);
        if (entry &&
            basis_id(entry->actual_basis) == row->motion_source.basis_ref) {
          if (basis)
            fail("MovementBoundaryOriginalBasisAmbiguous", source.id);
          basis = entry;
        }
      }
    if (!prefix || !unit || !basis)
      fail("MovementBoundaryOriginalMotionRequired", source.id);
    for (const auto &other : x.movement.boundary_contacts.rows())
      if (other.first != source.id &&
          other.second.prefix_id == row->prefix_id &&
          other.second.motion_source.unit_ref == row->motion_source.unit_ref &&
          same_source(other.second.original_field_source,
                      row->original_field_source))
        fail("MovementBoundaryDuplicateOriginalContact", source.id);
    take(validate_movement_boundary_geometry_source(x, catalog, *row, b));
    return MovementBoundarySourceRead{row, prefix, unit, basis};
  });
}
Status validate_movement_boundary_contact_sources(
    const FieldExactContactReadContext &x, const FieldBattleCatalog &catalog,
    ExactArithmeticBudget b) {
  return status([&] {
    if (!x.clock.bound_to(x.common, x.military, x.field))
      fail("MovementBoundaryOriginalSourceRequired");
    for (const auto &item : x.movement.boundary_contacts.rows()) {
      const SourceRef source{"travel", "movement_boundary_contact", item.first};
      const auto original =
          take(read_movement_boundary_contact_source(x, catalog, source, b));
      const auto &hit = *original.contact;
      const auto *close =
          x.movement.unit_closes.find(original.unit->id + ":close");
      const auto *done =
          std::get_if<MovementUnitFinalized>(&original.unit->status);
      if (!close || !done || close->prefix_id != hit.prefix_id ||
          close->unit_ref != original.unit->id ||
          close->kind != MovementCloseKind::contact ||
          !same_source(close->actual_cause, source) ||
          !same_source(done->original_close_source, source) ||
          !same_moment(close->occurred_at, hit.occurred_at, b) ||
          !take(
              same_spatial_anchor(close->actual_anchor, hit.actual_anchor, b)))
        fail("MovementBoundaryActualCloseRequired", source.id);
      const auto head =
          take(read_actual_common_fact_position(x.common, source));
      const auto close_head = take(read_actual_common_fact_position(
          x.common, {"travel", "movement_unit_close", close->id}));
      if (head >= close_head)
        fail("MovementBoundaryCloseOrderMismatch", source.id);
      const auto *effort =
          x.movement.exertions.find(original.unit->id + ":exertion");
      if (!effort || !same_source(effort->original_close_source, source) ||
          effort->prefix_id != hit.prefix_id ||
          effort->unit_ref != original.unit->id ||
          !same_moment(effort->actual_span.to, hit.occurred_at, b))
        fail("MovementBoundaryActualExertionRequired", source.id);
      take(read_verified_travel_exertion(x.movement, effort->id, b));
      const auto exertion_head = take(read_actual_common_fact_position(
          x.common, {"travel", "travel_exertion", effort->id}));
      if (exertion_head <= close_head)
        fail("MovementBoundaryExertionOrderMismatch", source.id);
    }
    for (const auto &id : x.common.state().facts.order()) {
      const auto *head = x.common.state().facts.find(id);
      if (head && head->domain == "travel" &&
          head->kind == "movement_boundary_contact" &&
          !x.movement.boundary_contacts.find(id))
        fail("MovementBoundaryHeaderWithoutSource", id);
    }
    for (const auto &item : x.movement.unit_closes.rows()) {
      const auto &close = item.second;
      if (close.actual_cause.domain != "travel" ||
          close.actual_cause.kind != "movement_boundary_contact")
        continue;
      const auto *hit =
          x.movement.boundary_contacts.find(close.actual_cause.id);
      if (!hit || close.prefix_id != hit->prefix_id ||
          close.unit_ref != hit->motion_source.unit_ref)
        fail("MovementBoundaryCloseWithoutSource", close.id);
    }
  });
}
Outcome<SourceRef> install_movement_boundary_contact_in_candidate(
    CommonStore &common, MovementExactState &movement,
    const FieldExactContactReadContext &x, const FieldBattleCatalog &catalog,
    const MovementBoundaryPrediction &prediction, ExactArithmeticBudget b) {
  return guard<SourceRef>([&] {
    if (&common != &x.common || &movement != &x.movement ||
        !x.clock.bound_to(common, x.military, x.field))
      fail("CrossCandidateMovementBoundary");
    const auto &proposed = prediction.contact;
    if (proposed.original_field_source.domain != "military" ||
        proposed.original_field_source.kind != "field_battle_session")
      fail("MovementBoundaryOriginalFieldRequired");
    auto current = take(predict_movement_field_boundary(
        x, catalog, proposed.original_field_source.id, proposed.prefix_id, b));
    if (!current || !same_prediction(proposed, current->contact, b))
      fail("StaleMovementBoundaryPrediction", proposed.prefix_id);
    auto now = take(x.clock.read_current_moment());
    const auto *prefix = movement.prefixes.find(proposed.prefix_id);
    if (!prefix || !same_moment(now, proposed.occurred_at, b) ||
        !same_moment(prefix->last_settled_at, now, b))
      fail("MovementBoundaryPrefixNotSettled", proposed.prefix_id);
    auto actual = std::move(current->contact);
    actual.id = take(common.next_id("movement_boundary_contact"));
    const SourceRef source{"travel", "movement_boundary_contact", actual.id};
    bind_anchor(actual.actual_anchor, source);
    const auto key = actual.id;
    movement.boundary_contacts.set(key, std::move(actual));
    take(common.append_fact({key, "movement_boundary_contact", "travel"}));
    take(read_movement_boundary_contact_source(x, catalog, source, b));
    return source;
  });
}
} // namespace hanmo::resident
