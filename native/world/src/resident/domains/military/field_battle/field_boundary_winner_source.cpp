#include "field_boundary_winner_source.hpp"
#include "field_binding_source.hpp"
#include "field_army_exit_selection.hpp"
#include "field_diplomacy_source.hpp"
#include "field_reinforcement_queue.hpp"
#include "field_roster_origin.hpp"
#include "field_segment_settlement.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/domains/military/training_exact.hpp"
#include "resident/domains/politics/relations/political_relations_read.hpp"
#include "resident/domains/travel/group_movement.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/domains/travel/spatial_carrier.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {
namespace {
// 只读本原军旅在实际续程头之前的真实退出/损失；不从别军全表反入其汇合几何。
// 全表payload反覆盖仍由外层World执行，这里逐行保留原typed reader及原成员/头序/唯一性。
CheckpointVector<const StableId *> read_winner_active_members_before(
    const FieldExactContactReadContext &x,
    const GroupContactMemberReleaseRead &original,
    const GroupMovementJourney &journey,
    const GroupMovementOpeningSource &opening,
    const std::optional<std::size_t> &adopted_cut) {
  using namespace movement_detail;
  if (!std::holds_alternative<ArmyCarrierRef>(journey.carrier) ||
      opening.journey_id != journey.journey_id)
    fail("FieldBoundaryWinnerOriginalArmyRequired", journey.journey_id);
  const auto opened = take(read_actual_common_fact_position(
      x.common, SourceRef{"travel", "group_movement_opening", opening.id}));
  CheckpointVector<const StableId *> removed;
  for (const auto &actor : opening.named_participants)
    if (actor.empty() || std::count(opening.named_participants.begin(),
                                   opening.named_participants.end(), actor) != 1)
      fail("GroupOpeningMemberDuplicate", actor);
  // 仅保存原opening成员地址；退出和损失两类不得重复覆盖同一人物。
  const auto remove_actual_member = [&](const StableId &actor) {
    const auto member = std::find(opening.named_participants.begin(),
                                  opening.named_participants.end(), actor);
    if (member == opening.named_participants.end())
      fail("GroupReleaseOriginalMemberRequired", actor);
    if (std::any_of(removed.begin(), removed.end(),
                    [&](const auto *prior) { return *prior == actor; }))
      fail("FieldBoundaryWinnerMemberReleasedTwice", actor);
    removed.push_back(&*member);
  };
  for (const auto &item : x.groups.contact_member_releases.rows()) {
    const auto &row = item.second;
    if (row.journey_id != journey.journey_id) continue;
    const auto at = take(read_actual_common_fact_position(
        x.common, SourceRef{"travel", "group_contact_member_release", row.id}));
    if (adopted_cut && at >= *adopted_cut) continue;
    if (item.first != row.id || row.release_schema != 1 || row.named_participants.empty() ||
        row.original_contact.domain != "travel" ||
        (row.original_contact.kind != "movement_contact" &&
         row.original_contact.kind != "movement_boundary_contact") ||
        row.actual_release_source.domain != "military" ||
        row.actual_release_source.kind != "field_battle_exit")
      fail("GroupMemberReleaseSourceShapeMismatch", row.id);
    const auto contact = take(read_actual_common_fact_position(x.common, row.original_contact));
    const auto exit = take(read_actual_common_fact_position(x.common, row.actual_release_source));
    if (opened >= contact || contact >= exit || exit >= at)
      fail("GroupMemberReleaseSourceOrderMismatch", row.id);
    take(original.validate_actual_group_member_release(row));
    for (const auto &actor : row.named_participants) remove_actual_member(actor);
  }
  for (const auto &item : x.groups.member_loss_handoffs.rows()) {
    const auto &row = item.second;
    if (row.journey_id != journey.journey_id) continue;
    const auto at = take(read_actual_common_fact_position(
        x.common, SourceRef{"travel", "group_member_loss_handoff", row.id}));
    if (adopted_cut && at >= *adopted_cut) continue;
    if (item.first != row.id || row.actor_id.empty() || row.casualty_ordinal < 0 ||
        row.original_contact.domain != "travel" ||
        (row.original_contact.kind != "movement_contact" &&
         row.original_contact.kind != "movement_boundary_contact") ||
        row.actual_casualty.domain != "military" || row.actual_casualty.kind != "military_casualties")
      fail("GroupMemberLossSourceShapeMismatch", row.id);
    const auto contact = take(read_actual_common_fact_position(x.common, row.original_contact));
    const auto casualty = take(read_actual_common_fact_position(x.common, row.actual_casualty));
    if (opened >= contact || contact >= casualty || casualty >= at)
      fail("GroupMemberLossActualOrderMismatch", row.id);
    take(original.validate_actual_group_member_loss(row));
    remove_actual_member(row.actor_id);
  }
  CheckpointVector<const StableId *> active;
  active.reserve(opening.named_participants.size());
  for (const auto &actor : opening.named_participants)
    if (std::none_of(removed.begin(), removed.end(),
                     [&](const auto *prior) { return *prior == actor; }))
      active.push_back(&actor);
  return active;
}
} // namespace
// 全程只借真实源；不重求战场出口，不改原军令、钱包、manifest或角色位置。
Outcome<FieldBoundaryWinnerRead> read_field_boundary_winner_resume_cause(
    const FieldExactContactReadContext &x,
    const PoliticalRelationsReadContext &relations,
    const FieldBattleCatalog &catalog, const FieldOriginalRosterReadContext &rosters,
    const SourceRef &source, ExactArithmeticBudget b,
    const SourceRef *actual_continuation) {
  return movement_detail::guard<FieldBoundaryWinnerRead>([&] {
    using namespace movement_detail;
    const auto *members = x.original_member_releases;
    if (!x.periods || x.periods != &rosters.periods || !members ||
        (x.group_context && x.group_context->member_releases != members) ||
        !members->bound_to(x.common, x.movement, x.groups) ||
        !x.clock.bound_to(x.common, x.military, x.field) ||
        &relations.common != &x.common || &relations.field != &x.field ||
        &relations.military != &x.military || &relations.formations != &x.formations)
      fail("FieldBoundaryWinnerOriginalOwnersRequired", source.id);
    const auto found = x.field.boundary_contacts.find(source.id);
    if (source.domain != "military" || source.kind != "field_battle_contact" ||
        found == x.field.boundary_contacts.end() || x.field.contacts.contains(source.id))
      fail("FieldBoundaryWinnerActualContactRequired", source.id);
    FieldBoundaryWinnerRead out;
    out.boundary = &found->second;
    const auto &boundary = *out.boundary;
    const auto *origin = std::get_if<FieldExactBoundaryOrigin>(&boundary.motion);
    if (!origin || boundary.contact_id != source.id || !boundary.army_id ||
        !boundary.side || *boundary.side < 0 || *boundary.side > 1 ||
        !boundary.entered_binding ||
        !boundary.entered_at || !boundary.entered_actual_at || !boundary.entered_observed_tick ||
        !boundary.entry_application_source || boundary.released_at)
      fail("FieldBoundaryWinnerActualEnteredSourceRequired", source.id);
    std::optional<std::size_t> adopted_cut;
    if (actual_continuation) {
      out.continuation = x.movement.boundary_continuations.find(actual_continuation->id);
      if (actual_continuation->domain != "travel" ||
          actual_continuation->kind != "movement_boundary_continuation" ||
          !out.continuation || out.continuation->id != actual_continuation->id ||
          out.continuation->held || boundary.status != FieldBoundaryStatus::Released ||
          !boundary.exact_continuation_source ||
          !same_source(*boundary.exact_continuation_source, *actual_continuation) ||
          !same_source(out.continuation->field_contact, source) ||
          !same_source(out.continuation->boundary_contact, origin->actual_contact))
        fail("FieldBoundaryWinnerActualContinuationRequired", source.id);
      adopted_cut = take(read_actual_common_fact_position(x.common, *actual_continuation));
    } else if (boundary.status != FieldBoundaryStatus::Entered || boundary.exact_continuation_source) {
      fail("FieldBoundaryWinnerUnadoptedSourceRequired", source.id);
    }
    out.original = take(read_field_boundary_original_source(x, catalog, origin->actual_contact, b));
    if (!out.original.army || out.original.army->id != *boundary.army_id ||
        !out.original.order || !out.original.opening || !out.original.journey ||
        boundary.battle_id != out.original.contact->original_field_source.id ||
        boundary.participant_actor_ids != out.original.basis->actual_participants)
      fail("FieldBoundaryWinnerOriginalArmyRequired", source.id);
    const auto boundary_cut = take(read_actual_common_fact_position(x.common, source));
    if (boundary.source_tick != out.original.contact->occurred_at.half_day_tick ||
        boundary.time_num || boundary.time_den ||
        boundary.entry_point_dm.x != out.original.entry_point_dm.x ||
        boundary.entry_point_dm.y != out.original.entry_point_dm.y ||
        take(read_actual_common_fact_position(x.common,
            {"travel", "movement_unit_close", out.original.closed->id})) >= boundary_cut)
      fail("FieldBoundaryWinnerOriginalRecordMismatch", source.id);
    const auto session = x.field.sessions.find(boundary.battle_id);
    if (session == x.field.sessions.end() || session->second.state != FieldBattleState::Ended ||
        !session->second.result || session->second.result->winner_side != *boundary.side)
      fail("FieldBoundaryWinnerActualTerminalRequired", source.id);
    out.session = &session->second;
    // 与真实Opening沿同一原终局判据互斥；胜军有序撤退也必须汇合，不借赢家余程绕过。
    if (take(read_field_army_terminal_exit_required(*out.session, *boundary.army_id)))
      fail("FieldBoundaryWinnerTerminalExitRequired", source.id);
    for (const auto &pending : out.session->pending_entries)
      if (pending.contact_id == source.id)
        fail("FieldBoundaryWinnerPendingNotInvalidated", source.id);
    // 冻结的侧别仍沿原接触前P3/同势力成军源，不借今日外交重选边。
    const auto relation = take(read_field_reinforcement_diplomacy_before_contact(relations, source, b));
    if (relation.side != boundary.side || boundary.diplomacy_sources.size() !=
        relation.actual_relation_sources.size() + (relation.same_faction_formation_source ? 1 : 0))
      fail("FieldBoundaryWinnerOriginalSideRequired", source.id);
    std::size_t relation_index = 0;
    for (const auto &actual : relation.actual_relation_sources)
      if (!same_source(boundary.diplomacy_sources[relation_index++], actual))
        fail("FieldBoundaryWinnerOriginalSideSourceMismatch", source.id);
    if (relation.same_faction_formation_source &&
        !same_source(boundary.diplomacy_sources[relation_index], *relation.same_faction_formation_source))
      fail("FieldBoundaryWinnerOriginalSideSourceMismatch", source.id);
    out.queue = take(read_field_original_reinforcement_queue(x, catalog, rosters, source, b));
    if (out.queue->admitted.empty()) fail("FieldBoundaryWinnerActualAdmissionRequired", source.id);
    const auto &first = out.queue->admitted.front();
    if (boundary.entered_at->tick != out.original.entry.tick ||
        boundary.entered_at->t != out.original.entry.t ||
        *boundary.entered_observed_tick != first.applied_at.half_day_tick ||
        !same_moment(*boundary.entered_actual_at, first.applied_at, b) ||
        !same_moment(out.queue->initialized_at, first.applied_at, b) ||
        !same_source(*boundary.entry_application_source, first.application_source))
      fail("FieldBoundaryWinnerOriginalFirstAdmissionRequired", source.id);
    out.binding = take(read_field_original_battle_binding(x.common, x.field, boundary.battle_id, *boundary.army_id));
    if (out.binding != &*boundary.entered_binding || !out.binding->action_id.empty())
      fail("FieldBoundaryWinnerOriginalBindingRequired", source.id);
    if (!actual_continuation) {
      const auto active_binding = x.field.army_bindings.find(*boundary.army_id);
      if (active_binding == x.field.army_bindings.end() ||
          active_binding->second.battle_id != boundary.battle_id ||
          active_binding->second.commander_actor_id != out.binding->commander_actor_id ||
          active_binding->second.members.size() != out.binding->members.size())
        fail("FieldBoundaryWinnerActiveBindingRequired", source.id);
      const auto *active_origin =
          std::get_if<FieldBoundaryContactBinding>(&active_binding->second.source);
      if (!active_origin || active_origin->contact_id != source.id ||
          active_binding->second.army_id != *boundary.army_id ||
          !active_binding->second.action_id.empty())
        fail("FieldBoundaryWinnerActiveBindingOriginRequired", source.id);
      for (const auto &member : out.binding->members)
        if (std::count_if(active_binding->second.members.begin(), active_binding->second.members.end(),
            [&](const auto &v) { return v.actor_id == member.actor_id && v.formation_id == member.formation_id; }) != 1)
          fail("FieldBoundaryWinnerActiveBindingMismatch", member.actor_id);
    } else {
      const auto active = x.field.army_bindings.find(*boundary.army_id);
      if (active != x.field.army_bindings.end() && active->second.battle_id == boundary.battle_id)
        fail("FieldBoundaryWinnerActiveBindingNotReleased", source.id);
    }
    bool retained = false;
    for (const auto &entry : out.queue->admitted) {
      const auto formation = out.session->formations.find(entry.formation_id);
      if (formation == out.session->formations.end() ||
          formation->second.army_id != *boundary.army_id || formation->second.side != *boundary.side ||
          take(read_field_reinforcement_admission_identity(x.common, x.field, *out.session, formation->second)) != &entry)
        fail("FieldBoundaryWinnerOriginalFormationRequired", entry.formation_id);
      if (formation->second.state == FieldFormationState::Routing)
        fail("FieldBoundaryWinnerTerminalDepartureRequired", entry.formation_id);
      if (formation->second.state != FieldFormationState::Withdrawn &&
          formation->second.state != FieldFormationState::Destroyed)
        retained = true;
    }
    if (!retained) fail("FieldBoundaryWinnerRetainedFormationRequired", source.id);
    for (const auto &segment : out.session->settlements) {
      if (!segment.battle_fact_id) continue;
      if (out.terminal) fail("FieldBoundaryWinnerTerminalAmbiguous", source.id);
      const SourceRef terminal{"military", "field_battle_segment", segment.fact_id};
      const auto verified = take(read_field_segment_settlement_source(x.common, x.military, x.field, terminal, &x.clock));
      if (verified.session != out.session || verified.settlement != &segment || !verified.terminal)
        fail("FieldBoundaryWinnerOriginalTerminalRequired", source.id);
      out.terminal = &segment;
    }
    if (!out.terminal || out.terminal != &out.session->settlements.back())
      fail("FieldBoundaryWinnerLastTerminalRequired", source.id);
    const SourceRef terminal{"military", "field_battle_segment", out.terminal->fact_id};
    const auto terminal_cut = take(read_actual_common_fact_position(x.common, terminal));
    if (take(read_actual_common_fact_position(x.common, source)) >= terminal_cut)
      fail("FieldBoundaryWinnerOriginalSourceOrderRequired", source.id);
    for (const auto &entry : out.queue->admitted)
      if (take(read_actual_common_fact_position(x.common, entry.application_source)) >= terminal_cut ||
          take(compare_world_moments(entry.applied_at, out.terminal->applied_at, b)) > 0)
        fail("FieldBoundaryWinnerAdmissionAfterTerminal", entry.application_source.id);
    const auto period = take(x.periods->read_training_period(out.terminal->applied_at.half_day_tick));
    if (period.from_tick == std::numeric_limits<Integer>::max() ||
        period.from_tick != out.terminal->applied_at.half_day_tick ||
        period.to_tick != period.from_tick + 1 || period.original_opening.source_schema != 1 ||
        !same_moment(period.opening_at, out.terminal->applied_at, b) ||
        terminal_cut >= take(read_actual_common_fact_position(x.common, period.original_opening.source_ref)) ||
        take(compare_world_moments(period.opening_at, take(x.clock.read_current_moment()), b)) > 0)
      fail("FieldBoundaryWinnerNextOpeningRequired", source.id);
    if (actual_continuation &&
        (!same_moment(out.continuation->occurred_at, period.opening_at, b) ||
         !same_source(out.continuation->terminal_segment, terminal) ||
         take(read_actual_common_fact_position(x.common, period.original_opening.source_ref)) >= *adopted_cut))
      fail("FieldBoundaryWinnerHistoricalOpeningMismatch", source.id);
    out.opening_at = period.opening_at;
    out.copied_sources.grow(0); // 原来源依旧完整复制，保持 job 归属。
    out.next_opening_source = {
        checkpoint_fresh_string_copy(period.original_opening.source_ref.domain),
        checkpoint_fresh_string_copy(period.original_opening.source_ref.kind),
        checkpoint_fresh_string_copy(period.original_opening.source_ref.id)};
    // 当前写者仍须是同一原军旅的真实contact停驻锚，不能拿旧关闭头重启别段。
    if (!actual_continuation) {
      const auto *closed = std::get_if<MovementClosed>(&out.original.prefix->lifecycle);
      const auto *writer = x.movement.spatial_bindings.find(spatial_carrier_key(out.original.journey->carrier));
      const auto *stable = writer ? std::get_if<StableMovementWriter>(&writer->writer) : nullptr;
      if (!closed || out.original.prefix->unit_ref != out.original.unit->id ||
          !same_source(closed->original_result_ref, origin->actual_contact) ||
          out.original.journey->state != GroupJourneyLifecycle::paused ||
          x.carriers.terminals.find(out.original.journey->journey_id) ||
          !out.original.journey->stop_reason ||
          !same_source(*out.original.journey->stop_reason, origin->actual_contact) ||
          !stable || !same_source(writer->binding_source, origin->actual_contact) ||
          !take(same_spatial_anchor(stable->anchor, out.original.closed->actual_anchor, b)))
        fail("FieldBoundaryWinnerOriginalPausedWriterRequired", source.id);
    }
    // 未入场差集只来自原单位成员减真实admitted并集；绝不把它们造作Field编队。
    out.admitted_original_members.reserve(boundary.participant_actor_ids.size());
    out.unadmitted_original_members.reserve(boundary.participant_actor_ids.size());
    for (const auto &actor : out.original.basis->actual_participants) {
      if (std::count(out.original.basis->actual_participants.begin(), out.original.basis->actual_participants.end(), actor) != 1)
        fail("FieldBoundaryWinnerOriginalMemberDuplicate", actor);
      const auto count = std::count_if(out.binding->members.begin(), out.binding->members.end(),
                                     [&](const auto &member) { return member.actor_id == actor; });
      if (count > 1) fail("FieldBoundaryWinnerAdmittedMemberDuplicate", actor);
      (count ? out.admitted_original_members : out.unadmitted_original_members).push_back(&actor);
    }
    if (out.admitted_original_members.size() != out.binding->members.size())
      fail("FieldBoundaryWinnerAdmittedUnionMismatch", source.id);
    // 热/冷共用同一原journey读取；冷态只采纳原续程头之前的真后果。
    auto active = read_winner_active_members_before(x, *members,
        *out.original.journey, *out.original.opening, adopted_cut);
    out.active_admitted_members.reserve(active.size());
    out.active_unadmitted_members.reserve(active.size());
    for (const auto *actor : active) {
      if (std::count(out.original.basis->actual_participants.begin(), out.original.basis->actual_participants.end(), *actor) != 1)
        fail("FieldBoundaryWinnerActiveMemberOutsideOriginalUnit", *actor);
      const auto member = std::find_if(out.binding->members.begin(), out.binding->members.end(),
                                      [&](const auto &v) { return v.actor_id == *actor; });
      if (member == out.binding->members.end()) {
        out.active_unadmitted_members.push_back(actor);
        continue;
      }
      const auto &formation = out.session->formations.at(member->formation_id);
      const bool fallen_officer = std::any_of(
          formation.commander_casualties.begin(), formation.commander_casualties.end(),
          [&](const auto &loss) { return loss.actor_id == *actor && loss.loss == 2; });
      if (fallen_officer || formation.state == FieldFormationState::Withdrawn || formation.state == FieldFormationState::Destroyed ||
          std::find(formation.dead.begin(), formation.dead.end(), *actor) != formation.dead.end() ||
          std::find(formation.captured.begin(), formation.captured.end(), *actor) != formation.captured.end() ||
          std::find(formation.deserted.begin(), formation.deserted.end(), *actor) != formation.deserted.end())
        fail("FieldBoundaryWinnerActualMemberReleaseRequired", *actor);
      out.active_admitted_members.push_back(actor);
    }
    if (out.active_admitted_members.empty())
      fail("FieldBoundaryWinnerActiveSurvivorsRequired", source.id);
    if (out.queue->admitted.size() == out.queue->formation_ids.size() && out.unadmitted_original_members.empty())
      out.coverage = FieldBoundaryWinnerCoverage::fully_admitted;
    if (actual_continuation) {
      if (out.coverage != FieldBoundaryWinnerCoverage::fully_admitted)
        fail("FieldBoundaryWinnerPartialRendezvousRequired", source.id);
      const auto *next = x.movement.units.find(out.continuation->new_unit_ref);
      const auto *next_group = next ? std::get_if<GroupMovementExecution>(&next->execution) : nullptr;
      const auto *basis = next && next->effort_basis_refs.size() == 1
          ? x.movement.exertion_bases.find(next->effort_basis_refs.front().id) : nullptr;
      if (!next || !next_group || next_group->journey_id != out.original.journey->journey_id ||
          out.continuation->prefix_id != out.original.prefix->id ||
          out.continuation->closed_unit_ref != out.original.unit->id || !basis ||
          basis->actual_participants.size() != out.active_admitted_members.size())
        fail("FieldBoundaryWinnerAdoptedMembersRequired", source.id);
      for (const auto *actor : out.active_admitted_members)
        if (std::count(basis->actual_participants.begin(), basis->actual_participants.end(), *actor) != 1)
          fail("FieldBoundaryWinnerAdoptedMemberMismatch", *actor);
    }
    // partial维持默认分类；现合同尚无“在不同真实位置已整军齐备”的合流来源。
    return out;
  });
}
} // namespace hanmo::resident
