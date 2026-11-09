// 同一严格字段回调；缺字段/未知字段由原 archive 明确拒绝。
template <class C> Status value(C &c, FieldBattleTime &v) {
  field(c, "tick", v.tick);
  field(c, "t", v.t);
  return c.status();
}
template <class C> Status value(C &c, FieldAnchor &v) {
  field(c, "route_id", v.route_id);
  field(c, "point_m", v.point_m);
  field(c, "tangent", v.tangent);
  return c.status();
}
template <class C> Status value(C &c, FieldMoveTo &v) {
  field(c, "point_dm", v.point_dm);
  field(c, "facing_mdeg", v.facing_mdeg);
  return c.status();
}
template <class C> Status value(C &c, FieldHold &v) {
  field(c, "point_dm", v.point_dm);
  field(c, "facing_mdeg", v.facing_mdeg);
  return c.status();
}
template <class C> Status value(C &c, FieldEngage &v) {
  field(c, "target_formation_id", v.target_formation_id);
  return c.status();
}
template <class C> Status value(C &c, FieldSupport &v) {
  field(c, "ally_formation_id", v.ally_formation_id);
  return c.status();
}
template <class C> Status value(C &c, FieldWithdraw &v) {
  field(c, "exit_side", v.exit_side);
  return c.status();
}
template <class C> Status value(C &c, FieldDirectControl &v) {
  field(c, "controller_actor_id", v.controller_actor_id);
  return c.status();
}
template <class C> Status value(C &c, FieldAutonomousControl &v) {
  field(c, "dispatched_at", v.dispatched_at);
  return c.status();
}
template <class C> Status value(C &c, FieldFormation &v) {
  field(c, "wave", v.wave);
  field(c, "wave_mission", v.wave_mission);
  field(c, "formation_id", v.formation_id);
  field(c, "army_id", v.army_id);
  field(c, "troop_type_id", v.troop_type_id);
  field(c, "commander_actor_id", v.commander_actor_id);
  field(c, "deputy_actor_id", v.deputy_actor_id);
  field(c, "side", v.side);
  field(c, "fit_start", v.fit_start);
  field(c, "category", v.category);
  field(c, "fit", v.fit);
  field(c, "wounded", v.wounded);
  field(c, "dead", v.dead);
  field(c, "captured", v.captured);
  field(c, "deserted", v.deserted);
  field(c, "commander_lost_at", v.commander_lost_at);
  field(c, "position_dm", v.position_dm);
  field(c, "facing_mdeg", v.facing_mdeg);
  field(c, "ranks", v.ranks);
  field(c, "morale", v.morale);
  field(c, "morale_start", v.morale_start);
  field(c, "state", v.state);
  field(c, "control", v.control);
  field(c, "mission", v.mission);
  field(c, "mission_revision", v.mission_revision);
  field(c, "adopted_order_id", v.adopted_order_id);
  field(c, "followup_mission", v.followup_mission);
  field(c, "pending_direct", v.pending_direct);
  field(c, "ability", v.ability);
  field(c, "statuses", v.statuses);
  field(c, "stopped_since", v.stopped_since);
  field(c, "hold_since", v.hold_since);
  field(c, "casualty_ordinal", v.casualty_ordinal);
  field(c, "captors", v.captors);
  field(c, "commander_casualties", v.commander_casualties);
  field(c, "has_engaged", v.has_engaged);
  field(c, "withdrawn_at", v.withdrawn_at);
  field(c, "navigation_target_dm", v.navigation_target_dm);
  field(c, "navigation_replanned_at", v.navigation_replanned_at);
  field(c, "ai_pursuit", v.ai_pursuit);
  field(c, "joined_at", v.joined_at);
  field(c, "mission_command_result", v.mission_command_result);
  field(c, "control_command_result", v.control_command_result);
  // 已有持久尾与同一字段表对称保存，不猜补冷读缺项。
  field(c, "pending_player_entry_order", v.pending_player_entry_order);
  field(c, "mission_transition_sequence", v.mission_transition_sequence);
  field(c, "flank_assignment_index", v.flank_assignment_index);
  field(c, "control_change_sources", v.control_change_sources);
  field(c, "deployment_command_result", v.deployment_command_result);
  field(c, "mission_ai_source_id", v.mission_ai_source_id);
  field(c, "navigation_observation_sequence",
        v.navigation_observation_sequence);
  field(c, "last_motion_sequence", v.last_motion_sequence);
  return c.status();
}
template <class C> Status value(C &c, FieldPlayerController &v) {
  field(c, "actor_id", v.actor_id);
  return c.status();
}
template <class C> Status value(C &c, FieldAiController &v) {
  return c.status();
}
template <class C> Status value(C &c, FieldBattleSide &v) {
  field(c, "side", v.side);
  field(c, "faction_id", v.faction_id);
  field(c, "commander_actor_id", v.commander_actor_id);
  field(c, "controller", v.controller);
  field(c, "deploy_zone", v.deploy_zone);
  field(c, "exits", v.exits);
  field(c, "strategist_actor_id", v.strategist_actor_id);
  field(c, "feint_strength_id", v.feint_strength_id);
  field(c, "delegated_by_actor_id", v.delegated_by_actor_id);
  field(c, "stratagem_points", v.stratagem_points);
  field(c, "initial_stratagem_points", v.initial_stratagem_points);
  field(c, "delegation_command_result", v.delegation_command_result);
  return c.status();
}
template <class C> Status value(C &c, FieldOrderRecord &v) {
  field(c, "order_id", v.order_id);
  field(c, "issuer_actor_id", v.issuer_actor_id);
  field(c, "target_formation_id", v.target_formation_id);
  field(c, "payload", v.payload);
  field(c, "issued", v.issued);
  field(c, "due", v.due);
  field(c, "delay_s", v.delay_s);
  field(c, "status", v.status);
  field(c, "status_at", v.status_at);
  field(c, "report_due", v.report_due);
  field(c, "reason", v.reason);
  field(c, "superseded_by", v.superseded_by);
  field(c, "distance_m", v.distance_m);
  field(c, "command_result", v.command_result);
  field(c, "ai_pursuit", v.ai_pursuit);
  field(c, "ai_issued", v.ai_issued);
  // 已有持久尾与同一字段表对称保存，不猜补冷读缺项。
  field(c, "resolution_event_sequence", v.resolution_event_sequence);
  field(c, "resolution_boundary", v.resolution_boundary);
  field(c, "adopted_mission_revision", v.adopted_mission_revision);
  return c.status();
}
template <class C> Status value(C &c, FieldPendingEntry &v) {
  field(c, "army_id", v.army_id);
  field(c, "side", v.side);
  field(c, "entry", v.entry);
  field(c, "entry_point_dm", v.entry_point_dm);
  field(c, "contact_id", v.contact_id);
  return c.status();
}
template <class C> Status value(C &c, FieldAiState &v) {
  field(c, "side", v.side);
  field(c, "last_eval", v.last_eval);
  field(c, "pending_triggers", v.pending_triggers);
  return c.status();
}
template <class C> Status value(C &c, FieldBattleResult &v) {
  field(c, "winner_side", v.winner_side);
  field(c, "reason", v.reason);
  field(c, "ended", v.ended);
  return c.status();
}
template <class C> Status value(C &c, FieldBattleSession &v) {
  field(c, "started_at", v.started_at);
  field(c, "councils", v.councils);
  field(c, "battle_id", v.battle_id);
  field(c, "field_hash", v.field_hash);
  field(c, "created_tick", v.created_tick);
  field(c, "window_complete", v.window_complete);
  field(c, "segment", v.segment);
  field(c, "now", v.now);
  field(c, "state", v.state);
  field(c, "anchor", v.anchor);
  field(c, "sides", v.sides);
  field(c, "formations", v.formations);
  field(c, "orders", v.orders);
  field(c, "pending_entries", v.pending_entries);
  field(c, "ai", v.ai);
  field(c, "rng_seed", v.rng_seed);
  field(c, "rng_state", v.rng_state);
  field(c, "pulse_remainders", v.pulse_remainders);
  field(c, "morale_remainders", v.morale_remainders);
  field(c, "flank_remainders", v.flank_remainders);
  field(c, "result", v.result);
  field(c, "stratagems", v.stratagems);
  field(c, "events", v.events);
  field(c, "settlements", v.settlements);
  field(c, "next_event_sequence", v.next_event_sequence);
  field(c, "next_stratagem_sequence", v.next_stratagem_sequence);
  field(c, "started_source", v.started_source);
  field(c, "started_observed_tick", v.started_observed_tick);
  field(c, "started_by_actor_id", v.started_by_actor_id);
  // 已有持久尾与同一字段表对称保存，不猜补冷读缺项。
  field(c, "started_actual_at", v.started_actual_at);
  return c.status();
}
template <class C> Status value(C &c, FieldRoadContact &v) {
  field(c, "source_tick", v.source_tick);
  field(c, "army_ids", v.army_ids);
  field(c, "contact", v.contact);
  return c.status();
}
// 已有精确接触分支只存真实 S1 头；三种 visitor 共用原 SourceRef 字段。
template <class C> Status value(C &c, FieldExactGroupContactOrigin &v) {
  return field(c, "actual_contact", v.actual_contact);
}
template <class C> Status value(C &c, FieldExactBoundaryOrigin &v) {
  return field(c, "actual_contact", v.actual_contact);
}
template <class C> Status value(C &c, FieldBattleMemberBinding &v) {
  field(c, "actor_id", v.actor_id);
  field(c, "formation_id", v.formation_id);
  return c.status();
}
template <class C> Status value(C &c, FieldBattleRoadBinding &v) {
  field(c, "army_id", v.army_id);
  field(c, "battle_id", v.battle_id);
  field(c, "action_id", v.action_id);
  field(c, "commander_actor_id", v.commander_actor_id);
  field(c, "source", v.source);
  field(c, "members", v.members);
  return c.status();
}
template <class C> Status value(C &c, FieldBattleContactRecord &v) {
  field(c, "contact_id", v.contact_id);
  field(c, "battle_id", v.battle_id);
  field(c, "source", v.source);
  // 已有持久尾与同一字段表对称保存，不猜补冷读缺项。
  field(c, "original_bindings", v.original_bindings);
  return c.status();
}
template <class C> Status value(C &c, FieldBattleWorldState &v) {
  field(c, "council_facts", v.council_facts);
  field(c, "schema", v.schema);
  field(c, "sessions", v.sessions);
  field(c, "contacts", v.contacts);
  field(c, "army_bindings", v.army_bindings);
  field(c, "boundary_contacts", v.boundary_contacts);
  field(c, "exits", v.exits);
  field(c, "army_exit_plans", v.army_exit_plans);
  return c.status();
}
template <class C> Status value(C &c, FieldCouncilAssignment &v) {
  field(c, "formation_id", v.formation_id);
  field(c, "mission", v.mission);
  field(c, "wave", v.wave);
  // 已有持久尾与同一字段表对称保存，不猜补冷读缺项。
  field(c, "flank_followup", v.flank_followup);
  return c.status();
}
template <class C> Status value(C &c, FieldCouncilStratagemPlan &v) {
  field(c, "kind", v.kind);
  field(c, "formation_id", v.formation_id);
  field(c, "area_dm", v.area_dm);
  // 已有持久尾与同一字段表对称保存，不猜补冷读缺项。
  field(c, "command_result", v.command_result);
  field(c, "voided_at_adoption", v.voided_at_adoption);
  return c.status();
}
template <class C> Status value(C &c, FieldCouncilPlan &v) {
  field(c, "layout", v.layout);
  field(c, "assignments", v.assignments);
  field(c, "stratagems", v.stratagems);
  return c.status();
}
template <class C> Status value(C &c, FieldCouncilParticipant &v) {
  field(c, "actor_id", v.actor_id);
  field(c, "role", v.role);
  field(c, "formation_id", v.formation_id);
  field(c, "stance", v.stance);
  return c.status();
}
template <class C> Status value(C &c, FieldCouncilProposal &v) {
  field(c, "proposal_id", v.proposal_id);
  field(c, "speaker_actor_id", v.speaker_actor_id);
  field(c, "stance", v.stance);
  field(c, "plan", v.plan);
  field(c, "text_key", v.text_key);
  field(c, "command_result", v.command_result);
  return c.status();
}
template <class C> Status value(C &c, FieldCouncilRoleRequest &v) {
  field(c, "role", v.role);
  return c.status();
}
template <class C> Status value(C &c, FieldCouncilTargetRequest &v) {
  field(c, "target_formation_id", v.target_formation_id);
  return c.status();
}
template <class C> Status value(C &c, FieldCouncilRequest &v) {
  field(c, "actor_id", v.actor_id);
  field(c, "value", v.value);
  field(c, "status", v.status);
  field(c, "reason", v.reason);
  field(c, "command_result", v.command_result);
  return c.status();
}
template <class C> Status value(C &c, FieldCouncilIntelFormation &v) {
  field(c, "formation_id", v.formation_id);
  field(c, "category", v.category);
  field(c, "fit", v.fit);
  return c.status();
}
template <class C> Status value(C &c, FieldCouncilIntelCommander &v) {
  field(c, "actor_id", v.actor_id);
  field(c, "leadership", v.leadership);
  field(c, "martial", v.martial);
  field(c, "intellect", v.intellect);
  field(c, "administration", v.administration);
  field(c, "charisma", v.charisma);
  return c.status();
}
template <class C> Status value(C &c, FieldCouncilIntelSource &v) {
  field(c, "level", v.level);
  field(c, "total_estimate", v.total_estimate);
  field(c, "formations", v.formations);
  field(c, "commanders", v.commanders);
  field(c, "enemy_plan", v.enemy_plan);
  return c.status();
}
template <class C> Status value(C &c, FieldCouncilRecord &v) {
  field(c, "player_actor_id", v.player_actor_id);
  field(c, "participants", v.participants);
  field(c, "intel", v.intel);
  field(c, "proposals", v.proposals);
  field(c, "requests", v.requests);
  field(c, "adopted_proposal_id", v.adopted_proposal_id);
  field(c, "closed", v.closed);
  return c.status();
}
template <class C> Status value(C &c, FieldCouncilFact &v) {
  field(c, "fact_id", v.fact_id);
  field(c, "battle_id", v.battle_id);
  field(c, "side", v.side);
  field(c, "at", v.at);
  field(c, "kind", v.kind);
  field(c, "council", v.council);
  field(c, "order_id", v.order_id);
  field(c, "command_result", v.command_result);
  field(c, "recorded_tick", v.recorded_tick);
  field(c, "player_command_kind", v.player_command_kind);
  field(c, "player_actor_id", v.player_actor_id);
  field(c, "formation_id", v.formation_id);
  field(c, "player_mission", v.player_mission);
  field(c, "mission_revision", v.mission_revision);
  field(c, "tactic_id", v.tactic_id);
  field(c, "activation_id", v.activation_id);
  // 已有持久尾与同一字段表对称保存，不猜补冷读缺项。
  field(c, "ai_pursuit", v.ai_pursuit);
  field(c, "adopted_actual_at", v.adopted_actual_at);
  field(c, "ai_control_acceptance", v.ai_control_acceptance);
  return c.status();
}
template <class C> Status value(C &c, FieldAvailableTactic &v) {
  field(c, "tactic_id", v.tactic_id);
  field(c, "learned_tactic_id", v.learned_tactic_id);
  field(c, "acquisition_fact_id", v.acquisition_fact_id);
  field(c, "kind", v.kind);
  field(c, "point_cost", v.point_cost);
  field(c, "power_bp", v.power_bp);
  field(c, "morale_effect", v.morale_effect);
  return c.status();
}
template <class C> Status value(C &c, FieldTacticReadiness &v) {
  field(c, "source", v.source);
  field(c, "ready_at", v.ready_at);
  return c.status();
}
template <class C> Status value(C &c, FieldTacticActivation &v) {
  field(c, "tactic_id", v.tactic_id);
  field(c, "kind", v.kind);
  field(c, "started", v.started);
  field(c, "ends", v.ends);
  field(c, "remaining_pulses", v.remaining_pulses);
  field(c, "first_contact_consumed", v.first_contact_consumed);
  field(c, "command_result", v.command_result);
  field(c, "order_id", v.order_id);
  field(c, "activation_id", v.activation_id);
  field(c, "issuer_actor_id", v.issuer_actor_id);
  field(c, "commander_actor_id", v.commander_actor_id);
  field(c, "accepted_source", v.accepted_source);
  field(c, "ai_issued", v.ai_issued);
  return c.status();
}
template <class C> Status value(C &c, FieldFormationStatus &v) {
  field(c, "kind", v.kind);
  field(c, "source_id", v.source_id);
  field(c, "until", v.until);
  return c.status();
}
template <class C> Status value(C &c, FieldFormationAbilityState &v) {
  field(c, "commander_actor_id", v.commander_actor_id);
  field(c, "traits", v.traits);
  field(c, "tactics", v.tactics);
  field(c, "capacity", v.capacity);
  field(c, "points", v.points);
  field(c, "last_point_recovery", v.last_point_recovery);
  field(c, "active", v.active);
  field(c, "source_ref", v.source_ref);
  field(c, "observed_tick", v.observed_tick);
  // 已有持久尾与同一字段表对称保存，不猜补冷读缺项。
  field(c, "commander_changes", v.commander_changes);
  return c.status();
}
template <class C> Status value(C &c, FieldPendingDirectMission &v) {
  field(c, "issuer_actor_id", v.issuer_actor_id);
  field(c, "request_sequence", v.request_sequence);
  field(c, "accepted_revision", v.accepted_revision);
  field(c, "mission", v.mission);
  field(c, "apply_at", v.apply_at);
  field(c, "command_result", v.command_result);
  field(c, "ai_pursuit", v.ai_pursuit);
  // 已有持久尾与同一字段表对称保存，不猜补冷读缺项。
  field(c, "ai_issued", v.ai_issued);
  field(c, "ai_source_id", v.ai_source_id);
  return c.status();
}
template <class C> Status value(C &c, FieldTacticIntent &v) {
  field(c, "tactic_id", v.tactic_id);
  return c.status();
}
template <class C> Status value(C &c, FieldStratagemRecord &v) {
  field(c, "adopted_source", v.adopted_source);
  field(c, "observed_tick", v.observed_tick);
  field(c, "strategist_actor_id", v.strategist_actor_id);
  field(c, "target_commander_actor_id", v.target_commander_actor_id);
  field(c, "success_chance_quarters", v.success_chance_quarters);
  field(c, "stratagem_id", v.stratagem_id);
  field(c, "issuer_actor_id", v.issuer_actor_id);
  field(c, "side", v.side);
  field(c, "issued_sequence", v.issued_sequence);
  field(c, "kind", v.kind);
  field(c, "formation_id", v.formation_id);
  field(c, "target_formation_id", v.target_formation_id);
  field(c, "area_dm", v.area_dm);
  field(c, "issued", v.issued);
  field(c, "resolve_at", v.resolve_at);
  field(c, "resolved_at", v.resolved_at);
  field(c, "success", v.success);
  field(c, "command_result", v.command_result);
  field(c, "revealed_at", v.revealed_at);
  field(c, "first_contact_consumed", v.first_contact_consumed);
  // 已有持久尾与同一字段表对称保存，不猜补冷读缺项。
  field(c, "applied_at", v.applied_at);
  return c.status();
}
template <class C> Status value(C &c, FieldBattleEvent &v) {
  field(c, "sequence", v.sequence);
  field(c, "at", v.at);
  field(c, "kind", v.kind);
  field(c, "formation_id", v.formation_id);
  field(c, "target_formation_id", v.target_formation_id);
  field(c, "source_id", v.source_id);
  field(c, "count", v.count);
  field(c, "side", v.side);
  field(c, "tactic", v.tactic);
  field(c, "stratagem", v.stratagem);
  field(c, "success", v.success);
  // 已有持久尾与同一字段表对称保存，不猜补冷读缺项。
  return visit_field_event_payload_fields(c, v.kind, v.payloads);
}
template <class C> Status value(C &c, FieldSegmentSettlement &v) {
  field(c, "fact_id", v.fact_id);
  field(c, "recorded_tick", v.recorded_tick);
  field(c, "applied_at", v.applied_at);
  field(c, "segment", v.segment);
  field(c, "settled_at", v.settled_at);
  field(c, "casualty_fact_ids", v.casualty_fact_ids);
  field(c, "battle_fact_id", v.battle_fact_id);
  return c.status();
}
template <class C> Status value(C &c, FieldBattleViewIdentity &v) {
  field(c, "battle_id", v.battle_id);
  field(c, "observer_actor_id", v.observer_actor_id);
  field(c, "segment", v.segment);
  field(c, "revision", v.revision);
  field(c, "now", v.now);
  field(c, "state", v.state);
  return c.status();
}
template <class C> Status value(C &c, FieldCellView &v) {
  field(c, "terrain", v.terrain);
  field(c, "height_dm", v.height_dm);
  return c.status();
}
template <class C> Status value(C &c, FieldBattleFieldView &v) {
  field(c, "anchor", v.anchor);
  field(c, "field_hash", v.field_hash);
  field(c, "length_m", v.length_m);
  field(c, "width_m", v.width_m);
  field(c, "cell_m", v.cell_m);
  field(c, "cells", v.cells);
  return c.status();
}
template <class C> Status value(C &c, FieldBattleSideView &v) {
  field(c, "side", v.side);
  field(c, "display_name", v.display_name);
  field(c, "faction_id", v.faction_id);
  field(c, "controller", v.controller);
  field(c, "is_observer_side", v.is_observer_side);
  field(c, "deploy_zone", v.deploy_zone);
  field(c, "exits", v.exits);
  return c.status();
}
template <class C> Status value(C &c, FieldMissionView &v) {
  field(c, "kind", v.kind);
  field(c, "point_dm", v.point_dm);
  field(c, "facing_mdeg", v.facing_mdeg);
  field(c, "exit_side", v.exit_side);
  field(c, "target_formation_id", v.target_formation_id);
  return c.status();
}
template <class C> Status value(C &c, FieldCommanderView &v) {
  field(c, "actor_id", v.actor_id);
  field(c, "display_name", v.display_name);
  field(c, "state", v.state);
  return c.status();
}
template <class C> Status value(C &c, FieldTacticView &v) {
  field(c, "tactic_id", v.tactic_id);
  field(c, "kind", v.kind);
  field(c, "point_cost", v.point_cost);
  field(c, "ready_at", v.ready_at);
  return c.status();
}
template <class C> Status value(C &c, FieldStatusView &v) {
  field(c, "kind", v.kind);
  field(c, "until", v.until);
  return c.status();
}
template <class C> Status value(C &c, FieldFormationView &v) {
  field(c, "formation_id", v.formation_id);
  field(c, "army_id", v.army_id);
  field(c, "display_name", v.display_name);
  field(c, "category", v.category);
  field(c, "troop_type_id", v.troop_type_id);
  field(c, "side", v.side);
  field(c, "position_dm", v.position_dm);
  field(c, "facing_mdeg", v.facing_mdeg);
  field(c, "width_dm", v.width_dm);
  field(c, "depth_dm", v.depth_dm);
  field(c, "state", v.state);
  field(c, "control", v.control);
  field(c, "mission", v.mission);
  field(c, "fit", v.fit);
  field(c, "wounded", v.wounded);
  field(c, "dead", v.dead);
  field(c, "captured", v.captured);
  field(c, "deserted", v.deserted);
  field(c, "morale", v.morale);
  field(c, "commander", v.commander);
  field(c, "deputy_actor_id", v.deputy_actor_id);
  field(c, "traits", v.traits);
  field(c, "tactics", v.tactics);
  field(c, "tactic_points", v.tactic_points);
  field(c, "wave", v.wave);
  field(c, "statuses", v.statuses);
  return c.status();
}
template <class C> Status value(C &c, FieldOrderView &v) {
  field(c, "order_id", v.order_id);
  field(c, "recipient", v.recipient);
  field(c, "status", v.status);
  field(c, "mission", v.mission);
  field(c, "tactic_id", v.tactic_id);
  field(c, "issued", v.issued);
  field(c, "eta_range", v.eta_range);
  return c.status();
}
template <class C> Status value(C &c, FieldBattleEventView &v) {
  field(c, "sequence", v.sequence);
  field(c, "at", v.at);
  field(c, "kind", v.kind);
  field(c, "formation_id", v.formation_id);
  field(c, "target_formation_id", v.target_formation_id);
  field(c, "tactic", v.tactic);
  field(c, "stratagem", v.stratagem);
  field(c, "count", v.count);
  field(c, "success", v.success);
  return c.status();
}
template <class C> Status value(C &c, FieldBattleView &v) {
  field(c, "identity", v.identity);
  field(c, "field", v.field);
  field(c, "sides", v.sides);
  field(c, "formations", v.formations);
  field(c, "council", v.council);
  field(c, "orders", v.orders);
  field(c, "events", v.events);
  field(c, "result", v.result);
  return c.status();
}
template <class C> Status value(C &c, FieldBattleAdvanceResult &v) {
  field(c, "battle_id", v.battle_id);
  field(c, "from", v.from);
  field(c, "to", v.to);
  field(c, "events", v.events);
  field(c, "result", v.result);
  return c.status();
}
// 原 Common 前缀字段及严格形状门复用唯一算法；lease 不是持久权限。
template <class C> Status value(C &c, CommonActualFactPrefix &v) {
  return common_fact_prefix_fields::fields(
      c, v, [](auto &child, auto &source) { return value(child, source); });
}
template <class C> Status value(C &c, FieldControlAcceptanceBoundary &v) {
  field(c, "common_prefix", v.common_prefix);
  field(c, "last_local_event_sequence", v.last_local_event_sequence);
  field(c, "observed_at", v.observed_at);
  if (c.status().ok && v.last_local_event_sequence < 0)
    return c.reject("InvalidFieldControlAcceptanceBoundary");
  return c.status();
}
template <class C> Status value(C &c, FieldDeploymentApplied &v) {
  field(c, "position_dm", v.position_dm);
  field(c, "facing_mdeg", v.facing_mdeg);
  field(c, "control", v.control);
  field(c, "deputy_actor_id", v.deputy_actor_id);
  field(c, "immediate_mission", v.immediate_mission);
  field(c, "wave_mission", v.wave_mission);
  return c.status();
}
template <class C> Status value(C &c, FieldBattleMutationResult &v) {
  field(c, "battle_id", v.battle_id);
  field(c, "state", v.state);
  field(c, "at", v.at);
  field(c, "formation_id", v.formation_id);
  field(c, "order_id", v.order_id);
  field(c, "council_fact_id", v.council_fact_id);
  field(c, "proposal_id", v.proposal_id);
  field(c, "stratagem_id", v.stratagem_id);
  field(c, "mission_revision", v.mission_revision);
  field(c, "tactic_id", v.tactic_id);
  field(c, "request_index", v.request_index);
  field(c, "mission", v.mission);
  field(c, "effective_at", v.effective_at);
  field(c, "activation_id", v.activation_id);
  field(c, "council_stratagem", v.council_stratagem);
  field(c, "deployment", v.deployment);
  field(c, "actual_control_acceptance", v.actual_control_acceptance);
  field(c, "army_exit_choice_id", v.army_exit_choice_id);
  field(c, "selected_exit_cell", v.selected_exit_cell);
  return c.status();
}
template <class C> Status value(C &c, FieldBattleReceipt &v) {
  field(c, "result", v.result);
  return c.status();
}
template <class C> Status value(C &c, FieldCouncilProposalView &v) {
  field(c, "proposal_id", v.proposal_id);
  field(c, "speaker_actor_id", v.speaker_actor_id);
  field(c, "stance", v.stance);
  field(c, "plan", v.plan);
  field(c, "text_key", v.text_key);
  return c.status();
}
template <class C> Status value(C &c, FieldCouncilRequestView &v) {
  field(c, "actor_id", v.actor_id);
  field(c, "value", v.value);
  field(c, "status", v.status);
  field(c, "reason", v.reason);
  return c.status();
}
template <class C> Status value(C &c, FieldCouncilView &v) {
  field(c, "participants", v.participants);
  field(c, "intel", v.intel);
  field(c, "proposals", v.proposals);
  field(c, "requests", v.requests);
  field(c, "adopted_proposal_id", v.adopted_proposal_id);
  field(c, "closed", v.closed);
  return c.status();
}

