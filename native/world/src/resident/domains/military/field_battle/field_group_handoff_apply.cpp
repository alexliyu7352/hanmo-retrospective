#include "resident/domains/travel/movement_exact.hpp"
#include "field_group_handoff.hpp"
#include "resident/domains/travel/field_battle_exit_placement.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/common_store.hpp"
#include <limits>
namespace hanmo::resident {
Outcome<SourceRef> apply_field_exit_group_handoff_in_candidate(
    CommonStore &common, MovementExactState &movement,
    GroupMovementExecutionState &groups,
    const FieldGroupHandoffReadContext &context, const SourceRef &source,
    const FieldBattleExitPlacementRead &placements,
    std::vector<BoundSpatialLocationWrite> &writes,
    CheckpointMemoryCharge &owner, ExactArithmeticBudget budget) {
  if (&common != &context.common || &movement != &context.movement ||
      &groups != &context.groups || !placements.bound_to(common, movement))
    return {false,
            {},
            common_failure("FieldGroupHandoffOwnerMismatch", source.id).error};
  auto prepared = prepare_field_group_member_handoff(context, source, budget);
  if (!prepared.ok)
    return {false, {}, prepared.error};
  const auto exit = context.field.exits.find(source.id);
  if (exit == context.field.exits.end())
    return {false,
            {},
            common_failure("FieldExitActualSourceRequired", source.id).error};
  const auto &members = exit->second.named_actor_ids;

  if (members.size() > writes.max_size() - writes.size())
    return {false,
            {},
            common_failure("FieldExitCaptureSizeExhausted", source.id).error};
  if (writes.size() + members.size() > writes.capacity()) {

    (owner).grow(0);
    writes.reserve(writes.size() + members.size());
  }


  auto source_owner = CheckpointMemoryCharge(0);
  const MomentOwnerSource actual_source{source, 1};
  for (const auto &actor : members) {
    auto target = placements.read_field_exit_placement(actual_source, actor);
    if (!target.ok)
      return {false, {}, target.error};
    // Only the two original exit projections are accepted. Settlement exits
    // have already passed the actual departure/P3/controller source reader;
    // this does not let a caller choose an arbitrary location.
    const auto *road =
        std::get_if<RoadFractionalAnchor>(&target.value.actual_anchor);
    const auto *original =
        std::get_if<OriginalMovementAnchor>(&target.value.actual_anchor);
    const auto *settlement =
        original ? std::get_if<SettlementLocation>(&original->location)
                 : nullptr;
    const auto *anchor_source = road       ? &road->actual_source
                                : original ? &original->actual_source
                                           : nullptr;
    if ((!road && !settlement) || !anchor_source ||
        anchor_source->domain != source.domain ||
        anchor_source->kind != source.kind || anchor_source->id != source.id)
      return {false,
              {},
              common_failure("FieldExitOriginalAnchorRequired", actor).error};

    // Reserve the stable binding key/body, copied anchor and actual mutation
    // strings before allocating them. Common capture owns its own tap fields.
    (void)0;


    for (int i = 0; i != 2; ++i) {
      if (road) {


      } else {


      }
    }

    (owner).grow(0);
    auto location =
        project_movement_location(target.value.actual_anchor, budget);
    if (!location.ok)
      return {false, {}, location.error};
    const auto sequence_before =
        common.actor_location_execution_drafts().size();
    if (sequence_before >= std::size_t(std::numeric_limits<Integer>::max()))
      return {false,
              {},
              common_failure("FieldExitCaptureSizeExhausted", actor).error};
    ActorMutation mutation;
    mutation.actor_id = actor;
    mutation.location = std::move(location.value);
    auto changed = common.apply_actor(mutation);
    if (!changed.ok)
      return {false, {}, changed.error};
    movement.spatial_bindings.set(
        "actor:" + actor,
        {"actor", actor, StableMovementWriter{target.value.actual_anchor},
         source});
    auto bound = bind_actual_field_exit_location_write(
        common, movement, placements, actor, actual_source,
        target.value.actual_anchor, Integer(sequence_before) + 1, budget);
    if (!bound.ok)
      return {false, {}, bound.error};
    writes.push_back(std::move(bound.value));
  }
  return adopt_field_group_member_handoff_in_candidate(
      common, movement, groups, context, std::move(prepared.value), writes,
      placements, owner, budget);
}
} // namespace hanmo::resident
