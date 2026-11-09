#include "field_member_loss_desertion.hpp"
#include "field_exit_placement.hpp"
#include "field_member_loss_group_source.hpp"
#include "resident/domains/actor_foundation/character_condition_read.hpp"
#include "resident/domains/actor_foundation/military_availability.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <limits>
#include <new>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
void take(Status value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
}
void require(bool yes, const char *code) {
  if (!yes)
    throw Failure{common_failure(code).error};
}
SourceRef fresh(const SourceRef &source) {
  return {checkpoint_fresh_string_copy(source.domain),
          checkpoint_fresh_string_copy(source.kind),
          checkpoint_fresh_string_copy(source.id)};
}
} // namespace
// 真原段/真退出点 → 原 S4 许可 → 一次 Common mutation → 原 S4/S1 两捕获。
Outcome<FieldDesertedMemberApplication>
apply_field_deserted_group_member_with_capture_in_candidate(
    CommonStore &common, AuthoredActorFoundationState &foundation,
    MovementExactState &movement, GroupMovementExecutionState &groups,
    const FieldGroupHandoffReadContext &x, const SourceRef &casualty,
    const StableId &actor, const ReadStamp &stamp,
    const ReadStampAuthority &stamps, const DomainReadVersionSource &versions,
    CheckpointMemoryCharge &owner, ExactArithmeticBudget budget) {
  try {
    require(&common == &x.common && &movement == &x.movement &&
                &groups == &x.groups && x.original_body &&
                &x.original_body->common == &common &&
                &x.original_body->state == &foundation &&
                common.actor_location_capture_identity() &&
                common.actor_condition_capture_identity(),
            "FieldDesertionSameCandidateCaptureRequired");
    const auto &body = *x.original_body;
    auto origin =
        take(read_field_member_loss_group_origin(x, casualty, actor, budget));
    require(!origin.ordinary_loss && origin.segment.settlement &&
                origin.actor_change->status == "deserted" &&
                origin.actor_change->captor_army_id.empty() &&
                !origin.actor_change->garrison_id &&
                origin.journey->state == GroupJourneyLifecycle::paused,
            "FieldDesertionOriginalPausedMemberRequired");
    require(take(compare_world_moments(take(x.clock.read_current_moment()),
                                       origin.segment.settlement->applied_at,
                                       budget)) == 0,
            "FieldDesertionActualMomentRequired");
    const auto *exit =
        take(read_field_member_desertion_exit_source(x, origin, budget));
    for (const auto &entry : groups.member_loss_handoffs.rows())
      require(entry.second.journey_id != origin.journey->journey_id ||
                  entry.second.actor_id != actor,
              "FieldDesertionMemberAlreadyLost");
    for (const auto &entry : groups.contact_member_releases.rows())
      require(entry.second.journey_id != origin.journey->journey_id ||
                  std::find(entry.second.named_participants.begin(),
                            entry.second.named_participants.end(),
                            actor) == entry.second.named_participants.end(),
              "FieldDesertionMemberAlreadyReleased");
    const SpatialWriterBinding *previous = nullptr;
    const StableId *original_key = nullptr;
    for (const auto &entry : movement.spatial_bindings.rows())
      if (entry.second.subject_kind == "actor" &&
          entry.second.subject_id == actor) {
        require(!previous && entry.first.size() == 6 + actor.size() &&
                    entry.first.compare(0, 6, "actor:") == 0 &&
                    entry.first.compare(6, actor.size(), actor) == 0,
                "FieldDesertionOriginalBindingAmbiguous");
        previous = &entry.second;
        original_key = &entry.first;
      }
    const auto *carried =
        previous ? std::get_if<CarriedMovementWriter>(&previous->writer)
                 : nullptr;
    const auto *army =
        carried ? std::get_if<ArmyCarrierRef>(&carried->carrier_ref) : nullptr;
    require(army && army->army_id == origin.actor_change->army_id &&
                previous->binding_source.domain == "travel" &&
                previous->binding_source.kind == "group_movement_opening" &&
                previous->binding_source.id == origin.opening->id,
            "FieldDesertionOriginalCarriedMemberRequired");
    const auto location_capture = common.actor_location_capture_identity();
    const auto location_offset =
        common.actor_location_execution_drafts().size();
    const auto condition_offset =
        common.actor_condition_execution_drafts().size();
    require(
        location_offset < std::size_t(std::numeric_limits<Integer>::max()) &&
            condition_offset < std::size_t(std::numeric_limits<Integer>::max()),
        "FieldDesertionCaptureSequenceExhausted");
    auto permit = take(prepare_field_military_availability(
        common, foundation, body, casualty, origin.casualty_ordinal, stamp,
        stamps, versions));
    require(bool(permit), "FieldDesertionActualAvailabilityPermitRequired");


    (owner).grow(0);
    const SourceRef exit_source{checkpoint_fresh_text_copy("military"),
                                checkpoint_fresh_text_copy("field_battle_exit"),
                                checkpoint_fresh_string_copy(exit->fact_id)};
    MovementAnchor anchor;
    if (const auto *road = std::get_if<OnRouteLocation>(&exit->location)) {
      require(bool(exit->exact_route_progress),
              "FieldDesertionOriginalProgressRequired");
      anchor = RoadFractionalAnchor{road->edge_id, road->origin,
                                    *exit->exact_route_progress, exit_source};
    } else if (const auto *town =
                   std::get_if<SettlementLocation>(&exit->location)) {
      require(!exit->exact_route_progress, "FieldDesertionUnexpectedProgress");
      anchor = OriginalMovementAnchor{*town, exit_source};
    } else
      require(false, "FieldDesertionOriginalPlacementRequired");
    const auto binding_key = checkpoint_fresh_string_copy(*original_key);
    ActorMutation mutation;
    mutation.actor_id = checkpoint_fresh_string_copy(actor);
    mutation.available = false;
    mutation.location = take(project_movement_location(anchor, budget));
    take(common.apply_actor(mutation));
    auto availability = take(
        bind_field_military_availability(common, foundation, body, *permit));
    const auto &locations = common.actor_location_execution_drafts();
    require(availability.transition &&
                availability.capture_sequence ==
                    Integer(condition_offset + 1) &&
                common.actor_condition_execution_drafts().size() ==
                    condition_offset + 1 &&
                locations.size() == location_offset + 1 &&
                locations.back().sequence == Integer(location_offset + 1) &&
                locations.back().actor_id == actor && locations.back().before &&
                locations.back().write_identity &&
                locations.back().kind ==
                    ActorLocationExecutionDraft::Kind::mutation,
            "FieldDesertionSingleActualWriteRequired");
    const auto write_identity = locations.back().write_identity;
    const SourceRef capacity{
        checkpoint_fresh_text_copy("actor_foundation"),
        checkpoint_fresh_text_copy("capacity_transition"),
        checkpoint_fresh_string_copy(availability.transition->transition_id)};
    const auto id = take(common.next_id("group_member_loss_handoff"));
    GroupMemberLossHandoffSource row;
    row.id = checkpoint_fresh_string_copy(id);
    row.journey_id = checkpoint_fresh_string_copy(origin.journey->journey_id);
    row.actor_id = checkpoint_fresh_string_copy(actor);
    row.original_contact = fresh(*origin.original_contact);
    row.actual_casualty = fresh(casualty);
    row.casualty_ordinal = origin.casualty_ordinal;
    row.occurred_at = origin.segment.settlement->applied_at;
    row.actual_body = GroupMemberDesertionApplication{fresh(capacity)};
    const SourceRef handoff{
        checkpoint_fresh_text_copy("travel"),
        checkpoint_fresh_text_copy("group_member_loss_handoff"),
        checkpoint_fresh_string_copy(id)};
    SpatialWriterBinding next;
    next.subject_kind = checkpoint_fresh_text_copy("actor");
    next.subject_id = checkpoint_fresh_string_copy(actor);
    next.writer = StableMovementWriter{anchor};
    next.binding_source = fresh(handoff);
    movement.spatial_bindings.set(binding_key, std::move(next));
    groups.member_loss_handoffs.set(checkpoint_fresh_string_copy(id),
                                    std::move(row));
    take(common.append_fact(
        {checkpoint_fresh_string_copy(id),
         checkpoint_fresh_text_copy("group_member_loss_handoff"),
         checkpoint_fresh_text_copy("travel")}));
    OriginalFieldBattleExitPlacementRead original(x, budget);
    auto spatial = take(bind_actual_field_member_loss_location_write(
        common, movement, original, actor, MomentOwnerSource{fresh(handoff), 1},
        location_capture, write_identity, Integer(location_offset + 1),
        budget));
    return {
        true,
        {{checkpoint_fresh_string_copy(availability.transition->transition_id),
          availability.capture_identity, availability.capture_sequence},
         std::move(spatial)},
        {}};
  } catch (Failure &failure) {
    return {false, {}, std::move(failure.error)};
  } catch (const std::bad_alloc &) {
    return {false,
            {},
            common_failure("FieldDesertionResourceExhausted", {},
                           ErrorKind::internal)
                .error};
  }
}
} // namespace hanmo::resident