template <class C> Status value(C &c, FieldCommanderCasualty &v) {
  field(c, "actor_id", v.actor_id);
  field(c, "loss", v.loss);
  field(c, "at", v.at);
  return c.status();
}

template <class C> Status value(C &c, FieldInitialContactBinding &v) {
  field(c, "contact_id", v.contact_id);
  field(c, "source_endpoint", v.source_endpoint);
  return c.status();
}

template <class C> Status value(C &c, FieldBoundaryContactBinding &v) {
  field(c, "contact_id", v.contact_id);
  return c.status();
}

template <class C> Status value(C &c, FieldBattleBoundaryRecord &v) {
  field(c, "contact_id", v.contact_id);
  field(c, "battle_id", v.battle_id);
  field(c, "source_tick", v.source_tick);
  field(c, "time_num", v.time_num);
  field(c, "time_den", v.time_den);
  field(c, "motion", v.motion);
  field(c, "entry_point_dm", v.entry_point_dm);
  field(c, "army_id", v.army_id);
  field(c, "side", v.side);
  field(c, "participant_actor_ids", v.participant_actor_ids);
  field(c, "diplomacy_sources", v.diplomacy_sources);
  field(c, "status", v.status);
  field(c, "entered_at", v.entered_at);
  field(c, "released_at", v.released_at);
  field(c, "entry_application_source", v.entry_application_source);
  field(c, "entered_observed_tick", v.entered_observed_tick);
  // 已有持久尾与同一字段表对称保存，不猜补冷读缺项。
  field(c, "entered_actual_at", v.entered_actual_at);
  field(c, "entered_binding", v.entered_binding);
  field(c, "reinforcement_queue", v.reinforcement_queue);
  field(c, "exact_continuation_source", v.exact_continuation_source);
  return c.status();
}

