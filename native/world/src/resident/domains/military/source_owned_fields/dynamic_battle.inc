// 职责：集中军旅与战斗中的动态会战及其后果访问；仅借用原字段，不改变顺序或失败行为。
// 由中央登记头在 source_owned_fields
// 命名空间内包含；依赖与具名助手已先行可见。

template <class Visitor>
inline void
visit(const ::hanmo::resident::DynamicMilitaryActualEffectRead &value,
      Visitor &&visitor) {
  visit_dynamic_military_actual_effect_read_members(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::DynamicMilitaryActualEffectReadBuildView &value,
      Visitor &&visitor) {
  visit_dynamic_military_actual_effect_read_members(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicBattleEffectGroup2 &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.session_ref);
  visitor(value.effective_at);
  visitor(value.solver_ref);
  visitor(value.due_effect_refs);
  visitor(value.precondition_source_refs);
  visitor(value.geometry_basis_refs);
  visitor(value.posture_effects);
  visitor(value.strike_results);
  visitor(value.rng_slots);
  visitor(value.member_allocations);
  visitor(value.morale_changes);
  visitor(value.status_changes);
  visitor(value.vessel_changes);
  visitor(value.condition_source_refs);
  visitor(value.population_source_refs);
  visitor(value.objective_result_ref);
  visitor(value.terminal_refs);
  visitor(value.once_keys);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicBattleMoraleChange2 &value,
                  Visitor &&visitor) {
  visitor(value.layer);
  visitor(value.original_change);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicBattleStrikeResult2 &value,
                  Visitor &&visitor) {
  visitor(value.original_result);
  visitor(value.actual_target_count);
  visitor(value.actual_support_count);
  visitor(value.consumed_support_activation_ref);
  visitor(value.supporter_unit_ref);
  visitor(value.target_member_refs);
  visitor(value.support_member_refs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicBattleVesselChange2 &value,
                  Visitor &&visitor) {
  visitor(value.vessel_ref);
  visitor(value.before_integrity);
  visitor(value.actual_damage);
  visitor(value.after_integrity);
  visitor(value.original_effect_refs);
  visitor(value.actual_result_refs);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::DynamicDecisionAwaitingTrigger &value,
      Visitor &&visitor) {
  visitor(value.last_closed_decision_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicDecisionCommitted &value,
                  Visitor &&visitor) {
  visitor(value.decision_source_ref);
  visitor(value.adopted_intent_refs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicDecisionPending &value,
                  Visitor &&visitor) {
  visitor(value.trigger_source_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicDefendPosture &value,
                  Visitor &&visitor) {
  visitor(value.activation_ref);
  visitor(value.valid_from);
  visitor(value.expires_at);
  visitor(value.guard_value);
  visitor(value.qualification_source_refs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicFeintPosture &value,
                  Visitor &&visitor) {
  visitor(value.activation_ref);
  visitor(value.valid_from);
  visitor(value.expires_at);
  visitor(value.qualification_source_refs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicHoldUnqualified &value,
                  Visitor &&visitor) {
  visitor(value.last_condition_source_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicHolding &value,
                  Visitor &&visitor) {
  visitor(value.held_since);
  visitor(value.qualification_transition_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicLandActivationResult &value,
                  Visitor &&visitor) {
  visitor(value.effect_ref);
  visitor(value.action_ref);
  visitor(value.unit_ref);
  visitor(value.action_instance_ordinal);
  visitor(value.effect_ordinal);
  visitor(value.original_action_kind);
  visitor(value.original_tactic_ref);
  visitor(value.failure);
  visitor(value.original_point_cost);
  visitor(value.actual_target_unit_refs);
  visitor(value.installed_posture);
  visitor(value.original_qualification_sources);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicLandMemberAllocation &value,
                  Visitor &&visitor) {
  visitor(value.member_id);
  visitor(value.unit_ref);
  visitor(value.effect_ref);
  visitor(value.direct_target_unit_ref);
  visitor(value.support_share);
  visitor(value.resulting_status);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicLandMoraleChange &value,
                  Visitor &&visitor) {
  visitor(value.unit_ref);
  visitor(value.before_morale);
  visitor(value.delta);
  visitor(value.after_morale);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicLandReformChange &value,
                  Visitor &&visitor) {
  visitor(value.unit_ref);
  visitor(value.actual_confused_source);
  visitor(value.actual_activation_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicLandStatusChange &value,
                  Visitor &&visitor) {
  visitor(value.unit_ref);
  visitor(value.kind);
  visitor(value.before);
  visitor(value.after);
  visitor(value.actual_activation_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicLandStrikeResult &value,
                  Visitor &&visitor) {
  visitor(value.effect_ref);
  visitor(value.action_ref);
  visitor(value.attacker_unit_ref);
  visitor(value.target_unit_ref);
  visitor(value.action_instance_ordinal);
  visitor(value.effect_ordinal);
  visitor(value.original_damage_leaf);
  visitor(value.original_damage_version);
  visitor(value.failure);
  visitor(value.raw);
  visitor(value.potential);
  visitor(value.chance);
  visitor(value.flank_morale);
  visitor(value.tactic_morale);
  visitor(value.actual_random);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicLearnedGuardPosture &value,
                  Visitor &&visitor) {
  visitor(value.activation_ref);
  visitor(value.valid_from);
  visitor(value.expires_at);
  visitor(value.learned_tactic_entry_ref);
  visitor(value.qualification_source_refs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicMilitaryAction2 &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.session_ref);
  visitor(value.generation);
  visitor(value.unit_ref);
  visitor(value.accepted_intent_source_ref);
  visitor(value.accepted_at);
  visitor(value.timing_definition_ref);
  visitor(value.timing_definition_version);
  visitor(value.action_instance_ordinal);
  visitor(value.original_order);
  visitor(value.service_stages);
  visitor(value.phase);
  visitor(value.effect_state);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicMilitaryActive &value,
                  Visitor &&visitor) {
  (void)value;
  (void)visitor;
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicMilitaryBattleFact2 &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.session_ref);
  visitor(value.effective_at);
  visitor(value.started_at);
  visitor(value.effect_group_ref);
  visitor(value.army_ids);
  visitor(value.unit_ids);
  visitor(value.result);
  visitor(value.source_ref);
  visitor(value.contact_source_ref);
  visitor(value.winner_side);
  visitor(value.final);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicMilitaryBodyEntry &value,
                  Visitor &&visitor) {
  visitor(value.body_ref);
  visitor(value.initial_heading);
  visitor(value.actual_heading_source_ref);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::DynamicMilitaryBodyTacticalState &value,
      Visitor &&visitor) {
  visitor(value.local_body_ref);
  visitor(value.spatial_binding_ref);
  visitor(value.heading);
  visitor(value.heading_source_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicMilitaryCasualtyFact2 &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.effective_at);
  visitor(value.session_ref);
  visitor(value.effect_group_ref);
  visitor(value.original_casualty_rule_ref);
  visitor(value.roster_receipts);
  visitor(value.actor_changes);
  visitor(value.member_effect_links);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::DynamicMilitaryDecisionCurrent &value,
      Visitor &&visitor) {
  visitor(value.controller_ref);
  visitor(value.control_binding_generation);
  visitor(value.phase);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicMilitaryEffectOnceKey2 &value,
                  Visitor &&visitor) {
  visitor(value.session_ref);
  visitor(value.action_ref);
  visitor(value.effect_ordinal);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicMilitaryEscapedUnit &value,
                  Visitor &&visitor) {
  visitor(value.unit_ref);
  visitor(value.actual_exit_source_ref);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::DynamicMilitaryObjectiveCurrent &value,
      Visitor &&visitor) {
  visitor(value.objective_definition_ref);
  visitor(value.started_at);
  visitor(value.deadline_at);
  visitor(value.hold_state);
  visitor(value.escaped_units);
  visitor(value.last_transition_source_ref);
  visitor(value.outcome_source_ref);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::DynamicMilitaryObjectiveEvidence2 &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.session_ref);
  visitor(value.generation);
  visitor(value.effective_at);
  visitor(value.objective_definition_ref);
  visitor(value.session_initialization_source_ref);
  visitor(value.objective_transition_source_ref);
  visitor(value.cause_kind);
  visitor(value.cause_source_refs);
  visitor(value.actual_judgment_source_refs);
  visitor(value.actual_exit_source_refs);
  visitor(value.outcome);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicMilitaryRandomSlot &value,
                  Visitor &&visitor) {
  visitor(value.effect_ref);
  visitor(value.stage);
  visitor(value.stream_identity);
  visitor(value.algorithm_version);
  visitor(value.requested_min);
  visitor(value.requested_max);
  visitor(value.seed);
  visitor(value.before_state);
  visitor(value.after_state);
  visitor(value.before_draw_ordinal);
  visitor(value.after_draw_ordinal);
  visitor(value.draw_result);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::DynamicMilitarySessionCurrent2 &value,
      Visitor &&visitor) {
  visitor(value.session_ref);
  visitor(value.generation);
  visitor(value.definition_lock_ref);
  visitor(value.initialization_source_ref);
  visitor(value.opened_at);
  visitor(value.lifecycle);
  visitor(value.body_tactical_states);
  visitor(value.unit_tactical_states);
  visitor(value.objective_current);
  visitor(value.decision_current);
  visitor(value.session_rng);
  visitor(value.last_session_transition_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicMilitarySessionRandom &value,
                  Visitor &&visitor) {
  visitor(value.stream_identity);
  visitor(value.algorithm_version);
  visitor(value.seed);
  visitor(value.current_state);
  visitor(value.draw_ordinal);
  visitor(value.last_commit_source_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicMilitaryStatus &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.retained_activation_ref);
  visitor(value.valid_from);
  visitor(value.expires_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicMilitaryStreamIdentity &value,
                  Visitor &&visitor) {
  visitor(value.session_ref);
  visitor(value.generation);
  visitor(value.initialization_source_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicMilitaryTerminal &value,
                  Visitor &&visitor) {
  visitor(value.outcome_source_ref);
  visitor(value.closed_at);
  visitor(value.unresolved_committed_effect_refs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicMilitaryUnitEntry &value,
                  Visitor &&visitor) {
  visitor(value.unit_ref);
  visitor(value.body_ref);
  visitor(value.entry_snapshot_ref);
  visitor(value.initial_tactic_points);
  visitor(value.original_point_initialization_source_ref);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::DynamicMilitaryUnitTacticalState &value,
      Visitor &&visitor) {
  visitor(value.unit_ref);
  visitor(value.body_ref);
  visitor(value.entry_snapshot_ref);
  visitor(value.tactic_points);
  visitor(value.tactic_points_source_ref);
  visitor(value.statuses);
  visitor(value.posture);
  visitor(value.qualification_source_refs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicNoPosture &value,
                  Visitor &&visitor) {
  (void)value;
  (void)visitor;
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicNotHold &value,
                  Visitor &&visitor) {
  (void)value;
  (void)visitor;
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::DynamicObjectiveQualificationTransition &value,
      Visitor &&visitor) {
  visitor(value.objective_definition_ref);
  visitor(value.from_state);
  visitor(value.to_state);
  visitor(value.previous_holding_source_ref);
  visitor(value.cause_kind);
  visitor(value.cause_source_refs);
  visitor(value.actual_qualified_unit_refs);
  visitor(value.actual_contesting_unit_refs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicPostureClosed &value,
                  Visitor &&visitor) {
  visitor(value.unit_ref);
  visitor(value.activation_ref);
  visitor(value.reason);
  visitor(value.expiry_or_cause_ref);
  visitor(value.replacement_activation_ref);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::DynamicSessionDecisionCommitted &value,
      Visitor &&visitor) {
  visitor(value.controller_ref);
  visitor(value.control_binding_generation);
  visitor(value.trigger_source_ref);
  visitor(value.knowledge_snapshot_ref);
  visitor(value.policy_definition_ref);
  visitor(value.selected_intent_refs);
  visitor(value.rng_slots);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicSessionInitialized &value,
                  Visitor &&visitor) {
  visitor(value.entry_or_contact_source_ref);
  visitor(value.definition_lock_ref);
  visitor(value.unit_entry_rows);
  visitor(value.body_entry_rows);
  visitor(value.objective_definition_ref);
  visitor(value.initial_objective_condition_ref);
  visitor(value.initial_rng_source_ref);
  visitor(value.initial_rng_seed);
  visitor(value.initial_rng_state);
  visitor(value.initial_draw_ordinal);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicSessionStateSource2 &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.session_ref);
  visitor(value.generation);
  visitor(value.effective_at);
  visitor(value.payload);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicSessionTerminated &value,
                  Visitor &&visitor) {
  visitor(value.objective_outcome_source_ref);
  visitor(value.actual_exit_or_stay_source_refs);
  visitor(value.closed_posture_refs);
  visitor(value.retained_committed_effect_refs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicStatusExpired &value,
                  Visitor &&visitor) {
  visitor(value.unit_ref);
  visitor(value.status_kind);
  visitor(value.retained_activation_ref);
  visitor(value.scheduled_expires_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicSupportConsumption2 &value,
                  Visitor &&visitor) {
  visitor(value.supporter_unit_ref);
  visitor(value.target_unit_ref);
  visitor(value.activation_ref);
  visitor(value.consumed_by_effect_ref);
  visitor(value.actual_shared_members);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicSupportPosture &value,
                  Visitor &&visitor) {
  visitor(value.token);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicSupportToken &value,
                  Visitor &&visitor) {
  visitor(value.activation_ref);
  visitor(value.supporter_unit_ref);
  visitor(value.target_unit_ref);
  visitor(value.valid_from);
  visitor(value.expires_at);
  visitor(value.qualification_source_refs);
  visitor(value.consumed_by_effect_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicTacticPointChange2 &value,
                  Visitor &&visitor) {
  visitor(value.unit_ref);
  visitor(value.before_points);
  visitor(value.delta);
  visitor(value.cap);
  visitor(value.after_points);
  visitor(value.actual_cost_source_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicTacticalApplied &value,
                  Visitor &&visitor) {
  visitor(value.effect_source_ref);
  visitor(value.effective_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicTacticalCompleted &value,
                  Visitor &&visitor) {
  visitor(value.result_source_ref);
  visitor(value.completed_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicTacticalInterrupted &value,
                  Visitor &&visitor) {
  visitor(value.actual_cause);
  visitor(value.interrupted_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicTacticalPreparing &value,
                  Visitor &&visitor) {
  (void)value;
  (void)visitor;
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicTacticalRecovering &value,
                  Visitor &&visitor) {
  visitor(value.effect_source_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicTacticalReleased &value,
                  Visitor &&visitor) {
  visitor(value.release_source_ref);
  visitor(value.released_at);
  visitor(value.impact_due_at);
  visitor(value.impact_source_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicTacticalUncommitted &value,
                  Visitor &&visitor) {
  (void)value;
  (void)visitor;
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicMilitaryActionFact2 &value,
                  Visitor &&visitor) {
  visit_DynamicMilitaryActionFact2_members(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::DynamicMilitaryActionFactBuildView2 &value,
      Visitor &&visitor) {
  visit_DynamicMilitaryActionFact2_members(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicMilitaryLogisticsFact2 &value,
                  Visitor &&visitor) {
  visit_DynamicMilitaryLogisticsFact2_members(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::DynamicMilitaryLogisticsFactBuildView2 &value,
      Visitor &&visitor) {
  visit_DynamicMilitaryLogisticsFact2_members(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicMilitaryReturnFact2 &value,
                  Visitor &&visitor) {
  visit_DynamicMilitaryReturnFact2_members(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::DynamicMilitaryReturnFactBuildView2 &value,
      Visitor &&visitor) {
  visit_DynamicMilitaryReturnFact2_members(value, visitor);
}

// native/world/src/resident/domains/military/training_exact_model.hpp:40;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicMilitaryTrainingFact2 &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.actor_id);
  visitor(value.phase);
  visitor(value.started_at);
  visitor(value.completed_at);
  visitor(value.success);
  visitor(value.terms);
  visitor(value.elapsed_source_ref);
  visitor(value.work_binding_ref);
  visitor(value.lessons);
  visitor(value.terminal_code);
  visitor(value.terminal_source_ref);
  visitor(value.drill);
  visitor(value.rule_version);
  visitor(value.completion_application_source);
}

// native/world/src/resident/domains/military/training_exact_model.hpp:32;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void
visit(const ::hanmo::resident::DynamicMilitaryTrainingLesson2 &value,
      Visitor &&visitor) {
  visitor(value.ordinal);
  visitor(value.service_from);
  visitor(value.closed_at);
  visitor(value.gate_work_units);
  visitor(value.participation_span_refs);
  visitor(value.period_witness_refs);
  visitor(value.closing_supply_gate);
  visitor(value.fee_paid);
  visitor(value.fee_effect_ref);
  visitor(value.training_before);
  visitor(value.training_after);
  visitor(value.fatigue_before);
  visitor(value.fatigue_after);
  visitor(value.drill_frozen_source_ref);
  visitor(value.learning_result_ref);
}

// native/world/src/resident/domains/military/naval_repair_exact_model.hpp:6;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicNavalRepairTiming2 &value,
                  Visitor &&visitor) {
  visitor(value.start_fact_ref);
  visitor(value.action_ref);
  visitor(value.started_at);
  visitor(value.completion_or_interruption_at);
  visitor(value.elapsed_ref);
  visitor(value.result_ref);
  visitor(value.charge_source_refs);
}

// native/world/src/resident/domains/military/training_exact_model.hpp:12;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicTrainingDrillInstance2 &value,
                  Visitor &&visitor) {
  visitor(value.origin_seed);
  visitor(value.origin_state);
  visitor(value.origin_field_order);
  visitor(value.challenge);
  visitor(value.phase);
  visitor(value.frozen_at);
  visitor(value.performance_bp);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicMilitaryActionCatalog2 &value,
                  Visitor &&visitor) {
  visitor(value.timings);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicLandAllocationUnit &value,
                  Visitor &&visitor) {
  visitor(value.unit_ref);
  visitor(value.original_fit_ids);
  visitor(value.original_morale);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicLandStrikeDamage &value,
                  Visitor &&visitor) {
  visitor(value.effect_ref);
  visitor(value.action_ref);
  visitor(value.attacker_unit_ref);
  visitor(value.target_unit_ref);
  visitor(value.action_instance_ordinal);
  visitor(value.effect_ordinal);
  visitor(value.raw_casualties);
  visitor(value.original_flank_morale);
  visitor(value.original_tactic_morale);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicLandQualifiedSupport &value,
                  Visitor &&visitor) {
  visitor(value.token);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicLandStrikeAllocation &value,
                  Visitor &&visitor) {
  visitor(value.effect_ref);
  visitor(value.raw_casualties);
  visitor(value.actual_target_count);
  visitor(value.actual_support_count);
  visitor(value.consumed_support_activation_ref);
  visitor(value.supporter_unit_ref);
  visitor(value.target_member_ids);
  visitor(value.support_member_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicLandMergedAllocation &value,
                  Visitor &&visitor) {
  visitor(value.strikes);
  visitor(value.member_allocations);
  visitor(value.morale_changes);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::DynamicLandCommanderConsequence &value,
      Visitor &&visitor) {
  visitor(value.unit_ref);
  visitor(value.army_ref);
  visitor(value.actor_ref);
  visitor(value.before_loss);
  visitor(value.after_loss);
  visitor(value.actual_enlisted_member);
  visitor(value.actual_effect_group);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicCommandShockCause &value,
                  Visitor &&visitor) {
  visitor(value.army_id);
  visitor(value.actor_id);
  visitor(value.unit_id);
  visitor(value.kind);
  visitor(value.actual_effect_group);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicLandMemberDisposition &value,
                  Visitor &&visitor) {
  visitor(value.unit_ref);
  visitor(value.member_ref);
  visitor(value.before_status);
  visitor(value.after_status);
  visitor(value.captor_unit_ref);
  visitor(value.captor_army_ref);
  visitor(value.actual_effect_group);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicLandActualExit &value,
                  Visitor &&visitor) {
  visitor(value.unit_ref);
  visitor(value.accepted_withdrawal_source);
  visitor(value.actual_exit_geometry_source);
  visitor(value.actual_effect_group);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicLandDispositionMerge &value,
                  Visitor &&visitor) {
  visitor(value.member_dispositions);
  visitor(value.actual_exits);
}

template <class Visitor>
inline void visit_payload(
    const ::hanmo::resident::DynamicMilitaryLandConsequenceRead &value,
    Visitor &&visitor) {
  visit_land_consequence_payload(value, visitor);
}

template <class Visitor>
inline void visit_payload(
    const ::hanmo::resident::DynamicMilitaryLandConsequenceBuildView &value,
    Visitor &&visitor) {
  visit_land_consequence_payload(value, visitor);
}

template <class Visitor>
inline void visit_payload(
    const ::hanmo::resident::DynamicMilitaryLandConsequenceApplication &value,
    Visitor &&visitor) {
  visitor(value.battle_source);
  visitor(value.casualty_source);
  visitor(value.actual_named_consequences);
  visitor(value.already_applied);
}

template <class Visitor>
inline void
visit_payload(const ::hanmo::resident::DynamicMilitaryCommanderLossRead &value,
              Visitor &&visitor) {
  visit_DynamicMilitaryCommanderLossRead_payload(value, visitor);
}

template <class Visitor>
inline void visit_payload(
    const ::hanmo::resident::DynamicMilitaryCommanderLossBuildView &value,
    Visitor &&visitor) {
  visit_DynamicMilitaryCommanderLossRead_payload(value, visitor);
}

// 遍历原会话定义的五个实际拥有字段，供复制和保留共用。
// 只登记既有读值，不补运行会话、入场来源或默认规则。
template <class Visitor>
inline void
visit(const ::hanmo::resident::DynamicMilitarySessionDefinitionRead &value,
      Visitor &&visitor) {
  visitor(value.definition_lock_ref);
  visitor(value.objective_definition_ref);
  visitor(value.random_algorithm_version);
  visitor(value.deadline_duration);
  visitor(value.hold_duration);
}

// 遍历原会话定义目录唯一的实际拥有表。
// 只借用 definitions，不创建第二份目录身份或领域状态。
template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitarySessionDefinitionCatalog &value,
      Visitor &&visitor) {
  visitor(value.definitions);
}
