#include "field_event_legacy_kinds.hpp"
#include "field_event_mission_transition.hpp"
#include "field_event_navigation_observation.hpp"
#include "field_event_motion_application.hpp"
#include "field_event_mission_application.hpp"
#include "field_event_contact_phase_from.hpp"
#include "field_control_acceptance.hpp"
#include "field_army_exit_selection.hpp"
#include "field_initial_placement_source.hpp"
#include "field_mission_transition.hpp"
#include "field_order_resolution.hpp"
#include <algorithm>
#include <limits>
#include <map>
#include <set>
// 同一候选在原 result/history 已存在后才绑定；读/冷恢复绝不补造出处。
#include "field_binding_source.hpp"
#include "field_command_source.hpp"
#include "field_local_control_loss.hpp"
#include "field_reinforcement_queue.hpp"
#include "field_result_codec.hpp"
#include "field_validation.hpp"
#include "resident/domains/family/family_life_history4.hpp"
#include "resident/kernel/command_types.hpp"
#include "resident/kernel/common_command_sources.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/common_store.hpp"
#include <charconv>
#include <tuple>
namespace hanmo::resident {
namespace {
Status invalid(const StableId &id) {
  return common_failure("FieldCommandSourceMismatch", id);
}
Outcome<const FieldBattleMutationResult *>
mutation(const CommitResult &receipt) {
  const auto *field =
      receipt.details ? std::get_if<FieldBattleReceipt>(&receipt.details->data)
                      : nullptr;
  if (!field)
    return {false, nullptr, invalid(receipt.result_id).error};
  auto shape = validate_field_battle_receipt(receipt.kind, *field);
  if (!shape.ok)
    return {false, nullptr, shape.error};
  auto *value = std::get_if<FieldBattleMutationResult>(&field->result);
  if (!value)
    return {false, nullptr, invalid(receipt.result_id).error};
  return {true, value, {}};
}
// 其它持久记录逐个核实际 Common 结果及其精确结果槽，不把同 battle
// 任意成功当来源。
Status record_source(const CommonStore &common, const SourceRef &ref,
                     const StableId &battle, const StableId &kind,
                     const StableId &actor, const StableId &slot,
                     const StableId &id,
                     std::optional<Integer> request_index = {}) {
  auto source = read_common_command_source(common, ref);
  if (!source.ok)
    return {false, {}, source.error};
  const auto &result = *source.value.result;
  if (result.kind != kind ||
      (!actor.empty() && source.value.history->actor_id != actor))
    return invalid(ref.id);
  auto typed = mutation(result);
  if (!typed.ok)
    return {false, {}, typed.error};
  const auto &value = *typed.value;
  if (value.battle_id != battle)
    return invalid(ref.id);
  if ((slot == "proposal" && value.proposal_id != id) ||
      (slot == "council_stratagem" &&
       (value.proposal_id != id || value.request_index != request_index ||
        value.stratagem_id || value.state != FieldBattleState::Council)) ||
      (slot == "order" && value.order_id != id) ||
      (slot == "stratagem" && value.stratagem_id != id) ||
      (slot == "formation" && value.formation_id != id) ||
      (slot == "activation" && value.activation_id != id) ||
      (slot == "request" && value.request_index != request_index))
    return invalid(ref.id);
  return common_success();
}
// 只验真正 Adopt 的双向事实引用，历史 result 不拿当前任务版本倒填。
Status fact_source(const CommonStore &common,
                   const FieldBattleWorldState &world,
                   const FieldCouncilFact &fact,
                   const FamilyLifeHistoryRead4 *original_life = nullptr) {
  const auto session = world.sessions.find(fact.battle_id);
  if (session == world.sessions.end() || fact.side < 0 || fact.side > 1)
    return invalid(fact.fact_id);
  if (fact.kind == FieldCouncilFactKind::AiOrder) {
    auto original = read_field_ai_command_source(common, world, fact.fact_id,
                                                 original_life);
    return original.ok ? common_success() : Status{false, {}, original.error};
  }
  if (fact.ai_pursuit || fact.ai_control_acceptance)
    return invalid(fact.fact_id);
  if (fact.kind == FieldCouncilFactKind::OrderDelivered) {
    if (fact.command_result || !fact.order_id || !fact.formation_id ||
        !fact.player_actor_id)
      return invalid(fact.fact_id);
    const auto order = session->second.orders.find(*fact.order_id);
    if (order == session->second.orders.end() ||
        order->second.status != FieldOrderStatus::Delivered ||
        order->second.target_formation_id != *fact.formation_id ||
        !order->second.status_at ||
        order->second.status_at->tick != fact.at.tick ||
        order->second.status_at->t != fact.at.t)
      return invalid(fact.fact_id);
    return common_success();
  }
  if (fact.kind == FieldCouncilFactKind::PlayerOrder) {
    if (!fact.command_result || !fact.player_actor_id ||
        !fact.player_command_kind)
      return invalid(fact.fact_id);
    auto source = read_common_command_source(common, *fact.command_result);
    if (!source.ok)
      return {false, {}, source.error};
    const auto &result = *source.value.result;
    auto typed = mutation(result);
    if (!typed.ok)
      return {false, {}, typed.error};
    const auto &value = *typed.value;
    if (result.kind != *fact.player_command_kind ||
        result.tick != fact.recorded_tick ||
        source.value.history->actor_id != *fact.player_actor_id ||
        value.battle_id != fact.battle_id ||
        value.council_fact_id != fact.fact_id ||
        value.formation_id != fact.formation_id ||
        value.order_id != fact.order_id ||
        value.mission_revision != fact.mission_revision ||
        value.tactic_id != fact.tactic_id ||
        value.activation_id != fact.activation_id ||
        bool(value.mission) != bool(fact.player_mission) ||
        (value.mission &&
         !same_field_battle_mission(*value.mission, *fact.player_mission)) ||
        value.at.tick != fact.at.tick || value.at.t != fact.at.t)
      return invalid(fact.fact_id);
    return common_success();
  }
  const bool player = fact.council.player_actor_id.has_value();
  if (!fact.command_result)
    return player ? invalid(fact.fact_id) : common_success();
  auto source = read_common_command_source(common, *fact.command_result);
  if (!source.ok)
    return {false, {}, source.error};
  const auto &result = *source.value.result;
  if (result.kind != "field_council_adopt" || !fact.council.player_actor_id ||
      source.value.history->actor_id != *fact.council.player_actor_id ||
      result.tick != fact.recorded_tick)
    return invalid(fact.fact_id);
  auto typed = mutation(result);
  if (!typed.ok)
    return {false, {}, typed.error};
  const auto &value = *typed.value;
  if (value.battle_id != fact.battle_id ||
      value.council_fact_id != fact.fact_id ||
      value.proposal_id != fact.council.adopted_proposal_id ||
      value.at.tick != fact.at.tick || value.at.t != fact.at.t)
    return invalid(fact.fact_id);
  return common_success();
}
} // namespace
namespace {
bool same_control(const FieldOriginalControlRead &read,
                  const FieldFormationControl &control) {
  if (const auto *direct = std::get_if<FieldDirectControl>(&control))
    return read.direct &&
           read.controller_actor_id == direct->controller_actor_id &&
           !read.dispatched_at;
  const auto &autonomous = std::get<FieldAutonomousControl>(control);
  return !read.direct && read.controller_actor_id.empty() &&
         bool(read.dispatched_at) == bool(autonomous.dispatched_at) &&
         (!read.dispatched_at ||
          std::tie(read.dispatched_at->tick, read.dispatched_at->t) ==
              std::tie(autonomous.dispatched_at->tick,
                       autonomous.dispatched_at->t));
}
FieldOriginalControlRead borrow_control(const FieldFormationControl &control) {
  if (const auto *direct = std::get_if<FieldDirectControl>(&control))
    return {true, direct->controller_actor_id, {}};
  return {false, {}, std::get<FieldAutonomousControl>(control).dispatched_at};
}
Outcome<FieldOriginalControlRead> reduce_original_control(
    const CommonStore &common, const FieldBattleWorldState &world,
    const FieldBattleSession &session, const FieldFormation &formation,
    bool verify_current, std::optional<Integer> before_event = {},
    const FieldControlAcceptanceBoundary *boundary_cut = nullptr) {
  auto fail = [&]() -> Outcome<FieldOriginalControlRead> {
    return {false, {}, invalid(formation.formation_id).error};
  };
  const auto s = world.sessions.find(session.battle_id);
  const auto f = session.formations.find(formation.formation_id);
  if (s == world.sessions.end() || &s->second != &session ||
      f == session.formations.end() || &f->second != &formation ||
      formation.side < 0 || formation.side > 1)
    return fail();
  auto binding = read_field_original_battle_binding(
      common, world, session.battle_id, formation.army_id);
  if (!binding.ok)
    return {false, {}, binding.error};
  bool member = false;
  for (const auto &row : binding.value->members)
    member = member || row.formation_id == formation.formation_id;
  if (!member)
    return fail();
  FieldOriginalControlRead result;
  if (std::holds_alternative<FieldBoundaryContactBinding>(
          binding.value->source)) {
    auto entry = read_field_reinforcement_admission_identity(
        common, world, session, formation);
    if (!entry.ok)
      return {false, {}, entry.error};
    result = borrow_control(entry.value->initial_control);
  } else if (formation.deployment_command_result) {
    auto deployed =
        read_field_original_deployment_application(common, session, formation);
    if (!deployed.ok)
      return {false, {}, deployed.error};
    result = borrow_control(deployed.value->control);
  } else {
    const FieldCouncilFact *adopted = nullptr;
    for (const auto &[id, row] : world.council_facts)
      if (row.battle_id == session.battle_id && row.side == formation.side &&
          row.kind == FieldCouncilFactKind::Adopted) {
        if (adopted || id != row.fact_id)
          return fail();
        adopted = &row;
      }
    if (!adopted)
      return fail();
    auto checked = fact_source(common, world, *adopted);
    if (!checked.ok)
      return {false, {}, checked.error};
    // Original formation construction, not today's metadata.player/Army.
    if (formation.commander_actor_id == binding.value->commander_actor_id ||
        adopted->council.player_actor_id == formation.commander_actor_id)
      result = {true, formation.commander_actor_id, {}};
  }
  FieldOriginalControlRead at_boundary = result;
  Integer previous_revision = 0;
  Integer previous_prefix = 0, previous_event_cursor = 0;
  std::optional<FieldBattleTime> previous_at;
  CheckpointVector<std::string_view> seen;
  for (const auto &ref : formation.control_change_sources) {
    if (std::find(seen.begin(), seen.end(), std::string_view(ref.id)) !=
        seen.end())
      return fail();
    seen.push_back(ref.id);
    auto source = read_common_command_source(common, ref);
    if (!source.ok)
      return {false, {}, source.error};
    const auto &receipt = *source.value.result;
    auto typed = mutation(receipt);
    if (!typed.ok)
      return {false, {}, typed.error};
    const auto &value = *typed.value;
    if (value.battle_id != session.battle_id ||
        value.state != FieldBattleState::Running ||
        std::tie(value.at.tick, value.at.t) <
            std::tie(formation.joined_at.tick, formation.joined_at.t) ||
        receipt.revision <= previous_revision ||
        std::tie(value.at.tick, value.at.t) >
            std::tie(session.now.tick, session.now.t) ||
        (previous_at && std::tie(value.at.tick, value.at.t) <
                            std::tie(previous_at->tick, previous_at->t)))
      return fail();
    if (!value.actual_control_acceptance)
      return fail();
    const auto &cut = *value.actual_control_acceptance;
    auto accepted = validate_field_control_acceptance(common, session, cut);
    if (!accepted.ok)
      return {false, {}, accepted.error};
    if (cut.common_prefix.common_fact_count < previous_prefix ||
        cut.last_local_event_sequence < previous_event_cursor)
      return fail();
    for (const auto &e : session.events) {
      if (e.sequence <= cut.last_local_event_sequence &&
          std::tie(e.at.tick, e.at.t) > std::tie(value.at.tick, value.at.t))
        return fail();
      if (e.sequence > cut.last_local_event_sequence &&
          std::tie(e.at.tick, e.at.t) < std::tie(value.at.tick, value.at.t))
        return fail();
    }
    previous_prefix = cut.common_prefix.common_fact_count;
    previous_event_cursor = cut.last_local_event_sequence;
    const auto &actor = source.value.history->actor_id;
    if (receipt.kind == "field_battle_dispatch") {
      if (value.formation_id != formation.formation_id || !result.direct ||
          result.controller_actor_id != actor)
        return fail();
      result = {false, {}, value.at};
    } else if (receipt.kind == "field_battle_rejoin") {
      if (value.formation_id != formation.formation_id || result.direct)
        return fail();
      result = {true, actor, {}};
    } else if (receipt.kind == "field_battle_delegate") {
      const auto &side = session.sides[formation.side];
      if (!result.direct ||
          formation.commander_actor_id == side.commander_actor_id ||
          actor != side.commander_actor_id || value.council_fact_id ||
          side.delegated_by_actor_id != actor ||
          !side.delegation_command_result ||
          !same_source_ref(*side.delegation_command_result, ref))
        return fail();
      result = {};
    } else
      return fail();
    if (before_event && cut.last_local_event_sequence < *before_event) {
      if (!boundary_cut || cut.common_prefix.common_fact_count >
                               boundary_cut->common_prefix.common_fact_count)
        return fail();
      auto time =
          compare_world_moments(cut.observed_at, boundary_cut->observed_at);
      if (!time.ok)
        return {false, {}, time.error};
      if (time.value > 0)
        return fail();
      at_boundary = result;
    }
    previous_revision = receipt.revision;
    previous_at = value.at;
  }
  if (verify_current && !same_control(result, formation.control))
    return fail();
  return {true, before_event ? at_boundary : result, {}};
}
} // namespace
Outcome<FieldOriginalControlRead> read_field_original_control_history(
    const CommonStore &common, const FieldBattleWorldState &world,
    const FieldBattleSession &session, const FieldFormation &formation) {
  return reduce_original_control(common, world, session, formation, true);
}
Outcome<FieldOriginalControlRead>
read_field_original_control_before_order_resolution(
    const CommonStore &common, const FieldBattleWorldState &world,
    const FieldBattleSession &session, const FieldOrderRecord &order) {
  auto o = session.orders.find(order.order_id);
  auto f = session.formations.find(order.target_formation_id);
  if (o == session.orders.end() || &o->second != &order ||
      f == session.formations.end() || !order.resolution_event_sequence ||
      !order.status_at)
    return {false, {}, invalid(order.order_id).error};
  if (!order.resolution_boundary)
    return {false, {}, invalid(order.order_id).error};
  auto boundary = validate_field_control_acceptance(common, session,
                                                    *order.resolution_boundary);
  if (!boundary.ok)
    return {false, {}, boundary.error};
  auto actual = validate_field_order_resolution_sources(session);
  if (!actual.ok)
    return {false, {}, actual.error};
  // Still validate every retained command and the true current reduction.
  // Select only controls accepted strictly before this actual local event.
  return reduce_original_control(common, world, session, f->second, true,
                                 *order.resolution_event_sequence,
                                 &*order.resolution_boundary);
}
Outcome<bool> read_field_original_player_control_at_boundary(
    const CommonStore &common, const FieldBattleWorldState &world,
    const FieldBattleSession &session, const FieldFormation &formation,
    std::string_view actor, const FieldControlAcceptanceBoundary &cut,
    const FamilyLifeHistoryRead4 *life) {
  auto bad = [&]() -> Outcome<bool> {
    return {false, false, invalid(formation.formation_id).error};
  };
  if (!world.sessions.contains(session.battle_id) ||
      &world.sessions.at(session.battle_id) != &session || formation.side < 0 ||
      formation.side > 1 ||
      !session.formations.contains(formation.formation_id) ||
      &session.formations.at(formation.formation_id) != &formation)
    return bad();
  auto boundary = validate_field_control_acceptance(common, session, cut);
  if (!boundary.ok)
    return {false, false, boundary.error};
  // A source present today is not proof that the target existed at this cut.
  auto binding = read_field_original_battle_binding(
      common, world, session.battle_id, formation.army_id);
  if (!binding.ok)
    return {false, false, binding.error};
  const FieldReinforcementAdmission *actual_entry = nullptr;
  if (std::holds_alternative<FieldBoundaryContactBinding>(
          binding.value->source)) {
    auto entry = read_field_reinforcement_admission_identity(
        common, world, session, formation);
    if (!entry.ok)
      return {false, false, entry.error};
    auto position = read_actual_common_fact_position(
        common, entry.value->application_source);
    if (!position.ok)
      return {false, false, position.error};
    auto time = compare_world_moments(entry.value->applied_at, cut.observed_at);
    if (!time.ok)
      return {false, false, time.error};
    if (position.value >= cut.common_prefix.common_fact_count ||
        time.value > 0 ||
        entry.value->event_sequence > cut.last_local_event_sequence)
      return bad();
    actual_entry = entry.value;
  }
  const StableId *original_actor = nullptr;
  if (session.councils[formation.side].player_actor_id &&
      *session.councils[formation.side].player_actor_id == actor) {
    const FieldCouncilFact *adopted = nullptr;
    for (const auto &[id, row] : world.council_facts)
      if (row.battle_id == session.battle_id && row.side == formation.side &&
          row.kind == FieldCouncilFactKind::Adopted) {
        if (adopted || row.council.player_actor_id != actor)
          return bad();
        adopted = &row;
      }
    if (!adopted)
      return bad();
    auto source = fact_source(common, world, *adopted, life);
    if (!source.ok)
      return {false, false, source.error};
    const auto *head = common.state().facts.find(adopted->fact_id);
    if (!head || head->domain != "military" || head->kind != "military_council")
      return bad();
    auto position = read_actual_common_fact_position(common, *head);
    if (!position.ok)
      return {false, false, position.error};
    if (position.value >= cut.common_prefix.common_fact_count)
      return bad();
    original_actor = &*adopted->council.player_actor_id;
  } else {
    if (!actual_entry)
      return {true, false, {}};
    if (!actual_entry->pending_player_entry_order || actual_entry->ai_mission ||
        binding.value->commander_actor_id != actor)
      return {true, false, {}};
    original_actor = &binding.value->commander_actor_id;
  }
  if (!life || !life->bound_to(common))
    return {
        false, false,
        common_failure("FieldPlayerOriginalLifePrefixRequired", *original_actor)
            .error};
  auto death = life->read_actor_death_record_at_original_prefix(
      *original_actor, cut.observed_at, cut.common_prefix);
  if (!death.ok)
    return {false, false, death.error};
  if (death.value)
    return {true, false, {}};
  // Actual local CommanderLost is independent of (usually later) S4 DeathRef.
  // Bind the original loss row and unique ordered event, never today's dead
  // bit.
  for (const auto &[id, f] : session.formations)
    if (f.side == formation.side && f.commander_actor_id == *original_actor) {
      const FieldCommanderCasualty *loss = nullptr;
      for (const auto &row : f.commander_casualties)
        if (row.actor_id == *original_actor && row.loss == 2) {
          if (loss)
            return bad();
          loss = &row;
        }
      if (!loss)
        continue;
      const FieldBattleEvent *event = nullptr;
      for (const auto &e : session.events)
        if (e.kind == field_event_kind::CommanderLost &&
            e.formation_id == id && e.at.tick == loss->at.tick &&
            e.at.t == loss->at.t) {
          if (event || e.side != formation.side || e.count != 2)
            return bad();
          event = &e;
        }
      if (!event || !f.commander_lost_at ||
          f.commander_lost_at->tick != loss->at.tick ||
          f.commander_lost_at->t != loss->at.t)
        return bad();
      if (event->sequence <= cut.last_local_event_sequence)
        return {true, false, {}};
    }
  const auto &side = session.sides[formation.side];
  if (side.commander_actor_id == *original_actor &&
      side.delegation_command_result) {
    auto command =
        read_common_command_source(common, *side.delegation_command_result);
    if (!command.ok)
      return {false, false, command.error};
    auto value = mutation(*command.value.result);
    if (!value.ok)
      return {false, false, value.error};
    if (command.value.result->kind != "field_battle_delegate" ||
        command.value.history->actor_id != *original_actor ||
        value.value->battle_id != session.battle_id ||
        !value.value->actual_control_acceptance)
      return bad();
    const auto &accepted = *value.value->actual_control_acceptance;
    auto valid = validate_field_control_acceptance(common, session, accepted);
    if (!valid.ok)
      return {false, false, valid.error};
    if (accepted.last_local_event_sequence <= cut.last_local_event_sequence) {
      auto time = compare_world_moments(accepted.observed_at, cut.observed_at);
      if (!time.ok)
        return {false, false, time.error};
      if (time.value > 0 || accepted.common_prefix.common_fact_count >
                                cut.common_prefix.common_fact_count)
        return bad();
      return {true, false, {}};
    }
  }
  return {true, true, {}};
}
Status validate_field_delivered_order_authority(
    const CommonStore &common, const FieldBattleWorldState &world,
    const FieldBattleSession &session, const FieldOrderRecord &order,
    const FamilyLifeHistoryRead4 *life) {
  if (order.status != FieldOrderStatus::Delivered)
    return common_success();
  auto control = read_field_original_control_before_order_resolution(
      common, world, session, order);
  if (!control.ok)
    return {false, {}, control.error};
  if (!control.value.direct || !order.resolution_boundary)
    return invalid(order.order_id);
  auto player = read_field_original_player_control_at_boundary(
      common, world, session, session.formations.at(order.target_formation_id),
      control.value.controller_actor_id, *order.resolution_boundary, life);
  return !player.ok     ? Status{false, {}, player.error}
         : player.value ? common_success()
                        : invalid(order.order_id);
}
Status bind_field_battle_command_result(const CommonStore &common,
                                        FieldBattleWorldState &world,
                                        const StableId &actor, Integer sequence,
                                        const CommitResult &receipt,
                                        CheckpointMemoryCharge &world_owner) {
  if (!field_deployment_command_kind(receipt.kind))
    return common_success();
  if (receipt.kind == "field_battle_read")
    return invalid(receipt.kind);
  auto original =
      read_common_command_source(common, std::string_view(receipt.result_id));
  if (!original.ok)
    return {false, {}, original.error};
  if (original.value.result != &receipt ||
      original.value.history->actor_id != actor ||
      original.value.request_seq != sequence)
    return invalid(receipt.result_id);
  // 借同 World 原 checkpoint_copy_memory
  // 拥有新增持久字段；局部引用另持临时租约。


  const auto source_temporary_memory = CheckpointMemoryCharge(0);
  const SourceRef source{"common", "command_result",
                         checkpoint_fresh_string_copy(receipt.result_id)};
  const auto install_source = [&](std::optional<SourceRef> &destination) {
    (world_owner).grow(0);
    SourceRef fresh{checkpoint_fresh_string_copy(source.domain),
                    checkpoint_fresh_string_copy(source.kind),
                    checkpoint_fresh_string_copy(source.id)};
    destination = std::move(fresh);
  };
  const auto append_control_source = [&](FieldFormation &formation) -> Status {
    for (const auto &prior : formation.control_change_sources)
      if (same_source_ref(prior, source))
        return invalid(receipt.result_id);
    // The tracked vector charges actual capacity/overlap through its original
    // allocator. This lease covers only fresh owned SourceRef string payload.


    (world_owner).grow(0);
    formation.control_change_sources.push_back(
        {checkpoint_fresh_string_copy(source.domain),
         checkpoint_fresh_string_copy(source.kind),
         checkpoint_fresh_string_copy(source.id)});
    return common_success();
  };
  if (receipt.kind == "field_council_speak") {
    auto typed = mutation(receipt);
    if (!typed.ok)
      return {false, {}, typed.error};
    auto session = world.sessions.find(typed.value->battle_id);
    if (session == world.sessions.end())
      return invalid(receipt.result_id);
    if (typed.value->proposal_id) {
      FieldCouncilProposal *found = nullptr;
      for (auto &council : session->second.councils)
        for (auto &p : council.proposals)
          if (p.proposal_id == *typed.value->proposal_id &&
              p.speaker_actor_id == actor) {
            if (found)
              return invalid(receipt.result_id);
            found = &p;
          }
      if (!found || found->command_result)
        return invalid(receipt.result_id);
      install_source(found->command_result);
      return record_source(common, source, typed.value->battle_id, receipt.kind,
                           actor, "proposal", found->proposal_id);
    }
    if (!typed.value->request_index || *typed.value->request_index < 0)
      return invalid(receipt.result_id);
    FieldCouncilRequest *request = nullptr;
    for (auto &council : session->second.councils) {
      if (council.player_actor_id != actor ||
          static_cast<std::size_t>(*typed.value->request_index) >=
              council.requests.size())
        continue;
      auto &actual =
          council
              .requests[static_cast<std::size_t>(*typed.value->request_index)];
      if (request || actual.actor_id != actor || actual.command_result ||
          actual.status == FieldCouncilRequestStatus::Pending)
        return invalid(receipt.result_id);
      request = &actual;
    }
    if (!request)
      return invalid(receipt.result_id);
    FieldBattleEvent *event = nullptr;
    for (auto &candidate : session->second.events) {
      if (candidate.kind != field_event_kind::RequestGranted &&
          candidate.kind != field_event_kind::RequestDenied)
        continue;
      if (!candidate.side || *candidate.side < 0 || *candidate.side > 1 ||
          session->second.councils[*candidate.side].player_actor_id != actor)
        continue;
      if (event || candidate.source_id)
        return invalid(receipt.result_id);
      event = &candidate;
    }
    if (!event)
      return invalid(receipt.result_id);


    (world_owner).grow(0);
    event->source_id = checkpoint_fresh_string_copy(receipt.result_id);
    install_source(request->command_result);
    return record_source(common, source, typed.value->battle_id, receipt.kind,
                         actor, "request", {}, typed.value->request_index);
  }
  if (receipt.kind == "field_battle_stratagem") {
    auto typed = mutation(receipt);
    if (!typed.ok)
      return {false, {}, typed.error};
    const auto &value = *typed.value;
    auto session = world.sessions.find(value.battle_id);
    if (session == world.sessions.end())
      return invalid(receipt.result_id);
    if (value.proposal_id && value.request_index) {
      FieldCouncilStratagemPlan *item = nullptr;
      for (Integer side = 0; side < 2; ++side) {
        if (session->second.sides[side].commander_actor_id != actor ||
            session->second.councils[side].player_actor_id != actor)
          continue;
        for (auto &p : session->second.councils[side].proposals)
          if (p.proposal_id == *value.proposal_id) {
            if (item || *value.request_index < 0 ||
                std::size_t(*value.request_index) >= p.plan.stratagems.size())
              return invalid(receipt.result_id);
            item = &p.plan.stratagems[std::size_t(*value.request_index)];
          }
      }
      if (!item || item->command_result || item->voided_at_adoption)
        return invalid(receipt.result_id);
      const auto &payload = *value.council_stratagem;
      if (payload.kind != item->kind ||
          payload.formation_id != item->formation_id ||
          bool(payload.area_dm) != bool(item->area_dm) ||
          (payload.area_dm && (payload.area_dm->x != item->area_dm->x ||
                               payload.area_dm->y != item->area_dm->y)))
        return invalid(receipt.result_id);
      install_source(item->command_result);
      return record_source(common, source, value.battle_id, receipt.kind, actor,
                           "council_stratagem", *value.proposal_id,
                           value.request_index);
    }
    if (!value.stratagem_id)
      return invalid(receipt.result_id);
    auto row = session->second.stratagems.find(*value.stratagem_id);
    if (row == session->second.stratagems.end() || row->second.command_result ||
        row->second.issuer_actor_id != actor ||
        row->second.kind != FieldStratagemKind::Confuse ||
        !value.effective_at || !row->second.resolve_at ||
        value.effective_at->tick != row->second.resolve_at->tick ||
        value.effective_at->t != row->second.resolve_at->t ||
        row->second.issued.tick != value.at.tick ||
        row->second.issued.t != value.at.t)
      return invalid(receipt.result_id);
    install_source(row->second.command_result);
    return record_source(common, source, value.battle_id, receipt.kind, actor,
                         "stratagem", *value.stratagem_id);
  }
  if (receipt.kind == "field_battle_set_deployment") {
    auto typed = mutation(receipt);
    if (!typed.ok)
      return {false, {}, typed.error};
    const auto &v = *typed.value;
    auto session = world.sessions.find(v.battle_id);
    if (session == world.sessions.end() ||
        session->second.state != FieldBattleState::Deploying ||
        !v.formation_id || !v.mission_revision || !v.deployment || !v.mission ||
        !v.effective_at)
      return invalid(receipt.result_id);
    auto formation = session->second.formations.find(*v.formation_id);
    if (formation == session->second.formations.end())
      return invalid(receipt.result_id);
    auto &f = formation->second;
    const auto &d = *v.deployment;
    const auto *direct = std::get_if<FieldDirectControl>(&d.control);
    const auto *actual_direct = std::get_if<FieldDirectControl>(&f.control);
    const auto *autonomous = std::get_if<FieldAutonomousControl>(&d.control);
    if (f.mission_revision != *v.mission_revision ||
        f.mission_transition_sequence || f.position_dm.x != d.position_dm.x ||
        f.position_dm.y != d.position_dm.y || f.facing_mdeg != d.facing_mdeg ||
        f.deputy_actor_id != d.deputy_actor_id ||
        f.control.index() != d.control.index() ||
        (direct &&
         (!actual_direct ||
          direct->controller_actor_id != actual_direct->controller_actor_id)) ||
        (autonomous && autonomous->dispatched_at) ||
        !same_field_battle_mission(f.mission, d.immediate_mission) ||
        bool(f.wave_mission) != bool(d.wave_mission) ||
        (f.wave_mission &&
         !same_field_battle_mission(*f.wave_mission, *d.wave_mission)) ||
        !same_field_battle_mission(*v.mission, d.wave_mission
                                                   ? *d.wave_mission
                                                   : d.immediate_mission) ||
        std::tie(v.effective_at->tick, v.effective_at->t) !=
            std::tie(session->second.now.tick, session->second.now.t))
      return invalid(receipt.result_id);
    install_source(f.deployment_command_result);
    return record_source(common, source, v.battle_id, receipt.kind, actor,
                         "formation", *v.formation_id);
  }
  if (receipt.kind == "field_battle_start")
    return common_success();
  if (receipt.kind == "field_battle_tactic") {
    auto typed = mutation(receipt);
    if (!typed.ok)
      return {false, {}, typed.error};
    const auto &value = *typed.value;
    auto found = world.sessions.find(value.battle_id);
    if (found == world.sessions.end() || !value.formation_id ||
        !value.tactic_id || !value.effective_at ||
        !found->second.formations.contains(*value.formation_id))
      return invalid(receipt.result_id);
    auto &session = found->second;
    auto &formation = session.formations.at(*value.formation_id);
    if (value.order_id) {
      auto order = session.orders.find(*value.order_id);
      if (value.activation_id || order == session.orders.end() ||
          order->second.command_result || order->second.ai_issued ||
          order->second.issuer_actor_id != actor ||
          order->second.target_formation_id != *value.formation_id ||
          !std::holds_alternative<FieldTacticIntent>(order->second.payload) ||
          std::get<FieldTacticIntent>(order->second.payload).tactic_id !=
              *value.tactic_id ||
          order->second.due.tick != value.effective_at->tick ||
          order->second.due.t != value.effective_at->t)
        return invalid(receipt.result_id);
      install_source(order->second.command_result);
    } else {
      FieldTacticActivation *activation = nullptr;
      if (!formation.ability || !value.activation_id)
        return invalid(receipt.result_id);
      for (auto &entry : formation.ability->active)
        if (entry.activation_id == *value.activation_id) {
          if (activation)
            return invalid(receipt.result_id);
          activation = &entry;
        }
      if (!activation || activation->command_result || activation->order_id ||
          activation->ai_issued || activation->issuer_actor_id != actor ||
          activation->tactic_id != *value.tactic_id ||
          activation->started.tick != value.effective_at->tick ||
          activation->started.t != value.effective_at->t ||
          activation->started.tick != value.at.tick ||
          activation->started.t != value.at.t)
        return invalid(receipt.result_id);
      install_source(activation->command_result);
    }
    const bool subordinate =
        session.councils[formation.side].player_actor_id == actor &&
        session.sides[formation.side].commander_actor_id != actor;
    if (bool(value.council_fact_id) != subordinate)
      return invalid(receipt.result_id);
    if (value.council_fact_id) {
      auto fact = world.council_facts.find(*value.council_fact_id);
      if (fact == world.council_facts.end() || fact->second.command_result ||
          fact->second.kind != FieldCouncilFactKind::PlayerOrder)
        return invalid(receipt.result_id);
      install_source(fact->second.command_result);
      return fact_source(common, world, fact->second);
    }
    return common_success();
  }
  if (receipt.kind == "field_battle_direct" ||
      receipt.kind == "field_battle_order" ||
      receipt.kind == "field_battle_dispatch" ||
      receipt.kind == "field_battle_rejoin" ||
      receipt.kind == "field_battle_delegate") {
    auto typed = mutation(receipt);
    if (!typed.ok)
      return {false, {}, typed.error};
    const auto &value = *typed.value;
    auto found = world.sessions.find(value.battle_id);
    if (found == world.sessions.end())
      return invalid(receipt.result_id);
    auto &session = found->second;
    Integer side = -1;
    if (receipt.kind == "field_battle_delegate") {
      for (auto &s : session.sides)
        if (s.delegated_by_actor_id == actor) {
          if (side != -1 || s.delegation_command_result ||
              !std::holds_alternative<FieldAiController>(s.controller))
            return invalid(receipt.result_id);
          side = s.side;
          install_source(s.delegation_command_result);
          // The original delegate writer changed only these actual prior
          // Direct formations. Re-read the retained pre-command chain; do not
          // infer affected members from today's Autonomous state.
          for (auto &[id, formation] : session.formations) {
            if (formation.side != side)
              continue;
            auto prior = reduce_original_control(common, world, session,
                                                 formation, false);
            if (!prior.ok)
              return {false, {}, prior.error};
            if (!prior.value.direct ||
                formation.commander_actor_id == s.commander_actor_id)
              continue;
            if (!same_control(FieldOriginalControlRead{}, formation.control))
              return invalid(id);
            auto appended = append_control_source(formation);
            if (!appended.ok)
              return appended;
          }
        }
    } else {
      if (!value.formation_id ||
          !session.formations.contains(*value.formation_id))
        return invalid(receipt.result_id);
      auto &f = session.formations.at(*value.formation_id);
      side = f.side;
      if (receipt.kind == "field_battle_direct") {
        if (!f.pending_direct || f.pending_direct->ai_issued ||
            f.pending_direct->command_result ||
            f.pending_direct->issuer_actor_id != actor ||
            f.pending_direct->request_sequence != sequence ||
            value.mission_revision != f.pending_direct->accepted_revision ||
            !value.mission || !value.effective_at ||
            !same_field_battle_mission(*value.mission,
                                       f.pending_direct->mission) ||
            value.effective_at->tick != f.pending_direct->apply_at.tick ||
            value.effective_at->t != f.pending_direct->apply_at.t)
          return invalid(receipt.result_id);
        install_source(f.pending_direct->command_result);
      } else if (receipt.kind == "field_battle_order") {
        if (!value.order_id || !session.orders.contains(*value.order_id))
          return invalid(receipt.result_id);
        auto &order = session.orders.at(*value.order_id);
        if (order.command_result || order.ai_issued ||
            order.issuer_actor_id != actor ||
            order.target_formation_id != *value.formation_id ||
            !value.mission || !value.effective_at ||
            !std::holds_alternative<FieldMission>(order.payload) ||
            !same_field_battle_mission(*value.mission,
                                       std::get<FieldMission>(order.payload)) ||
            value.effective_at->tick != order.due.tick ||
            value.effective_at->t != order.due.t)
          return invalid(receipt.result_id);
        install_source(order.command_result);
      } else if (receipt.kind == "field_battle_dispatch") {
        const auto *control = std::get_if<FieldAutonomousControl>(&f.control);
        if (!control || !control->dispatched_at || !value.mission ||
            value.mission_revision != f.mission_revision ||
            !same_field_battle_mission(*value.mission, f.mission))
          return invalid(receipt.result_id);
        install_source(f.mission_command_result);
        install_source(f.control_command_result);
        auto retained = append_control_source(f);
        if (!retained.ok)
          return retained;
        auto checked_control =
            read_field_original_control_history(common, world, session, f);
        if (!checked_control.ok)
          return {false, {}, checked_control.error};
        auto applied = record_field_actual_mission_application(
            common, world, session, f, world_owner);
        if (!applied.ok)
          return applied;
      } else {
        const auto *control = std::get_if<FieldDirectControl>(&f.control);
        if (!control || control->controller_actor_id != actor)
          return invalid(receipt.result_id);
        install_source(f.control_command_result);
        auto retained = append_control_source(f);
        if (!retained.ok)
          return retained;
        auto checked_control =
            read_field_original_control_history(common, world, session, f);
        if (!checked_control.ok)
          return {false, {}, checked_control.error};
      }
    }
    if (side < 0 || side > 1)
      return invalid(receipt.result_id);
    const bool subordinate = session.councils[side].player_actor_id == actor &&
                             session.sides[side].commander_actor_id != actor;
    if (bool(value.council_fact_id) != subordinate)
      return invalid(receipt.result_id);
    if (value.council_fact_id) {
      auto f = world.council_facts.find(*value.council_fact_id);
      if (f == world.council_facts.end() ||
          f->second.kind != FieldCouncilFactKind::PlayerOrder ||
          f->second.command_result)
        return invalid(receipt.result_id);
      install_source(f->second.command_result);
      return fact_source(common, world, f->second);
    }
    return common_success();
  }
  if (receipt.kind != "field_council_adopt")
    return common_failure("NOT_IMPLEMENTED",
                          "field_battle/command_result_binding",
                          ErrorKind::unsupported_capability);
  auto typed = mutation(receipt);
  if (!typed.ok)
    return {false, {}, typed.error};
  if (!typed.value->council_fact_id)
    return invalid(receipt.result_id);
  auto fact = world.council_facts.find(*typed.value->council_fact_id);
  if (fact == world.council_facts.end() ||
      fact->second.battle_id != typed.value->battle_id)
    return invalid(receipt.result_id);
  if (fact->second.command_result) {
    const auto &old = *fact->second.command_result;
    if (old.domain != source.domain || old.kind != source.kind ||
        old.id != source.id)
      return invalid(receipt.result_id);
  } else
    install_source(fact->second.command_result);
  return fact_source(common, world, fact->second);
}
Status visit_field_battle_command_sources(
    const CommonStore &common, const FieldBattleWorldState &world,
    const std::function<Status(const SourceRef &)> &consume,
    const FamilyLifeHistoryRead4 *original_life) {
  // 新军主将选口仍保留唯一Common成功结果；PIN不能漏掉这条真实引用。
  for (const auto &[id, plan] : world.army_exit_plans) {
    if (id != plan.fact_id) return invalid(id);
    auto valid = validate_field_army_exit_player_command_source(common, world, plan);
    if (!valid.ok) return valid;
    if (plan.player_command_result) {
      valid = consume(*plan.player_command_result);
      if (!valid.ok) return valid;
    }
  }
  for (const auto &[id, fact] : world.council_facts) {
    if (id != fact.fact_id)
      return invalid(id);
    auto valid = fact_source(common, world, fact, original_life);
    if (!valid.ok)
      return valid;
    if (fact.command_result) {
      valid = consume(*fact.command_result);
      if (!valid.ok)
        return valid;
    }
  }
  for (const auto &[battle_id, session] : world.sessions) {
    auto use = [&](const std::optional<SourceRef> &source, const StableId &kind,
                   const StableId &actor, const StableId &slot,
                   const StableId &id,
                   std::optional<Integer> request_index = {}) -> Status {
      if (!source)
        return common_success();
      auto valid = record_source(common, *source, battle_id, kind, actor, slot,
                                 id, request_index);
      return valid.ok ? consume(*source) : valid;
    };
    for (Integer side = 0; side < 2; ++side) {
      const auto &player = session.councils[side].player_actor_id;
      for (const auto &p : session.councils[side].proposals) {
        if (player && p.speaker_actor_id == *player && !p.command_result)
          return invalid(p.proposal_id);
        auto valid = use(p.command_result, "field_council_speak",
                         p.speaker_actor_id, "proposal", p.proposal_id);
        if (!valid.ok)
          return valid;
        for (std::size_t i = 0; i < p.plan.stratagems.size(); ++i) {
          const auto &item = p.plan.stratagems[i];
          valid = use(item.command_result, "field_battle_stratagem",
                      session.sides[side].commander_actor_id,
                      "council_stratagem", p.proposal_id, Integer(i));
          if (!valid.ok)
            return valid;
          if (item.command_result) {
            auto original =
                read_common_command_source(common, *item.command_result);
            if (!original.ok)
              return {false, {}, original.error};
            auto value = mutation(*original.value.result);
            if (!value.ok)
              return {false, {}, value.error};
            const auto &payload = *value.value->council_stratagem;
            if (payload.kind != item.kind ||
                payload.formation_id != item.formation_id ||
                bool(payload.area_dm) != bool(item.area_dm) ||
                (payload.area_dm && (payload.area_dm->x != item.area_dm->x ||
                                     payload.area_dm->y != item.area_dm->y)))
              return invalid(item.command_result->id);
          }
        }
      }
      for (std::size_t i = 0; i < session.councils[side].requests.size(); ++i) {
        const auto &r = session.councils[side].requests[i];
        if (!r.command_result)
          return invalid(r.actor_id);
        auto valid = use(r.command_result, "field_council_speak", r.actor_id,
                         "request", {}, Integer(i));
        if (!valid.ok)
          return valid;
      }
    }
    for (const auto &side : session.sides) {
      if (bool(side.delegated_by_actor_id) !=
          bool(side.delegation_command_result))
        return invalid(battle_id + "/delegation");
      auto valid = use(side.delegation_command_result, "field_battle_delegate",
                       side.delegated_by_actor_id.value_or(""), "", "");
      if (!valid.ok)
        return valid;
    }
    for (const auto &[id, order] : session.orders) {
      if (order.ai_issued == bool(order.command_result))
        return invalid(id);
      if (order.ai_issued) {
        const FieldCouncilFact *accepted = nullptr;
        for (const auto &[source_id, fact] : world.council_facts)
          if (fact.kind == FieldCouncilFactKind::AiOrder &&
              fact.order_id == id) {
            if (accepted || fact.battle_id != battle_id)
              return invalid(id);
            accepted = &fact;
          }
        if (!accepted)
          return invalid(id);
        auto source = read_field_ai_command_source(
            common, world, accepted->fact_id, original_life);
        if (!source.ok)
          return {false, {}, source.error};
      }

      auto valid = use(order.command_result,
                       std::holds_alternative<FieldTacticIntent>(order.payload)
                           ? "field_battle_tactic"
                           : "field_battle_order",
                       order.issuer_actor_id, "order", id);
      if (!valid.ok)
        return valid;
      if (order.command_result) {
        auto original =
            read_common_command_source(common, *order.command_result);
        if (!original.ok)
          return {false, {}, original.error};
        auto value = mutation(*original.value.result);
        if (!value.ok)
          return {false, {}, value.error};
        if (const auto *intent = std::get_if<FieldTacticIntent>(&order.payload))
          if (value.value->tactic_id != intent->tactic_id ||
              value.value->formation_id != order.target_formation_id ||
              !value.value->effective_at || value.value->activation_id ||
              value.value->effective_at->tick != order.due.tick ||
              value.value->effective_at->t != order.due.t ||
              value.value->at.tick != order.issued.tick ||
              value.value->at.t != order.issued.t)
            return invalid(id);
        if (std::holds_alternative<FieldMission>(order.payload) &&
            (!value.value->mission || !value.value->effective_at ||
             value.value->formation_id != order.target_formation_id ||
             !same_field_battle_mission(
                 *value.value->mission,
                 std::get<FieldMission>(order.payload)) ||
             value.value->effective_at->tick != order.due.tick ||
             value.value->effective_at->t != order.due.t ||
             value.value->at.tick != order.issued.tick ||
             value.value->at.t != order.issued.t))
          return invalid(id);
      }
    }
    for (const auto &[id, stratagem] : session.stratagems) {
      auto valid = use(stratagem.command_result, "field_battle_stratagem",
                       stratagem.issuer_actor_id, "stratagem", id);
      if (!valid.ok)
        return valid;
    }
    for (const auto &event : session.events)
      if (field_event_mission_application(event)) {
        auto valid = validate_field_actual_mission_application(
            common, world, session, event, original_life);
        if (!valid.ok)
          return valid;
        if (field_event_mission_application(event)->command_result) {
          valid = consume(*field_event_mission_application(event)->command_result);
          if (!valid.ok)
            return valid;
        }
      }
    // Observations survive a later command replacement. Keep their actual
    // command result in the same Common source retention traversal.
    for (const auto &event : session.events) {
      if (!field_event_navigation_observation(event))
        continue;
      if (event.kind != field_event_kind::NavigationObserved ||
          !event.formation_id)
        return invalid(battle_id);
      const auto &n = *field_event_navigation_observation(event);
      if (n.original_command_result) {
        auto original =
            read_common_command_source(common, *n.original_command_result);
        if (!original.ok)
          return {false, {}, original.error};
        const auto &kind = original.value.result->kind;
        if (kind != "field_battle_direct" && kind != "field_battle_dispatch")
          return invalid(*event.formation_id);
        auto pinned = use(n.original_command_result, kind, {}, "formation",
                          *event.formation_id);
        if (!pinned.ok)
          return pinned;
      }
    }
    // Superseding a current order releases only its active pointer, not the
    // actual source of a retained local transition. Keep those real receipts
    // in the original Common pin traversal as well.
    for (const auto &event : session.events) {
      if (event.kind != field_event_kind::MissionTransition)
        continue;
      if (!field_event_mission_transition(event) || !event.formation_id)
        return invalid(battle_id);
      const auto &transition = *field_event_mission_transition(event);
      if (transition.previous_sequence)
        continue; // root source is visited once per chain
      if (transition.original_command_result) {
        auto original = read_common_command_source(
            common, *transition.original_command_result);
        if (!original.ok)
          return {false, {}, original.error};
        const auto &kind = original.value.result->kind;
        if (kind != "field_battle_direct" && kind != "field_battle_dispatch")
          return invalid(*event.formation_id);
        auto retained = use(transition.original_command_result, kind, {},
                            "formation", *event.formation_id);
        if (!retained.ok)
          return retained;
        auto actual = mutation(*original.value.result);
        if (!actual.ok)
          return {false, {}, actual.error};
        if (!actual.value->mission || !actual.value->mission_revision ||
            *actual.value->mission_revision != transition.mission_revision ||
            !same_field_battle_mission(*actual.value->mission,
                                       transition.from) ||
            transition.from_ai_pursuit)
          return invalid(*event.formation_id);
      } else if (transition.original_ai_source_id) {
        auto original = read_field_ai_direct_source(
            common, world, *transition.original_ai_source_id, original_life);
        if (!original.ok)
          return {false, {}, original.error};
        const auto &fact = *original.value;
        if (fact.battle_id != battle_id ||
            fact.formation_id != event.formation_id || !fact.player_mission ||
            !fact.mission_revision ||
            *fact.mission_revision != transition.mission_revision ||
            fact.ai_pursuit != transition.from_ai_pursuit ||
            !same_field_battle_mission(*fact.player_mission, transition.from))
          return invalid(*event.formation_id);
      } else if (transition.original_order_id) {
        auto order = session.orders.find(*transition.original_order_id);
        if (order == session.orders.end() ||
            order->second.target_formation_id != *event.formation_id ||
            !std::holds_alternative<FieldMission>(order->second.payload) ||
            order->second.status != FieldOrderStatus::Adopted ||
            order->second.adopted_mission_revision !=
                transition.mission_revision ||
            !order->second.status_at ||
            std::tie(order->second.status_at->tick,
                     order->second.status_at->t) >
                std::tie(event.at.tick, event.at.t) ||
            order->second.ai_pursuit != transition.from_ai_pursuit ||
            !same_field_battle_mission(
                std::get<FieldMission>(order->second.payload), transition.from))
          return invalid(*event.formation_id);
      }
    }
    for (const auto &[id, formation] : session.formations) {
      if (session.started_source) {
        auto control_history = read_field_original_control_history(
            common, world, session, formation);
        if (!control_history.ok)
          return {false, {}, control_history.error};
      } else if (!formation.control_change_sources.empty())
        return invalid(id);
      for (const auto &source : formation.control_change_sources) {
        auto kept = consume(source);
        if (!kept.ok)
          return kept;
      }
      auto mission_origin =
          read_field_current_mission_origin(session, formation);
      if (!mission_origin.ok)
        return {false, {}, mission_origin.error};
      if (formation.deployment_command_result) {
        auto retained = use(formation.deployment_command_result,
                            "field_battle_set_deployment", {}, "formation", id);
        if (!retained.ok)
          return retained;
        auto source = read_common_command_source(
            common, *formation.deployment_command_result);
        if (!source.ok)
          return {false, {}, source.error};
        auto original = mutation(*source.value.result);
        if (!original.ok)
          return {false, {}, original.error};
        if (!original.value->deployment || !original.value->mission_revision)
          return invalid(id);
        if (!formation.mission_command_result &&
            !formation.mission_ai_source_id && !formation.adopted_order_id &&
            !same_field_battle_mission(
                original.value->deployment->immediate_mission,
                *mission_origin.value.mission))
          return invalid(id);
      }
      for (const auto *ref : {&formation.mission_command_result,
                              &formation.control_command_result}) {
        if (!*ref)
          continue;
        auto source = read_common_command_source(common, **ref);
        if (!source.ok)
          return {false, {}, source.error};
        const auto &kind = source.value.result->kind;
        if ((ref == &formation.mission_command_result &&
             kind != "field_battle_direct" &&
             kind != "field_battle_dispatch") ||
            (ref == &formation.control_command_result &&
             kind != "field_battle_rejoin" && kind != "field_battle_dispatch"))
          return invalid(id);
        auto valid = use(*ref, kind, {}, "formation", id);
        if (!valid.ok)
          return valid;
        auto actual = mutation(*source.value.result);
        if (!actual.ok)
          return {false, {}, actual.error};
        if (ref == &formation.mission_command_result &&
            (!actual.value->mission || !actual.value->mission_revision ||
             !same_field_battle_mission(*actual.value->mission,
                                        *mission_origin.value.mission) ||
             (!formation.pending_direct &&
              *actual.value->mission_revision != formation.mission_revision) ||
             (formation.pending_direct &&
              *actual.value->mission_revision >=
                  formation.pending_direct->accepted_revision)))
          return invalid(id);
        if (ref == &formation.control_command_result) {
          if (kind == "field_battle_rejoin") {
            const auto *direct =
                std::get_if<FieldDirectControl>(&formation.control);
            if (!direct ||
                direct->controller_actor_id != source.value.history->actor_id)
              return invalid(id);
          } else {
            const auto *autonomous =
                std::get_if<FieldAutonomousControl>(&formation.control);
            if (!autonomous || !autonomous->dispatched_at ||
                autonomous->dispatched_at->tick != actual.value->at.tick ||
                autonomous->dispatched_at->t != actual.value->at.t)
              return invalid(id);
          }
        }
      }
      if (formation.mission_ai_source_id) {
        if (formation.mission_command_result || formation.adopted_order_id)
          return invalid(id);
        auto source = read_field_ai_direct_source(
            common, world, *formation.mission_ai_source_id, original_life);
        if (!source.ok)
          return {false, {}, source.error};
        const auto &fact = *source.value;
        if (fact.battle_id != session.battle_id || fact.formation_id != id ||
            fact.ai_pursuit != mission_origin.value.ai_pursuit ||
            !same_field_battle_mission(*fact.player_mission,
                                       *mission_origin.value.mission) ||
            (!formation.pending_direct &&
             *fact.mission_revision != formation.mission_revision) ||
            (formation.pending_direct &&
             *fact.mission_revision >=
                 formation.pending_direct->accepted_revision))
          return invalid(id);
      }
      if (formation.pending_direct) {
        const auto &pending = *formation.pending_direct;
        if (pending.ai_issued) {
          if (!pending.ai_source_id)
            return invalid(id);
          auto source = read_field_ai_direct_source(
              common, world, *pending.ai_source_id, original_life);
          if (!source.ok)
            return {false, {}, source.error};
          const auto &fact = *source.value;
          if (fact.battle_id != session.battle_id || fact.formation_id != id ||
              fact.player_actor_id != pending.issuer_actor_id ||
              fact.mission_revision != pending.accepted_revision ||
              fact.ai_pursuit != pending.ai_pursuit ||
              !same_field_battle_mission(*fact.player_mission,
                                         pending.mission) ||
              fact.at.tick != pending.apply_at.tick ||
              fact.at.t != pending.apply_at.t)
            return invalid(id);
          const auto *direct =
              std::get_if<FieldDirectControl>(&formation.control);
          if (pending.command_result || !direct ||
              direct->controller_actor_id != pending.issuer_actor_id ||
              session.sides[formation.side].commander_actor_id !=
                  pending.issuer_actor_id ||
              !std::holds_alternative<FieldAiController>(
                  session.sides[formation.side].controller))
            return invalid(id);
        } else {
          if (!pending.command_result || pending.ai_source_id)
            return invalid(id);
          auto valid = use(pending.command_result, "field_battle_direct",
                           pending.issuer_actor_id, "formation", id);
          if (!valid.ok)
            return valid;
          auto original =
              read_common_command_source(common, *pending.command_result);
          if (!original.ok)
            return {false, {}, original.error};
          auto actual = mutation(*original.value.result);
          if (!actual.ok)
            return {false, {}, actual.error};
          if (!std::holds_alternative<FieldDirectControl>(formation.control)) {
            const auto &side = session.sides[formation.side];
            if (side.delegated_by_actor_id != pending.issuer_actor_id ||
                !side.delegation_command_result)
              return invalid(id);
            auto delegation = read_common_command_source(
                common, *side.delegation_command_result);
            if (!delegation.ok)
              return {false, {}, delegation.error};
            if (delegation.value.history->actor_id != pending.issuer_actor_id ||
                delegation.value.request_seq <= pending.request_sequence)
              return invalid(id);
          }
          if (original.value.request_seq != pending.request_sequence ||
              actual.value->mission_revision != pending.accepted_revision ||
              !actual.value->mission || !actual.value->effective_at ||
              !same_field_battle_mission(*actual.value->mission,
                                         pending.mission) ||
              actual.value->effective_at->tick != pending.apply_at.tick ||
              actual.value->effective_at->t != pending.apply_at.t)
            return invalid(id);
        }
      }
      if (formation.ability)
        for (const auto &active : formation.ability->active) {
          if (active.command_result && active.order_id)
            return invalid(id);
          if (active.command_result) {
            auto original =
                read_common_command_source(common, *active.command_result);
            if (!original.ok)
              return {false, {}, original.error};
            auto actual = mutation(*original.value.result);
            if (!actual.ok)
              return {false, {}, actual.error};
            if (actual.value->formation_id != id ||
                actual.value->tactic_id != active.tactic_id ||
                actual.value->order_id ||
                actual.value->activation_id != active.activation_id ||
                actual.value->at.tick != active.started.tick ||
                actual.value->at.t != active.started.t)
              return invalid(active.activation_id);
          }
          if (active.order_id) {
            auto order = session.orders.find(*active.order_id);
            if (order == session.orders.end() ||
                !std::holds_alternative<FieldTacticIntent>(
                    order->second.payload) ||
                std::get<FieldTacticIntent>(order->second.payload).tactic_id !=
                    active.tactic_id ||
                order->second.target_formation_id != id ||
                order->second.issuer_actor_id != active.issuer_actor_id ||
                order->second.ai_issued != active.ai_issued ||
                order->second.status != FieldOrderStatus::Adopted ||
                !order->second.status_at ||
                order->second.status_at->tick != active.started.tick ||
                order->second.status_at->t != active.started.t)
              return invalid(id);
          }
          if (active.ai_issued && !active.order_id) {
            const FieldCouncilFact *accepted = nullptr;
            for (const auto &[source_id, fact] : world.council_facts)
              if (fact.kind == FieldCouncilFactKind::AiOrder &&
                  fact.activation_id == active.activation_id) {
                if (accepted || fact.battle_id != battle_id ||
                    fact.formation_id != id)
                  return invalid(active.activation_id);
                accepted = &fact;
              }
            if (!accepted)
              return invalid(active.activation_id);
            auto source = read_field_ai_command_source(
                common, world, accepted->fact_id, original_life);
            if (!source.ok)
              return {false, {}, source.error};
          }
          if ((!active.order_id && !active.ai_issued) !=
              bool(active.command_result))
            return invalid(active.activation_id);
          auto valid =
              use(active.command_result, "field_battle_tactic",
                  active.issuer_actor_id, "activation", active.activation_id);
          if (!valid.ok)
            return valid;
        }
    }
  }
  return common_success();
}
Status validate_field_battle_command_results(
    const CommonStore &common, const FieldBattleWorldState *world,
    const FamilyLifeHistoryRead4 *original_life) {
  if (world)
    for (const auto &[battle_id, session] : world->sessions)
      for (const auto &[order_id, order] : session.orders) {
        auto valid = validate_field_delivered_order_authority(
            common, *world, session, order, original_life);
        if (!valid.ok)
          return valid;
      }
  for (const auto &[key, cached] : common.state().results.rows()) {
    if (!cached)
      continue;
    if (cached->kind == "FieldArmyExitSelect") {
      if (!world) return invalid(key);
      auto typed = mutation(*cached);
      if (!typed.ok) return {false, {}, typed.error};
      const auto plan = world->army_exit_plans.find(*typed.value->army_exit_choice_id);
      if (plan == world->army_exit_plans.end() || !plan->second.player_command_result ||
          plan->second.player_command_result->domain != "common" ||
          plan->second.player_command_result->kind != "command_result" ||
          plan->second.player_command_result->id != cached->result_id)
        return invalid(key);
      auto valid = validate_field_army_exit_player_command_source(common, *world, plan->second);
      if (!valid.ok) return valid;
      continue;
    }
    if (!field_deployment_command_kind(cached->kind)) {
      if (cached->details &&
          std::holds_alternative<FieldBattleReceipt>(cached->details->data))
        return invalid(key);
      continue;
    }
    if (!world || cached->kind == "field_battle_read")
      return invalid(key);
    auto original =
        read_common_command_source(common, std::string_view(cached->result_id));
    if (!original.ok)
      return {false, {}, original.error};
    auto typed = mutation(*cached);
    if (!typed.ok)
      return {false, {}, typed.error};
    const auto &r = *typed.value;
    auto session = world->sessions.find(r.battle_id);
    if (session == world->sessions.end() || r.at.tick > cached->tick ||
        static_cast<__int128_t>(cached->tick) - r.at.tick > 1 ||
        std::tie(r.at.tick, r.at.t) >
            std::tie(session->second.now.tick, session->second.now.t))
      return invalid(key);
    if (r.actual_control_acceptance) {
      auto actual = validate_field_control_acceptance(
          common, session->second, *r.actual_control_acceptance);
      if (!actual.ok)
        return actual;
      if (r.actual_control_acceptance->observed_at.half_day_tick !=
          cached->tick)
        return invalid(key);
    }
    if (r.formation_id && !session->second.formations.contains(*r.formation_id))
      return invalid(key);
    if (cached->kind == "field_battle_dispatch" ||
        cached->kind == "field_battle_rejoin") {
      if (!r.formation_id)
        return invalid(key);
      const auto &formation = session->second.formations.at(*r.formation_id);
      Integer retained = 0;
      for (const auto &source : formation.control_change_sources)
        if (source.domain == "common" && source.kind == "command_result" &&
            source.id == cached->result_id)
          ++retained;
      if (retained != 1)
        return invalid(key);
    }
    if (cached->kind == "field_council_adopt") {
      if (!r.council_fact_id)
        return invalid(key);
      auto fact = world->council_facts.find(*r.council_fact_id);
      if (fact == world->council_facts.end() || !fact->second.command_result ||
          fact->second.command_result->id != cached->result_id)
        return invalid(key);
    } else if (cached->kind == "field_council_speak") {
      bool found = false;
      for (const auto &c : session->second.councils) {
        for (const auto &p : c.proposals)
          if (r.proposal_id == p.proposal_id &&
              p.speaker_actor_id == original.value.history->actor_id &&
              p.command_result && p.command_result->id == cached->result_id)
            found = true;
        if (r.request_index && *r.request_index >= 0 &&
            static_cast<std::size_t>(*r.request_index) < c.requests.size()) {
          const auto &request =
              c.requests[static_cast<std::size_t>(*r.request_index)];
          if (request.actor_id == original.value.history->actor_id &&
              request.command_result &&
              request.command_result->id == cached->result_id)
            found = true;
        }
      }
      if (!found)
        return invalid(key);
    } else if (cached->kind == "field_battle_direct" ||
               cached->kind == "field_battle_order" ||
               cached->kind == "field_battle_dispatch" ||
               cached->kind == "field_battle_rejoin" ||
               cached->kind == "field_battle_delegate" ||
               cached->kind == "field_battle_tactic") {
      if (r.council_fact_id) {
        auto fact = world->council_facts.find(*r.council_fact_id);
        if (fact == world->council_facts.end() ||
            !fact->second.command_result ||
            fact->second.command_result->id != cached->result_id)
          return invalid(key);
        auto valid = fact_source(common, *world, fact->second, original_life);
        if (!valid.ok)
          return valid;
      }
    } else if (cached->kind == "field_battle_stratagem") {
      // 缓存也必须反向命中唯一实际附件或混乱记录；不能只因存在同场结果放行。
      Integer matched = 0;
      const auto matches = [&](const std::optional<SourceRef> &source) {
        return source && source->domain == "common" &&
               source->kind == "command_result" &&
               source->id == cached->result_id;
      };
      if (r.proposal_id && r.request_index) {
        for (Integer side = 0; side < 2; ++side)
          for (const auto &proposal : session->second.councils[side].proposals)
            if (proposal.proposal_id == *r.proposal_id &&
                session->second.sides[side].commander_actor_id ==
                    original.value.history->actor_id &&
                *r.request_index >= 0 &&
                std::size_t(*r.request_index) <
                    proposal.plan.stratagems.size() &&
                matches(proposal.plan.stratagems[std::size_t(*r.request_index)]
                            .command_result))
              ++matched;
      } else if (r.stratagem_id) {
        const auto found = session->second.stratagems.find(*r.stratagem_id);
        if (found != session->second.stratagems.end()) {
          const auto &row = found->second;
          if (row.kind == FieldStratagemKind::Confuse &&
              row.issuer_actor_id == original.value.history->actor_id &&
              matches(row.command_result) && r.effective_at && row.resolve_at &&
              row.issued.tick == r.at.tick && row.issued.t == r.at.t &&
              row.resolve_at->tick == r.effective_at->tick &&
              row.resolve_at->t == r.effective_at->t)
            ++matched;
        }
      }
      if (matched != 1)
        return invalid(key);
      // 下方统一原 source visitor 继续核附件正文、原结果槽与真实来源。
    } else if (cached->kind != "field_battle_set_deployment" &&
               cached->kind != "field_battle_start")
      return common_failure("NOT_IMPLEMENTED",
                            "field_battle/command_result_source",
                            ErrorKind::unsupported_capability);
  }
  if (world)
    return visit_field_battle_command_sources(
        common, *world, [](const SourceRef &) { return common_success(); },
        original_life);
  return common_success();
}
namespace {
Outcome<bool> ai_reinforcement_permission(const CommonStore &common,
                                          const FieldBattleWorldState &world,
                                          const FieldBattleSession &session,
                                          const FieldFormation &formation,
                                          const FieldBattleTime &at,
                                          const FieldCouncilFact *actual_ai,
                                          const FamilyLifeHistoryRead4 *life) {
  bool entry = false;
  for (const auto &[id, boundary] : world.boundary_contacts)
    if (boundary.battle_id == session.battle_id && boundary.reinforcement_queue)
      for (const auto &admitted : boundary.reinforcement_queue->admitted)
        if (admitted.formation_id == formation.formation_id)
          entry = true;
  if (!entry)
    return {true, true, {}};
  auto original = read_field_reinforcement_admission_identity(
      common, world, session, formation);
  if (!original.ok)
    return {false, false, original.error};
  auto binding = read_field_original_battle_binding(
      common, world, session.battle_id, formation.army_id);
  if (!binding.ok)
    return {false, false, binding.error};
  // The immutable actual admission identifies player-owned waiting control;
  // a later global player handoff must not rewrite this source's authority.
  if (original.value->ai_mission)
    return {true, true, {}};
  if (!original.value->pending_player_entry_order)
    return {false, false, invalid(formation.formation_id).error};
  const auto &player = binding.value->commander_actor_id;
  if (actual_ai) {
    if (!actual_ai->ai_control_acceptance)
      return {false, false, invalid(formation.formation_id).error};
    auto controlled = read_field_original_player_control_at_boundary(
        common, world, session, formation, player,
        *actual_ai->ai_control_acceptance, life);
    if (!controlled.ok)
      return {false, false, controlled.error};
    return {true, !controlled.value, {}};
  }
  auto lost = read_field_original_player_local_loss(common, world, session,
                                                    formation.side, player);
  if (!lost.ok)
    return {false, false, lost.error};
  if (lost.value &&
      std::tie(lost.value->tick, lost.value->t) <= std::tie(at.tick, at.t))
    return {true, true, {}};
  // No Family death is invented during the still-unsettled local window.
  if (life) {
    if (!life->bound_to(common))
      return {false, false, invalid(formation.formation_id).error};
    auto death = life->read_actor_death(player);
    if (!death.ok)
      return {false, false, death.error};
    if (death.value)
      return {true, true, {}};
  }
  return {true, false, {}};
}
} // namespace
Outcome<bool> read_field_current_ai_formation_permission(
    const CommonStore &common, const FieldBattleWorldState &world,
    const FieldBattleSession &session, const FieldFormation &formation,
    const FamilyLifeHistoryRead4 *life) {
  if (!world.sessions.contains(session.battle_id) ||
      &world.sessions.at(session.battle_id) != &session ||
      !session.formations.contains(formation.formation_id) ||
      &session.formations.at(formation.formation_id) != &formation)
    return {false, false, invalid(formation.formation_id).error};
  return ai_reinforcement_permission(common, world, session, formation,
                                     session.now, nullptr, life);
}
Status validate_field_current_ai_side_authority(
    const CommonStore &common, const FieldBattleWorldState &world,
    const FieldBattleSession &session, Integer side, const StableId &actor,
    const FamilyLifeHistoryRead4 *life) {
  if (!world.sessions.contains(session.battle_id) ||
      &world.sessions.at(session.battle_id) != &session || side < 0 ||
      side > 1 || session.sides[side].commander_actor_id != actor ||
      !std::holds_alternative<FieldAiController>(
          session.sides[side].controller))
    return invalid(actor);
  const FieldCouncilFact *adopted = nullptr;
  for (const auto &[id, row] : world.council_facts)
    if (row.kind == FieldCouncilFactKind::Adopted &&
        row.battle_id == session.battle_id && row.side == side) {
      if (adopted || id != row.fact_id)
        return invalid(actor);
      adopted = &row;
    }
  if (!adopted)
    return invalid(actor);
  auto checked = fact_source(common, world, *adopted, life);
  if (!checked.ok)
    return checked;
  if (adopted->council.player_actor_id != actor)
    return common_success();
  auto lost = read_field_original_player_local_loss(common, world, session,
                                                    side, actor);
  if (!lost.ok)
    return {false, {}, lost.error};
  if (lost.value)
    return common_success();
  if (life) {
    if (!life->bound_to(common))
      return invalid(actor);
    auto death = life->read_actor_death(actor);
    if (!death.ok)
      return {false, {}, death.error};
    if (death.value)
      return common_success();
  }
  const auto &control = session.sides[side];
  if (control.delegated_by_actor_id != actor ||
      !control.delegation_command_result)
    return invalid(actor);
  auto command =
      read_common_command_source(common, *control.delegation_command_result);
  if (!command.ok)
    return {false, {}, command.error};
  if (command.value.history->actor_id != actor ||
      command.value.result->kind != "field_battle_delegate")
    return invalid(actor);
  auto detail = mutation(*command.value.result);
  if (!detail.ok)
    return {false, {}, detail.error};
  if (detail.value->battle_id != session.battle_id ||
      detail.value->council_fact_id ||
      std::tie(detail.value->at.tick, detail.value->at.t) >
          std::tie(session.now.tick, session.now.t))
    return invalid(actor);
  return common_success();
}
Outcome<const FieldCouncilFact *> read_field_ai_command_source(
    const CommonStore &common, const FieldBattleWorldState &world,
    const StableId &id, const FamilyLifeHistoryRead4 *original_life) {
  auto fail = [&]() -> Outcome<const FieldCouncilFact *> {
    return {false, {}, invalid(id).error};
  };
  auto found = world.council_facts.find(id);
  const auto *head = common.state().facts.find(id);
  if (found == world.council_facts.end() || !head ||
      head->domain != "military" || head->kind != "military_council")
    return fail();
  const auto &f = found->second;
  auto session = world.sessions.find(f.battle_id);
  if (f.fact_id != id || f.kind != FieldCouncilFactKind::AiOrder ||
      session == world.sessions.end() || f.side < 0 || f.side > 1 ||
      f.recorded_tick < session->second.created_tick ||
      f.recorded_tick > common.tick() ||
      std::tie(f.at.tick, f.at.t) >
          std::tie(session->second.now.tick, session->second.now.t) ||
      f.command_result || f.adopted_actual_at || !f.player_actor_id ||
      !f.player_command_kind || !f.formation_id ||
      !session->second.formations.contains(*f.formation_id) ||
      session->second.formations.at(*f.formation_id).side != f.side ||
      session->second.sides[f.side].commander_actor_id != *f.player_actor_id ||

      f.council.player_actor_id || !f.council.participants.empty() ||
      !f.council.proposals.empty() || !f.council.requests.empty() ||
      f.council.closed || f.council.adopted_proposal_id ||
      f.council.intel.level != 0 ||
      f.council.intel.total_estimate != std::array<Integer, 2>{0, 0} ||
      f.council.intel.formations || f.council.intel.commanders ||
      f.council.intel.enemy_plan)
    return fail();
  const bool direct = *f.player_command_kind == "field_battle_direct";
  const bool order_command = *f.player_command_kind == "field_battle_order";
  const bool tactic_command = *f.player_command_kind == "field_battle_tactic";
  if (!direct && !order_command && !tactic_command)
    return fail();
  if (!f.ai_control_acceptance)
    return fail();
  if (f.ai_control_acceptance) {
    const auto &cut = *f.ai_control_acceptance;
    auto verified =
        validate_field_control_acceptance(common, session->second, cut);
    if (!verified.ok)
      return {false, {}, verified.error};
    auto actual = read_actual_common_fact_position(common, *head);
    if (!actual.ok)
      return {false, {}, actual.error};
    if (cut.common_prefix.common_fact_count < 0 ||
        static_cast<std::size_t>(cut.common_prefix.common_fact_count) !=
            actual.value)
      return fail();
    for (const auto &e : session->second.events) {
      if (e.sequence <= cut.last_local_event_sequence &&
          std::tie(e.at.tick, e.at.t) > std::tie(f.at.tick, f.at.t))
        return fail();
      if (e.sequence > cut.last_local_event_sequence &&
          std::tie(e.at.tick, e.at.t) < std::tie(f.at.tick, f.at.t))
        return fail();
    }
  }

  if (direct) {
    if (f.order_id || f.tactic_id || f.activation_id || !f.player_mission ||
        !f.mission_revision || *f.mission_revision <= 0)
      return fail();
  } else if (order_command) {
    if (!f.order_id || !f.player_mission || f.mission_revision || f.tactic_id ||
        f.activation_id)
      return fail();
  } else if (!f.tactic_id || f.player_mission || f.mission_revision ||
             bool(f.order_id) == bool(f.activation_id) || f.ai_pursuit) {
    return fail();
  }
  if (f.ai_pursuit && (!f.player_mission ||
                       !std::holds_alternative<FieldEngage>(*f.player_mission)))
    return fail();
  if (f.order_id) {
    const auto order = session->second.orders.find(*f.order_id);
    if (order == session->second.orders.end() || !order->second.ai_issued ||
        order->second.command_result ||
        order->second.issuer_actor_id != *f.player_actor_id ||
        order->second.target_formation_id != *f.formation_id ||
        std::tie(order->second.issued.tick, order->second.issued.t) !=
            std::tie(f.at.tick, f.at.t) ||
        order->second.ai_pursuit != f.ai_pursuit)
      return fail();
    if (order_command) {
      const auto *mission = std::get_if<FieldMission>(&order->second.payload);
      if (!mission || !same_field_battle_mission(*mission, *f.player_mission))
        return fail();
    } else {
      const auto *intent =
          std::get_if<FieldTacticIntent>(&order->second.payload);
      if (!intent || intent->tactic_id != *f.tactic_id)
        return fail();
    }
  }
  if (f.activation_id) {
    const auto &formation = session->second.formations.at(*f.formation_id);
    if (!formation.ability)
      return fail();
    const FieldTacticActivation *source = nullptr;
    for (const auto &active : formation.ability->active)
      if (active.activation_id == *f.activation_id) {
        if (source)
          return fail();
        source = &active;
      }
    if (!source || !source->ai_issued || source->command_result ||
        source->order_id || source->issuer_actor_id != *f.player_actor_id ||
        source->tactic_id != *f.tactic_id ||
        std::tie(source->started.tick, source->started.t) !=
            std::tie(f.at.tick, f.at.t))
      return fail();
  }
  // 初始控制取真正 Adopted 军议头；玩家委托取原成功封口，不能以今天
  // AiController 倒证历史。
  const FieldCouncilFact *adoption = nullptr;
  for (const auto &[key, row] : world.council_facts)
    if (row.kind == FieldCouncilFactKind::Adopted &&
        row.battle_id == f.battle_id && row.side == f.side) {
      if (adoption)
        return fail();
      const auto *original = common.state().facts.find(key);
      if (!original || original->domain != "military" ||
          original->kind != "military_council")
        return fail();
      adoption = &row;
    }
  if (!adoption)
    return fail();
  auto adopted_payload = fact_source(common, world, *adoption, original_life);
  if (!adopted_payload.ok)
    return {false, {}, adopted_payload.error};
  auto serial = [&](const StableId &value) -> Integer {
    const auto colon = value.rfind(':');
    if (colon == StableId::npos || colon + 1 == value.size() ||
        value[colon + 1] == '0')
      return 0;
    Integer n = 0;
    const char *first = value.data() + colon + 1,
               *last = value.data() + value.size();
    auto parsed = std::from_chars(first, last, n);
    return parsed.ec == std::errc{} && parsed.ptr == last && n > 0 &&
                   n < common.state().next_instance_id
               ? n
               : 0;
  };
  const auto actual_serial = serial(f.fact_id);
  auto actual_position = read_actual_common_fact_position(
      common, SourceRefBuildView{"military", "military_council", f.fact_id});
  if (!actual_position.ok)
    return {false, {}, actual_position.error};
  auto adopted_position = read_actual_common_fact_position(
      common,
      SourceRefBuildView{"military", "military_council", adoption->fact_id});
  if (!adopted_position.ok)
    return {false, {}, adopted_position.error};
  // Allocated ID order alone is not the actual Common append prefix.
  if (!actual_serial || adopted_position.value >= actual_position.value)
    return fail();
  auto recipient = ai_reinforcement_permission(
      common, world, session->second,
      session->second.formations.at(*f.formation_id), f.at, &f, original_life);
  if (!recipient.ok)
    return {false, {}, recipient.error};
  if (!recipient.value)
    return fail();
  if (adoption->council.player_actor_id == f.player_actor_id) {
    auto controlled = read_field_original_player_control_at_boundary(
        common, world, session->second,
        session->second.formations.at(*f.formation_id), *f.player_actor_id,
        *f.ai_control_acceptance, original_life);
    if (!controlled.ok)
      return {false, {}, controlled.error};
    if (controlled.value)
      return fail();
  }
  return {true, &f, {}};
}
Status validate_field_reinforcement_ai_control_source(
    const CommonStore &common, const FieldBattleWorldState &world,
    const FieldBattleSession &session, const FieldFormation &formation,
    const FamilyLifeHistoryRead4 *original_life) {
  const auto bad = [&] {
    return common_failure("FieldReinforcementAiAuthorityRequired",
                          formation.formation_id);
  };
  auto admitted = read_field_reinforcement_admission_identity(
      common, world, session, formation);
  if (!admitted.ok)
    return {false, {}, admitted.error};
  const auto &entry = *admitted.value;
  if (!entry.ai_mission || formation.side < 0 || formation.side > 1)
    return bad();
  const auto &side = session.sides[formation.side];
  const FieldCouncilFact *adoption = nullptr;
  for (const auto &[id, row] : world.council_facts)
    if (row.kind == FieldCouncilFactKind::Adopted &&
        row.battle_id == session.battle_id && row.side == formation.side) {
      if (adoption || id != row.fact_id)
        return bad();
      adoption = &row;
    }
  if (!adoption)
    return bad();
  auto adopted = fact_source(common, world, *adoption, original_life);
  if (!adopted.ok)
    return {false, {}, adopted.error};
  auto actual_cut =
      read_actual_common_fact_position(common, entry.application_source);
  if (!actual_cut.ok)
    return {false, {}, actual_cut.error};
  auto adopted_cut = read_actual_common_fact_position(
      common,
      SourceRefBuildView{"military", "military_council", adoption->fact_id});
  if (!adopted_cut.ok)
    return {false, {}, adopted_cut.error};
  if (adopted_cut.value >= actual_cut.value)
    return bad();
  // An AI-led side may include a player subordinate; that is not player
  // command of the side. Keep the original council's actual actor identity.
  if (adoption->council.player_actor_id != side.commander_actor_id)
    return common_success();
  const auto &player = side.commander_actor_id;
  auto lost = read_field_original_player_local_loss(common, world, session,
                                                    formation.side, player);
  if (!lost.ok)
    return {false, {}, lost.error};
  if (lost.value) {
    const FieldBattleEvent *loss_event = nullptr;
    for (const auto &event : session.events) {
      if (event.kind != field_event_kind::CommanderLost ||
          event.side != formation.side || event.count != 2 ||
          event.at.tick != lost.value->tick || event.at.t != lost.value->t ||
          !event.formation_id)
        continue;
      const auto f = session.formations.find(*event.formation_id);
      if (f == session.formations.end() ||
          f->second.commander_actor_id != player)
        continue;
      if (loss_event)
        return bad();
      loss_event = &event;
    }
    if (!loss_event)
      return bad();
    if (loss_event->sequence < entry.event_sequence)
      return common_success();
  }
  if (!original_life || !original_life->bound_to(common))
    return common_failure("FieldPlayerOriginalLifePrefixRequired", player);
  // Derive the exact original prefix from this real Entry head, never from
  // result ID allocation order or a world moment constructed from local time.
  auto before =
      read_actual_common_fact_prefix_before(common, entry.application_source);
  if (!before.ok)
    return {false, {}, before.error};
  FieldControlAcceptanceBoundary original_cut;


  original_cut.copied_fields = CheckpointCopyLease::reserve();
  original_cut.common_prefix.common_fact_count = before.value.common_fact_count;
  if (const auto *h = before.value.last_common_fact)
    original_cut.common_prefix.last_common_fact =
        SourceRef{checkpoint_fresh_string_copy(h->domain),
                  checkpoint_fresh_string_copy(h->kind),
                  checkpoint_fresh_string_copy(h->id)};
  original_cut.observed_at = entry.applied_at;
  original_cut.last_local_event_sequence = entry.event_sequence - 1;
  auto death = original_life->read_actor_death_record_at_original_prefix(
      player, original_cut.observed_at, original_cut.common_prefix);
  if (!death.ok)
    return {false, {}, death.error};
  if (death.value)
    return common_success();
  if (side.delegated_by_actor_id != player || !side.delegation_command_result)
    return bad();
  auto delegated =
      read_common_command_source(common, *side.delegation_command_result);
  if (!delegated.ok)
    return {false, {}, delegated.error};
  if (delegated.value.history->actor_id != player ||
      delegated.value.result->kind != "field_battle_delegate")
    return bad();
  auto detail = mutation(*delegated.value.result);
  if (!detail.ok)
    return {false, {}, detail.error};
  if (detail.value->battle_id != session.battle_id ||
      !detail.value->actual_control_acceptance)
    return bad();
  const auto &accepted = *detail.value->actual_control_acceptance;
  auto checked = validate_field_control_acceptance(common, session, accepted);
  if (!checked.ok)
    return checked;
  auto time = compare_world_moments(accepted.observed_at, entry.applied_at);
  if (!time.ok)
    return {false, {}, time.error};
  return time.value <= 0 &&
                 accepted.common_prefix.common_fact_count <=
                     original_cut.common_prefix.common_fact_count &&
                 accepted.last_local_event_sequence < entry.event_sequence
             ? common_success()
             : bad();
}
Outcome<const FieldCouncilFact *> read_field_ai_direct_source(
    const CommonStore &common, const FieldBattleWorldState &world,
    const StableId &id, const FamilyLifeHistoryRead4 *original_life) {
  auto result = read_field_ai_command_source(common, world, id, original_life);
  if (!result.ok)
    return result;
  if (result.value->player_command_kind != StableId("field_battle_direct"))
    return {false, {}, invalid(id).error};
  return result;
}
Outcome<const FieldDeploymentApplied *>
read_field_original_deployment_application(const CommonStore &common,
                                           const FieldBattleSession &session,
                                           const FieldFormation &formation) {
  auto fail = [&]() -> Outcome<const FieldDeploymentApplied *> {
    return {false, nullptr, invalid(formation.formation_id).error};
  };
  if (!formation.deployment_command_result)
    return fail();
  auto original =
      read_common_command_source(common, *formation.deployment_command_result);
  if (!original.ok)
    return {false, nullptr, original.error};
  auto valid = record_source(common, *formation.deployment_command_result,
                             session.battle_id, "field_battle_set_deployment",
                             {}, "formation", formation.formation_id);
  if (!valid.ok)
    return {false, nullptr, valid.error};
  auto value = mutation(*original.value.result);
  if (!value.ok)
    return {false, nullptr, value.error};
  if (!value.value->deployment || !value.value->mission_revision ||
      value.value->state != FieldBattleState::Deploying ||
      !value.value->effective_at ||
      (session.started_at &&
       std::tie(value.value->effective_at->tick, value.value->effective_at->t) >
           std::tie(session.started_at->tick, session.started_at->t)))
    return fail();
  return {true, &*value.value->deployment, {}};
}

Status validate_field_navigation_mission_source(
    const CommonStore &common, const FieldBattleWorldState &world,
    const FieldBattleSession &session, const FieldFormation &formation,
    const FieldBattleEvent &event, const FieldInitialPlacementRead &initial,
    const FamilyLifeHistoryRead4 *original_life) {
  if (event.kind != field_event_kind::NavigationObserved ||
      event.formation_id != formation.formation_id ||
      !field_event_navigation_observation(event))
    return invalid(formation.formation_id);
  const auto &n = *field_event_navigation_observation(event);
  auto same_optional = [](const std::optional<SourceRef> &a,
                          const std::optional<SourceRef> &b) {
    return bool(a) == bool(b) && (!a || same_source_ref(*a, *b));
  };
  if (int(bool(n.original_command_result)) +
              int(bool(n.original_ai_source_id)) +
              int(bool(n.original_order_id)) >
          1 ||
      n.mission_revision < 0)
    return invalid(formation.formation_id);
  const FieldMission *mission = &n.mission;
  bool pursuit = n.ai_pursuit;
  auto sequence = n.mission_transition_sequence;
  Integer limit = event.sequence;
  while (sequence) {
    const FieldBattleEvent *found = nullptr;
    for (const auto &e : session.events)
      if (e.sequence == *sequence) {
        if (found)
          return invalid(formation.formation_id);
        found = &e;
      }
    if (!found || found->sequence >= limit ||
        found->formation_id != event.formation_id ||
        found->kind != field_event_kind::MissionTransition ||
        !field_event_mission_transition(*found) ||
        std::tie(found->at.tick, found->at.t) >
            std::tie(n.evaluated_at.tick, n.evaluated_at.t))
      return invalid(formation.formation_id);
    const auto &t = *field_event_mission_transition(*found);
    if (t.mission_revision != n.mission_revision ||
        !same_optional(t.original_command_result, n.original_command_result) ||
        t.original_ai_source_id != n.original_ai_source_id ||
        t.original_order_id != n.original_order_id ||
        t.to_ai_pursuit != pursuit ||
        !same_field_battle_mission(t.to, *mission))
      return invalid(formation.formation_id);
    mission = &t.from;
    pursuit = t.from_ai_pursuit;
    limit = found->sequence;
    sequence = t.previous_sequence;
  }
  std::optional<Integer> latest;
  for (const auto &e : session.events)
    if (e.sequence < event.sequence && e.formation_id == event.formation_id &&
        field_event_mission_transition(e)) {
      const auto &t = *field_event_mission_transition(e);
      if (t.mission_revision == n.mission_revision &&
          same_optional(t.original_command_result, n.original_command_result) &&
          t.original_ai_source_id == n.original_ai_source_id &&
          t.original_order_id == n.original_order_id)
        latest = e.sequence;
    }
  if (latest != n.mission_transition_sequence)
    return invalid(formation.formation_id);
  if (n.original_command_result) {
    auto source =
        read_common_command_source(common, *n.original_command_result);
    if (!source.ok)
      return {false, {}, source.error};
    const auto &kind = source.value.result->kind;
    if (kind != "field_battle_direct" && kind != "field_battle_dispatch")
      return invalid(formation.formation_id);
    auto valid =
        record_source(common, *n.original_command_result, session.battle_id,
                      kind, {}, "formation", formation.formation_id);
    if (!valid.ok)
      return valid;
    auto value = mutation(*source.value.result);
    if (!value.ok)
      return {false, {}, value.error};
    if (!value.value->mission ||
        value.value->mission_revision != n.mission_revision || pursuit ||
        !same_field_battle_mission(*value.value->mission, *mission) ||
        std::tie(value.value->at.tick, value.value->at.t) >
            std::tie(n.evaluated_at.tick, n.evaluated_at.t))
      return invalid(formation.formation_id);
  } else if (n.original_ai_source_id) {
    auto source = read_field_ai_direct_source(
        common, world, *n.original_ai_source_id, original_life);
    if (!source.ok)
      return {false, {}, source.error};
    const auto &f = *source.value;
    if (f.battle_id != session.battle_id ||
        f.formation_id != formation.formation_id || !f.player_mission ||
        f.mission_revision != n.mission_revision || f.ai_pursuit != pursuit ||
        !same_field_battle_mission(*f.player_mission, *mission) ||
        std::tie(f.at.tick, f.at.t) >
            std::tie(n.evaluated_at.tick, n.evaluated_at.t))
      return invalid(formation.formation_id);
  } else if (n.original_order_id) {
    auto it = session.orders.find(*n.original_order_id);
    if (it == session.orders.end())
      return invalid(formation.formation_id);
    const auto &o = it->second;
    if (o.target_formation_id != formation.formation_id ||
        o.status != FieldOrderStatus::Adopted || !o.status_at ||
        o.adopted_mission_revision != n.mission_revision ||
        !std::holds_alternative<FieldMission>(o.payload) ||
        o.ai_pursuit != pursuit ||
        !same_field_battle_mission(std::get<FieldMission>(o.payload),
                                   *mission) ||
        std::tie(o.status_at->tick, o.status_at->t) >
            std::tie(n.evaluated_at.tick, n.evaluated_at.t))
      return invalid(formation.formation_id);
  } else {
    if (pursuit)
      return invalid(formation.formation_id);
    if (initial.deployment_source) {
      auto source =
          read_common_command_source(common, *initial.deployment_source);
      if (!source.ok)
        return {false, {}, source.error};
      auto result = mutation(*source.value.result);
      if (!result.ok)
        return {false, {}, result.error};
      if (result.value->mission_revision != n.mission_revision)
        return invalid(formation.formation_id);
    } else if (n.mission_revision != 0)
      return invalid(formation.formation_id);
    if (initial.immediate_mission) {
      if (!same_field_battle_mission(*initial.immediate_mission, *mission))
        return invalid(formation.formation_id);
    } else if (initial.original_second_wave_hold) {
      if (!same_field_battle_mission(
              FieldMission{*initial.original_second_wave_hold}, *mission))
        return invalid(formation.formation_id);
    } else
      return invalid(formation.formation_id);
  }
  return common_success();
}

Status validate_field_actual_mission_application(
    const CommonStore &common, const FieldBattleWorldState &world,
    const FieldBattleSession &session, const FieldBattleEvent &event,
    const FamilyLifeHistoryRead4 *original_life) {
  if (event.kind != field_event_kind::MissionApplied ||
      !field_event_mission_application(event) || !event.formation_id ||
      !session.formations.contains(*event.formation_id) ||
      event.side != session.formations.at(*event.formation_id).side ||
      event.target_formation_id || event.source_id || event.count ||
      event.tactic || event.stratagem || event.success ||
      field_event_mission_transition(event) || field_event_navigation_observation(event) ||
      field_event_motion_application(event) || field_event_contact_phase_from(event))
    return invalid(session.battle_id);
  const auto &a = *field_event_mission_application(event);
  if (a.mission_revision <= 0 || int(bool(a.command_result)) +
                                         int(bool(a.ai_source_id)) +
                                         int(bool(a.order_id)) !=
                                     1)
    return invalid(*event.formation_id);
  const auto at = [&](const FieldBattleTime &v) {
    return v.tick == event.at.tick && v.t == event.at.t;
  };
  if (a.command_result) {
    auto source = read_common_command_source(common, *a.command_result);
    if (!source.ok)
      return {false, {}, source.error};
    const auto &kind = source.value.result->kind;
    if (kind != "field_battle_direct" && kind != "field_battle_dispatch")
      return invalid(*event.formation_id);
    auto valid = record_source(common, *a.command_result, session.battle_id,
                               kind, {}, "formation", *event.formation_id);
    if (!valid.ok)
      return valid;
    auto value = mutation(*source.value.result);
    if (!value.ok)
      return {false, {}, value.error};
    if (!value.value->mission || !value.value->effective_at ||
        !at(*value.value->effective_at) ||
        value.value->mission_revision != a.mission_revision || a.ai_pursuit ||
        !same_field_battle_mission(*value.value->mission, a.mission))
      return invalid(*event.formation_id);
  } else if (a.ai_source_id) {
    auto source = read_field_ai_direct_source(common, world, *a.ai_source_id,
                                              original_life);
    if (!source.ok)
      return {false, {}, source.error};
    const auto &v = *source.value;
    if (v.battle_id != session.battle_id ||
        v.formation_id != event.formation_id || !v.player_mission ||
        v.mission_revision != a.mission_revision || !at(v.at) ||
        v.ai_pursuit != a.ai_pursuit ||
        !same_field_battle_mission(*v.player_mission, a.mission))
      return invalid(*event.formation_id);
  } else {
    auto it = session.orders.find(*a.order_id);
    if (it == session.orders.end())
      return invalid(*event.formation_id);
    const auto &o = it->second;
    if (o.target_formation_id != *event.formation_id ||
        o.status != FieldOrderStatus::Adopted || !o.status_at ||
        !at(*o.status_at) || o.adopted_mission_revision != a.mission_revision ||
        !std::holds_alternative<FieldMission>(o.payload) ||
        o.ai_pursuit != a.ai_pursuit ||
        !same_field_battle_mission(std::get<FieldMission>(o.payload),
                                   a.mission))
      return invalid(*event.formation_id);
  }
  return common_success();
}
Status record_field_actual_mission_application(
    const CommonStore &common, FieldBattleWorldState &world,
    FieldBattleSession &session, FieldFormation &f,
    CheckpointMemoryCharge &owner,
    const FamilyLifeHistoryRead4 *original_life) {
  if (!world.sessions.contains(session.battle_id) ||
      &world.sessions.at(session.battle_id) != &session ||
      !session.formations.contains(f.formation_id) ||
      &session.formations.at(f.formation_id) != &f ||
      session.state != FieldBattleState::Running || f.pending_direct ||
      f.mission_transition_sequence || session.next_event_sequence <= 0 ||
      session.next_event_sequence == std::numeric_limits<Integer>::max())
    return invalid(f.formation_id);
  for (const auto &e : session.events)
    if (e.formation_id == f.formation_id && field_event_mission_application(e))
      if (field_event_mission_application(e)->mission_revision >= f.mission_revision)
        return invalid(f.formation_id);


  (owner).grow(0);
  FieldBattleEvent event;
  event.sequence = session.next_event_sequence;
  event.at = session.now;
  event.kind = field_event_kind::MissionApplied;
  event.formation_id = f.formation_id;
  event.side = f.side;
  field_event_mission_application(event) = FieldMissionApplication{f.mission,
                                                      f.mission_revision,
                                                      f.ai_pursuit,
                                                      f.mission_command_result,
                                                      f.mission_ai_source_id,
                                                      f.adopted_order_id};
  auto valid = validate_field_actual_mission_application(common, world, session,
                                                         event, original_life);
  if (!valid.ok)
    return valid;
  ++session.next_event_sequence;
  session.events.push_back(std::move(event));
  f.flank_assignment_index.reset();
  return common_success();
}

Status validate_field_mission_application_history(
    const CommonStore &common, const FieldBattleWorldState &world,
    const FieldBattleSession &session,
    const FamilyLifeHistoryRead4 *original_life) {
  std::map<StableId, const FieldMissionApplication *> latest;
  std::set<StableId> adopted_orders;
  auto pins = [](const FieldMissionApplication &a, const auto &command,
                 const auto &ai, const auto &order, Integer revision) {
    return a.mission_revision == revision && a.ai_source_id == ai &&
           a.order_id == order && bool(a.command_result) == bool(command) &&
           (!command || same_source_ref(*a.command_result, *command));
  };
  for (const auto &e : session.events) {
    if (bool(field_event_mission_application(e)) !=
        (e.kind == field_event_kind::MissionApplied))
      return invalid(session.battle_id);
    if (field_event_mission_application(e)) {
      auto valid = validate_field_actual_mission_application(
          common, world, session, e, original_life);
      if (!valid.ok)
        return valid;
      const auto &a = *field_event_mission_application(e);
      auto prior = latest.find(*e.formation_id);
      if ((prior != latest.end() &&
           prior->second->mission_revision >= a.mission_revision) ||
          (a.order_id && !adopted_orders.insert(*a.order_id).second))
        return invalid(*e.formation_id);
      latest[*e.formation_id] = &a;
    } else if (field_event_navigation_observation(e) || field_event_mission_transition(e)) {
      if (!e.formation_id)
        return invalid(session.battle_id);
      const auto prior = latest.find(*e.formation_id);
      if (field_event_navigation_observation(e)) {
        const auto &n = *field_event_navigation_observation(e);
        if (prior == latest.end()) {
          if (n.original_command_result || n.original_ai_source_id ||
              n.original_order_id)
            return invalid(*e.formation_id);
        } else if (!pins(*prior->second, n.original_command_result,
                         n.original_ai_source_id, n.original_order_id,
                         n.mission_revision))
          return invalid(*e.formation_id);
      } else {
        const auto &t = *field_event_mission_transition(e);
        if (prior == latest.end()) {
          if (t.original_command_result || t.original_ai_source_id ||
              t.original_order_id)
            return invalid(*e.formation_id);
        } else if (!pins(*prior->second, t.original_command_result,
                         t.original_ai_source_id, t.original_order_id,
                         t.mission_revision))
          return invalid(*e.formation_id);
      }
    }
  }
  for (const auto &[id, o] : session.orders) {
    const bool applied = o.status == FieldOrderStatus::Adopted &&
                         std::holds_alternative<FieldMission>(o.payload);
    if (applied != adopted_orders.contains(id))
      return invalid(id);
  }
  for (const auto &[id, f] : session.formations) {
    auto current = latest.find(id);
    if (current == latest.end()) {
      if (f.mission_command_result || f.mission_ai_source_id ||
          f.adopted_order_id)
        return invalid(id);
    } else {
      const auto &a = *current->second;
      if (!pins(a, f.mission_command_result, f.mission_ai_source_id,
                f.adopted_order_id, a.mission_revision) ||
          (!f.pending_direct && f.mission_revision != a.mission_revision) ||
          (f.pending_direct &&
           f.pending_direct->accepted_revision <= a.mission_revision))
        return invalid(id);
      auto origin = read_field_current_mission_origin(session, f);
      if (!origin.ok)
        return {false, {}, origin.error};
      if (!same_field_battle_mission(a.mission, *origin.value.mission) ||
          a.ai_pursuit != origin.value.ai_pursuit)
        return invalid(id);
    }
  }
  return common_success();
}
} // namespace hanmo::resident