template <class C> Status value(C &c, FieldBattleExitRecord &v) {
  field(c, "fact_id", v.fact_id);
  field(c, "battle_id", v.battle_id);
  field(c, "formation_id", v.formation_id);
  field(c, "army_id", v.army_id);
  field(c, "at", v.at);
  field(c, "recorded_tick", v.recorded_tick);
  field(c, "field_point_dm", v.field_point_dm);
  field(c, "location", v.location);
  field(c, "source_action_id", v.source_action_id);
  field(c, "origin", v.origin);
  field(c, "named_actor_ids", v.named_actor_ids);
  field(c, "continuation", v.continuation);
  // 已有持久尾与同一字段表对称保存，不猜补冷读缺项。
  field(c, "applied_at", v.applied_at);
  field(c, "exact_route_progress", v.exact_route_progress);
  return c.status();
}

// 已有局部规则和援军载荷使用同一严格二进制/JSON/投影字段回调。
template <class C> Status value(C &c, FieldCommanderChange &v) {
  field(c, "source_ref", v.source_ref);
  field(c, "previous_actor_id", v.previous_actor_id);
  field(c, "commander_actor_id", v.commander_actor_id);
  field(c, "at", v.at);
  field(c, "observed_tick", v.observed_tick);
  field(c, "lost_event_sequence", v.lost_event_sequence);
  field(c, "applied_at", v.applied_at);
  return c.status();
}




