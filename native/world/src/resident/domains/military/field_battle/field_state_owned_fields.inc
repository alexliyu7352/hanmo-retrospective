// 稳定 kind 外壳仍沿原整数字段参与真实来源遍历。
template<class Visitor>
inline void visit(const ::hanmo::resident::FieldBattleEventKind &v, Visitor &&f) { f(v.value); }
// opaque payload 的来源出口保持原 SourceRef 身份；一般数值仍由原 Field validators 验证。
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldEventPayloads &v, Visitor &&f) {
  struct Sink final : ::hanmo::resident::FieldEventSourceSink {
    std::remove_reference_t<Visitor> &visit;
    explicit Sink(std::remove_reference_t<Visitor> &value) : visit(value) {}
    void source(const ::hanmo::resident::SourceRef &value) override { visit(value); }
  } sink(f);
  v.visit_sources(sink);
}
// 有限会话与命令/输出的唯一拥有字段；不给未实现 writer 赋予成功能力。
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldBattleTime &v, Visitor &&f) {
  f(v.tick);
  f(v.t);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldAnchor &v, Visitor &&f) {
  f(v.route_id);
  f(v.point_m);
  f(v.tangent);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldMoveTo &v, Visitor &&f) {
  f(v.point_dm);
  f(v.facing_mdeg);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldHold &v, Visitor &&f) {
  f(v.point_dm);
  f(v.facing_mdeg);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldEngage &v, Visitor &&f) {
  f(v.target_formation_id);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldSupport &v, Visitor &&f) {
  f(v.ally_formation_id);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldWithdraw &v, Visitor &&f) {
  f(v.exit_side);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldDirectControl &v, Visitor &&f) {
  f(v.controller_actor_id);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldAutonomousControl &v,
                  Visitor &&f) {
  f(v.dispatched_at);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldFormation &v, Visitor &&f) {
  f(v.wave);
  f(v.wave_mission);
  f(v.formation_id);
  f(v.army_id);
  f(v.troop_type_id);
  f(v.commander_actor_id);
  f(v.deputy_actor_id);
  f(v.side);
  f(v.fit_start);
  f(v.category);
  f(v.fit);
  f(v.wounded);
  f(v.dead);
  f(v.captured);
  f(v.deserted);
  f(v.commander_lost_at);
  f(v.position_dm);
  f(v.facing_mdeg);
  f(v.ranks);
  f(v.morale);
  f(v.morale_start);
  f(v.state);
  f(v.control);
  f(v.mission);
  f(v.mission_revision);
  f(v.adopted_order_id);
  f(v.followup_mission);
  f(v.pending_direct);
  f(v.ability);
  f(v.statuses);
  f(v.stopped_since);
  f(v.hold_since);
  f(v.casualty_ordinal);
  f(v.captors);
  f(v.commander_casualties);
  f(v.has_engaged);
  f(v.withdrawn_at);
  f(v.navigation_target_dm);
  f(v.navigation_replanned_at);
  f(v.ai_pursuit);
  f(v.joined_at);
  f(v.mission_command_result);
  f(v.control_command_result);
  // 与原持久根同属一个拥有闭包，保真实来源遍历。
  f(v.pending_player_entry_order);
  f(v.mission_transition_sequence);
  f(v.flank_assignment_index);
  f(v.control_change_sources);
  f(v.deployment_command_result);
  f(v.mission_ai_source_id);
  f(v.navigation_observation_sequence);
  f(v.last_motion_sequence);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldPlayerController &v,
                  Visitor &&f) {
  f(v.actor_id);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldAiController &v, Visitor &&f) {}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldBattleSide &v, Visitor &&f) {
  f(v.side);
  f(v.faction_id);
  f(v.commander_actor_id);
  f(v.controller);
  f(v.deploy_zone);
  f(v.exits);
  f(v.strategist_actor_id);
  f(v.feint_strength_id);
  f(v.delegated_by_actor_id);
  f(v.stratagem_points);
  f(v.initial_stratagem_points);
  f(v.delegation_command_result);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldOrderRecord &v, Visitor &&f) {
  f(v.order_id);
  f(v.issuer_actor_id);
  f(v.target_formation_id);
  f(v.payload);
  f(v.issued);
  f(v.due);
  f(v.delay_s);
  f(v.status);
  f(v.status_at);
  f(v.report_due);
  f(v.reason);
  f(v.superseded_by);
  f(v.distance_m);
  f(v.command_result);
  f(v.ai_pursuit);
  f(v.ai_issued);
  // 与原持久根同属一个拥有闭包，保真实来源遍历。
  f(v.resolution_event_sequence);
  f(v.resolution_boundary);
  f(v.adopted_mission_revision);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldPendingEntry &v, Visitor &&f) {
  f(v.army_id);
  f(v.side);
  f(v.entry);
  f(v.entry_point_dm);
  f(v.contact_id);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldAiState &v, Visitor &&f) {
  f(v.side);
  f(v.last_eval);
  f(v.pending_triggers);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldBattleResult &v, Visitor &&f) {
  f(v.winner_side);
  f(v.reason);
  f(v.ended);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldBattleSession &v, Visitor &&f) {
  f(v.started_at);
  f(v.councils);
  f(v.battle_id);
  f(v.field_hash);
  f(v.created_tick);
  f(v.window_complete);
  f(v.segment);
  f(v.now);
  f(v.state);
  f(v.anchor);
  f(v.sides);
  f(v.formations);
  f(v.orders);
  f(v.pending_entries);
  f(v.ai);
  f(v.rng_seed);
  f(v.rng_state);
  f(v.pulse_remainders);
  f(v.morale_remainders);
  f(v.flank_remainders);
  f(v.result);
  f(v.stratagems);
  f(v.events);
  f(v.settlements);
  f(v.next_event_sequence);
  f(v.next_stratagem_sequence);
  f(v.started_source);
  f(v.started_observed_tick);
  f(v.started_by_actor_id);
  // 与原持久根同属一个拥有闭包，保真实来源遍历。
  f(v.started_actual_at);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldRoadContact &v, Visitor &&f) {
  f(v.source_tick);
  f(v.army_ids);
  f(v.contact);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldBattleMemberBinding &v,
                  Visitor &&f) {
  f(v.actor_id);
  f(v.formation_id);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldBattleRoadBinding &v,
                  Visitor &&f) {
  f(v.army_id);
  f(v.battle_id);
  f(v.action_id);
  f(v.commander_actor_id);
  f(v.source);
  f(v.members);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldBattleContactRecord &v,
                  Visitor &&f) {
  f(v.contact_id);
  f(v.battle_id);
  f(v.source);
  // 与原持久根同属一个拥有闭包，保真实来源遍历。
  f(v.original_bindings);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldBattleWorldState &v,
                  Visitor &&f) {
  f(v.council_facts);
  f(v.schema);
  f(v.sessions);
  f(v.contacts);
  f(v.army_bindings);
  f(v.boundary_contacts);
  f(v.exits);
  f(v.army_exit_plans);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldCouncilAssignment &v,
                  Visitor &&f) {
  f(v.formation_id);
  f(v.mission);
  f(v.wave);
  // 与原持久根同属一个拥有闭包，保真实来源遍历。
  f(v.flank_followup);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldCouncilStratagemPlan &v,
                  Visitor &&f) {
  f(v.kind);
  f(v.formation_id);
  f(v.area_dm);
  // 与原持久根同属一个拥有闭包，保真实来源遍历。
  f(v.command_result);
  f(v.voided_at_adoption);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldCouncilPlan &v, Visitor &&f) {
  f(v.layout);
  f(v.assignments);
  f(v.stratagems);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldCouncilParticipant &v,
                  Visitor &&f) {
  f(v.actor_id);
  f(v.role);
  f(v.formation_id);
  f(v.stance);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldCouncilProposal &v,
                  Visitor &&f) {
  f(v.proposal_id);
  f(v.speaker_actor_id);
  f(v.stance);
  f(v.plan);
  f(v.text_key);
  f(v.command_result);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldCouncilRoleRequest &v,
                  Visitor &&f) {
  f(v.role);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldCouncilTargetRequest &v,
                  Visitor &&f) {
  f(v.target_formation_id);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldCouncilRequest &v,
                  Visitor &&f) {
  f(v.actor_id);
  f(v.value);
  f(v.status);
  f(v.reason);
  f(v.command_result);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldCouncilIntelFormation &v,
                  Visitor &&f) {
  f(v.formation_id);
  f(v.category);
  f(v.fit);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldCouncilIntelCommander &v,
                  Visitor &&f) {
  f(v.actor_id);
  f(v.leadership);
  f(v.martial);
  f(v.intellect);
  f(v.administration);
  f(v.charisma);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldCouncilIntelSource &v,
                  Visitor &&f) {
  f(v.level);
  f(v.total_estimate);
  f(v.formations);
  f(v.commanders);
  f(v.enemy_plan);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldCouncilRecord &v, Visitor &&f) {
  f(v.player_actor_id);
  f(v.participants);
  f(v.intel);
  f(v.proposals);
  f(v.requests);
  f(v.adopted_proposal_id);
  f(v.closed);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldCouncilFact &v, Visitor &&f) {
  f(v.fact_id);
  f(v.battle_id);
  f(v.side);
  f(v.at);
  f(v.kind);
  f(v.council);
  f(v.order_id);
  f(v.command_result);
  f(v.recorded_tick);
  f(v.player_command_kind);
  f(v.player_actor_id);
  f(v.formation_id);
  f(v.player_mission);
  f(v.mission_revision);
  f(v.tactic_id);
  f(v.activation_id);
  // 与原持久根同属一个拥有闭包，保真实来源遍历。
  f(v.ai_pursuit);
  f(v.adopted_actual_at);
  f(v.ai_control_acceptance);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldBattleSetDeployment &v,
                  Visitor &&f) {
  f(v.battle_id);
  f(v.formation_id);
  f(v.position_dm);
  f(v.facing_mdeg);
  f(v.mission);
  f(v.control);
  f(v.deputy_actor_id);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldBattleStart &v, Visitor &&f) {
  f(v.battle_id);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldCouncilSpeak &v, Visitor &&f) {
  f(v.battle_id);
  f(v.speech);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldCouncilAdopt &v, Visitor &&f) {
  f(v.battle_id);
  f(v.proposal_id);
  f(v.overrides);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldAvailableTactic &v,
                  Visitor &&f) {
  f(v.tactic_id);
  f(v.learned_tactic_id);
  f(v.acquisition_fact_id);
  f(v.kind);
  f(v.point_cost);
  f(v.power_bp);
  f(v.morale_effect);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldTacticReadiness &v,
                  Visitor &&f) {
  f(v.source);
  f(v.ready_at);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldTacticActivation &v,
                  Visitor &&f) {
  f(v.tactic_id);
  f(v.kind);
  f(v.started);
  f(v.ends);
  f(v.remaining_pulses);
  f(v.first_contact_consumed);
  f(v.command_result);
  f(v.order_id);
  f(v.activation_id);
  f(v.issuer_actor_id);
  f(v.commander_actor_id);
  f(v.accepted_source);
  f(v.ai_issued);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldFormationStatus &v,
                  Visitor &&f) {
  f(v.kind);
  f(v.source_id);
  f(v.until);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldFormationAbilityState &v,
                  Visitor &&f) {
  f(v.commander_actor_id);
  f(v.traits);
  f(v.tactics);
  f(v.capacity);
  f(v.points);
  f(v.last_point_recovery);
  f(v.active);
  f(v.source_ref);
  f(v.observed_tick);
  // 与原持久根同属一个拥有闭包，保真实来源遍历。
  f(v.commander_changes);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldPendingDirectMission &v,
                  Visitor &&f) {
  f(v.issuer_actor_id);
  f(v.request_sequence);
  f(v.accepted_revision);
  f(v.mission);
  f(v.apply_at);
  f(v.command_result);
  f(v.ai_pursuit);
  // 与原持久根同属一个拥有闭包，保真实来源遍历。
  f(v.ai_issued);
  f(v.ai_source_id);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldTacticIntent &v, Visitor &&f) {
  f(v.tactic_id);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldStratagemRecord &v,
                  Visitor &&f) {
  f(v.adopted_source);
  f(v.observed_tick);
  f(v.strategist_actor_id);
  f(v.target_commander_actor_id);
  f(v.success_chance_quarters);
  f(v.stratagem_id);
  f(v.issuer_actor_id);
  f(v.side);
  f(v.issued_sequence);
  f(v.kind);
  f(v.formation_id);
  f(v.target_formation_id);
  f(v.area_dm);
  f(v.issued);
  f(v.resolve_at);
  f(v.resolved_at);
  f(v.success);
  f(v.command_result);
  f(v.revealed_at);
  f(v.first_contact_consumed);
  // 与原持久根同属一个拥有闭包，保真实来源遍历。
  f(v.applied_at);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldBattleEvent &v, Visitor &&f) {
  f(v.sequence);
  f(v.at);
  f(v.kind);
  f(v.formation_id);
  f(v.target_formation_id);
  f(v.source_id);
  f(v.count);
  f(v.side);
  f(v.tactic);
  f(v.stratagem);
  f(v.success);
  // 与原持久根同属一个拥有闭包，保真实来源遍历。
  f(v.payloads);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldSegmentSettlement &v,
                  Visitor &&f) {
  f(v.fact_id);
  f(v.recorded_tick);
  f(v.applied_at);
  f(v.segment);
  f(v.settled_at);
  f(v.casualty_fact_ids);
  f(v.battle_fact_id);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldBattleRead &v, Visitor &&f) {
  f(v.battle_id);
  f(v.known_field_hash);
  f(v.after_event_sequence);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldBattleDirect &v, Visitor &&f) {
  f(v.battle_id);
  f(v.formation_id);
  f(v.expected_mission_revision);
  f(v.mission);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldBattleOrder &v, Visitor &&f) {
  f(v.battle_id);
  f(v.formation_id);
  f(v.mission);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldBattleDispatch &v,
                  Visitor &&f) {
  f(v.battle_id);
  f(v.formation_id);
  f(v.mission);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldBattleRejoin &v, Visitor &&f) {
  f(v.battle_id);
  f(v.formation_id);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldBattleDelegate &v,
                  Visitor &&f) {
  f(v.battle_id);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldBattleAdvance &v, Visitor &&f) {
  f(v.battle_id);
  f(v.to);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldBattleTactic &v, Visitor &&f) {
  f(v.battle_id);
  f(v.formation_id);
  f(v.tactic_id);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldBattleStratagem &v,
                  Visitor &&f) {
  f(v.battle_id);
  f(v.kind);
  f(v.formation_id);
  f(v.target_formation_id);
  f(v.area_dm);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldBattleViewIdentity &v,
                  Visitor &&f) {
  f(v.battle_id);
  f(v.observer_actor_id);
  f(v.segment);
  f(v.revision);
  f(v.now);
  f(v.state);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldCellView &v, Visitor &&f) {
  f(v.terrain);
  f(v.height_dm);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldBattleFieldView &v,
                  Visitor &&f) {
  f(v.anchor);
  f(v.field_hash);
  f(v.length_m);
  f(v.width_m);
  f(v.cell_m);
  f(v.cells);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldBattleSideView &v,
                  Visitor &&f) {
  f(v.side);
  f(v.display_name);
  f(v.faction_id);
  f(v.controller);
  f(v.is_observer_side);
  f(v.deploy_zone);
  f(v.exits);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldMissionView &v, Visitor &&f) {
  f(v.kind);
  f(v.point_dm);
  f(v.facing_mdeg);
  f(v.exit_side);
  f(v.target_formation_id);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldCommanderView &v, Visitor &&f) {
  f(v.actor_id);
  f(v.display_name);
  f(v.state);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldTacticView &v, Visitor &&f) {
  f(v.tactic_id);
  f(v.kind);
  f(v.point_cost);
  f(v.ready_at);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldStatusView &v, Visitor &&f) {
  f(v.kind);
  f(v.until);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldFormationView &v, Visitor &&f) {
  f(v.formation_id);
  f(v.army_id);
  f(v.display_name);
  f(v.category);
  f(v.troop_type_id);
  f(v.side);
  f(v.position_dm);
  f(v.facing_mdeg);
  f(v.width_dm);
  f(v.depth_dm);
  f(v.state);
  f(v.control);
  f(v.mission);
  f(v.fit);
  f(v.wounded);
  f(v.dead);
  f(v.captured);
  f(v.deserted);
  f(v.morale);
  f(v.commander);
  f(v.deputy_actor_id);
  f(v.traits);
  f(v.tactics);
  f(v.tactic_points);
  f(v.wave);
  f(v.statuses);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldOrderView &v, Visitor &&f) {
  f(v.order_id);
  f(v.recipient);
  f(v.status);
  f(v.mission);
  f(v.tactic_id);
  f(v.issued);
  f(v.eta_range);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldBattleEventView &v,
                  Visitor &&f) {
  f(v.sequence);
  f(v.at);
  f(v.kind);
  f(v.formation_id);
  f(v.target_formation_id);
  f(v.tactic);
  f(v.stratagem);
  f(v.count);
  f(v.success);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldBattleView &v, Visitor &&f) {
  f(v.identity);
  f(v.field);
  f(v.sides);
  f(v.formations);
  f(v.council);
  f(v.orders);
  f(v.events);
  f(v.result);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldBattleReadQuote &v,
                  Visitor &&f) {
  f(v.view);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldBattleAdvanceResult &v,
                  Visitor &&f) {
  f(v.battle_id);
  f(v.from);
  f(v.to);
  f(v.events);
  f(v.result);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldControlAcceptanceBoundary &v,
                  Visitor &&f) {
  f(v.common_prefix);
  f(v.last_local_event_sequence);
  f(v.observed_at);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldDeploymentApplied &v,
                  Visitor &&f) {
  f(v.position_dm);
  f(v.facing_mdeg);
  f(v.control);
  f(v.deputy_actor_id);
  f(v.immediate_mission);
  f(v.wave_mission);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldBattleMutationResult &v,
                  Visitor &&f) {
  f(v.battle_id);
  f(v.state);
  f(v.at);
  f(v.formation_id);
  f(v.order_id);
  f(v.council_fact_id);
  f(v.proposal_id);
  f(v.stratagem_id);
  f(v.mission_revision);
  f(v.tactic_id);
  f(v.request_index);
  f(v.mission);
  f(v.effective_at);
  f(v.activation_id);
  f(v.council_stratagem);
  f(v.deployment);
  f(v.actual_control_acceptance);
  f(v.army_exit_choice_id);
  f(v.selected_exit_cell);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldBattleReceipt &v, Visitor &&f) {
  f(v.result);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldCouncilProposalView &v,
                  Visitor &&f) {
  f(v.proposal_id);
  f(v.speaker_actor_id);
  f(v.stance);
  f(v.plan);
  f(v.text_key);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldCouncilRequestView &v,
                  Visitor &&f) {
  f(v.actor_id);
  f(v.value);
  f(v.status);
  f(v.reason);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldCouncilView &v, Visitor &&f) {
  f(v.participants);
  f(v.intel);
  f(v.proposals);
  f(v.requests);
  f(v.adopted_proposal_id);
  f(v.closed);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FieldCommanderCasualty &v,
                  Visitor &&f) {
  f(v.actor_id);
  f(v.loss);
  f(v.at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FieldInitialContactBinding &v,
                  Visitor &&f) {
  f(v.contact_id);
  f(v.source_endpoint);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FieldBoundaryContactBinding &v,
                  Visitor &&f) {
  f(v.contact_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FieldBattleBoundaryRecord &v,
                  Visitor &&f) {
  f(v.contact_id);
  f(v.battle_id);
  f(v.source_tick);
  f(v.time_num);
  f(v.time_den);
  f(v.motion);
  f(v.entry_point_dm);
  f(v.army_id);
  f(v.side);
  f(v.participant_actor_ids);
  f(v.diplomacy_sources);
  f(v.status);
  f(v.entered_at);
  f(v.released_at);
  f(v.entry_application_source);
  f(v.entered_observed_tick);
  // 与原持久根同属一个拥有闭包，保真实来源遍历。
  f(v.entered_actual_at);
  f(v.entered_binding);
  f(v.reinforcement_queue);
  f(v.exact_continuation_source);
}

// 真实离场来源与原路线锚点全部纳入同一拥有图。
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldBattleExitRecord &v,
                  Visitor &&f) {
  f(v.fact_id);
  f(v.battle_id);
  f(v.formation_id);
  f(v.army_id);
  f(v.at);
  f(v.recorded_tick);
  f(v.field_point_dm);
  f(v.location);
  f(v.source_action_id);
  f(v.origin);
  f(v.named_actor_ids);
  f(v.continuation);
  // 与原持久根同属一个拥有闭包，保真实来源遍历。
  f(v.applied_at);
  f(v.exact_route_progress);
}

// 原有 variant 分支的唯一真实拥有字段；不新增载荷或替代来源。
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldExactGroupContactOrigin &value,
                  Visitor &&visitor) {
  visitor(value.actual_contact);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FieldExactBoundaryOrigin &value,
                  Visitor &&visitor) {
  visitor(value.actual_contact);
}

