#include "field_event_legacy_kinds.hpp"
#include "field_reinforcement_queue.hpp"
#include "field_author_data.hpp"
#include "field_reinforcement_moment.hpp"
#include "field_formations.hpp"
#include "field_reinforcement_geometry.hpp"
#include "field_roster_origin.hpp"
#include "field_segment_settlement.hpp"
#include "resident/domains/military/military_formation_source.hpp"
#include "resident/domains/military/military_return_source.hpp"
#include "resident/domains/military/military_roster_history.hpp"
#include "resident/domains/military/naval_contact_history.hpp"
#include "resident/domains/travel/movement_boundary_source.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <charconv>
#include <string_view>
namespace hanmo::resident {
using namespace movement_detail;
namespace {
void require_actual_generated_entry_head(const CommonStore &common,
                                         const SourceRef &source,
                                         std::string_view prefix) {
  const std::string_view id = source.id;
  if (!id.starts_with(prefix))
    fail("FieldReinforcementOriginalIdMismatch", source.id);
  const auto digits = id.substr(prefix.size());
  Integer serial = 0;
  const auto parsed =
      std::from_chars(digits.data(), digits.data() + digits.size(), serial);
  if (digits.empty() || digits.front() == '0' || parsed.ec != std::errc{} ||
      parsed.ptr != digits.data() + digits.size() || serial <= 0 ||
      serial >= common.state().next_instance_id)
    fail("FieldReinforcementOriginalIdMismatch", source.id);
}
} // namespace
Outcome<const FieldReinforcementAdmission *>
read_field_reinforcement_admission_identity(const CommonStore &common,
                                            const FieldBattleWorldState &world,
                                            const FieldBattleSession &session,
                                            const FieldFormation &formation) {
  return guard<const FieldReinforcementAdmission *>([&] {
    const auto stored_session = world.sessions.find(session.battle_id);
    const auto stored_formation =
        session.formations.find(formation.formation_id);
    if (stored_session == world.sessions.end() ||
        &stored_session->second != &session ||
        stored_formation == session.formations.end() ||
        &stored_formation->second != &formation || !session.started_source)
      fail("FieldReinforcementAdmissionOriginalOwnerRequired",
           formation.formation_id);
    const FieldReinforcementAdmission *result = nullptr;
    for (const auto &[id, boundary] : world.boundary_contacts) {
      if (!boundary.reinforcement_queue)
        continue;
      const auto &queue = *boundary.reinforcement_queue;
      for (std::size_t n = 0; n < queue.admitted.size(); ++n) {
        const auto &row = queue.admitted[n];
        if (row.formation_id != formation.formation_id)
          continue;
        if (result || n >= queue.formation_ids.size() ||
            queue.formation_ids[n] != row.formation_id ||
            boundary.contact_id != id ||
            boundary.battle_id != session.battle_id || !boundary.army_id ||
            *boundary.army_id != formation.army_id || !boundary.side ||
            *boundary.side != formation.side ||
            row.application_source.domain != "military" ||
            row.application_source.kind != "field_battle_entered" ||
            queue.initialized_source.domain != "military" ||
            queue.initialized_source.kind != "field_battle_entry_queue" ||
            row.at.tick != formation.joined_at.tick ||
            row.at.t != formation.joined_at.t || row.event_sequence <= 0 ||
            row.event_sequence >= session.next_event_sequence ||
            row.position_dm.x != boundary.entry_point_dm.x ||
            row.position_dm.y != boundary.entry_point_dm.y)
          fail("FieldReinforcementAdmissionIdentityMismatch",
               formation.formation_id);
        if (row.ai_mission == row.pending_player_entry_order)
          fail("FieldReinforcementInitialTaskSelectionMismatch",
               row.formation_id);
        if (row.pending_player_entry_order) {
          const auto *inert = std::get_if<FieldHold>(&row.initial_mission);
          if (!inert || inert->point_dm.x != 0 || inert->point_dm.y != 0 ||
              inert->facing_mdeg != 0 || !boundary.entered_binding ||
              boundary.entered_binding->commander_actor_id !=
                  common.state().metadata.player_actor_id)
            fail("FieldReinforcementPlayerInitialTaskMismatch",
                 row.formation_id);
        }
        require_actual_generated_entry_head(common, row.application_source,
                                            "field_entry:");
        require_actual_generated_entry_head(common, queue.initialized_source,
                                            "field_battle_entry_queue:");
        const auto head = take(
            read_actual_common_fact_position(common, row.application_source));
        const auto queue_head = take(
            read_actual_common_fact_position(common, queue.initialized_source));
        if (take(read_actual_common_fact_position(
                common, SourceRef{"military", "field_battle_contact", id})) >=
                queue_head ||
            queue_head >= head ||
            take(read_actual_common_fact_position(
                common, *session.started_source)) >= head)
          fail("FieldReinforcementAdmissionOriginalOrderMismatch",
               row.application_source.id);
        if (take(compare_world_moments(queue.initialized_at, row.applied_at)) >
            0)
          fail("FieldReinforcementAdmissionActualMomentMismatch",
               row.application_source.id);
        if (n) {
          const auto &previous = queue.admitted[n - 1];
          if (take(read_actual_common_fact_position(
                  common, previous.application_source)) >= head ||
              take(compare_world_moments(previous.applied_at, row.applied_at)) >
                  0 ||
              previous.at.tick > row.at.tick ||
              (previous.at.tick == row.at.tick && previous.at.t > row.at.t) ||
              previous.event_sequence >= row.event_sequence)
            fail("FieldReinforcementAdmissionPrefixOrderMismatch",
                 row.application_source.id);
        }
        std::size_t events = 0;
        for (const auto &event : session.events) {
          if (event.sequence != row.event_sequence &&
              !(event.kind == field_event_kind::Entered &&
                event.formation_id == row.formation_id))
            continue;
          if (++events != 1 || event.sequence != row.event_sequence ||
              event.kind != field_event_kind::Entered ||
              event.formation_id != row.formation_id ||
              event.source_id != row.application_source.id ||
              event.side != formation.side || event.at.tick != row.at.tick ||
              event.at.t != row.at.t)
            fail("FieldReinforcementAdmissionActualEventMismatch",
                 row.application_source.id);
        }
        if (events != 1)
          fail("FieldReinforcementAdmissionActualEventRequired",
               row.application_source.id);
        result = &row;
      }
    }
    if (!result)
      fail("FieldReinforcementAdmissionSourceRequired", formation.formation_id);
    // A real entry head may not be shared by two admissions, even in another
    // battle. Each formation owns its own XP/learned/effective source cut.
    for (const auto &[id, boundary] : world.boundary_contacts)
      if (boundary.reinforcement_queue)
        for (const auto &row : boundary.reinforcement_queue->admitted)
          if (&row != result &&
              row.application_source.id == result->application_source.id)
            fail("FieldReinforcementAdmissionSourceReused",
                 result->application_source.id);
    return result;
  });
}
Outcome<const FieldReinforcementQueue *>
read_field_original_reinforcement_queue(
    const FieldExactContactReadContext &x, const FieldBattleCatalog &catalog,
    const FieldOriginalRosterReadContext &history, const SourceRef &contact,
    ExactArithmeticBudget b) {
  return guard<const FieldReinforcementQueue *>([&] {
    if (contact.domain != "military" ||
        contact.kind != "field_battle_contact" ||
        &history.common != &x.common || &history.military != &x.military ||
        &history.formations != &x.formations ||
        !x.clock.bound_to(x.common, x.military, x.field) ||
        !history.returns.bound_to(x.common, x.military) ||
        !history.field_applications ||
        !history.field_applications->bound_to(x.common, x.military, x.field))
      fail("FieldReinforcementQueueOriginalOwnersRequired", contact.id);
    const auto found = x.field.boundary_contacts.find(contact.id);
    if (found == x.field.boundary_contacts.end() ||
        !found->second.reinforcement_queue)
      fail("FieldReinforcementQueueSourceRequired", contact.id);
    const auto &boundary = found->second;
    const auto &queue = *boundary.reinforcement_queue;
    const auto *origin =
        std::get_if<FieldExactBoundaryOrigin>(&boundary.motion);
    if (!origin || !boundary.army_id || !boundary.side ||
        boundary.contact_id != contact.id || queue.formation_ids.empty() ||
        queue.formation_ids.size() > 32 ||
        queue.admitted.size() > queue.formation_ids.size() ||
        queue.initialized_source.domain != "military" ||
        queue.initialized_source.kind != "field_battle_entry_queue")
      fail("FieldReinforcementQueueIdentityMismatch", contact.id);
    const auto original = take(read_field_boundary_original_source(
        x, catalog, origin->actual_contact, b));
    if (!original.army || original.army->id != *boundary.army_id ||
        boundary.battle_id != original.contact->original_field_source.id)
      fail("FieldReinforcementQueueOriginalContactMismatch", contact.id);
    // 原格式已有发生／应用双时刻；冷读不把今日 clock 回拨成历史 M。
    take(validate_field_reinforcement_application_time(
        x, original, original.entry, queue.initialized_at, catalog.parameters,
        false, b, &queue.initialized_source));
    for (const auto &admission : queue.admitted)
      take(validate_field_reinforcement_application_time(
          x, original, admission.at, admission.applied_at, catalog.parameters,
          false, b, &admission.application_source));
    if (!queue.admitted.empty()) {
      const auto &first = queue.admitted.front();
      if (first.at.tick != original.entry.tick ||
          first.at.t != original.entry.t ||
          !same_moment(first.applied_at, queue.initialized_at, b))
        fail("FieldReinforcementFirstActualEntryRequired", contact.id);
    }
    require_actual_generated_entry_head(x.common, queue.initialized_source,
                                        "field_battle_entry_queue:");
    const auto position = take(
        read_actual_common_fact_position(x.common, queue.initialized_source));
    if (take(read_actual_common_fact_position(x.common, contact)) >= position)
      fail("FieldReinforcementQueueOriginalOrderMismatch", contact.id);
    const auto formed =
        take(x.formations.read_exact_army_formation(*boundary.army_id));
    if (!formed || !formed->original_terms ||
        take(read_actual_common_fact_position(
            x.common, formed->completed_source.source_ref)) >= position)
      fail("FieldReinforcementQueueOriginalFormationRequired", contact.id);
    const auto prefix = take(read_naval_original_prefix_before(
        x.common, queue.initialized_source.id));
    const auto now = take(x.clock.read_current_moment());
    if (!same_moment(now, take(history.clock.read_current_moment()), b))
      fail("FieldReinforcementQueueOriginalClockMismatch", contact.id);
    // 使用同世界真实 Field 回执源；未选旧 dynamic 不构造空根。
    const MilitaryFieldHistoryRead field_history{x.field, *history.field_applications};
    const auto rosters = take(read_military_rosters_at_selected_actual_prefix(
        x.common, x.military, x.military_catalog, history.identity, history.exact,
        formed->original_terms->unit_ids, prefix.prefix, now,
        history.population, history.population_catalog, b, &history.formations,
        &history.returns, &history.periods, nullptr, &field_history));
    struct Unit {
      const MilitaryUnitRecord *row;
      int rank;
    };
    CheckpointVector<Unit> ordered;
    std::size_t commanders = 0;
    for (const auto &unit : rosters.units) {
      if (unit.fit.empty() || unit.morale <= 0)
        continue;
      if (unit.army_id != *boundary.army_id)
        fail("FieldReinforcementQueueOriginalArmyMismatch", unit.id);
      const TroopDefinition *type = nullptr;
      for (const auto &candidate :
           x.military_catalog.battle_catalog.troop_types)
        if (candidate.id == unit.troop_type_id) {
          if (type)
            fail("FieldReinforcementTroopTypeAmbiguous", unit.id);
          type = &candidate;
        }
      if (!type)
        fail("FieldReinforcementTroopTypeRequired", unit.id);
      int rank = type->category == "infantry"  ? 0
                 : type->category == "archer"  ? 1
                 : type->category == "cavalry" ? 2
                                               : -1;
      if (rank < 0)
        fail("InvalidFieldTroopCategory", unit.id);
      if (unit.commander_actor_id == formed->original_terms->actor_id) {
        rank = 3;
        ++commanders;
      }
      ordered.push_back({&unit, rank});
    }
    if (commanders != 1 || ordered.size() != queue.formation_ids.size())
      fail("FieldReinforcementQueueRosterCoverageMismatch", contact.id);
    std::sort(ordered.begin(), ordered.end(), [](const auto &a, const auto &b) {
      return a.rank != b.rank ? a.rank < b.rank : a.row->id < b.row->id;
    });
    for (std::size_t n = 0; n < ordered.size(); ++n) {
      if (queue.formation_ids[n] != ordered[n].row->id ||
          (n && queue.formation_ids[n] == queue.formation_ids[n - 1]))
        fail("FieldReinforcementQueueOriginalOrderingMismatch", contact.id);
      if (n < queue.admitted.size() &&
          queue.admitted[n].formation_id != queue.formation_ids[n])
        fail("FieldReinforcementAdmissionPrefixMismatch", contact.id);
    }
    for (const auto &[id, other] : x.field.boundary_contacts)
      if (id != contact.id && other.reinforcement_queue &&
          same_source(other.reinforcement_queue->initialized_source,
                      queue.initialized_source))
        fail("FieldReinforcementQueueSourceReused", contact.id);
    return &queue;
  });
}
Status validate_field_reinforcement_queue_sources(
    const FieldExactContactReadContext &x, const FieldBattleCatalog &catalog,
    const FieldOriginalRosterReadContext &history, ExactArithmeticBudget b) {
  return status([&] {
    for (const auto &[id, boundary] : x.field.boundary_contacts) {
      if (!boundary.reinforcement_queue) {
        if (boundary.entry_application_source)
          fail("FieldReinforcementQueueSourceRequired", id);
        continue;
      }
      const auto *queue = take(read_field_original_reinforcement_queue(
          x, catalog, history,
          SourceRef{"military", "field_battle_contact", id}, b));
      const auto session = x.field.sessions.find(boundary.battle_id);
      if (session == x.field.sessions.end())
        fail("FieldReinforcementQueueSessionRequired", id);
      std::size_t pending_count = 0;
      for (const auto &pending : session->second.pending_entries)
        if (pending.contact_id == id) {
          ++pending_count;
          if (!boundary.army_id || !boundary.side ||
              pending.army_id != *boundary.army_id ||
              pending.side != *boundary.side ||
              pending.entry_point_dm.x != boundary.entry_point_dm.x ||
              pending.entry_point_dm.y != boundary.entry_point_dm.y)
            fail("FieldReinforcementPendingQueueMismatch", id);
        }
      const std::size_t expected_pending =
          session->second.state != FieldBattleState::Ended &&
                  queue->admitted.size() < queue->formation_ids.size()
              ? 1
              : 0;
      if (pending_count != expected_pending)
        fail("FieldReinforcementPendingQueueCoverageMismatch", id);
      for (std::size_t n = 0; n < queue->formation_ids.size(); ++n) {
        const auto found =
            session->second.formations.find(queue->formation_ids[n]);
        if (n < queue->admitted.size()) {
          if (found == session->second.formations.end() ||
              take(read_field_reinforcement_admission_identity(
                  x.common, x.field, session->second, found->second)) !=
                  &queue->admitted[n])
            fail("FieldReinforcementOriginalAdmissionMissing",
                 queue->formation_ids[n]);
        } else if (found != session->second.formations.end())
          fail("FieldReinforcementQueuedUnitInSimulation",
               queue->formation_ids[n]);
      }
      if (queue->admitted.empty()) {
        if (boundary.entry_application_source || boundary.entered_at ||
            boundary.entered_actual_at || boundary.entered_observed_tick ||
            boundary.entered_binding)
          fail("FieldReinforcementUnadmittedEntryEvidence", id);
      } else {
        const auto &first = queue->admitted.front();
        if (!boundary.entry_application_source ||
            !same_source(*boundary.entry_application_source,
                         first.application_source) ||
            !boundary.entered_at ||
            boundary.entered_at->tick != first.at.tick ||
            boundary.entered_at->t != first.at.t ||
            !boundary.entered_actual_at ||
            !same_moment(*boundary.entered_actual_at, first.applied_at, b) ||
            !boundary.entered_observed_tick ||
            *boundary.entered_observed_tick != first.applied_at.half_day_tick)
          fail("FieldReinforcementFirstAdmissionBindingMismatch", id);
      }
    }
    for (const auto &[id, head] : x.common.state().facts.rows()) {
      if (head.kind != "field_battle_entry_queue" &&
          head.kind != "field_battle_entered")
        continue;
      std::size_t owners = 0;
      for (const auto &[key, boundary] : x.field.boundary_contacts) {
        if (!boundary.reinforcement_queue)
          continue;
        const auto &queue = *boundary.reinforcement_queue;
        const auto matches = [&](const SourceRef &ref) {
          return ref.id == id && ref.domain == head.domain &&
                 ref.kind == head.kind;
        };
        if (head.kind == "field_battle_entry_queue")
          owners += matches(queue.initialized_source) ? 1 : 0;
        else
          for (const auto &row : queue.admitted)
            owners += matches(row.application_source) ? 1 : 0;
      }
      if (head.domain != "military" || owners != 1)
        fail("FieldReinforcementOriginalHeadCoverageMismatch", id);
    }
  });
}
Outcome<FieldReinforcementNextPreparation>
prepare_next_field_reinforcement_in_candidate(
    const FieldExactContactReadContext &x, const FieldBattleCatalog &catalog,
    const FieldOriginalRosterReadContext &history, const SourceRef &contact,
    ExactArithmeticBudget b) {
  return guard<FieldReinforcementNextPreparation>([&] {
    FieldReinforcementNextPreparation out;
    out.queue = take(read_field_original_reinforcement_queue(
        x, catalog, history, contact, b));
    out.boundary = &x.field.boundary_contacts.at(contact.id);
    const auto &boundary = *out.boundary;
    const auto &queue = *out.queue;
    if (queue.admitted.size() >= queue.formation_ids.size() ||
        boundary.exact_continuation_source ||
        (queue.admitted.empty()
             ? boundary.status != FieldBoundaryStatus::Stopped
             : boundary.status != FieldBoundaryStatus::Entered))
      fail("FieldReinforcementPendingAdmissionRequired", contact.id);
    const auto session = x.field.sessions.find(boundary.battle_id);
    if (session == x.field.sessions.end() ||
        session->second.state != FieldBattleState::Running ||
        session->second.result)
      fail("FieldReinforcementRunningSessionRequired", contact.id);
    out.session = &session->second;
    const FieldPendingEntry *pending = nullptr;
    for (const auto &row : out.session->pending_entries)
      if (row.contact_id == contact.id) {
        if (pending)
          fail("FieldReinforcementPendingEntryAmbiguous", contact.id);
        pending = &row;
      }
    if (!pending || pending->army_id != *boundary.army_id ||
        pending->side != *boundary.side ||
        pending->entry_point_dm.x != boundary.entry_point_dm.x ||
        pending->entry_point_dm.y != boundary.entry_point_dm.y ||
        out.session->now.tick < pending->entry.tick ||
        (out.session->now.tick == pending->entry.tick &&
         out.session->now.t < pending->entry.t))
      fail("FieldReinforcementExactEntryBoundaryRequired", contact.id);
    out.actual_at = take(x.clock.read_current_moment());
    if (take(compare_world_moments(queue.initialized_at, out.actual_at)) > 0)
      fail("FieldReinforcementActualEntryMomentRequired", contact.id);
    const auto *entry_origin =
        std::get_if<FieldExactBoundaryOrigin>(&boundary.motion);
    if (!entry_origin)
      fail("FieldReinforcementActualBoundaryRequired", contact.id);
    const auto actual_original = take(read_field_boundary_original_source(
        x, catalog, entry_origin->actual_contact, b));
    // 每一后续编队也须真实 closing，不能以“首个已过”放行后来分数 M。
    take(validate_field_reinforcement_application_time(
        x, actual_original, out.session->now, out.actual_at, catalog.parameters,
        true, b));
    if (queue.admitted.empty()) {
      if (!same_moment(queue.initialized_at, out.actual_at, b) ||
          out.session->now.tick != pending->entry.tick ||
          out.session->now.t != pending->entry.t)
        fail("FieldReinforcementFirstActualEntryRequired", contact.id);
    } else {
      for (const auto &row : queue.admitted) {
        const auto formation = out.session->formations.find(row.formation_id);
        if (formation == out.session->formations.end())
          fail("FieldReinforcementOriginalAdmissionMissing", row.formation_id);
        if (take(read_field_reinforcement_admission_identity(
                x.common, x.field, *out.session, formation->second)) != &row)
          fail("FieldReinforcementOriginalAdmissionMismatch", row.formation_id);
      }
      if (!take(read_field_reinforcement_entry_clear(
              *out.session, boundary.entry_point_dm,
              queue.admitted.back().formation_id)))
        fail("FieldReinforcementPreviousCircleStillAtEntry", contact.id);
    }
    const auto &next = queue.formation_ids[queue.admitted.size()];
    if (out.session->formations.contains(next))
      fail("FieldReinforcementFormationAlreadyEntered", next);
    std::size_t present = 0;
    for (const auto &[id, formation] : out.session->formations)
      if (formation.side == *boundary.side)
        ++present;
    if (present >= 32)
      fail("FieldReinforcementFormationLimit", contact.id);
    out.formation = take(read_field_reinforcement_formation_at_original_clock(
        x.common, x.military, x.military_catalog, x.field, *boundary.army_id,
        next, *boundary.side, x.common.state().metadata.player_actor_id,
        x.clock, x.formations, out.copied_formation));
    return out;
  });
}
Outcome<SourceRef> initialize_field_reinforcement_queue_in_candidate(
    CommonStore &common, FieldBattleWorldState &world,
    const FieldExactContactReadContext &x,
    const PoliticalRelationsReadContext &relations,
    const FieldBattleCatalog &catalog,
    const FieldOriginalRosterReadContext &history, const SourceRef &contact,
    CheckpointMemoryCharge &owner, ExactArithmeticBudget b) {
  return guard<SourceRef>([&] {
    if (&common != &x.common || &world != &x.field)
      fail("FieldReinforcementQueueSameCandidateRequired", contact.id);
    const auto existing = world.boundary_contacts.find(contact.id);
    if (existing != world.boundary_contacts.end() &&
        existing->second.reinforcement_queue)
      return take(read_field_original_reinforcement_queue(x, catalog, history,
                                                          contact, b))
          ->initialized_source;
    const auto prepared = take(prepare_field_reinforcement_at_original_entry(
        x, relations, catalog, contact, b));
    const auto id = take(common.next_id("field_battle_entry_queue"));


    (owner).grow(0);
    FieldReinforcementQueue queue;
    queue.initialized_source = {"military", "field_battle_entry_queue", id};
    queue.initialized_at = prepared.actual_at;
    queue.formation_ids.reserve(prepared.original_formations.size());
    for (const auto &formation : prepared.original_formations)
      queue.formation_ids.push_back(formation.formation_id);
    world.boundary_contacts.at(contact.id).reinforcement_queue =
        std::move(queue);
    take(common.append_fact({id, "field_battle_entry_queue", "military"}));
    return take(read_field_original_reinforcement_queue(x, catalog, history,
                                                        contact, b))
        ->initialized_source;
  });
}
} // namespace hanmo::resident