template <class C> Status value(C &c, FieldReinforcementAdmission &v) {
  field(c, "application_source", v.application_source);
  field(c, "formation_id", v.formation_id);
  field(c, "at", v.at);
  field(c, "applied_at", v.applied_at);
  field(c, "event_sequence", v.event_sequence);
  field(c, "position_dm", v.position_dm);
  field(c, "facing_mdeg", v.facing_mdeg);
  field(c, "initial_control", v.initial_control);
  field(c, "initial_mission", v.initial_mission);
  field(c, "ai_mission", v.ai_mission);
  field(c, "pending_player_entry_order", v.pending_player_entry_order);
  return c.status();
}
template <class C> Status value(C &c, FieldReinforcementQueue &v) {
  field(c, "initialized_source", v.initialized_source);
  field(c, "initialized_at", v.initialized_at);
  field(c, "formation_ids", v.formation_ids);
  field(c, "admitted", v.admitted);
  return c.status();
}

// 单口来源、逐步几何与原接续引用共用二进制/JSON字段表。
template <class C> Status value(C &c, FieldArmyExitWeight &v) {
  field(c, "formation_id", v.formation_id);
  field(c, "position_dm", v.position_dm);
  field(c, "fit", v.fit);
  field(c, "wounded", v.wounded);
  return c.status();
}
template <class C> Status value(C &c, FieldArmyExitMotion &v) {
  field(c, "formation_id", v.formation_id);
  field(c, "from_dm", v.from_dm);
  field(c, "to_dm", v.to_dm);
  field(c, "reached", v.reached);
  field(c, "blocked", v.blocked);
  field(c, "deserted_member_ids", v.deserted_member_ids);
  return c.status();
}
template <class C> Status value(C &c, FieldArmyExitStep &v) {
  field(c, "fact_id", v.fact_id);
  field(c, "from", v.from);
  field(c, "to", v.to);
  field(c, "applied_at", v.applied_at);
  field(c, "formations", v.formations);
  return c.status();
}
template <class C> Status value(C &c, FieldArmyExitRallyFormation &v) {
  field(c, "formation_id", v.formation_id);
  field(c, "initial_position_dm", v.initial_position_dm);
  field(c, "ordinary_member_ids", v.ordinary_member_ids);
  field(c, "retained_officer_ids", v.retained_officer_ids);
  field(c, "original_exit", v.original_exit);
  return c.status();
}
template <class C> Status value(C &c, FieldArmyExitPlan &v) {
  field(c, "fact_id", v.fact_id);
  field(c, "battle_id", v.battle_id);
  field(c, "army_id", v.army_id);
  field(c, "commander_actor_id", v.commander_actor_id);
  field(c, "field_hash", v.field_hash);
  field(c, "destination_settlement_id", v.destination_settlement_id);
  field(c, "return_garrison_id", v.return_garrison_id);
  field(c, "exit_cell", v.exit_cell);
  field(c, "selected_at", v.selected_at);
  field(c, "applied_at", v.applied_at);
  field(c, "original_journey_opening", v.original_journey_opening);
  field(c, "selection_boundary", v.selection_boundary);
  field(c, "player_command_result", v.player_command_result);
  field(c, "weights", v.weights);
  field(c, "terminal_segment", v.terminal_segment);
  field(c, "ordinary_deadline", v.ordinary_deadline);
  field(c, "rally_formations", v.rally_formations);
  field(c, "steps", v.steps);
  field(c, "ready_source", v.ready_source);
  field(c, "ready_at", v.ready_at);
  field(c, "ready_applied_at", v.ready_applied_at);
  field(c, "actual_continuation", v.actual_continuation);
  return c.status();
}
