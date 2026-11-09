#pragma once
#include "transport_section_owned_fields.hpp"
namespace hanmo::resident::transport_section_fields {
template <class C>
Status group_journey(C &c, const StableId &key, GroupMovementJourney &row,
                     State &state) {
  auto &selected = state.record_selection;
  field(c, "journey_id", row.journey_id, selected);
  field(c, "carrier", row.carrier, selected);
  field(c, "position_ref", row.position_ref, selected);
  field(c, "composition_revision", row.composition_revision, selected);
  if (selected.group_version == 4) {
    if (c.reading())
      row.initiator.emplace<ActorGroupInitiator>();
    auto *actor = std::get_if<ActorGroupInitiator>(&row.initiator);
    if (!actor)
      return c.reject("HouseholdInitiatorRequiresGroup5", key);
    field(c, "initiator_actor_id", actor->initiator_actor_id, selected);
    field(c, "sponsor", row.sponsor, selected);
    field(c, "authority_ref", actor->authority_ref, selected);
  } else {
    field(c, "initiator", row.initiator, selected);
    field(c, "sponsor", row.sponsor, selected);
  }
  field(c, "payer_wallet_id", row.payer_wallet_id, selected);
  field(c, "expense_authority", row.expense_authority, selected);
  field(c, "shipment_refs", row.shipment_refs, selected);
  field(c, "route_profile", row.route_profile, selected);
  c.object("movement_execution", [&](auto &a) {
    return movement_execution_ref_codec_fields::group_only_fields(
        a, row.movement_execution);
  });
  field(c, "started_at", row.started_at, selected);
  field(c, "last_step_at", row.last_step_at, selected);
  field(c, "state", row.state, selected);
  field(c, "stop_reason", row.stop_reason, selected);
  field(c, "expense_source_refs", row.expense_source_refs, selected);
  field(c, "movement_source_refs", row.movement_source_refs, selected);
  if (!c.status().ok)
    return c.status();
  if (row.journey_id != key)
    return c.reject("GroupJourneyKeyMismatch", key);
  const MovementPrefixState *prefix = nullptr;
  for (const auto &candidate : state.roots.movement.prefixes.rows())
    if (const auto *execution =
            std::get_if<GroupMovementExecution>(&candidate.second.execution);
        execution && execution->journey_id == row.journey_id &&
        execution->segment_ordinal == row.movement_execution.segment_ordinal &&
        spatial_carrier_key(execution->carrier) ==
            spatial_carrier_key(row.carrier)) {
      if (prefix)
        return c.reject("GroupJourneyPhysicalPrefixNotUnique", key);
      prefix = &candidate.second;
    }
  if (!prefix)
    return c.reject("GroupJourneyPhysicalPrefixRequired", key);
  if (c.reading())
    row.movement_prefix_ref = prefix->id;
  else if (row.movement_prefix_ref != prefix->id)
    return c.reject("GroupJourneyPhysicalPrefixMismatch", key);
  auto &groups = *state.roots.executions;
  auto same_journey = [&](const auto &, const auto &source) {
    return source.journey_id == key;
  };
  const auto before = state.openings.values.size();
  auto result = optional_child(
      c, "opening_source", groups.openings, state.openings,
      [&](auto &a, const StableId &id, GroupMovementOpeningSource &v) {
        auto status = value(a, v, selected);
        if (status.ok && v.id != id)
          return a.reject("GroupOpeningKeyMismatch", id);
        return status;
      },
      same_journey);
  if (!result.ok)
    return result;
  if (c.reading() && state.openings.values.size() != before + 1)
    return c.reject("GroupJourneyPhysicalOpeningRequired", key);
  if (!c.reading()) {
    Integer count = 0;
    for (const auto &source : groups.openings.rows())
      if (source.second.journey_id == key)
        ++count;
    if (count != 1)
      return c.reject("GroupJourneyPhysicalOpeningRequired", key);
  }
  children(
      c, "unit_terms_sources", groups.unit_terms, state.unit_terms,
      [&](auto &a, const StableId &id, GroupMovementUnitTerms &v) {
        auto status = value(a, v, selected);
        if (status.ok && v.unit_ref != id)
          return a.reject("GroupUnitTermsKeyMismatch", id);
        return status;
      },
      same_journey);
  auto controls = children(
      c, "control_sources", groups.controls, state.group_controls,
      [&](auto &a, const StableId &id, GroupMovementControlSource &v) {
        auto status = value(a, v, selected);
        if (status.ok && v.id != id)
          return a.reject("GroupControlKeyMismatch", id);
        return status;
      },
      same_journey);
  if(!controls.ok)return controls;
  if(selected.group_version!=7){
    if(!groups.military_supply_rebinds.empty())return c.reject("MilitarySupplyRebindRequiresGroup7",key);
    return c.status();
  }
  return children(c,"military_supply_rebind_sources",groups.military_supply_rebinds,state.military_supply_rebinds,
    [&](auto& a,const StableId& id,GroupMilitarySupplyRebindSource& v){
      auto status=value(a,v,selected);
      if(status.ok&&v.id!=id)return a.reject("MilitarySupplyRebindKeyMismatch",id);
      return status;
    },same_journey);
}
template <class C> Status group_transport(C &c, State &state) {
  if (!state.roots.carriers || !state.roots.executions)
    return c.reject("GroupTransportOriginalRootsRequired");
  auto &carriers = *state.roots.carriers;
  auto regular = [&](auto &a, const StableId &key, auto &row) {
    return simple(a, key, row, state);
  };
  auto checked = transport_section_fields::all(
      c, "units", carriers.transport_units, state.transport_units, regular);
  if (!checked.ok)
    return checked;
  checked = transport_section_fields::all(
      c, "carrier_positions", carriers.positions, state.positions, regular);
  if (!checked.ok)
    return checked;
  checked = transport_section_fields::all(
      c, "group_movement_journeys", carriers.journeys, state.journeys,
      [&](auto &a, const auto &key, auto &row) {
        return group_journey(a, key, row, state);
      });
  if (!checked.ok)
    return checked;
  checked = transport_section_fields::all(c, "group_movement_bindings",
                                          carriers.bindings,
                                          state.group_bindings, regular);
  if (!checked.ok)
    return checked;
  checked = transport_section_fields::all(c, "journey_steps", carriers.steps,
                                          state.steps, regular);
  if (!checked.ok)
    return checked;
  checked = transport_section_fields::all(
      c, "journey_terminals", carriers.terminals, state.terminals, regular);
  if (!checked.ok)
    return checked;
  checked = transport_section_fields::all(c, "journey_purpose_bindings",
                                          carriers.journey_purpose_bindings,
                                          state.purposes, regular);
  if (!checked.ok)
    return checked;
  return state.reading ? state.adopt_group_rows() : c.status();
}
} // namespace hanmo::resident::transport_section_fields