// 实际事件与入场队列的原有载荷；复制、保留与来源扫描共用。
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldCommanderChange &v,
                  Visitor &&f) {
  f(v.source_ref);
  f(v.previous_actor_id);
  f(v.commander_actor_id);
  f(v.at);
  f(v.observed_tick);
  f(v.lost_event_sequence);
  f(v.applied_at);
}




template <class Visitor>
inline void visit(const ::hanmo::resident::FieldReinforcementAdmission &v,
                  Visitor &&f) {
  f(v.application_source);
  f(v.formation_id);
  f(v.at);
  f(v.applied_at);
  f(v.event_sequence);
  f(v.position_dm);
  f(v.facing_mdeg);
  f(v.initial_control);
  f(v.initial_mission);
  f(v.ai_mission);
  f(v.pending_player_entry_order);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldReinforcementQueue &v,
                  Visitor &&f) {
  f(v.initialized_source);
  f(v.initialized_at);
  f(v.formation_ids);
  f(v.admitted);
}

// 新真实源归同一拥有闭包，复制/保留/来源扫描不遗漏持久尾。
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldArmyExitWeight &v, Visitor &&f) {
  f(v.formation_id);
  f(v.position_dm);
  f(v.fit);
  f(v.wounded);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldArmyExitMotion &v, Visitor &&f) {
  f(v.formation_id);
  f(v.from_dm);
  f(v.to_dm);
  f(v.reached);
  f(v.blocked);
  f(v.deserted_member_ids);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldArmyExitStep &v, Visitor &&f) {
  f(v.fact_id);
  f(v.from);
  f(v.to);
  f(v.applied_at);
  f(v.formations);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldArmyExitRallyFormation &v, Visitor &&f) {
  f(v.formation_id);
  f(v.initial_position_dm);
  f(v.ordinary_member_ids);
  f(v.retained_officer_ids);
  f(v.original_exit);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldArmyExitPlan &v, Visitor &&f) {
  f(v.fact_id);
  f(v.battle_id);
  f(v.army_id);
  f(v.commander_actor_id);
  f(v.field_hash);
  f(v.destination_settlement_id);
  f(v.return_garrison_id);
  f(v.exit_cell);
  f(v.selected_at);
  f(v.applied_at);
  f(v.original_journey_opening);
  f(v.selection_boundary);
  f(v.player_command_result);
  f(v.weights);
  f(v.terminal_segment);
  f(v.ordinary_deadline);
  f(v.rally_formations);
  f(v.steps);
  f(v.ready_source);
  f(v.ready_at);
  f(v.ready_applied_at);
  f(v.actual_continuation);
}
