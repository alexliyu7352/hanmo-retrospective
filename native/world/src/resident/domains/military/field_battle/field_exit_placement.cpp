#include "field_exit_placement.hpp"
#include "field_binding_source.hpp"
#include "field_segment_settlement.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
namespace hanmo::resident {
bool OriginalFieldBattleExitPlacementRead::bound_to(
    const CommonStore &c, const MovementExactState &m) const noexcept {
  return &c == &context_.common && &m == &context_.movement;
}
bool OriginalFieldBattleExitPlacementRead::bound_to(
    const CommonStore &c, const MovementExactState &m,
    const GroupMovementExecutionState &g) const noexcept {
  return bound_to(c, m) && &g == &context_.groups;
}
Outcome<ActualActorSpatialWriteRead>
OriginalFieldBattleExitPlacementRead::read_field_exit_placement(
    const MomentOwnerSource &source, const StableId &actor) const {
  const auto &x = context_;
  if (source.source_schema != 1)
    return {false,
            {},
            common_failure("FieldExitPlacementSchemaRequired", actor).error};
  auto original = read_field_actual_exit_application(
      x.common, x.military, x.field, x.travel, x.catalog, source.source_ref,
      x.clock, budget_, x.exact_contacts, x.diplomacy, x.territory);
  if (!original.ok)
    return {false, {}, original.error};
  const auto &exit = *original.value;
  if (std::count(exit.named_actor_ids.begin(), exit.named_actor_ids.end(),
                 actor) != 1)
    return {
        false,
        {},
        common_failure("FieldExitPlacementActualMemberRequired", actor).error};
  const auto road = std::get_if<OnRouteLocation>(&exit.location);
  const auto settlement = std::get_if<SettlementLocation>(&exit.location);
  if ((!road && !settlement) || bool(exit.exact_route_progress) != bool(road))
    return {false,
            {},
            common_failure("FieldExitOriginalPlacementRequired", actor).error};


  if (road) {


  } else {


  }
  auto temporary = CheckpointMemoryCharge(0);
  MovementAnchor anchor =
      road ? MovementAnchor{RoadFractionalAnchor{road->edge_id, road->origin,
                                                 *exit.exact_route_progress,
                                                 source.source_ref}}
           : MovementAnchor{
                 OriginalMovementAnchor{*settlement, source.source_ref}};
  CheckpointVector<ActualSpatialSourceBuildView> heads;
  heads.push_back(
      {{source.source_ref.domain, source.source_ref.kind, source.source_ref.id},
       1});
  return {true,
          ActualActorSpatialWriteRead{ActualActorSpatialWriteReadBuildView{
              actor, exit.applied_at, anchor, heads}},
          {}};
}
Status
OriginalFieldBattleExitPlacementRead::validate_actual_group_member_release(
    const GroupContactMemberReleaseSource &release) const {
  const auto &x = context_;
  auto fail = [&](const char *code) {
    return common_failure(code, release.id);
  };
  if (x.groups.contact_member_releases.find(release.id) != &release ||
      release.release_schema != 1 || release.named_participants.empty())
    return fail("FieldGroupActualReleaseRowRequired");
  auto original = read_field_actual_exit_application(
      x.common, x.military, x.field, x.travel, x.catalog,
      release.actual_release_source, x.clock, budget_, x.exact_contacts,
      x.diplomacy, x.territory);
  if (!original.ok)
    return {false, {}, original.error};
  const auto &exit = *original.value;
  if (release.named_participants != exit.named_actor_ids ||
      release.original_contact.domain != "travel" ||
      (release.original_contact.kind != "movement_contact" &&
       release.original_contact.kind != "movement_boundary_contact"))
    return fail("FieldGroupReleaseOriginalMembersMismatch");
  auto same_m =
      compare_world_moments(release.occurred_at, exit.applied_at, budget_);
  if (!same_m.ok)
    return {false, {}, same_m.error};
  if (same_m.value != 0)
    return fail("FieldGroupReleaseActualMomentMismatch");
  if (release.original_contact.kind == "movement_boundary_contact") {
    auto entry = read_field_single_group_exit_source(x, exit, budget_);
    if (!entry.ok)
      return {false, {}, entry.error};
    if (!entry.value.journey || !entry.value.closed ||
        entry.value.journey->journey_id != release.journey_id ||
        !same_source_ref(entry.value.closed->actual_cause,
                         release.original_contact))
      return fail("FieldGroupSingleReleaseSourceMismatch");
    auto before = read_actual_common_fact_position(
        x.common, release.actual_release_source);
    auto after = read_actual_common_fact_position(
        x.common, {"travel", "group_contact_member_release", release.id});
    if (!before.ok)
      return {false, {}, before.error};
    if (!after.ok)
      return {false, {}, after.error};
    if (before.value >= after.value)
      return fail("FieldGroupSingleReleaseOrderMismatch");
    return common_success();
  }
  const auto journey = x.carriers.journeys.find(release.journey_id);
  const auto army =
      journey ? std::get_if<ArmyCarrierRef>(&journey->carrier) : nullptr;
  const auto contact = x.movement.contacts.find(release.original_contact.id);
  if (!journey || !army || army->army_id != exit.army_id || !contact)
    return fail("FieldGroupReleaseOriginalJourneyRequired");
  const GroupMovementOpeningSource *opening = nullptr;
  for (const auto &entry : x.groups.openings.rows())
    if (entry.second.journey_id == release.journey_id) {
      const auto carrier =
          std::get_if<ArmyCarrierRef>(&entry.second.carrier_origin);
      if (opening || entry.first != entry.second.id || !carrier ||
          carrier->army_id != exit.army_id || !entry.second.military_order)
        return fail("FieldGroupReleaseOriginalOpeningMismatch");
      opening = &entry.second;
    }
  if (!opening)
    return fail("FieldGroupReleaseOriginalOpeningRequired");
  for (const auto &actor : release.named_participants)
    if (std::count(opening->named_participants.begin(),
                   opening->named_participants.end(), actor) != 1)
      return fail("FieldGroupReleaseOriginalMemberRequired");
  // A later unit/journey/battle must not invalidate the retained contact cut.
  // Find the actual retained close, not today's active prefix/Army binding.
  const MovementUnitCloseSource *closed = nullptr;
  for (const auto &entry : x.movement.unit_closes.rows()) {
    const auto execution =
        std::get_if<GroupMovementExecution>(&entry.second.execution);
    if (!execution || execution->journey_id != release.journey_id ||
        !same_source_ref(entry.second.actual_cause, release.original_contact))
      continue;
    if (closed || entry.first != entry.second.id ||
        entry.second.kind != MovementCloseKind::contact ||
        (contact->first_prefix_id != entry.second.prefix_id &&
         contact->second_prefix_id != entry.second.prefix_id))
      return fail("FieldGroupReleaseOriginalCloseAmbiguous");
    closed = &entry.second;
  }
  if (!closed)
    return fail("FieldGroupReleaseOriginalCloseRequired");
  auto close_m =
      compare_world_moments(closed->occurred_at, contact->occurred_at, budget_);
  auto earlier =
      compare_world_moments(contact->occurred_at, release.occurred_at, budget_);
  if (!close_m.ok)
    return {false, {}, close_m.error};
  if (!earlier.ok)
    return {false, {}, earlier.error};
  if (close_m.value != 0 || earlier.value > 0)
    return fail("FieldGroupReleaseOriginalCloseMomentMismatch");
  auto original_binding = read_field_original_battle_binding(
      x.common, x.field, exit.battle_id, exit.army_id);
  if (!original_binding.ok)
    return {false, {}, original_binding.error};
  const StableId *original_contact_id = nullptr;
  std::visit(
      [&](const auto &value) { original_contact_id = &value.contact_id; },
      original_binding.value->source);
  const MovementContactInstallationSource *installation = nullptr;
  for (const auto &entry : x.movement.contact_installations.rows()) {
    if (entry.second.contact_ref != contact->id)
      continue;
    const auto &business = entry.second.actual_business_source;
    if (business.domain != "military" ||
        business.kind != "field_battle_contact" || !original_contact_id ||
        business.id != *original_contact_id)
      continue;
    bool same_battle = false;
    if (auto found = x.field.contacts.find(business.id);
        found != x.field.contacts.end())
      same_battle = found->second.battle_id == exit.battle_id;
    if (auto found = x.field.boundary_contacts.find(business.id);
        found != x.field.boundary_contacts.end())
      same_battle = same_battle || found->second.battle_id == exit.battle_id;
    if (!same_battle)
      continue;
    if (installation || entry.first != entry.second.id ||
        entry.second.business_source_schema != 1)
      return fail("FieldGroupReleaseInstallationAmbiguous");
    installation = &entry.second;
  }
  if (!installation)
    return fail("FieldGroupReleaseActualInstallationRequired");
  auto installed_m = compare_world_moments(installation->occurred_at,
                                           contact->occurred_at, budget_);
  if (!installed_m.ok)
    return {false, {}, installed_m.error};
  if (installed_m.value != 0)
    return fail("FieldGroupReleaseInstallationMomentMismatch");
  auto close_head = read_actual_common_fact_position(
      x.common, SourceRef{"travel", "movement_unit_close", closed->id});
  auto exit_head =
      read_actual_common_fact_position(x.common, release.actual_release_source);
  if (!close_head.ok)
    return {false, {}, close_head.error};
  if (!exit_head.ok)
    return {false, {}, exit_head.error};
  if (close_head.value >= exit_head.value)
    return fail("FieldGroupReleaseCloseSourceOrderMismatch");
  const std::array<SourceRef, 6> ordered{
      {{"travel", "group_movement_opening", opening->id},
       release.original_contact,
       installation->actual_business_source,
       {"travel", "movement_contact_installation", installation->id},
       release.actual_release_source,
       {"travel", "group_contact_member_release", release.id}}};
  std::optional<std::size_t> previous;
  for (const auto &source : ordered) {
    auto position = read_actual_common_fact_position(x.common, source);
    if (!position.ok)
      return {false, {}, position.error};
    if (previous && position.value <= *previous)
      return fail("FieldGroupReleaseActualSourceOrderMismatch");
    previous = position.value;
  }
  // This genuine release head is appended only after all same-candidate
  // Common capture members pass; it is the retained adoption, not bare exit.
  return common_success();
}
Outcome<CheckpointVector<ActualActorSpatialWriteRead>>
OriginalFieldBattleExitPlacementRead::read_field_exit_placement_history(
    const StableId &actor) const {
  CheckpointVector<ActualActorSpatialWriteRead> result;
  for (const auto &entry : context_.groups.contact_member_releases.rows()) {
    if (std::find(entry.second.named_participants.begin(),
                  entry.second.named_participants.end(),
                  actor) == entry.second.named_participants.end())
      continue;
    auto valid = validate_actual_group_member_release(entry.second);
    if (!valid.ok)
      return {false, {}, valid.error};
    auto value = read_field_exit_placement(
        {entry.second.actual_release_source, 1}, actor);
    if (!value.ok)
      return {false, {}, value.error};
    result.push_back(std::move(value.value));
  }
  return {true, std::move(result), {}};
}
} // namespace hanmo::resident
