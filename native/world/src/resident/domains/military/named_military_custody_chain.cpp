#include "named_military_custody_chain.hpp"
#include "military_return_source.hpp"
#include "named_military_custody_terms.hpp"
#include "naval_battle_state.hpp"
#include "occupation_rules.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
namespace hanmo::resident {
using namespace movement_detail;
namespace {
template <class T>
const T *unique(const std::vector<T> &rows, const StableId &id) {
  const T *out = nullptr;
  for (const auto &row : rows)
    if (row.id == id) {
      if (out)
        fail("NamedMilitaryCustodyDuplicatePayload", id);
      out = &row;
    }
  return out;
}
struct Event {
  std::size_t order = 0;
  const MilitaryCasualtyFact *capture = nullptr;
  const MilitaryReturnFact *returned = nullptr;
  const MilitaryOccupationFact *occupation = nullptr;
};
std::vector<Event> collect_events(const CommonStore &common,
                                  const MilitaryState &military,
                                  const StableId &actor, std::size_t begin,
                                  std::size_t end) {
  const auto position = [&](const SourceRef &source) {
    return take(read_actual_common_fact_position(common, source));
  };
  std::vector<Event> events;
  for (const auto &row : military.casualty_facts) {
    bool relevant = false;
    for (const auto &change : row.actor_changes)
      if (change.actor_id == actor && change.status == "captured") {
        if (relevant)
          fail("NamedMilitaryCustodyDuplicateCapture", row.id);
        relevant = true;
      }
    if (relevant) {
      const auto at = position({"military", "military_casualties", row.id});
      if (at >= begin && at < end)
        events.push_back({at, &row, nullptr, nullptr});
    }
  }
  for (const auto &row : military.return_facts) {
    const auto count =
        std::count(row.prisoner_ids.begin(), row.prisoner_ids.end(), actor);
    if (count > 1)
      fail("NamedMilitaryCustodyReturnSourceMismatch", row.id);
    if (count) {
      const auto at = position({"military", "military_return", row.id});
      if (at >= begin && at < end)
        events.push_back({at, nullptr, &row, nullptr});
    }
  }
  for (const auto &row : military.occupation_facts)
    if (row.custody_transfers) {
      Integer count = 0;
      for (const auto &transfer : *row.custody_transfers)
        if (transfer.person_id == actor)
          ++count;
      if (count > 1)
        fail("NamedMilitaryCustodyOccupationTransferRepeated", row.id);
      if (count) {
        const auto at = position({"military", "military_occupation", row.id});
        if (at >= begin && at < end)
          events.push_back({at, nullptr, nullptr, &row});
      }
    }
  std::sort(events.begin(), events.end(),
            [](const Event &a, const Event &b) { return a.order < b.order; });
  return events;
}

} // namespace
Outcome<OriginalNamedMilitaryCustodyAtRelease>
read_named_military_custody_before_release(
    const NamedMilitaryCustodyContext &context,
    const NamedMilitaryCustodyRelease &release) {
  return guard<OriginalNamedMilitaryCustodyAtRelease>([&] {
    const auto &common = context.common;
    const auto &military = context.military;
    if (unique(military.named_custody.closures, release.id) != &release)
      fail("NamedMilitaryCustodyActualReleaseRequired", release.id);
    const auto *term = unique(military.named_custody.terms, release.term_id);
    if (!term || term->actor_id != release.actor_id)
      fail("NamedMilitaryCustodyReleaseTermRequired", release.id);
    if (release.recorded_tick != release.released_at.half_day_tick ||
        take(compare_world_moments(release.released_at, term->due_at)) != 0)
      fail("NamedMilitaryCustodyActualDeadlineRequired", release.id);
    const auto position = [&](const SourceRef &source) {
      return take(read_actual_common_fact_position(common, source));
    };
    const auto begin = position(term->capture_source);
    const auto end =
        position({"military", "military_named_custody_released", release.id});
    if (begin >= end)
      fail("NamedMilitaryCustodyReleaseOrderMismatch", release.id);
    auto events =
        collect_events(common, military, release.actor_id, begin, end);
    OriginalNamedMilitaryCustodyAtRelease out;
    out.term = term;
    Integer previous_tick = term->capture_tick;
    std::size_t previous = 0;
    bool started = false;
    for (const auto &event : events) {
      if (started && event.order <= previous)
        fail("NamedMilitaryCustodyTransferOrderMismatch", release.id);
      if (const auto *capture = event.capture) {
        const SourceRef source{"military", "military_casualties", capture->id};
        const auto original = take(read_original_named_military_capture(
            context, source, release.actor_id));
        if (capture->tick < previous_tick ||
            capture->tick > release.recorded_tick)
          fail("NamedMilitaryCustodyTransferClockMismatch", capture->id);
        if (!started) {
          if (capture->id != term->capture_source.id ||
              original.transfers_existing_custody ||
              original.captured_army->id != term->captured_army_id ||
              original.captured_army->organization_id !=
                  term->captured_organization_id)
            fail("NamedMilitaryCustodyInitialCaptureRequired", capture->id);
        } else if (!original.transfers_existing_custody || out.garrison ||
                   original.change->army_id != out.keeper_army->id)
          fail("NamedMilitaryCustodyOriginalTransferRequired", capture->id);
        out.keeper_army =
            unique(military.armies, original.change->captor_army_id);
        if (!out.keeper_army)
          fail("NamedMilitaryCustodyCaptorRequired", capture->id);
        out.actual_capture = capture;
        out.garrison = nullptr;
        out.last_custody_source = source;
        const auto *head = common.state().facts.find(capture->battle_id);
        if (head && head->domain == "military_naval" &&
            head->kind == "naval_capture")
          out.last_custody_source = {head->domain, head->kind, head->id};
        previous_tick = capture->tick;
        started = true;
      } else if (const auto *returned = event.returned) {
        const auto *garrison =
            unique(context.catalog.garrisons, returned->garrison_id);
        if (!started || out.garrison || !garrison ||
            returned->army_id != out.keeper_army->id ||
            returned->tick < previous_tick ||
            returned->tick > release.recorded_tick ||
            out.keeper_army->status != "closed" ||
            out.keeper_army->return_garrison_id.value_or(
                out.keeper_army->garrison_id) != garrison->id)
          fail("NamedMilitaryCustodyReturnSourceMismatch", returned->id);
        out.garrison = garrison;
        out.last_custody_source = {"military", "military_return", returned->id};
        previous_tick = returned->tick;
      } else {
        const auto &occupation = *event.occupation;
        if (!context.access)
          fail("NamedMilitaryCustodyOccupationReadRequired", occupation.id);
        take(validate_applied_occupation_manifest(
            common, military, context.catalog, *context.access, occupation.id));
        const OccupationCustodyTransfer *transfer = nullptr;
        for (const auto &row : *occupation.custody_transfers)
          if (row.person_id == release.actor_id)
            transfer = &row;
        if (!started || !out.garrison ||
            out.garrison->id != occupation.garrison_id || !transfer ||
            transfer->from_army_id != out.keeper_army->id ||
            transfer->to_army_id != occupation.army_id ||
            transfer->from_army_id == transfer->to_army_id ||
            occupation.tick < previous_tick ||
            occupation.tick > release.recorded_tick)
          fail("NamedMilitaryCustodyOccupationTransferMismatch", occupation.id);
        out.keeper_army = unique(military.armies, transfer->to_army_id);
        if (!out.keeper_army)
          fail("NamedMilitaryCustodyCaptorRequired", occupation.id);
        out.garrison = nullptr;
        out.last_custody_source = {"military", "military_occupation",
                                   occupation.id};
        previous_tick = occupation.tick;
      }
      previous = event.order;
    }
    if (!started || !out.keeper_army ||
        out.keeper_army->id != release.keeper_army_id ||
        out.keeper_army->organization_id != release.keeper_organization_id ||
        !same_source(out.last_custody_source, release.prior_custody_source))
      fail("NamedMilitaryCustodyReleaseKeeperMismatch", release.id);
    return out;
  });
}
Outcome<OriginalNamedMilitaryCustodyAtRelease>
read_named_military_custody_at_original_prefix(
    const NamedMilitaryCustodyContext &context,
    const NamedMilitaryCustodyTerm &term, const WorldMoment &at,
    const CommonActualFactPrefix &prefix,
    const MilitaryReturnSourceRead *returns) {
  return guard<OriginalNamedMilitaryCustodyAtRelease>([&] {
    const auto &common = context.common;
    const auto &military = context.military;
    if (!context.clock || at.half_day_tick < 0 ||
        take(read_original_named_military_custody_term(
            context, {"military", "military_named_custody_term", term.id})) !=
            &term)
      fail("NamedMilitaryCustodyActualTermRequired", term.id);
    const auto now = take(context.clock->read_current_moment());
    if (take(compare_world_moments(at, now)) > 0)
      fail("NamedMilitaryCustodyCarrierFutureObservation", term.id);
    const auto cut = take(read_actual_common_fact_prefix(common, prefix));
    const auto position = [&](const SourceRef &source) {
      return take(read_actual_common_fact_position(common, source));
    };
    const auto visible = [&](const WorldMoment &when, std::size_t order) {
      const auto cmp = take(compare_world_moments(when, at));
      if (order < cut) {
        if (cmp > 0)
          fail("NamedMilitaryCustodyCarrierCausalMismatch", term.id);
        return true;
      }
      if (cmp < 0)
        fail("NamedMilitaryCustodyCarrierOmittedEarlierSource", term.id);
      return false;
    };
    if (!visible(
            term.adopted_at,
            position({"military", "military_named_custody_term", term.id})))
      fail("NamedMilitaryCustodyTermOutsideObservation", term.id);
    for (const auto &release : military.named_custody.closures) {
      if (release.term_id != term.id)
        continue;
      if (release.actor_id != term.actor_id ||
          take(compare_world_moments(release.released_at, term.due_at)) != 0)
        fail("NamedMilitaryCustodyActualDeadlineRequired", release.id);
      if (visible(release.released_at,
                  position({"military", "military_named_custody_released",
                            release.id})))
        fail("NamedMilitaryCustodyTermClosedAtObservation", term.id);
    }
    const auto begin = position(term.capture_source);
    const auto current = take(read_current_common_fact_prefix(common));
    const auto events = collect_events(common, military, term.actor_id, begin,
                                       current.common_fact_count);
    OriginalNamedMilitaryCustodyAtRelease out;
    out.term = &term;
    bool started = false;
    std::size_t previous = 0;
    WorldMoment previous_at = term.adopted_at;
    for (const auto &event : events) {
      if (const auto *capture = event.capture) {
        const SourceRef source{"military", "military_casualties", capture->id};
        const auto original = take(read_original_named_military_capture(
            context, source, term.actor_id));
        if (!original.applied_at)
          fail("NamedMilitaryCustodyExactCaptureMomentRequired", capture->id);
        if (!visible(*original.applied_at, event.order))
          continue;
        if (started && (event.order <= previous ||
                        take(compare_world_moments(*original.applied_at,
                                                   previous_at)) < 0))
          fail("NamedMilitaryCustodyTransferOrderMismatch", capture->id);
        if (!started) {
          if (capture->id != term.capture_source.id ||
              original.transfers_existing_custody ||
              original.captured_army->id != term.captured_army_id ||
              original.captured_army->organization_id !=
                  term.captured_organization_id)
            fail("NamedMilitaryCustodyInitialCaptureRequired", capture->id);
        } else if (!original.transfers_existing_custody || out.garrison ||
                   original.change->army_id != out.keeper_army->id) {
          fail("NamedMilitaryCustodyOriginalTransferRequired", capture->id);
        }
        out.keeper_army =
            unique(military.armies, original.change->captor_army_id);
        if (!out.keeper_army)
          fail("NamedMilitaryCustodyCaptorRequired", capture->id);
        out.actual_capture = capture;
        out.garrison = nullptr;
        out.last_custody_source = source;
        previous_at = *original.applied_at;
        previous = event.order;
        started = true;
      } else if (const auto *returned = event.returned) {
        if (!returns || !returns->bound_to(common, military))
          fail("NamedMilitaryCustodyExactReturnPositionRequired", returned->id);
        const auto exact =
            take(returns->read_exact_army_return(returned->army_id));
        if (!exact || !exact->original || exact->original->id != returned->id ||
            exact->original->garrison_id != returned->garrison_id ||
            std::count(exact->original->prisoner_ids.begin(),
                       exact->original->prisoner_ids.end(),
                       term.actor_id) != 1 ||
            exact->actual_application.source_schema != 2)
          fail("NamedMilitaryCustodyExactReturnPositionRequired", returned->id);
        const bool receipt_visible =
            visible(exact->original->completed_at, event.order);
        const bool application_visible =
            visible(exact->original->completed_at,
                    position(exact->actual_application.source_ref));
        if (!application_visible)
          continue;
        if (!receipt_visible || !started || out.garrison ||
            returned->army_id != out.keeper_army->id || event.order <= previous)
          fail("NamedMilitaryCustodyExactReturnPositionRequired", returned->id);
        if (take(compare_world_moments(exact->original->completed_at,
                                       previous_at)) < 0)
          fail("NamedMilitaryCustodyTransferOrderMismatch", returned->id);
        out.garrison = unique(context.catalog.garrisons, returned->garrison_id);
        if (!out.garrison)
          fail("NamedMilitaryCustodyCaptorRequired", returned->id);
        out.last_custody_source = exact->actual_application.source_ref;
        previous_at = exact->original->completed_at;
        previous = event.order;
      } else {
        // A later projected tick is certainly outside this observation. At
        // the same tick the exact boundary remains unknown; never invent zero.
        if (event.order >= cut && event.occupation->tick < at.half_day_tick)
          fail("NamedMilitaryCustodyCarrierOmittedEarlierSource",
               event.occupation->id);
        if (event.order < cut || event.occupation->tick <= at.half_day_tick)
          fail("NamedMilitaryCustodyExactOccupationMomentRequired",
               event.occupation->id);
      }
    }
    if (!started || !out.keeper_army)
      fail("NamedMilitaryCustodyCaptureOutsideObservation", term.id);
    return out;
  });
}
} // namespace hanmo::resident
