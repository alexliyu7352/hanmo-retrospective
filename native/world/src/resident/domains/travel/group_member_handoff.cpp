#include "group_movement.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {
Status validate_group_contact_member_releases(
    const CommonStore &common, const MovementExactState &movement,
    const GroupMovementExecutionState &groups,
    const GroupContactMemberReleaseRead *original) {
  auto losses =
      validate_group_member_loss_handoffs(common, movement, groups, original);
  if (!losses.ok)
    return losses;
  if (!groups.contact_member_releases.empty() &&
      (!original || !original->bound_to(common, movement, groups)))
    return common_failure("GroupMemberReleaseOriginalOwnerRequired");
  for (const auto &row : groups.contact_member_releases.rows()) {
    const auto &release = row.second;
    if (row.first != release.id || release.journey_id.empty() ||
        release.release_schema != 1 || release.named_participants.empty() ||
        release.original_contact.domain != "travel" ||
        (release.original_contact.kind != "movement_contact" &&
         release.original_contact.kind != "movement_boundary_contact") ||
        release.actual_release_source.domain != "military" ||
        release.actual_release_source.kind != "field_battle_exit")
      return common_failure("GroupMemberReleaseSourceShapeMismatch", row.first);
    auto before =
        read_actual_common_fact_position(common, release.actual_release_source);
    auto after = read_actual_common_fact_position(
        common, SourceRef{"travel", "group_contact_member_release", row.first});
    if (!before.ok)
      return {false, {}, before.error};
    if (!after.ok)
      return {false, {}, after.error};
    if (before.value >= after.value)
      return common_failure("GroupMemberReleaseSourceOrderMismatch", row.first);
    auto validated = original->validate_actual_group_member_release(release);
    if (!validated.ok)
      return validated;
    for (const auto &actor : release.named_participants) {
      if (actor.empty() ||
          std::count(release.named_participants.begin(),
                     release.named_participants.end(), actor) != 1)
        return common_failure("GroupMemberReleaseDuplicateActor", row.first);
      for (const auto &other : groups.contact_member_releases.rows())
        if (other.first != row.first &&
            other.second.journey_id == release.journey_id &&
            std::find(other.second.named_participants.begin(),
                      other.second.named_participants.end(),
                      actor) != other.second.named_participants.end())
          return common_failure("GroupMemberReleasedTwice", actor);
    }
  }
  for (const auto &head : common.state().facts.rows())
    if (head.second.domain == "travel" &&
        head.second.kind == "group_contact_member_release" &&
        !groups.contact_member_releases.find(head.first))
      return common_failure("GroupMemberReleasePayloadMissing", head.first);
  return common_success();
}
Outcome<CheckpointVector<const StableId *>>
read_active_group_named_members(const CommonStore &common,
                                const MovementExactState &movement,
                                const GroupMovementExecutionState &groups,
                                const GroupMovementJourney &journey,
                                const std::vector<StableId> &original_members,
                                const GroupContactMemberReleaseRead *original) {
  auto checked = validate_group_contact_member_releases(common, movement,
                                                        groups, original);
  if (!checked.ok)
    return {false, {}, checked.error};
  CheckpointVector<const StableId *> active;
  active.reserve(original_members.size());
  for (const auto &actor : original_members) {
    if (std::count(original_members.begin(), original_members.end(), actor) !=
        1)
      return {false,
              {},
              common_failure("GroupOpeningMemberDuplicate", actor).error};
    bool released = false;
    for (const auto &row : groups.contact_member_releases.rows())
      if (row.second.journey_id == journey.journey_id &&
          std::find(row.second.named_participants.begin(),
                    row.second.named_participants.end(),
                    actor) != row.second.named_participants.end())
        released = true;
    for (const auto &row : groups.member_loss_handoffs.rows())
      if (row.second.journey_id == journey.journey_id &&
          row.second.actor_id == actor)
        released = true;
    if (!released)
      active.push_back(&actor);
  }
  for (const auto &row : groups.contact_member_releases.rows()) {
    if (row.second.journey_id != journey.journey_id)
      continue;
    if (!std::holds_alternative<ArmyCarrierRef>(journey.carrier))
      return {false,
              {},
              common_failure("FieldMemberReleaseArmyCarrierRequired",
                             journey.journey_id)
                  .error};
    for (const auto &actor : row.second.named_participants)
      if (std::count(original_members.begin(), original_members.end(), actor) !=
          1)
        return {
            false,
            {},
            common_failure("GroupReleaseOriginalMemberRequired", actor).error};
  }
  for (const auto &row : groups.member_loss_handoffs.rows())
    if (row.second.journey_id == journey.journey_id &&
        (!std::holds_alternative<ArmyCarrierRef>(journey.carrier) ||
         std::count(original_members.begin(), original_members.end(),
                    row.second.actor_id) != 1))
      return {
          false,
          {},
          common_failure("GroupLossOriginalMemberRequired", row.second.actor_id)
              .error};
  return {true, std::move(active), {}};
}
Outcome<std::uint64_t> read_group_member_presence_revision(
    const CommonStore &common, const MovementExactState &movement,
    const GroupMovementExecutionState &groups, const StableId &journey,
    const GroupContactMemberReleaseRead *original) {
  auto checked = validate_group_contact_member_releases(common, movement,
                                                        groups, original);
  if (!checked.ok)
    return {false, {}, checked.error};
  const GroupMovementOpeningSource *opening = nullptr;
  for (const auto &entry : groups.openings.rows())
    if (entry.second.journey_id == journey) {
      if (opening || entry.first != entry.second.id)
        return {false,
                {},
                common_failure("GroupPresenceOriginalOpeningAmbiguous", journey)
                    .error};
      opening = &entry.second;
    }
  if (!opening)
    return {
        false,
        {},
        common_failure("GroupPresenceOriginalOpeningRequired", journey).error};
  auto base = read_actual_common_fact_position(
      common, {"travel", "group_movement_opening", opening->id});
  if (!base.ok)
    return {false, {}, base.error};
  auto last = base.value;
  for (const auto &entry : groups.contact_member_releases.rows())
    if (entry.second.journey_id == journey) {
      auto at = read_actual_common_fact_position(
          common, {"travel", "group_contact_member_release", entry.first});
      if (!at.ok)
        return {false, {}, at.error};
      if (at.value <= base.value)
        return {
            false,
            {},
            common_failure("GroupPresenceReleaseBeforeOpening", journey).error};
      last = std::max(last, at.value);
    }
  for (const auto &entry : groups.member_loss_handoffs.rows())
    if (entry.second.journey_id == journey) {
      auto at = read_actual_common_fact_position(
          common, SourceRefBuildView{"travel", "group_member_loss_handoff",
                                     entry.first});
      if (!at.ok)
        return {false, {}, at.error};
      if (at.value <= base.value)
        return {
            false,
            {},
            common_failure("GroupPresenceLossBeforeOpening", journey).error};
      last = std::max(last, at.value);
    }
  if (last == std::numeric_limits<std::uint64_t>::max())
    return {false,
            {},
            common_failure("GroupPresenceRevisionExhausted", journey).error};
  return {true, static_cast<std::uint64_t>(last) + 1, {}};
}
} // namespace hanmo::resident
