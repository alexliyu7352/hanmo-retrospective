#include "resident/domains/travel/movement_exact.hpp"
#include "field_army_group_origin.hpp"
#include "field_binding_source.hpp"
#include "field_boundary_source.hpp"
#include "field_contact_moment.hpp"
#include "field_segment_settlement.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/domains/travel/movement_boundary_model.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
Outcome<FieldOriginalArmyGroupOrigin>
read_field_original_army_group_origin(const FieldGroupHandoffReadContext &x,
                                      const FieldBattleRoadBinding &binding,
                                      ExactArithmeticBudget budget) {
  auto fail = [&](const char *code) -> Outcome<FieldOriginalArmyGroupOrigin> {
    return {false, {}, common_failure(code, binding.army_id).error};
  };
  if (!x.exact_contacts || &x.exact_contacts->common != &x.common ||
      &x.exact_contacts->military != &x.military ||
      &x.exact_contacts->field != &x.field ||
      &x.exact_contacts->movement != &x.movement ||
      &x.exact_contacts->groups != &x.groups ||
      &x.exact_contacts->carriers != &x.carriers ||
      &x.exact_contacts->travel != &x.travel ||
      &x.exact_contacts->clock != &x.clock ||
      !x.clock.bound_to(x.common, x.military, x.field))
    return fail("FieldArmyGroupOriginalOwnerRequired");
  auto original = read_field_original_battle_binding(
      x.common, x.field, binding.battle_id, binding.army_id);
  if (!original.ok)
    return {false, {}, original.error};
  if (original.value != &binding)
    return fail("FieldArmyGroupOriginalBindingRequired");
  FieldOriginalArmyGroupOrigin out;
  out.battle_binding = &binding;
  const FactRecord *adopted = nullptr;
  if (const auto *edge =
          std::get_if<FieldBoundaryContactBinding>(&binding.source)) {
    const auto found = x.field.boundary_contacts.find(edge->contact_id);
    if (found == x.field.boundary_contacts.end() ||
        !found->second.entered_binding ||
        &*found->second.entered_binding != &binding)
      return fail("FieldArmyGroupBoundaryBindingRequired");
    const auto *motion =
        std::get_if<FieldExactBoundaryOrigin>(&found->second.motion);
    if (!motion)
      return fail("FieldArmyGroupBoundaryExactSourceRequired");
    auto actual = read_field_boundary_original_source(
        *x.exact_contacts, x.catalog, motion->actual_contact, budget);
    if (!actual.ok)
      return {false, {}, actual.error};
    if (!actual.value.army || actual.value.army->id != binding.army_id ||
        !actual.value.journey || !actual.value.opening || !actual.value.closed)
      return fail("FieldArmyGroupBoundaryArmyMismatch");
    out.original_contact = &motion->actual_contact;
    out.journey = actual.value.journey;
    out.opening = actual.value.opening;
    out.closed = actual.value.closed;
    adopted = x.common.state().facts.find(found->first);
    if (!adopted || adopted->domain != "military" ||
        adopted->kind != "field_battle_contact")
      return fail("FieldArmyGroupBoundaryHeadRequired");
  } else if (const auto *initial =
                 std::get_if<FieldInitialContactBinding>(&binding.source)) {
    const MovementContactInstallationSource *installation = nullptr;
    for (const auto &r : x.movement.contact_installations.rows()) {
      const auto &ref = r.second.actual_business_source;
      if (ref.domain != "military" || ref.kind != "field_battle_contact" ||
          ref.id != initial->contact_id)
        continue;
      if (installation || r.first != r.second.id ||
          r.second.business_source_schema != 1)
        return fail("FieldArmyGroupInitialInstallationAmbiguous");
      installation = &r.second;
    }
    if (!installation)
      return fail("FieldArmyGroupInitialInstallationRequired");
    auto actual = read_field_initial_contact_actual_moment(
        x.common, x.military, x.field, x.catalog,
        installation->actual_business_source, *x.exact_contacts, budget);
    if (!actual.ok)
      return {false, {}, actual.error};
    out.original_contact = actual.value.original_movement_contact;
    const auto *contact = x.movement.contacts.find(out.original_contact->id);
    if (!contact || installation->contact_ref != contact->id ||
        initial->source_endpoint < 0 || initial->source_endpoint > 1)
      return fail("FieldArmyGroupInitialContactMismatch");
    const auto &prefix_id = initial->source_endpoint == 0
                                ? contact->first_prefix_id
                                : contact->second_prefix_id;
    const auto *prefix = x.movement.prefixes.find(prefix_id);
    const auto *execution =
        prefix ? std::get_if<GroupMovementExecution>(&prefix->execution)
               : nullptr;
    const auto *army =
        execution ? std::get_if<ArmyCarrierRef>(&execution->carrier) : nullptr;
    if (!army || army->army_id != binding.army_id)
      return fail("FieldArmyGroupInitialPrefixRequired");
    out.journey = x.carriers.journeys.find(execution->journey_id);
    if (!out.journey || spatial_carrier_key(out.journey->carrier) !=
                            spatial_carrier_key(execution->carrier))
      return fail("FieldArmyGroupInitialJourneyRequired");
    for (const auto &r : x.groups.openings.rows())
      if (r.second.journey_id == out.journey->journey_id) {
        if (out.opening || r.first != r.second.id)
          return fail("FieldArmyGroupInitialOpeningAmbiguous");
        out.opening = &r.second;
      }
    for (const auto &r : x.movement.unit_closes.rows()) {
      const auto *e = std::get_if<GroupMovementExecution>(&r.second.execution);
      if (!e || e->journey_id != out.journey->journey_id ||
          !same_source_ref(r.second.actual_cause, *out.original_contact))
        continue;
      if (out.closed || r.first != r.second.id ||
          r.second.prefix_id != prefix_id ||
          r.second.kind != MovementCloseKind::contact)
        return fail("FieldArmyGroupInitialCloseAmbiguous");
      out.closed = &r.second;
    }
    if (!out.closed)
      return fail("FieldArmyGroupInitialCloseRequired");
    const auto *unit = x.movement.units.find(out.closed->unit_ref);
    const auto *finalized =
        unit ? std::get_if<MovementUnitFinalized>(&unit->status) : nullptr;
    if (!finalized || !same_source_ref(finalized->original_close_source,
                                       *out.original_contact))
      return fail("FieldArmyGroupOriginalFinalizedUnitRequired");
    auto anchor = read_exact_contact_endpoint_anchor(x.movement, *contact,
                                                     prefix_id, budget);
    if (!anchor.ok)
      return {false, {}, anchor.error};
    auto same_anchor =
        same_spatial_anchor(anchor.value, out.closed->actual_anchor, budget);
    auto same_close = compare_world_moments(out.closed->occurred_at,
                                            contact->occurred_at, budget);
    auto same_install = compare_world_moments(installation->occurred_at,
                                              contact->occurred_at, budget);
    if (!same_anchor.ok)
      return {false, {}, same_anchor.error};
    if (!same_close.ok)
      return {false, {}, same_close.error};
    if (!same_install.ok)
      return {false, {}, same_install.error};
    if (!same_anchor.value || same_close.value || same_install.value)
      return fail("FieldArmyGroupOriginalProjectionMismatch");
    adopted = x.common.state().facts.find(installation->id);
    if (!adopted || adopted->domain != "travel" ||
        adopted->kind != "movement_contact_installation")
      return fail("FieldArmyGroupInitialHeadRequired");
  } else
    return fail("FieldArmyGroupEntryKindUnsupported");
  const auto *army =
      out.opening ? std::get_if<ArmyCarrierRef>(&out.opening->carrier_origin)
                  : nullptr;
  if (!army || army->army_id != binding.army_id ||
      !out.opening->military_order ||
      out.opening->journey_id != out.journey->journey_id)
    return fail("FieldArmyGroupOriginalOpeningRequired");
  auto opening = read_actual_common_fact_position(
      x.common,
      SourceRefBuildView{"travel", "group_movement_opening", out.opening->id});
  auto contact =
      read_actual_common_fact_position(x.common, *out.original_contact);
  auto close = read_actual_common_fact_position(
      x.common,
      SourceRefBuildView{"travel", "movement_unit_close", out.closed->id});
  auto adoption = read_actual_common_fact_position(x.common, *adopted);
  if (!opening.ok)
    return {false, {}, opening.error};
  if (!contact.ok)
    return {false, {}, contact.error};
  if (!close.ok)
    return {false, {}, close.error};
  if (!adoption.ok)
    return {false, {}, adoption.error};
  if (opening.value >= contact.value || contact.value >= close.value ||
      close.value >= adoption.value)
    return fail("FieldArmyGroupOriginalSourceOrderMismatch");
  return {true, out, {}};
}
} // namespace hanmo::resident
