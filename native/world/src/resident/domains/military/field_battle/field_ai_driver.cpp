#include "field_ai_driver.hpp"
#include "field_author_data.hpp"
#include "field_command_source.hpp"
#include "field_contacts.hpp"
#include "field_deployment_commands.hpp"
#include "field_integer_geometry.hpp"
#include "field_local_control_loss.hpp"
#include "field_view.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <set>
#include <tuple>
namespace hanmo::resident {
Status apply_field_ai_missions_in_candidate(
    CommonStore &common, const MilitaryState &military,
    const MilitaryCatalog &military_catalog, const TravelCatalog &travel,
    const FieldBattleCatalog &catalog, FieldBattleWorldState &world,
    const StableId &battle_id, const GeneratedBattleField &field,
    const FieldBattleAbilityReadContext &abilities,
    const FieldBattleWindowSlice &step, CheckpointMemoryCharge &owner) {
  auto found = world.sessions.find(battle_id);
  if (found == world.sessions.end())
    return common_failure("FieldBattleNotFound", battle_id);
  auto &session = found->second;
  if (step.steps != 1 || session.now.tick != step.from.tick ||
      session.now.t != step.from.t || field.field_hash != session.field_hash)
    return common_failure("FieldAiStepSourceRequired", battle_id);
  for (const auto &[id, formation] : session.formations)
    if (formation.pending_direct)
      return common_failure("FieldAiArrivalBoundaryRequired", id);
  for (const auto &[id, order] : session.orders)
    if (order.status == FieldOrderStatus::InTransit &&
        std::tie(order.due.tick, order.due.t) <
            std::tie(step.to.tick, step.to.t))
      return common_failure("FieldAiArrivalBoundaryRequired", id);
  for (Integer side = 0; side < 2; ++side) {
    if (const auto *player = std::get_if<FieldPlayerController>(
            &session.sides[side].controller)) {
      auto local = read_field_original_player_local_loss(
          common, world, session, side, player->actor_id);
      if (!local.ok)
        return {false, {}, local.error};
      if (local.value)
        session.sides[side].controller = FieldAiController{};
    }

    if (!std::holds_alternative<FieldAiController>(
            session.sides[side].controller))
      continue;
    const auto actor = session.sides[side].commander_actor_id;
    FieldBattleRead request{battle_id, field.field_hash, 0};
    auto view = read_field_battle_candidate_view(
        common, military, travel, catalog, world, actor, request, abilities);
    if (!view.ok)
      return {false, {}, view.error};
    auto decision =
        read_field_ai_mission_decisions(session, view.value, field, side);
    if (!decision.ok)
      return {false, {}, decision.error};
    if (!decision.value.evaluated)
      continue;
    for (const auto &chosen : decision.value.missions) {
      auto &formation = session.formations.at(chosen.formation_id);
      // A living player-led reinforcement remains player-owned even after
      // its first Order; actual prior player loss permits the original AI
      // continuation, never a today's-controller flag alone.
      auto permission = read_field_current_ai_formation_permission(
          common, world, session, formation, abilities.original_life);
      if (!permission.ok)
        return {false, {}, permission.error};
      if (!permission.value)
        continue;
      const auto *direct = std::get_if<FieldDirectControl>(&formation.control);
      FieldDeploymentCommand command;
      if (direct && direct->controller_actor_id == actor)
        command = FieldBattleDirect{battle_id, chosen.formation_id,
                                    formation.mission_revision, chosen.mission};
      else
        command =
            FieldBattleOrder{battle_id, chosen.formation_id, chosen.mission};
      FieldBattleMutationResult result;
      auto written = apply_field_battle_control_command_in_candidate(
          common, military, military_catalog, travel, catalog, world, actor, 0,
          command, FieldControlCommandOrigin::Ai, abilities, result);
      if (!written.ok)
        return written;
      if (std::holds_alternative<FieldBattleDirect>(command)) {
        if (!formation.pending_direct || !formation.pending_direct->ai_issued ||
            !result.council_fact_id ||
            formation.pending_direct->ai_source_id != result.council_fact_id)
          return common_failure("FieldAiDirectSourceRequired",
                                chosen.formation_id);
        formation.pending_direct->ai_pursuit = chosen.pursuit;
        auto fact = world.council_facts.find(*result.council_fact_id);
        if (fact == world.council_facts.end() ||
            fact->second.kind != FieldCouncilFactKind::AiOrder)
          return common_failure("FieldAiDirectSourceRequired",
                                chosen.formation_id);
        fact->second.ai_pursuit = chosen.pursuit;
      } else {
        if (!result.order_id || !session.orders.contains(*result.order_id))
          return common_failure("FieldAiOrderSourceRequired",
                                chosen.formation_id);
        session.orders.at(*result.order_id).ai_pursuit = chosen.pursuit;
        if (!result.council_fact_id ||
            !world.council_facts.contains(*result.council_fact_id))
          return common_failure("FieldAiOrderSourceRequired",
                                chosen.formation_id);
        world.council_facts.at(*result.council_fact_id).ai_pursuit =
            chosen.pursuit;
      }
    }
    // §13.3 uses the same decision boundary and only visible enemy contacts.
    // Eligibility is from each original learned tactic; effects/cost/cooldown
    // remain the original command writer's responsibility.
    auto contacts = read_field_attack_contacts(session, field);
    if (!contacts.ok)
      return {false, {}, contacts.error};
    for (const auto &visible : view.value.formations) {
      if (visible.side != side || visible.fit <= 0 ||
          visible.state == "Withdrawn" || visible.state == "Destroyed" ||
          visible.state == "Deploying")
        continue;
      const auto &formation = session.formations.at(visible.formation_id);
      auto permission = read_field_current_ai_formation_permission(
          common, world, session, formation, abilities.original_life);
      if (!permission.ok)
        return {false, {}, permission.error};
      if (!permission.value || !formation.ability)
        continue;
      bool near_target = false, ranged_target = false;
      std::set<StableId> melee_enemies;
      for (const auto &enemy : view.value.formations) {
        if (enemy.side == side || enemy.fit <= 0 ||
            enemy.state == "Withdrawn" || enemy.state == "Destroyed" ||
            enemy.state == "Deploying")
          continue;
        if (visible.mission.target_formation_id == enemy.formation_id &&
            field_distance_squared(visible.position_dm, enemy.position_dm) <=
                __int128_t(1500) * 1500)
          near_target = true;
        for (const auto &contact : contacts.value.attacks) {
          if (contact.attacker_id == visible.formation_id &&
              contact.target_id == enemy.formation_id && contact.ranged)
            ranged_target = true;
          if (!contact.ranged &&
              ((contact.attacker_id == visible.formation_id &&
                contact.target_id == enemy.formation_id) ||
               (contact.target_id == visible.formation_id &&
                contact.attacker_id == enemy.formation_id)))
            melee_enemies.insert(enemy.formation_id);
        }
      }
      for (const auto &ready : formation.ability->tactics) {
        const auto kind = ready.source.kind;
        const bool triggered =
            (kind == FieldTacticKind::Charge && visible.category == "Cavalry" &&
             near_target) ||
            (kind == FieldTacticKind::Volley && visible.category == "Archer" &&
             ranged_target) ||
            (kind == FieldTacticKind::Rally && formation.morale < 40) ||
            (kind == FieldTacticKind::Guard && visible.category == "Infantry" &&
             melee_enemies.size() >= 2);
        if (!triggered || formation.ability->points < ready.source.point_cost ||
            (formation.state == FieldFormationState::Routing &&
             kind != FieldTacticKind::Rally) ||
            (ready.ready_at &&
             std::tie(session.now.tick, session.now.t) <
                 std::tie(ready.ready_at->tick, ready.ready_at->t)))
          continue;
        // A genuine in-flight tactic order is not issued twice at each eval.
        const bool pending = std::any_of(
            session.orders.begin(), session.orders.end(), [&](const auto &row) {
              const auto *intent =
                  std::get_if<FieldTacticIntent>(&row.second.payload);
              return row.second.status == FieldOrderStatus::InTransit &&
                     row.second.target_formation_id == visible.formation_id &&
                     intent && intent->tactic_id == ready.source.tactic_id;
            });
        if (pending)
          continue;
        FieldBattleMutationResult result;
        auto written = apply_field_battle_tactic_command_in_candidate(
            common, military, military_catalog, travel, catalog, world, actor,
            FieldBattleTactic{battle_id, visible.formation_id,
                              ready.source.tactic_id},
            FieldControlCommandOrigin::Ai, abilities, owner, result);
        if (!written.ok)
          return written;
      }
    }
    session.ai[side].last_eval = session.now;
    session.ai[side].pending_triggers.clear();
  }
  // 本步 AI 的本部直控令在移动前采用；其它编队原传令有真实正延迟，不提前采用。
  return apply_field_control_arrivals_in_candidate(
      common, world, battle_id, catalog.parameters, step, owner,
      abilities.original_life, abilities.actual_clock,
      abilities.original_read_boundary);
}
} // namespace hanmo::resident
