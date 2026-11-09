#include "resident/domains/military/military_terms.hpp"
#include "resident/domains/military/military_model.hpp"
#include "field_boundary_source.hpp"
#include "field_author_data.hpp"
#include "field_segment_settlement.hpp"
#include "resident/domains/military/military_march_source.hpp"
#include "resident/domains/travel/movement_boundary_source.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/domains/travel/spatial_carrier.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
using namespace movement_detail;
Outcome<FieldBoundaryOriginalRead> read_field_boundary_original_source(
    const FieldExactContactReadContext &x, const FieldBattleCatalog &catalog,
    const SourceRef &source, ExactArithmeticBudget b) {
  return guard<FieldBoundaryOriginalRead>([&] {
    const auto motion =
        take(read_movement_boundary_contact_source(x, catalog, source, b));
    const auto &hit = *motion.contact;
    const auto *closed =
        x.movement.unit_closes.find(motion.unit->id + ":close");
    const auto *finalized =
        std::get_if<MovementUnitFinalized>(&motion.unit->status);
    if (!closed || !finalized || closed->kind != MovementCloseKind::contact ||
        closed->prefix_id != hit.prefix_id ||
        closed->unit_ref != motion.unit->id ||
        !same_source(closed->actual_cause, source) ||
        !same_source(finalized->original_close_source, source) ||
        !same_moment(closed->occurred_at, hit.occurred_at, b) ||
        !take(same_spatial_anchor(closed->actual_anchor, hit.actual_anchor, b)))
      fail("FieldBoundaryActualMovementCloseRequired", source.id);
    const auto source_head =
        take(read_actual_common_fact_position(x.common, source));
    const auto close_head = take(read_actual_common_fact_position(
        x.common, {"travel", "movement_unit_close", closed->id}));
    if (source_head >= close_head)
      fail("FieldBoundaryCloseOrderMismatch", source.id);
    const auto effort = take(read_verified_travel_exertion(
        x.movement, motion.unit->id + ":exertion", b));
    if (!same_source(effort.original_close_source, source) ||
        !same_moment(effort.actual_span.to, hit.occurred_at, b))
      fail("FieldBoundaryActualExertionRequired", source.id);
    FieldBoundaryOriginalRead out;
    out.contact = motion.contact;
    out.prefix = motion.prefix;
    out.unit = motion.unit;
    out.basis = motion.basis;
    out.closed = closed;
    const auto geometry =
        take(read_movement_boundary_field_geometry(x, catalog, hit, b));
    out.entry_point_dm = geometry.point_dm;
    out.incoming_facing_mdeg = geometry.incoming_facing_mdeg;
    const auto &parameters = catalog.parameters;
    if (parameters.window_s <= 0 || parameters.step_s <= 0 ||
        parameters.window_s % parameters.step_s)
      fail("InvalidFieldBattleParameters");
    const auto seconds =
        mul(hit.occurred_at.sub_half_day, integer(parameters.window_s, b), b);
    out.entry = {hit.occurred_at.half_day_tick,
                 take(floor_fraction(seconds, b)) / parameters.step_s *
                     parameters.step_s};
    if (const auto *personal =
            std::get_if<PersonalMovementExecution>(&motion.prefix->execution)) {
      if (!motion.basis->original_action ||
          motion.basis->original_action->id != personal->action_id ||
          motion.basis->original_action->kind != "travel")
        fail("FieldBoundaryOriginalPersonalActionRequired", source.id);
      return out;
    }
    const auto *group =
        std::get_if<GroupMovementExecution>(&motion.prefix->execution);
    if (!group)
      fail("FieldBoundaryOriginalTrafficRequired", source.id);
    out.journey = x.carriers.journeys.find(group->journey_id);
    if (!out.journey || spatial_carrier_key(out.journey->carrier) !=
                            spatial_carrier_key(group->carrier))
      fail("FieldBoundaryOriginalJourneyRequired", source.id);
    for (const auto &item : x.groups.openings.rows()) {
      if (item.second.journey_id != group->journey_id)
        continue;
      if (out.opening || item.first != item.second.id)
        fail("FieldBoundaryOriginalOpeningAmbiguous", source.id);
      out.opening = &item.second;
    }
    if (!out.opening)
      fail("FieldBoundaryOriginalOpeningRequired", source.id);
    const SourceRef opening{"travel", "group_movement_opening",
                            out.opening->id};
    if (take(read_actual_common_fact_position(x.common, opening)) >=
        source_head)
      fail("FieldBoundaryOriginalOpeningOrderMismatch", source.id);
    const auto *army_carrier = std::get_if<ArmyCarrierRef>(&group->carrier);
    if (!army_carrier)
      return out;
    if (!x.formations.bound_to(x.common.state(), x.military))
      fail("FieldBoundaryOriginalFormationOwnerRequired", source.id);
    const auto now = take(x.clock.read_current_moment());
    out.order = take(read_military_group_order_acceptance(
        x.common, x.military, x.military_catalog, x.carriers, x.movement,
        x.groups, x.conditions, opening, now, x.author, b, &x.formations, x.original_finite_returns));
    if (!out.order || out.order->army_id != army_carrier->army_id)
      fail("FieldBoundaryOriginalArmyOrderRequired", source.id);
    for (const auto &army : x.military.armies) {
      if (army.id != army_carrier->army_id)
        continue;
      if (out.army)
        fail("FieldBoundaryOriginalArmyAmbiguous", source.id);
      out.army = &army;
    }
    if (!out.army)
      fail("FieldBoundaryOriginalArmyRequired", source.id);
    const auto formed = take(read_original_military_formation(
        x.common.state(), x.military, *out.army, hit.occurred_at, &x.formations,
        b));
    if (!formed.original_terms ||
        take(read_actual_common_fact_position(
            x.common, formed.completed_source.source_ref)) >= source_head)
      fail("FieldBoundaryOriginalFormationRequired", source.id);
    out.faction_id = &formed.original_terms->organization_id;
    return out;
  });
}
} // namespace hanmo::resident
