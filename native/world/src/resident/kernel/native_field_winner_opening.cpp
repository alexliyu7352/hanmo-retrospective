#include "native_field_winner_opening.hpp"
#include "exact_complete_world.hpp"
#include "native_complete_world_sources.hpp"
#include "native_field_group_world.hpp"
#include "native_field_world_scope.hpp"
#include "resident/domains/military/field_battle/field_winner_continuation.hpp"
#include "resident/domains/military/field_battle/field_binding_source.hpp"
#include "resident/domains/military/field_battle/field_army_exit_selection.hpp"
#include "resident/domains/travel/group_movement.hpp"
#include "resident/domains/travel/movement_boundary_resume.hpp"
#include "world_bundle.hpp"
#include "world_read_stamp.hpp"
#include <new>
#include <stdexcept>
#include <vector>
namespace hanmo::resident {
namespace {
// 原错误完整穿过中央候选，不用静默跳过不可证明的续行。
struct Failure {
  RuleError error;
};
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
void require(bool value, const char *code) {
  if (!value)
    throw Failure{common_failure(code, {}, ErrorKind::internal).error};
}
} // namespace
Status resume_native_field_winners_at_opening_in_candidate(
    WorldBundle &w, ExactPhaseWriteContext &writes) {
  try {
    require(w.catalog && w.catalog->field_battle && w.field_battle &&
                w.world_timeline && w.read_stamps &&
                w.active_exact_write_context() == &writes &&
                !writes.actual_advance_span() &&
                w.common.actor_condition_capture_identity() &&
                w.common.actor_location_capture_identity() &&
                w.world_timeline->boundary_state == PeriodBoundaryState::open,
            "FieldWinnerActualOpeningCandidateRequired");
    const auto at = take(w.actual_phase_moment());
    require(at.half_day_tick == w.common.tick() &&
                take(compare_fractions(at.sub_half_day, CanonicalFraction{})) ==
                    0,
            "FieldWinnerActualIntegerOpeningRequired");
    // 只收真实仍占本场、且尚有在场胜方编队的 Army；败方与已退出者不续行。
    std::vector<std::pair<StableId, StableId>> pending;
    for (const auto &[army, binding] : w.field_battle->army_bindings) {
      const auto found = w.field_battle->sessions.find(binding.battle_id);
      require(found != w.field_battle->sessions.end(),
              "FieldWinnerActualSessionRequired");
      const auto &session = found->second;
      if (session.state != FieldBattleState::Ended || !session.result ||
          session.result->winner_side < 0)
        continue;
      // 显式撤退的胜军仍归真实汇合/BW09；单纯预选口不构成撤退许可。
      if (take(read_field_army_terminal_exit_required(session, army)))
        continue;
      bool retained_winner = false;
      for (const auto &[id, formation] : session.formations)
        if (formation.army_id == army &&
            formation.side == session.result->winner_side &&
            formation.state != FieldFormationState::Withdrawn &&
            formation.state != FieldFormationState::Destroyed)
          retained_winner = true;
      if (retained_winner)
        pending.emplace_back(binding.battle_id, army);
    }
    for (const auto &[battle, army] : pending) {
      // 每次真实写后重借当前根；phase/owner/group按此顺序析构，绝不借前军旧票。
      auto phase = take(w.enter_phase_read_context("field_winner_opening",
                                                   "military", {at, at}, at));
      auto owner = take(w.complete_exact_time_bindings());
      require(owner && owner->reads().original_core,
              "FieldWinnerOriginalCompleteOwnerRequired");
      auto group = take(
          NativeFieldGroupWorld::create(w, *owner->reads().original_core, &w));
      const auto *exact = group->field().exact_contacts();
      require(exact && exact->periods,
              "FieldWinnerOriginalPeriodOwnerRequired");
      ReadStampWriteBoundary boundary(*w.read_stamps);
      const auto *binding = take(read_field_original_battle_binding(
          w.common, *w.field_battle, battle, army));
      SourceRef source;
      if (const auto *boundary = std::get_if<FieldBoundaryContactBinding>(&binding->source)) {
        require(group->field().diplomacy() && exact->original_rosters,
                "FieldBoundaryWinnerOriginalOwnersRequired");
        source = take(resume_exact_group_field_boundary_in_candidate(
            w.common, *w.spatial_carriers, *w.movement_exact,
            *w.group_movement_executions, *w.field_battle, group->group(), *exact,
            *group->field().diplomacy(), *w.catalog->field_battle,
            SourceRef{"military", "field_battle_contact", boundary->contact_id},
            w.checkpoint_copy_memory));
      } else {
        source = take(resume_exact_group_field_winner_in_candidate(
            w.common, *w.spatial_carriers, *w.movement_exact,
            *w.group_movement_executions, *w.field_battle, group->group(), *exact,
            *w.catalog->field_battle, battle, army, w.checkpoint_copy_memory));
      }
      require(source.domain == "travel" &&
                  (source.kind == "movement_contact_continuation" ||
                   source.kind == "movement_boundary_continuation" ||
                   source.kind == "movement_arrival") &&
                  !source.id.empty() &&
                  take(compare_world_moments(take(w.actual_phase_moment()),
                                             at)) == 0,
              "FieldWinnerActualContinuationRequired");
    }
    return common_success();
  } catch (Failure &failure) {
    return {false, {}, std::move(failure.error)};
  } catch (const std::bad_alloc &) {
    return common_failure("FieldWinnerOpeningResourceExhausted", {},
                          ErrorKind::internal);
  } catch (const std::length_error &) {
    return common_failure("FieldWinnerOpeningResourceExhausted", {},
                          ErrorKind::internal);
  }
}
} // namespace hanmo::resident
