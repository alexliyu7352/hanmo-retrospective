#include "native_field_boundary_continuation.hpp"
#include "exact_complete_world.hpp"
#include "native_complete_world_sources.hpp"
#include "native_field_group_world.hpp"
#include "native_field_world_scope.hpp"
#include "resident/domains/military/field_battle/field_boundary_continuation_source.hpp"
#include "resident/domains/military/field_battle/field_segment_settlement.hpp"
#include "resident/domains/travel/group_movement.hpp"
#include "resident/domains/travel/movement_boundary_resume.hpp"
#include "world_bundle.hpp"
#include "world_copy_plan.hpp"
#include "world_read_stamp.hpp"
#include <new>
#include <stdexcept>
#include <vector>
namespace hanmo::resident {
namespace {
// 失败交外层候选整体丢弃；不清 entered 历史来绕过真实 continuation 条件。
struct Failure { RuleError error; };
template <class T> T take(Outcome<T> value) {
  if (!value.ok) throw Failure{std::move(value.error)};
  return std::move(value.value);
}
void require(bool ok, const char *code, const StableId &id = {}) {
  if (!ok) throw Failure{common_failure(code, id, ErrorKind::internal).error};
}
} // namespace
// 只接已有原 S1 writer；源读票随每次写重借，世界时间、军令及余路均不重造。
Status resume_native_never_admitted_field_boundaries_in_candidate(
    WorldBundle &w, ExactPhaseWriteContext &writes, bool opening) {
  try {
    require(w.catalog && w.catalog->travel && w.catalog->field_battle && w.field_battle &&
                w.world_timeline && w.read_stamps && w.movement_exact &&
                w.spatial_carriers && w.group_movement_executions &&
                w.active_exact_write_context() == &writes &&
                !writes.actual_advance_span() &&
                w.common.actor_condition_capture_identity() &&
                w.common.actor_location_capture_identity() &&
                w.world_timeline->boundary_state ==
                    (opening ? PeriodBoundaryState::open : PeriodBoundaryState::ready),
            "FieldBoundaryActualContinuationCandidateRequired");
    const auto at = take(w.actual_phase_moment());
    require(at.half_day_tick == w.common.tick() &&
                take(compare_fractions(at.sub_half_day, CanonicalFraction{})) == 0,
            "FieldBoundaryActualIntegerContinuationRequired");
    const auto pending_memory = CheckpointMemoryCharge(0);
    std::vector<StableId> pending;
    pending.reserve(w.field_battle->boundary_contacts.size());
    for (const auto &[id, row] : w.field_battle->boundary_contacts) {
      if (row.status != FieldBoundaryStatus::Stopped || (row.side && !opening)) continue;
      const auto session = w.field_battle->sessions.find(row.battle_id);
      require(session != w.field_battle->sessions.end(), "FieldBoundaryOriginalSessionRequired", id);
      if (session->second.state == FieldBattleState::Ended && session->second.result)
        pending.push_back(id);
    }
    for (const auto &id : pending) {
      auto phase = take(w.enter_phase_read_context(
          opening ? "field_boundary_opening" : "field_boundary_terminal",
          "military", {at, at}, at));
      auto owner = take(w.complete_exact_time_bindings());
      require(owner && owner->reads().original_core, "FieldBoundaryOriginalCompleteOwnerRequired", id);
      auto group = take(NativeFieldGroupWorld::create(w, *owner->reads().original_core, &w));
      auto &scope = group->field();
      const auto *exact = scope.exact_contacts();
      require(exact && exact->travel_services && scope.diplomacy(),
              "FieldBoundaryOriginalContinuationOwnersRequired", id);
      const SourceRef contact{"military", "field_battle_contact", id};
      const auto cause = take(read_field_boundary_continuation_source(
          *exact, *scope.diplomacy(), *w.catalog->field_battle, contact));
      require(cause.original.prefix &&
                  (opening || cause.timing == FieldBoundaryResumeTiming::after_terminal),
              "FieldBoundaryActualContinuationTimingRequired", id);
      ReadStampWriteBoundary boundary(*w.read_stamps);
      SourceRef source;
      if (std::holds_alternative<GroupMovementExecution>(cause.original.prefix->execution))
        source = take(resume_exact_group_field_boundary_in_candidate(
            w.common, *w.spatial_carriers, *w.movement_exact,
            *w.group_movement_executions, *w.field_battle, group->group(),
            *exact, *scope.diplomacy(), *w.catalog->field_battle,
            contact, w.checkpoint_copy_memory));
      else
        source = take(resume_exact_personal_field_boundary_in_candidate(
            w.common, w.travel, *w.movement_exact, *w.field_battle,
            *w.catalog->travel, *exact->travel_services, *exact,
            *scope.diplomacy(), *w.catalog->field_battle, contact,
            w.checkpoint_copy_memory));
      require(source.domain == "travel" && source.kind == "movement_boundary_continuation" &&
                  !source.id.empty() &&
                  take(compare_world_moments(take(w.actual_phase_moment()), at)) == 0,
              "FieldBoundaryActualContinuationRequired", id);
    }
    return common_success();
  } catch (Failure &failure) {
    return {false, {}, std::move(failure.error)};
  } catch (const std::bad_alloc &) {
    return common_failure("FieldBoundaryContinuationResourceExhausted", {}, ErrorKind::internal);
  } catch (const std::length_error &) {
    return common_failure("FieldBoundaryContinuationResourceExhausted", {}, ErrorKind::internal);
  }
}
} // namespace hanmo::resident
