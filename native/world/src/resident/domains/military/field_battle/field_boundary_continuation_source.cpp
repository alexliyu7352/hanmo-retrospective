#include "resident/domains/military/military_model.hpp"
#include "field_boundary_continuation_source.hpp"
#include "field_boundary_winner_source.hpp"
#include "field_diplomacy_source.hpp"
#include "field_segment_settlement.hpp"
#include "resident/domains/politics/relations/political_relations_read.hpp"
#include "resident/domains/travel/movement_boundary_source.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/domains/travel/spatial_carrier.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
using namespace movement_detail;
Outcome<FieldBoundaryContinuationRead> read_field_boundary_continuation_source(
    const FieldExactContactReadContext &x,
    const PoliticalRelationsReadContext &relations,
    const FieldBattleCatalog &catalog, const SourceRef &source,
    ExactArithmeticBudget b, const SourceRef *actual_continuation) {
  return guard<FieldBoundaryContinuationRead>([&] {
    if (&relations.common != &x.common || &relations.field != &x.field ||
        &relations.military != &x.military ||
        &relations.formations != &x.formations ||
        !x.clock.bound_to(x.common, x.military, x.field))
      fail("FieldBoundaryContinuationSameWorldRequired", source.id);
    const auto found = x.field.boundary_contacts.find(source.id);
    if (source.domain != "military" || source.kind != "field_battle_contact" ||
        found == x.field.boundary_contacts.end() ||
        x.field.contacts.contains(source.id))
      fail("FieldBoundaryContinuationContactRequired", source.id);
    const auto &row = found->second;
    if (row.entered_binding || row.entered_at || row.entry_application_source ||
        row.entered_actual_at || row.entered_observed_tick) {
      if (!x.original_rosters)
        fail("FieldBoundaryWinnerOriginalRosterOwnerRequired", source.id);
      const auto winner = take(read_field_boundary_winner_resume_cause(
          x, relations, catalog, *x.original_rosters, source, b, actual_continuation));
      if (winner.coverage != FieldBoundaryWinnerCoverage::fully_admitted)
        fail("FieldBoundaryWinnerPartialRendezvousRequired", source.id);
      FieldBoundaryContinuationRead out;
      out.boundary = winner.boundary;
      out.original = winner.original;
      out.terminal = winner.terminal;
      out.timing = FieldBoundaryResumeTiming::next_half_day;
      out.actual_now = winner.opening_at;
      return out;
    }

    const auto *origin = std::get_if<FieldExactBoundaryOrigin>(&row.motion);
    if (!origin || row.contact_id != source.id || row.entered_at ||
        row.entry_application_source || row.entered_binding ||
        row.entered_actual_at || row.entered_observed_tick || row.released_at)
      fail("FieldBoundaryNeverAdmittedSourceRequired", source.id);
    const MovementBoundaryContinuationSource *resumed = nullptr;
    if (actual_continuation) {
      resumed = x.movement.boundary_continuations.find(actual_continuation->id);
      if (actual_continuation->domain != "travel" ||
          actual_continuation->kind != "movement_boundary_continuation" ||
          !resumed || !same_source(resumed->field_contact, source) ||
          !same_source(resumed->boundary_contact, origin->actual_contact) ||
          row.status != FieldBoundaryStatus::Released ||
          !row.exact_continuation_source ||
          !same_source(*row.exact_continuation_source, *actual_continuation))
        fail("FieldBoundaryActualContinuationRequired", source.id);
      take(read_actual_common_fact_position(x.common, *actual_continuation));
    } else if (row.status != FieldBoundaryStatus::Stopped ||
               row.exact_continuation_source) {
      fail("FieldBoundaryNeverAdmittedSourceRequired", source.id);
    }
    FieldBoundaryContinuationRead out;
    out.boundary = &row;
    out.original = take(read_field_boundary_original_source(
        x, catalog, origin->actual_contact, b));
    const auto &hit = *out.original.contact;
    if (row.battle_id != hit.original_field_source.id || row.time_num ||
        row.time_den || row.source_tick != hit.occurred_at.half_day_tick ||
        row.participant_actor_ids != out.original.basis->actual_participants ||
        bool(row.army_id) != bool(out.original.army) ||
        (out.original.army && *row.army_id != out.original.army->id))
      fail("FieldBoundaryContinuationOriginalMismatch", source.id);
    FieldReinforcementDiplomacyRead relation;
    if (out.original.army)
      relation = take(read_field_reinforcement_diplomacy_before_contact(
          relations, source, b));
    if (relation.side != row.side ||
        row.diplomacy_sources.size() !=
            relation.actual_relation_sources.size() +
                (relation.same_faction_formation_source ? 1 : 0))
      fail("FieldBoundaryContinuationRelationMismatch", source.id);
    std::size_t index = 0;
    for (const auto &ref : relation.actual_relation_sources)
      if (!same_source(row.diplomacy_sources[index++], ref))
        fail("FieldBoundaryContinuationRelationSourceMismatch", source.id);
    if (relation.same_faction_formation_source &&
        !same_source(row.diplomacy_sources[index],
                     *relation.same_faction_formation_source))
      fail("FieldBoundaryContinuationIdentitySourceMismatch", source.id);
    const auto session = x.field.sessions.find(row.battle_id);
    if (session == x.field.sessions.end() ||
        session->second.state != FieldBattleState::Ended ||
        !session->second.result)
      fail("FieldBoundaryBattleStillActive", source.id);
    for (const auto &entry : session->second.pending_entries)
      if (entry.contact_id == source.id)
        fail("FieldBoundaryPendingEntryNotInvalidated", source.id);
    const auto contact_head =
        take(read_actual_common_fact_position(x.common, source));
    for (const auto &settlement : session->second.settlements) {
      if (!settlement.battle_fact_id)
        continue;
      if (out.terminal)
        fail("FieldBoundaryTerminalSourceAmbiguous", source.id);
      const SourceRef terminal{"military", "military_battle",
                               *settlement.battle_fact_id};
      const auto verified = take(read_field_segment_settlement_source(
          x.common, x.military, x.field,
          SourceRef{"military", "field_battle_segment", settlement.fact_id},
          &x.clock));
      if (verified.session != &session->second ||
          verified.settlement != &settlement || !verified.terminal ||
          take(read_actual_common_fact_position(x.common, terminal)) <=
              contact_head)
        fail("FieldBoundaryTerminalSourceMismatch", source.id);
      out.terminal = &settlement;
    }
    if (!out.terminal)
      fail("FieldBoundaryActualTerminalRequired", source.id);
    const auto clock_now = take(x.clock.read_current_moment());
    out.actual_now = resumed ? resumed->occurred_at : clock_now;
    if (take(compare_world_moments(out.actual_now, clock_now, b)) > 0)
      fail("FieldBoundaryContinuationInFuture", source.id);
    if (resumed &&
        (!same_source(resumed->terminal_segment,
                      SourceRef{"military", "field_battle_segment",
                                out.terminal->fact_id}) ||
         take(read_actual_common_fact_position(x.common,
                                               *actual_continuation)) <=
             take(read_actual_common_fact_position(
                 x.common, SourceRef{"military", "military_battle",
                                     *out.terminal->battle_fact_id}))))
      fail("FieldBoundaryContinuationTerminalMismatch", source.id);
    if (take(compare_world_moments(out.terminal->applied_at, out.actual_now,
                                   b)) > 0)
      fail("FieldBoundaryTerminalInFuture", source.id);
    out.timing = row.side ? FieldBoundaryResumeTiming::next_half_day
                          : FieldBoundaryResumeTiming::after_terminal;
    // §4.4 is only the invalidated allied PendingEntry. §4.5 third parties
    // resume after actual terminal, without inventing an extra half-day wait.
    // Read the actual clock; this predicate neither constructs nor advances M.
    if (out.timing == FieldBoundaryResumeTiming::next_half_day) {
      // Original segment application is the real closing boundary. A Field
      // window may already normalize session.now into the following tick;
      // adding another tick to that local value would delay an extra window.
      // Legacy non-integral applications still wait for their next real tick.
      const auto &ended = out.terminal->applied_at;
      if (out.actual_now.half_day_tick < ended.half_day_tick ||
          (compare(ended.sub_half_day, {}, b) != 0 &&
           out.actual_now.half_day_tick == ended.half_day_tick))
        fail("FieldBoundaryOriginalNextHalfDayNotReached", source.id);
    }
    return out;
  });
}
} // namespace hanmo::resident
