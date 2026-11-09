#include "field_reinforcement_closing.hpp"
#include "field_ability_sources.hpp"
#include "field_reinforcement_geometry.hpp"
#include "field_segment_settlement.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <tuple>
namespace hanmo::resident {
// 沿真实 closing 逐队列采用，失败由外层候选整体撤销，不制造任务或接触。
Outcome<FieldReinforcementClosingResult>
admit_due_field_reinforcements_at_closing_in_candidate(
    CommonStore &common, FieldBattleWorldState &world,
    const FieldExactContactReadContext &x,
    const PoliticalRelationsReadContext &relations,
    const FieldBattleCatalog &catalog,
    const FieldOriginalRosterReadContext &history,
    const FieldBattleAbilityReadContext &abilities,
    const GeneratedBattleField &field, const StableId &battle,
    CheckpointMemoryCharge &memory, ExactArithmeticBudget b) {
  return movement_detail::guard<FieldReinforcementClosingResult>([&] {
    using namespace movement_detail;
    if (&common != &x.common || &world != &x.field ||
        abilities.exact_contacts != &x || abilities.original_rosters != &history ||
        abilities.actual_clock != &x.clock || !x.clock.bound_to(common, x.military, world))
      fail("FieldReinforcementClosingSameCandidateRequired", battle);
    const auto found = world.sessions.find(battle);
    if (found == world.sessions.end() || found->second.state != FieldBattleState::Running ||
        found->second.result || found->second.field_hash != field.field_hash)
      fail("FieldReinforcementClosingRunningSessionRequired", battle);
    auto &session = found->second;
    const auto actual = take(x.clock.read_current_moment());
    if (actual.half_day_tick != common.tick() ||
        take(compare_fractions(actual.sub_half_day, CanonicalFraction{}, b)) != 0 ||
        !same_moment(actual, take(x.clock.read_field_window_closing_moment()), b))
      fail("FieldReinforcementClosingActualBoundaryRequired", battle);
    // 只复制原待入场键；最后一个编队入场会移除 pending，不能迭代失效引用。
    const auto pending_memory = CheckpointMemoryCharge(0);
    std::vector<std::pair<FieldBattleTime, StableId>> due;
    due.reserve(session.pending_entries.size());
    for (const auto &entry : session.pending_entries)
      if (std::tie(entry.entry.tick, entry.entry.t) <= std::tie(session.now.tick, session.now.t))
        due.emplace_back(entry.entry, entry.contact_id);
    std::sort(due.begin(), due.end(), [](const auto &a, const auto &b) {
      return std::tie(a.first.tick, a.first.t, a.second) <
             std::tie(b.first.tick, b.first.t, b.second);
    });
    FieldReinforcementClosingResult result;
    for (const auto &[entry, id] : due) {
      const SourceRef source{"military", "field_battle_contact", id};
      const auto boundary_it = world.boundary_contacts.find(id);
      if (boundary_it == world.boundary_contacts.end() ||
          boundary_it->second.battle_id != battle)
        fail("FieldReinforcementClosingOriginalBoundaryRequired", id);
      auto &boundary = boundary_it->second;
      if (!boundary.reinforcement_queue)
        take(initialize_field_reinforcement_queue_in_candidate(
            common, world, x, relations, catalog, history, source, memory, b));
      const auto *queue = take(read_field_original_reinforcement_queue(
          x, catalog, history, source, b));
      const auto *origin = std::get_if<FieldExactBoundaryOrigin>(&boundary.motion);
      if (!origin)
        fail("FieldReinforcementClosingOriginalBoundaryRequired", id);
      const auto original = take(read_field_boundary_original_source(x, catalog, origin->actual_contact, b));
      if (std::tie(original.entry.tick, original.entry.t) != std::tie(entry.tick, entry.t))
        fail("FieldReinforcementClosingOriginalEntryMismatch", id);
      if (!original.army)
        fail("FieldReinforcementClosingOriginalArmyRequired", id);
      const bool player = original.army->commander_actor_id == common.state().metadata.player_actor_id;
      // 原队列顺序和前圆离场门不变；被前圆挡住时，让原 STEP 移动已入场编队。
      while (queue->admitted.size() < queue->formation_ids.size()) {
        if (!queue->admitted.empty() &&
            !take(read_field_reinforcement_entry_clear(session, boundary.entry_point_dm,
                                                        queue->admitted.back().formation_id)))
          break;
        const auto count = queue->admitted.size();
        if (player)
          take(admit_next_player_field_reinforcement_in_candidate(
              common, world, x, catalog, history, abilities, field, source, memory, b));
        else
          take(admit_next_ai_field_reinforcement_in_candidate(
              common, world, x, catalog, history, abilities, field, source, memory, b));
        if (queue->admitted.size() != count + 1)
          fail("FieldReinforcementClosingAdmissionProgressRequired", id);
        if (player) {
          result.player_input_required = true;
          return result;
        }
      }
    }
    return result;
  });
}
} // namespace hanmo::resident
