#include "group_member_loss_handoff.hpp"
#include "group_movement.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
namespace hanmo::resident {
Status validate_group_member_loss_handoffs(
    const CommonStore &common, const MovementExactState &movement,
    const GroupMovementExecutionState &groups,
    const GroupMemberLossHandoffRead *original) {
  if (!groups.member_loss_handoffs.empty() &&
      (!original || !original->bound_to(common, movement, groups)))
    return common_failure("GroupMemberLossOriginalOwnerRequired");
  for (const auto &row : groups.member_loss_handoffs.rows()) {
    const auto &loss = row.second;
    if (row.first != loss.id || loss.id.empty() || loss.journey_id.empty() ||
        loss.actor_id.empty() || loss.casualty_ordinal < 0 ||
        loss.original_contact.domain != "travel" ||
        (loss.original_contact.kind != "movement_contact" &&
         loss.original_contact.kind != "movement_boundary_contact") ||
        loss.actual_casualty.domain != "military" ||
        loss.actual_casualty.kind != "military_casualties")
      return common_failure("GroupMemberLossSourceShapeMismatch", row.first);
    const auto *head = common.state().facts.find(loss.id);
    if (!head || head->id != loss.id || head->domain != "travel" ||
        head->kind != "group_member_loss_handoff")
      return common_failure("GroupMemberLossActualHeadRequired", row.first);
    auto at = read_actual_common_fact_position(common, *head);
    auto contact =
        read_actual_common_fact_position(common, loss.original_contact);
    auto casualty =
        read_actual_common_fact_position(common, loss.actual_casualty);
    if (!at.ok)
      return {false, {}, at.error};
    if (!contact.ok)
      return {false, {}, contact.error};
    if (!casualty.ok)
      return {false, {}, casualty.error};
    if (contact.value >= casualty.value || casualty.value >= at.value)
      return common_failure("GroupMemberLossActualOrderMismatch", row.first);
    const GroupMovementOpeningSource *opening = nullptr;
    for (const auto &candidate : groups.openings.rows())
      if (candidate.second.journey_id == loss.journey_id) {
        if (opening || candidate.first != candidate.second.id)
          return common_failure("GroupMemberLossOriginalOpeningAmbiguous",
                                row.first);
        opening = &candidate.second;
      }
    if (!opening ||
        !std::holds_alternative<ArmyCarrierRef>(opening->carrier_origin) ||
        std::count(opening->named_participants.begin(),
                   opening->named_participants.end(), loss.actor_id) != 1)
      return common_failure("GroupMemberLossOriginalArmyMemberRequired",
                            row.first);
    auto opened = read_actual_common_fact_position(
        common,
        SourceRefBuildView{"travel", "group_movement_opening", opening->id});
    if (!opened.ok)
      return {false, {}, opened.error};
    if (opened.value >= contact.value)
      return common_failure("GroupMemberLossOpeningOrderMismatch", row.first);
    if (const auto *death =
            std::get_if<GroupMemberDeathApplication>(&loss.actual_body)) {
      if (death->actual_death.domain != "family_life" ||
          death->actual_death.kind != "family_death" ||
          death->actual_death.id.empty())
        return common_failure("GroupMemberLossDeathRefRequired", row.first);
    } else if (std::holds_alternative<GroupMemberCaptivityApplication>(
                   loss.actual_body) ||
               std::holds_alternative<GroupMemberDesertionApplication>(
                   loss.actual_body)) {
      const auto &source =
          std::holds_alternative<GroupMemberCaptivityApplication>(
              loss.actual_body)
              ? std::get<GroupMemberCaptivityApplication>(loss.actual_body)
                    .actual_capacity_transition
              : std::get<GroupMemberDesertionApplication>(loss.actual_body)
                    .actual_capacity_transition;
      // CapacityTransition is an original S4 source, not a Common header.
      // Its physical version and actual casualty cause are checked by the
      // same-World concrete owner, never inferred from its ID or today's actor.
      if (source.domain != "actor_foundation" ||
          source.kind != "capacity_transition" || source.id.empty())
        return common_failure("GroupMemberLossCapacitySourceRequired",
                              row.first);
    } else
      return common_failure("GroupMemberLossBodyKindUnsupported", row.first);
    for (const auto &other : groups.member_loss_handoffs.rows())
      if (other.first != row.first &&
          other.second.journey_id == loss.journey_id &&
          other.second.actor_id == loss.actor_id)
        return common_failure("GroupMemberLostTwice", loss.actor_id);
    for (const auto &release : groups.contact_member_releases.rows())
      if (release.second.journey_id == loss.journey_id &&
          std::find(release.second.named_participants.begin(),
                    release.second.named_participants.end(),
                    loss.actor_id) != release.second.named_participants.end())
        return common_failure("GroupMemberLostAndReleasedTwice", loss.actor_id);
    auto verified = original->validate_actual_group_member_loss(loss);
    if (!verified.ok)
      return verified;
  }
  for (const auto &head : common.state().facts.rows())
    if (head.second.domain == "travel" &&
        head.second.kind == "group_member_loss_handoff" &&
        !groups.member_loss_handoffs.find(head.first))
      return common_failure("GroupMemberLossPayloadMissing", head.first);
  return common_success();
}
} // namespace hanmo::resident
