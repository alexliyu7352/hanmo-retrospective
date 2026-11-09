#include "resident/domains/military/military_model.hpp"
#include "field_boundary_install.hpp"
#include "field_author_data.hpp"
#include "field_diplomacy_source.hpp"
#include "field_reinforcement_queue.hpp"
#include "field_reinforcement_moment.hpp"
#include "field_segment_settlement.hpp"
#include "resident/domains/politics/relations/political_relations_read.hpp"
#include "resident/domains/travel/movement_boundary_continuation_source.hpp"
#include "resident/domains/travel/movement_boundary_source.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
using namespace movement_detail;
Outcome<StableId> install_field_boundary_contact_in_candidate(
    CommonStore &common, FieldBattleWorldState &world,
    const FieldExactContactReadContext &x,
    const PoliticalRelationsReadContext &relations,
    const FieldBattleCatalog &catalog, const SourceRef &source,
    CheckpointMemoryCharge &owner, ExactArithmeticBudget b) {
  return guard<StableId>([&] {
    if (&common != &x.common || &world != &x.field ||
        &relations.common != &common || &relations.field != &world ||
        &relations.military != &x.military ||
        &relations.formations != &x.formations)
      fail("FieldBoundarySameCandidateRequired", source.id);
    const auto original =
        take(read_field_boundary_original_source(x, catalog, source, b));
    const auto &hit = *original.contact;
    const auto now = take(x.clock.read_current_moment());
    const auto relation_now =
        take(relations.actual_clock.read_current_moment());
    if (!same_moment(now, hit.occurred_at, b) ||
        !same_moment(now, relation_now, b))
      fail("FieldBoundaryActualMomentRequired", source.id);
    auto found = world.sessions.find(hit.original_field_source.id);
    if (found == world.sessions.end() ||
        found->second.state == FieldBattleState::Ended)
      fail("FieldBoundaryActiveSessionRequired", source.id);
    auto &session = found->second;
    if (original.entry.tick < session.now.tick ||
        (original.entry.tick == session.now.tick &&
         original.entry.t < session.now.t))
      fail("FieldBoundaryLocalApplicationInPast", source.id);
    for (const auto &[id, row] : world.boundary_contacts) {
      const auto *prior = std::get_if<FieldExactBoundaryOrigin>(&row.motion);
      if (prior && same_source(prior->actual_contact, source))
        fail("FieldBoundaryAlreadyInstalled", source.id);
    }
    FieldReinforcementDiplomacyRead diplomacy;
    if (original.army) {
      if (world.army_bindings.contains(original.army->id))
        fail("FieldBoundaryArmyAlreadyEngaged", original.army->id);
      for (const auto &entry : session.pending_entries)
        if (entry.army_id == original.army->id)
          fail("FieldBoundaryArmyAlreadyPending", original.army->id);
      diplomacy = take(read_current_field_reinforcement_diplomacy(
          relations, session.battle_id, original.army->id, b));
    }
    const auto contact_id = take(common.next_id("field_battle_contact"));


    const auto relation_count =
        diplomacy.actual_relation_sources.size() +
        (diplomacy.same_faction_formation_source ? 1 : 0);


    if (diplomacy.same_faction_formation_source) {
      const auto &ref = *diplomacy.same_faction_formation_source;


    }

    (owner).grow(0);
    FieldBattleBoundaryRecord record;
    record.contact_id = contact_id;
    record.battle_id = session.battle_id;
    record.source_tick = hit.occurred_at.half_day_tick;
    record.time_num = record.time_den = 0; // no fake legacy contact ratio
    record.motion = FieldExactBoundaryOrigin{source};
    record.entry_point_dm = original.entry_point_dm;
    if (original.army)
      record.army_id = original.army->id;
    record.side = diplomacy.side;
    record.participant_actor_ids = original.basis->actual_participants;
    record.diplomacy_sources = std::move(diplomacy.actual_relation_sources);
    if (diplomacy.same_faction_formation_source)
      record.diplomacy_sources.push_back(
          *diplomacy.same_faction_formation_source);
    if (record.side) {
      // Explicit fresh replacement, so the reserved vector layout matches its
      // actual owned payload; no guessed growth capacity or hidden extra Army.
      std::vector<FieldPendingEntry> pending;
      pending.reserve(session.pending_entries.size() + 1);
      for (const auto &old : session.pending_entries)
        pending.push_back(old);
      pending.push_back({*record.army_id, contact_id, *record.side,
                         original.entry, original.entry_point_dm});
      session.pending_entries = std::move(pending);
    }
    world.boundary_contacts.emplace(contact_id, std::move(record));
    take(common.append_fact({contact_id, "field_battle_contact", "military"}));
    return contact_id;
  });
}
Status validate_field_exact_boundary_contacts(
    const FieldExactContactReadContext &x,
    const PoliticalRelationsReadContext &relations,
    const FieldBattleCatalog &catalog, ExactArithmeticBudget b) {
  return status([&] {
    if (&relations.common != &x.common || &relations.field != &x.field ||
        &relations.military != &x.military ||
        &relations.formations != &x.formations)
      fail("FieldBoundarySameCandidateRequired");
    for (const auto &[id, row] : x.field.boundary_contacts) {
      const auto *origin = std::get_if<FieldExactBoundaryOrigin>(&row.motion);
      if (!origin)
        fail("FieldBoundaryLegacyOriginalProducerRequired", id);
      const auto actual = take(read_field_boundary_original_source(
          x, catalog, origin->actual_contact, b));
      const auto &hit = *actual.contact;
      const SourceRef source{"military", "field_battle_contact", id};
      const auto cut = take(read_actual_common_fact_position(x.common, source));
      const auto closed = take(read_actual_common_fact_position(
          x.common, {"travel", "movement_unit_close", actual.closed->id}));
      if (row.contact_id != id ||
          row.battle_id != hit.original_field_source.id ||
          row.source_tick != hit.occurred_at.half_day_tick || row.time_num ||
          row.time_den || closed >= cut ||
          row.entry_point_dm.x != actual.entry_point_dm.x ||
          row.entry_point_dm.y != actual.entry_point_dm.y ||
          row.participant_actor_ids != actual.basis->actual_participants ||
          bool(row.army_id) != bool(actual.army) ||
          (actual.army && *row.army_id != actual.army->id))
        fail("FieldBoundaryOriginalRecordMismatch", id);
      FieldReinforcementDiplomacyRead relation;
      if (actual.army)
        relation = take(read_field_reinforcement_diplomacy_before_contact(
            relations, source, b));
      if (row.side != relation.side ||
          row.diplomacy_sources.size() !=
              relation.actual_relation_sources.size() +
                  (relation.same_faction_formation_source ? 1 : 0))
        fail("FieldBoundaryOriginalRelationMismatch", id);
      std::size_t relation_index = 0;
      for (const auto &ref : relation.actual_relation_sources)
        if (!same_source(row.diplomacy_sources[relation_index++], ref))
          fail("FieldBoundaryOriginalRelationSourceMismatch", id);
      if (relation.same_faction_formation_source &&
          !same_source(row.diplomacy_sources[relation_index],
                       *relation.same_faction_formation_source))
        fail("FieldBoundaryOriginalIdentitySourceMismatch", id);
      const auto session = x.field.sessions.find(row.battle_id);
      if (session == x.field.sessions.end())
        fail("FieldBoundarySessionRequired", id);
      Integer pending = 0;
      for (const auto &entry : session->second.pending_entries) {
        if (entry.contact_id != id)
          continue;
        ++pending;
        if (!row.army_id || !row.side || entry.army_id != *row.army_id ||
            entry.side != *row.side || entry.entry.tick != actual.entry.tick ||
            entry.entry.t != actual.entry.t ||
            entry.entry_point_dm.x != row.entry_point_dm.x ||
            entry.entry_point_dm.y != row.entry_point_dm.y)
          fail("FieldBoundaryPendingOriginalMismatch", id);
      }
      if (row.status == FieldBoundaryStatus::Stopped) {
        const auto expected =
            row.side && session->second.state != FieldBattleState::Ended ? 1
                                                                         : 0;
        if (pending != expected || row.entered_at || row.released_at ||
            row.entry_application_source || row.entered_actual_at ||
            row.entered_observed_tick || row.entered_binding ||
            row.exact_continuation_source)
          fail("FieldBoundaryStoppedStateMismatch", id);
      } else if (row.status == FieldBoundaryStatus::Entered) {
        // The first actual entry does not consume the remaining 31ef queue.
        // Its original due stays unchanged until the last formation enters;
        // terminal invalidation removes only that active pending reference.
        if (!row.reinforcement_queue ||
            row.reinforcement_queue->admitted.empty() ||
            row.reinforcement_queue->admitted.size() >
                row.reinforcement_queue->formation_ids.size())
          fail("FieldBoundaryEnteredQueueRequired", id);
        const auto &queue = *row.reinforcement_queue;
        // 完整边界冷验也核双时刻，不依赖别的 validator 恰好先运行。
        take(validate_field_reinforcement_application_time(
            x, actual, actual.entry, queue.initialized_at, catalog.parameters,
            false, b, &queue.initialized_source));
        for (const auto &admission : queue.admitted)
          take(validate_field_reinforcement_application_time(
              x, actual, admission.at, admission.applied_at, catalog.parameters,
              false, b, &admission.application_source));
        const auto first = session->second.formations.find(
            queue.admitted.front().formation_id);
        if (first == session->second.formations.end() ||
            take(read_field_reinforcement_admission_identity(
                x.common, x.field, session->second, first->second)) !=
                &queue.admitted.front())
          fail("FieldBoundaryFirstAdmissionRequired", id);
        const Integer expected_pending =
            session->second.state != FieldBattleState::Ended &&
                    queue.admitted.size() < queue.formation_ids.size()
                ? 1
                : 0;
        if (pending != expected_pending || !row.side || !row.army_id ||
            !row.entered_at || !row.entered_actual_at ||
            !row.entered_observed_tick || !row.entry_application_source ||
            !row.entered_binding || row.released_at ||
            row.exact_continuation_source ||
            row.entered_at->tick != actual.entry.tick ||
            row.entered_at->t != actual.entry.t ||
            *row.entered_observed_tick !=
                queue.admitted.front().applied_at.half_day_tick ||
            !same_moment(*row.entered_actual_at,
                         queue.admitted.front().applied_at, b) ||
            !same_moment(queue.initialized_at,
                         queue.admitted.front().applied_at, b) ||
            row.entry_application_source->domain != "military" ||
            row.entry_application_source->kind != "field_battle_entered" ||
            take(read_actual_common_fact_position(
                x.common, *row.entry_application_source)) <= cut)
          fail("FieldBoundaryEntryApplicationRequired", id);
        // 已入场且尚未采用续程者必须仍有本场真实active绑定；不能裸删表解除hold。
        const auto active = x.field.army_bindings.find(*row.army_id);
        const auto *active_source = active == x.field.army_bindings.end()
            ? nullptr : std::get_if<FieldBoundaryContactBinding>(&active->second.source);
        if (!active_source || active_source->contact_id != id ||
            active->second.army_id != *row.army_id || active->second.battle_id != row.battle_id)
          fail("FieldBoundaryEnteredActiveBindingRequired", id);
        // Full actual formation/ability/member bindings are also checked by
        // the Field world validator; this reader never supplies those bodies.
      } else if (row.status == FieldBoundaryStatus::Released) {
        if (pending || !row.exact_continuation_source)
          fail("FieldBoundaryActualContinuationSourceRequired", id);
        const auto resumed = take(read_movement_boundary_continuation_source(
            x, relations, catalog, *row.exact_continuation_source, b));
        if (!same_source(resumed.continuation->field_contact, source))
          fail("FieldBoundaryContinuationOwnerMismatch", id);
      } else {
        fail("FieldBoundaryActualContinuationSourceRequired", id);
      }
      Integer matches = 0;
      for (const auto &[other_id, other] : x.field.boundary_contacts) {
        const auto *other_origin =
            std::get_if<FieldExactBoundaryOrigin>(&other.motion);
        if (other_origin &&
            same_source(other_origin->actual_contact, origin->actual_contact))
          ++matches;
      }
      if (matches != 1)
        fail("FieldBoundarySourceAdoptedMultipleTimes", id);
    }
    for (const auto &item : x.movement.boundary_contacts.rows()) {
      Integer adopted = 0;
      for (const auto &[id, row] : x.field.boundary_contacts) {
        const auto *origin = std::get_if<FieldExactBoundaryOrigin>(&row.motion);
        if (origin && origin->actual_contact.domain == "travel" &&
            origin->actual_contact.kind == "movement_boundary_contact" &&
            origin->actual_contact.id == item.first)
          ++adopted;
      }
      if (adopted != 1)
        fail("MovementBoundaryMissingFieldAdoption", item.first);
    }
  });
}
} // namespace hanmo::resident
