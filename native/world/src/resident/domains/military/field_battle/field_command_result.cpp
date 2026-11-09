// 回执从正式 writer 刚改变的同会话读取，后继能力缺实现时仍明确拒绝。
#include "field_result_codec.hpp"
namespace hanmo::resident {
Status validate_field_battle_receipt(const StableId &kind,
                                     const FieldBattleReceipt &value) {
  auto bad = []() { return common_failure("FieldBattleReceiptKindMismatch"); };
  if (kind != "FieldArmyExitSelect" && !field_deployment_command_kind(kind))
    return bad();
  if (kind == "field_battle_read") {
    auto *view = std::get_if<FieldBattleView>(&value.result);
    return view && !view->identity.battle_id.empty() &&
                   !view->identity.observer_actor_id.empty() &&
                   view->identity.revision >= 0 &&
                   !view->field.field_hash.empty()
               ? common_success()
               : bad();
  }
  if (kind == "field_battle_advance") {
    auto *result = std::get_if<FieldBattleAdvanceResult>(&value.result);
    if (!result || result->battle_id.empty() || result->from.tick < 0 ||
        result->from.t < 0 || result->to.t < 0)
      return bad();
    // 窗末沿半开 BattleTime 归一到下一 tick 的零秒，不能要求回执仍同 tick。
    const bool same_window =
        result->from.tick == result->to.tick && result->from.t <= result->to.t;
    const bool window_end =
        static_cast<__int128_t>(result->from.tick) + 1 == result->to.tick &&
        result->to.t == 0;
    if (!same_window && !window_end)
      return bad();
    const auto before = [](const FieldBattleTime &a, const FieldBattleTime &b) {
      return a.tick < b.tick || (a.tick == b.tick && a.t < b.t);
    };
    // 这里只验有限公开回执形状；真实事件与接受边界由原来源门核对。
    Integer previous = 0;
    for (const auto &event : result->events) {
      if (event.sequence <= previous || event.at.tick < 0 || event.at.t < 0 ||
          before(event.at, result->from) || before(result->to, event.at) ||
          (event.formation_id && event.formation_id->empty()) ||
          (event.target_formation_id && event.target_formation_id->empty()) ||
          (event.count && *event.count < 0))
        return bad();
      previous = event.sequence;
      const bool known =
          event.kind == "contact" || event.kind == "casualties" ||
          event.kind == "rout" || event.kind == "rallied" ||
          event.kind == "commander_lost" || event.kind == "order_arrived" ||
          event.kind == "entered" || event.kind == "withdrawn" ||
          event.kind == "ended" || event.kind == "council_adopted" ||
          event.kind == "request_granted" || event.kind == "request_denied" ||
          event.kind == "tactic" || event.kind == "stratagem" ||
          event.kind == "revealed";
      if (!known ||
          (event.tactic && *event.tactic != "charge" &&
           *event.tactic != "volley" && *event.tactic != "rally" &&
           *event.tactic != "guard") ||
          (event.stratagem && *event.stratagem != "ambush" &&
           *event.stratagem != "feint_strength" &&
           *event.stratagem != "confuse"))
        return bad();
    }
    if (result->result &&
        (result->result->winner_side < -1 || result->result->winner_side > 1 ||
         result->result->reason.empty() || result->result->ended.tick < 0 ||
         result->result->ended.t < 0 ||
         before(result->to, result->result->ended)))
      return bad();
    return common_success();
  }
  auto *r = std::get_if<FieldBattleMutationResult>(&value.result);
  if (!r || r->battle_id.empty() || r->at.tick < 0 || r->at.t < 0 ||
      static_cast<Integer>(r->state) < 0 || static_cast<Integer>(r->state) > 3)
    return bad();
  for (const auto *p :
       {&r->formation_id, &r->order_id, &r->council_fact_id, &r->proposal_id,
        &r->stratagem_id, &r->tactic_id, &r->activation_id})
    if (*p && (*p)->empty())
      return bad();
  if ((r->mission_revision && *r->mission_revision < 0) ||
      (r->request_index && *r->request_index < 0))
    return bad();
  if (kind == "FieldArmyExitSelect") {
    if (!r->army_exit_choice_id || r->army_exit_choice_id->empty() ||
        !r->selected_exit_cell || *r->selected_exit_cell < 0 ||
        !r->actual_control_acceptance || r->formation_id || r->order_id ||
        r->council_fact_id || r->proposal_id || r->stratagem_id || r->tactic_id ||
        r->activation_id || r->mission_revision || r->request_index ||
        r->mission || r->effective_at || r->council_stratagem || r->deployment ||
        (r->state != FieldBattleState::Running && r->state != FieldBattleState::Ended))
      return bad();
    const auto &cut = *r->actual_control_acceptance;
    auto at = encode_world_moment(cut.observed_at);
    if (!at.ok) return {false, {}, at.error};
    if (cut.last_local_event_sequence < 0 || cut.common_prefix.common_fact_count < 0 ||
        bool(cut.common_prefix.last_common_fact) != (cut.common_prefix.common_fact_count > 0))
      return bad();
    if (cut.common_prefix.last_common_fact &&
        (cut.common_prefix.last_common_fact->domain.empty() ||
         cut.common_prefix.last_common_fact->kind.empty() ||
         cut.common_prefix.last_common_fact->id.empty()))
      return bad();
    return common_success();
  }
  if (r->army_exit_choice_id || r->selected_exit_cell)
    return bad();
  const bool control =
      kind == "field_battle_direct" || kind == "field_battle_order" ||
      kind == "field_battle_dispatch" || kind == "field_battle_rejoin" ||
      kind == "field_battle_delegate" || kind == "field_battle_tactic";
  if (bool(r->actual_control_acceptance) != control)
    return bad();
  if (bool(r->deployment) != (kind == "field_battle_set_deployment"))
    return bad();
  if (r->deployment &&
      (r->deployment->facing_mdeg < 0 || r->deployment->facing_mdeg >= 360000 ||
       (r->deployment->deputy_actor_id &&
        r->deployment->deputy_actor_id->empty())))
    return bad();
  if (r->actual_control_acceptance) {
    const auto &cut = *r->actual_control_acceptance;
    auto at = encode_world_moment(cut.observed_at);
    if (!at.ok)
      return {false, {}, at.error};
    if (cut.last_local_event_sequence < 0 ||
        cut.common_prefix.common_fact_count < 0 ||
        bool(cut.common_prefix.last_common_fact) !=
            (cut.common_prefix.common_fact_count > 0))
      return bad();
    if (cut.common_prefix.last_common_fact &&
        (cut.common_prefix.last_common_fact->domain.empty() ||
         cut.common_prefix.last_common_fact->kind.empty() ||
         cut.common_prefix.last_common_fact->id.empty()))
      return bad();
  }
  const bool formation =
      kind == "field_battle_set_deployment" || kind == "field_battle_direct" ||
      kind == "field_battle_order" || kind == "field_battle_dispatch" ||
      kind == "field_battle_rejoin" || kind == "field_battle_tactic";
  const bool revisioned = kind == "field_battle_set_deployment" ||
                          kind == "field_battle_direct" ||
                          kind == "field_battle_dispatch";
  if (bool(r->formation_id) != formation ||
      bool(r->mission_revision) != revisioned)
    return bad();
  const bool mission = revisioned || kind == "field_battle_order";
  if (bool(r->mission) != mission ||
      bool(r->effective_at) !=
          (mission || kind == "field_battle_tactic" ||
           (kind == "field_battle_stratagem" && bool(r->stratagem_id))) ||
      (r->effective_at &&
       (r->effective_at->tick < r->at.tick || r->effective_at->t < 0 ||
        (r->effective_at->tick == r->at.tick && r->effective_at->t < r->at.t))))
    return bad();
  if (r->mission) {
    const bool shape = std::visit(
        [](const auto &mission) {
          using T = std::decay_t<decltype(mission)>;
          if constexpr (std::is_same_v<T, FieldMoveTo> ||
                        std::is_same_v<T, FieldHold>)
            return mission.facing_mdeg >= 0 && mission.facing_mdeg < 360000;
          else if constexpr (std::is_same_v<T, FieldEngage>)
            return !mission.target_formation_id.empty();
          else if constexpr (std::is_same_v<T, FieldSupport>)
            return !mission.ally_formation_id.empty();
          else
            return mission.exit_side == 0 || mission.exit_side == 1;
        },
        *r->mission);
    if (!shape)
      return bad();
  }
  if (kind == "field_battle_order") {
    if (!r->order_id)
      return bad();
  } else if (kind != "field_battle_tactic" && r->order_id)
    return bad();
  if (kind == "field_council_speak") {
    if (bool(r->proposal_id) == bool(r->request_index) ||
        (r->proposal_id && r->council_fact_id))
      return bad();
  } else if (kind == "field_council_adopt") {
    if (!r->proposal_id || !r->council_fact_id)
      return bad();
  } else if (kind == "field_battle_stratagem") {
    const bool attached = r->proposal_id && r->request_index;
    if (bool(r->council_stratagem) != attached ||
        attached == bool(r->stratagem_id) || r->council_fact_id ||
        (attached && r->state != FieldBattleState::Council) ||
        (!attached && (r->proposal_id || r->request_index)))
      return bad();
  } else if (r->proposal_id || r->request_index ||
             (r->council_fact_id && kind != "field_battle_direct" &&
              kind != "field_battle_order" && kind != "field_battle_dispatch" &&
              kind != "field_battle_rejoin" && kind != "field_battle_tactic"))
    return bad();
  if (r->council_stratagem) {
    const auto &item = *r->council_stratagem;
    if (kind != "field_battle_stratagem" || item.command_result ||
        item.voided_at_adoption)
      return bad();
    if (item.kind == FieldCouncilStratagemKind::Ambush) {
      if (!item.formation_id || item.formation_id->empty() || !item.area_dm)
        return bad();
    } else if (item.kind == FieldCouncilStratagemKind::FeintStrength) {
      if (item.formation_id || item.area_dm)
        return bad();
    } else
      return bad();
  }
  if (r->stratagem_id && kind != "field_battle_stratagem")
    return bad();
  if (kind == "field_battle_tactic") {
    if (bool(r->order_id) == bool(r->activation_id))
      return bad();
  } else if (r->activation_id)
    return bad();
  if (bool(r->tactic_id) != (kind == "field_battle_tactic"))
    return bad();
  return common_success();
}
Outcome<FieldBattleReceipt>
read_applied_field_command_result(const FieldBattleWorldState &world,
                                  const FieldDeploymentCommand &command,
                                  const StableId &actor) {
  return std::visit(
      [&](const auto &c) -> Outcome<FieldBattleReceipt> {
        using T = std::decay_t<decltype(c)>;
        const auto found = world.sessions.find(c.battle_id);
        if (found == world.sessions.end())
          return {false,
                  {},
                  common_failure("FieldBattleNotFound", c.battle_id).error};
        const auto &session = found->second;
        FieldBattleMutationResult r;
        r.battle_id = c.battle_id;
        r.state = session.state;
        r.at = session.now;
        if constexpr (std::is_same_v<T, FieldBattleSetDeployment>) {
          const auto f = session.formations.find(c.formation_id);
          if (f == session.formations.end())
            return {
                false,
                {},
                common_failure("FieldFormationNotFound", c.formation_id).error};
          r.formation_id = c.formation_id;
          // Read only the actual post-write Formation. Second-wave deployment
          // retains its accepted task separately from its immediate Hold.
          const auto &applied = f->second;
          r.mission_revision = applied.mission_revision;
          r.mission =
              applied.wave_mission ? *applied.wave_mission : applied.mission;
          r.effective_at = session.now;
          r.deployment = FieldDeploymentApplied{
              applied.position_dm, applied.facing_mdeg,
              applied.control,     applied.deputy_actor_id,
              applied.mission,     applied.wave_mission};
        } else if constexpr (std::is_same_v<T, FieldBattleStart>) {
          if (session.state != FieldBattleState::Running)
            return {false,
                    {},
                    common_failure("FieldBattleNotRunning", c.battle_id).error};
        } else if constexpr (std::is_same_v<T, FieldCouncilSpeak>) {
          if (std::holds_alternative<FieldCouncilPlan>(c.speech)) {
            const FieldCouncilProposal *proposal = nullptr;
            for (const auto &council : session.councils)
              for (const auto &p : council.proposals)
                if (p.speaker_actor_id == actor)
                  proposal = &p;
            if (!proposal)
              return {
                  false,
                  {},
                  common_failure("FieldCouncilProposalRequired", actor).error};
            r.proposal_id = proposal->proposal_id;
          } else {
            for (const auto &council : session.councils)
              for (std::size_t i = 0; i < council.requests.size(); ++i)
                if (council.requests[i].actor_id == actor) {
                  if (r.request_index)
                    return {false,
                            {},
                            common_failure("FieldCouncilAlreadySpoken", actor)
                                .error};
                  r.request_index = static_cast<Integer>(i);
                }
            if (!r.request_index)
              return {
                  false,
                  {},
                  common_failure("InvalidFieldCouncilRequest", actor).error};
          }
        } else if constexpr (std::is_same_v<T, FieldCouncilAdopt>) {
          const FieldCouncilFact *fact = nullptr;
          for (const auto &[id, f] : world.council_facts)
            if (f.battle_id == c.battle_id && f.side >= 0 && f.side < 2 &&
                f.council.player_actor_id == actor &&
                (!c.proposal_id ||
                 f.council.adopted_proposal_id == c.proposal_id)) {
              if (fact)
                return {false,
                        {},
                        common_failure("FieldCouncilFactRepeated", c.battle_id)
                            .error};
              fact = &f;
            }
          if (!fact)
            return {
                false,
                {},
                common_failure("FieldCouncilFactRequired", c.battle_id).error};
          r.council_fact_id = fact->fact_id;
          r.proposal_id = fact->council.adopted_proposal_id;
        } else
          return {false,
                  {},
                  common_failure("NOT_IMPLEMENTED",
                                 "field_battle/command_result",
                                 ErrorKind::unsupported_capability)
                      .error};
        FieldBattleReceipt result{std::move(r)};
        auto valid = validate_field_battle_receipt(
            field_deployment_command_kind(command), result);
        if (!valid.ok)
          return {false, {}, valid.error};
        return {true, std::move(result), {}};
      },
      command);
}
} // namespace hanmo::resident
