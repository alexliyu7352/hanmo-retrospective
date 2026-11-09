// 职责：集中军旅与战斗中的军籍职务、训练、军令和共同战斗状态访问；仅借用原字段，不改变顺序或失败行为。
// 由中央登记头在 source_owned_fields
// 命名空间内包含；依赖与具名助手已先行可见。

template <class Visitor>
inline void visit(const ::hanmo::resident::BattleUnit &value,
                  Visitor &&visitor) {
  visitor(value.unit_id);
  visitor(value.army_id);
  visitor(value.commander_actor_id);
  visitor(value.troop_type_id);
  visitor(value.side);
  visitor(value.position);
  visitor(value.heading);
  visitor(value.morale);
  visitor(value.tactic_points);
  visitor(value.statuses);
  visitor(value.withdrawn);
  visitor(value.member_ids);
  visitor(value.fit);
  visitor(value.wounded);
  visitor(value.dead);
  visitor(value.captured);
  visitor(value.deserted);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DrillMove &value,
                  Visitor &&visitor) {
  visitor(value.group_id);
  visitor(value.from);
  visitor(value.target);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DrillObjectives &value,
                  Visitor &&visitor) {
  visitor(value.supply);
  visitor(value.passage);
  visitor(value.screen);
  visitor(value.cavalry);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DrillPending &value,
                  Visitor &&visitor) {
  visitor(value.group_id);
  visitor(value.target);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DrillResult &value,
                  Visitor &&visitor) {
  visitor(value.completion);
  visitor(value.score);
  visitor(value.raw_score);
  visitor(value.auto_score);
  visitor(value.performance_bp);
  visitor(value.objectives);
  visitor(value.choices);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DrillSnapshot &value,
                  Visitor &&visitor) {
  visitor(value.skill_level);
  visitor(value.auxiliary_attribute);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DrillState &value,
                  Visitor &&visitor) {
  visitor(value.format);
  visitor(value.definition_id);
  visitor(value.source_sha256);
  visitor(value.actor_id);
  visitor(value.source_id);
  visitor(value.binding_field_order);
  visitor(value.actor_snapshot);
  visitor(value.seed);
  visitor(value.mode);
  visitor(value.phase);
  visitor(value.moves);
  visitor(value.pending);
  visitor(value.result);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LegacyMilitaryOfficeRoot &value,
                  Visitor &&visitor) {
  visitor(value.profile_id);
  visitor(value.military_schema_version);
  visitor(value.military_office_id);
  visitor(value.office_definition_id);
  visitor(value.holder_actor_id);
  visitor(value.enabled);
  visitor(value.original_appointment_id);
  visitor(value.organization_id);
  visitor(value.garrison_id);
  visitor(value.command_scope);
  visitor(value.source_world);
  visitor(value.original_source_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryActionFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.command);
  visitor(value.actor_id);
  visitor(value.start_tick);
  visitor(value.completed_tick);
  visitor(value.phase);
  visitor(value.success);
  visitor(value.terms);
  visitor(value.context);
  visitor(value.result);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryActionPayload &value,
                  Visitor &&visitor) {
  visitor(value.action_id);
  visitor(value.terms);
  visitor(value.request);
  visitor(value.context);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryActionResult &value,
                  Visitor &&visitor) {
  visit_MilitaryActionResult_members(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryActionResultBuildView &value,
                  Visitor &&visitor) {
  visit_MilitaryActionResult_members(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryAuthorityRef &value,
                  Visitor &&visitor) {
  visitor(value.principal);
  visitor(value.acting_actor_id);
  visitor(value.source);
  visitor(value.allowed_scope);
  visitor(value.allowed_target_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryBattleEvent &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.unit_id);
  visitor(value.target_id);
  visitor(value.attacker_id);
  visitor(value.tactic_id);
  visitor(value.code);
  visitor(value.captor_unit_id);
  visitor(value.captor_army_id);
  visitor(value.reason);
  visitor(value.morale);
  visitor(value.casualties);
  visitor(value.hit_chance);
  visitor(value.original_casualties);
  visitor(value.captured);
  visitor(value.deserted);
  visitor(value.winner_side);
  visitor(value.position);
  visitor(value.captured_ids);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryBattleFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.phase);
  visitor(value.tick);
  visitor(value.start_tick);
  visitor(value.round);
  visitor(value.session_id);
  visitor(value.army_ids);
  visitor(value.unit_ids);
  visitor(value.result);
  visitor(value.source_id);
  visitor(value.contact_source_id);
  visitor(value.winner_side);
  visitor(value.final);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryBattleLifecycleEvent1 &value,
                  Visitor &&visitor) {
  visitor(value.source_id);
  visitor(value.occurred_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryBattleLifecycle1 &value,
                  Visitor &&visitor) {
  visitor(value.started);
  visitor(value.closed);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryBattleRecord &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.kind);
  visitor(value.battlefield_id);
  visitor(value.objective_id);
  visitor(value.objective_unit_ids);
  visitor(value.phase);
  visitor(value.round);
  visitor(value.units);
  visitor(value.ai_plans);
  visitor(value.objective_state);
  visitor(value.rng_seed);
  visitor(value.rng_state);
  visitor(value.rng_field_order);
  visitor(value.last_events);
  visitor(value.result);
  visitor(value.created_tick);
  visitor(value.last_tick);
  visitor(value.completed_tick);
  visitor(value.source_id);
  visitor(value.army_ids);
  visitor(value.participant_actor_ids);
  visitor(value.plan_submitted);
  visitor(value.pending_orders);
  visitor(value.round_receipt_ids);
  visitor(value.settlement_receipt_id);
  visitor(value.applied);
  visitor(value.controller_actor_id);
  visitor(value.controller_side);
  visitor(value.command_scope);
  visitor(value.command_army_id);
  visitor(value.command_unit_ids);
  visitor(value.authority_office_id);
  visitor(value.field_order);
  visitor(value.rout_retreat_enabled);
  visitor(value.command_shock);
  visitor(value.combat_conditions);
  visitor(value.commanded_damage);
  visitor(value.command_resources);
  visitor(value.learned_tactics);
  visitor(value.lifecycle);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryBattleReleaseFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.battle_id);
  visitor(value.actor_id);
  visitor(value.settlement_receipt_id);
  visitor(value.tick);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryBattleRequest &value,
                  Visitor &&visitor) {
  visitor(value.battle_id);
  visitor(value.request_key);
  visitor(value.request_kind);
  visitor(value.unit_id);
  visitor(value.target_unit_id);
  visitor(value.fields_valid);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryBattleResult &value,
                  Visitor &&visitor) {
  visitor(value.winner_side);
  visitor(value.reason);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryCasualtyFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.battle_id);
  visitor(value.tick);
  visitor(value.units);
  visitor(value.actor_changes);
  visitor(value.field_order);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitaryCombatConditionSnapshot &value,
      Visitor &&visitor) {
  visitor(value.unit_id);
  visitor(value.source_garrison_id);
  visitor(value.last_condition_fact_id);
  visitor(value.training);
  visitor(value.fatigue);
  visitor(value.efficiency_bp);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryCombatConditionState &value,
                  Visitor &&visitor) {
  visitor(value.entry_tick);
  visitor(value.units);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryCommandResourcesState &value,
                  Visitor &&visitor) {
  visitor(value.entry_tick);
  visitor(value.units);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryCommandResourcesUnit &value,
                  Visitor &&visitor) {
  visitor(value.unit_id);
  visitor(value.commander_actor_id);
  visitor(value.authored_actor_id);
  visitor(value.experience);
  visitor(value.level);
  visitor(value.causal_fact_ids);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryCommandShockCause &value,
                  Visitor &&visitor) {
  visitor(value.army_id);
  visitor(value.actor_id);
  visitor(value.unit_id);
  visitor(value.kind);
  visitor(value.round);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryCommandShockLeader &value,
                  Visitor &&visitor) {
  visitor(value.army_id);
  visitor(value.actor_id);
  visitor(value.unit_id);
  visitor(value.command_source);
  visitor(value.field_order);
  visitor(value.source_field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryCommandShockState &value,
                  Visitor &&visitor) {
  visitor(value.leaders);
  visitor(value.causes);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryCommandedDamageState &value,
                  Visitor &&visitor) {
  visitor(value.entry_tick);
  visitor(value.units);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryCommandedDamageUnit &value,
                  Visitor &&visitor) {
  visitor(value.unit_id);
  visitor(value.commander_actor_id);
  visitor(value.troop_type_id);
  visitor(value.skill_id);
  visitor(value.authored_actor_id);
  visitor(value.command_source);
  visitor(value.leadership);
  visitor(value.experience);
  visitor(value.level);
  visitor(value.training);
  visitor(value.causal_fact_ids);
  visitor(value.field_order);
  visitor(value.source_field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryContactFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.battlefield_id);
  visitor(value.objective_id);
  visitor(value.tick);
  visitor(value.unit_ids);
  visitor(value.objective_unit_ids);
  visitor(value.side_by_army);
  visitor(value.army_order_ids);
  visitor(value.army_locations);
  visitor(value.time_num);
  visitor(value.time_den);
  visitor(value.motion_sources);
  visitor(value.siege_id);
  visitor(value.node_id);
  visitor(value.battle_id);
  visitor(value.objective_field_order);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryExactState &value,
                  Visitor &&visitor) {
  visitor(value.actions);
  visitor(value.session_current_states);
  visitor(value.session_state_sources);
  visitor(value.local_motion);
  visitor(value.spatial_handoffs);
  visitor(value.effect_groups);
  visitor(value.dynamic_battle_facts);
  visitor(value.dynamic_casualty_facts);
  visitor(value.supply_effect_sources);
  visitor(value.duty_sources);
  visitor(value.objective_evidence);
  visitor(value.formations);
  visitor(value.missions);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryFieldCommandDeath &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.casualty_fact_id);
  visitor(value.battle_id);
  visitor(value.tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryFieldCommandFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.request_key);
  visitor(value.actor_id);
  visitor(value.authorization_fact_id);
  visitor(value.acceptance_fact_id);
  visitor(value.target);
  visitor(value.issuer_actor_id);
  visitor(value.office_id);
  visitor(value.grant_id);
  visitor(value.vessel_id);
  visitor(value.authority_epoch);
  visitor(value.issued_tick);
  visitor(value.accepted_tick);
  visitor(value.tick);
  visitor(value.army_anchor);
  visitor(value.field_order);
  visitor(value.target_field_order);
  visitor(value.source_field_order);
  visitor(value.candidate_source_field_order);
  visitor(value.death_field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryFieldCommandSource &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryFieldCommandTarget &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.organization_id);
  visitor(value.army_id);
  visitor(value.campaign_id);
  visitor(value.formation_source_id);
  visitor(value.unit_id);
  visitor(value.previous_actor_id);
  visitor(value.next_actor_id);
  visitor(value.previous_source);
  visitor(value.candidate_unit_source);
  visitor(value.predecessor_death);
  visitor(value.fleet_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryGarrisonRestFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.unit_id);
  visitor(value.garrison_id);
  visitor(value.organization_id);
  visitor(value.ownership_id);
  visitor(value.supply_fact_id);
  visitor(value.tick);
  visitor(value.training);
  visitor(value.fatigue_before);
  visitor(value.fatigue_after);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryGarrisonRestState &value,
                  Visitor &&visitor) {
  visitor(value.facts);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryGroupOrderAcceptance &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.campaign_id);
  visitor(value.army_id);
  visitor(value.issuing_actor_id);
  visitor(value.commander_actor_id);
  visitor(value.formation_source);
  visitor(value.command_source);
  visitor(value.issuing_authority);
  visitor(value.issuing_condition_head);
  visitor(value.commander_condition_head);
  visitor(value.destination_settlement_id);
  visitor(value.return_garrison_id);
  visitor(value.accepted_at);
  visitor(value.original_authority_sources);
  visitor(value.original_return_cause);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryMemberEffectLink &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.effect_index);
  visitor(value.purpose);
  visitor(value.effect_source);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryObjectiveProgress &value,
                  Visitor &&visitor) {
  visitor(value.hold_rounds);
  visitor(value.escaped_unit_ids);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryOccupationFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.phase);
  visitor(value.tick);
  visitor(value.siege_id);
  visitor(value.campaign_id);
  visitor(value.army_id);
  visitor(value.stronghold_id);
  visitor(value.garrison_id);
  visitor(value.from_organization_id);
  visitor(value.to_organization_id);
  visitor(value.previous_occupation_id);
  visitor(value.public_inventory_ids);
  visitor(value.public_wallet_ids);
  visitor(value.assets_applied);
  visitor(value.asset_manifest);
  visitor(value.custody_transfers);
  visitor(value.disabled_office_ids);
  visitor(value.actor_changes);
  visitor(value.field_order);
  visitor(value.exact_time);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryOccupationTime2 &value,
                  Visitor &&visitor) {
  visitor(value.actual_occupation_at);
  visitor(value.producer_kind);
  visitor(value.trigger_source);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryOfficerAssignmentFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.request_key);
  visitor(value.actor_id);
  visitor(value.authorization_fact_id);
  visitor(value.acceptance_fact_id);
  visitor(value.place_id);
  visitor(value.target);
  visitor(value.issuer_actor_id);
  visitor(value.office_id);
  visitor(value.grant_id);
  visitor(value.authority_epoch);
  visitor(value.issued_tick);
  visitor(value.accepted_tick);
  visitor(value.tick);
  visitor(value.source_field_order);
  visitor(value.target_field_order);
  visitor(value.field_order);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitaryOfficerAssignmentTarget &value,
      Visitor &&visitor) {
  visitor(value.organization_id);
  visitor(value.garrison_id);
  visitor(value.unit_id);
  visitor(value.previous_actor_id);
  visitor(value.next_actor_id);
  visitor(value.previous_source);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryOrder &value,
                  Visitor &&visitor) {
  visitor(value.action);
  visitor(value.destination);
  visitor(value.heading);
  visitor(value.target_id);
  visitor(value.tactic_id);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryRecoveryFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.casualty_fact_id);
  visitor(value.unit_id);
  visitor(value.garrison_id);
  visitor(value.organization_id);
  visitor(value.ownership_id);
  visitor(value.supply_fact_id);
  visitor(value.tick);
  visitor(value.qualified_half_days_before);
  visitor(value.qualified_half_days_after);
  visitor(value.recovered_ids);
  visitor(value.actor_recovery_sources);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryRecoveryState &value,
                  Visitor &&visitor) {
  visitor(value.facts);
  visitor(value.battle_releases);
  visitor(value.siege_releases);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryReliefFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.phase);
  visitor(value.tick);
  visitor(value.siege_id);
  visitor(value.campaign_id);
  visitor(value.army_id);
  visitor(value.stronghold_id);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryRequestDecision &value,
                  Visitor &&visitor) {
  visitor(value.accepted);
  visitor(value.request_kind);
  visitor(value.reason);
  visitor(value.response_unit_id);
  visitor(value.response_order);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryRequestReceipt &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.actor_id);
  visitor(value.commander_actor_id);
  visitor(value.army_id);
  visitor(value.office_id);
  visitor(value.formation_source);
  visitor(value.appointment_source);
  visitor(value.tick);
  visitor(value.round);
  visitor(value.request);
  visitor(value.decision);
  visitor(value.previous_order);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryResignationFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.actor_id);
  visitor(value.office_id);
  visitor(value.tick);
  visitor(value.field_order);
  visitor(value.exact_resignation);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryRosterReceipt &value,
                  Visitor &&visitor) {
  visitor(value.unit_id);
  visitor(value.fit);
  visitor(value.wounded);
  visitor(value.dead);
  visitor(value.captured);
  visitor(value.deserted);
  visitor(value.member_ids);
  visitor(value.morale);
  visitor(value.captors);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryRoutEntryFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.tick);
  visitor(value.initial);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryRoutProgress &value,
                  Visitor &&visitor) {
  visitor(value.unit_id);
  visitor(value.started_round);
  visitor(value.blocked_rounds);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryRoutRoundFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.battle_id);
  visitor(value.battle_round_fact_id);
  visitor(value.casualty_fact_id);
  visitor(value.tick);
  visitor(value.round);
  visitor(value.resolution_kind);
  visitor(value.termination_reason);
  visitor(value.before);
  visitor(value.after);
  visitor(value.damage_sources);
  visitor(value.progress_before);
  visitor(value.progress_after);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryRoutState &value,
                  Visitor &&visitor) {
  visitor(value.entries);
  visitor(value.rounds);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryState &value,
                  Visitor &&visitor) {
  visitor(value.recovery);
  visitor(value.garrison_rest);
  visitor(value.supply_attrition);
  visitor(value.rout);
  visitor(value.field_commands);
  visitor(value.officer_assignments);
  visitor(value.request_facts);
  visitor(value.battles);
  visitor(value.sieges);
  visitor(value.siege_engineering);
  visitor(value.siege_lifecycle);
  visitor(value.training_facts);
  visitor(value.training_actions);
  visitor(value.march_facts);
  visitor(value.occupation_facts);
  visitor(value.relief_facts);
  visitor(value.siege_material_facts);
  visitor(value.logistics_actions);
  visitor(value.logistics_facts);
  visitor(value.return_facts);
  visitor(value.muster_facts);
  visitor(value.resignation_facts);
  visitor(value.delivery_facts);
  visitor(value.settlement_facts);
  visitor(value.contact_facts);
  visitor(value.battle_facts);
  visitor(value.casualty_facts);
  visitor(value.offices);
  visitor(value.garrisons);
  visitor(value.military_units);
  visitor(value.armies);
  visitor(value.campaigns);
  visitor(value.arrival_facts);
  visitor(value.strongholds);
  visitor(value.actions);
  visitor(value.action_facts);
  visitor(value.supply_facts);
  visitor(value.siege_integration);
  visitor(value.integration_facts);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryTacticAcquisitionFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.actor_id);
  visitor(value.tactic_id);
  visitor(value.source_id);
  visitor(value.source_kind);
  visitor(value.tick);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryTacticEntryState &value,
                  Visitor &&visitor) {
  visitor(value.entry_tick);
  visitor(value.units);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryTacticEntryUnit &value,
                  Visitor &&visitor) {
  visitor(value.unit_id);
  visitor(value.commander_actor_id);
  visitor(value.tactic_acquisition_ids);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryTacticLearningState &value,
                  Visitor &&visitor) {
  visitor(value.acquisitions);
  visitor(value.field_order);
  visitor(value.practices);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryTacticPracticeFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.actor_id);
  visitor(value.tactic_id);
  visitor(value.training_action_id);
  visitor(value.tick);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryTrainingAction &value,
                  Visitor &&visitor) {
  visitor(value.action_id);
  visitor(value.training);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryTrainingFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.phase);
  visitor(value.actor_id);
  visitor(value.start_tick);
  visitor(value.completed_tick);
  visitor(value.success);
  visitor(value.terms);
  visitor(value.lessons);
  visitor(value.code);
  visitor(value.drill);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryTrainingLesson &value,
                  Visitor &&visitor) {
  visitor(value.tick);
  visitor(value.supply_fact_id);
  visitor(value.fee_paid);
  visitor(value.training_before);
  visitor(value.training_after);
  visitor(value.fatigue_before);
  visitor(value.fatigue_after);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryTrainingTerms &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.office_id);
  visitor(value.authority_source);
  visitor(value.unit_id);
  visitor(value.garrison_id);
  visitor(value.organization_id);
  visitor(value.ownership_id);
  visitor(value.place_id);
  visitor(value.commander_actor_id);
  visitor(value.troop_type_id);
  visitor(value.fit_ids);
  visitor(value.participants);
  visitor(value.skill_id);
  visitor(value.skill_xp);
  visitor(value.skill_level);
  visitor(value.gain_total);
  visitor(value.training_before);
  visitor(value.fatigue_before);
  visitor(value.fatigue_per_tick);
  visitor(value.payer_wallet_id);
  visitor(value.recipient_wallet_id);
  visitor(value.duration);
  visitor(value.fee);
  visitor(value.drill);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryUnitCommandSource &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryUnitRecord &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.source_garrison_id);
  visitor(value.organization_id);
  visitor(value.garrison_id);
  visitor(value.army_id);
  visitor(value.mobilizing_action_id);
  visitor(value.commander_actor_id);
  visitor(value.troop_type_id);
  visitor(value.member_ids);
  visitor(value.fit);
  visitor(value.wounded);
  visitor(value.dead);
  visitor(value.captured);
  visitor(value.deserted);
  visitor(value.captors);
  visitor(value.retainer_sources);
  visitor(value.morale);
  visitor(value.training);
  visitor(value.fatigue);
  visitor(value.training_action_id);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OfficeAccept &value,
                  Visitor &&visitor) {
  visitor(value.office_id);
  visitor(value.context);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OfficeRecord &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.holder_actor_id);
  visitor(value.enabled);
  visitor(value.appointment_id);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OfficeTerms &value,
                  Visitor &&visitor) {
  visitor(value.office_id);
  visitor(value.actor_id);
  visitor(value.organization_id);
  visitor(value.place_id);
  visitor(value.provider_actor_id);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OrganizationOfficeGrantRoot &value,
                  Visitor &&visitor) {
  visitor(value.office_id);
  visitor(value.grant_id);
  visitor(value.authority_epoch);
  visitor(value.appointment_source_id);
  visitor(value.organization_id);
  visitor(value.scope);
  visitor(value.authority_basis_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateServiceCommandRoot &value,
                  Visitor &&visitor) {
  visitor(value.agreement_id);
  visitor(value.accepted_terms_revision);
  visitor(value.command_assignment_id);
  visitor(value.assignment_revision);
  visitor(value.commander_actor_id);
  visitor(value.principal_id);
  visitor(value.controlled_force_ids);
  visitor(value.valid_from_tick);
  visitor(value.valid_until_tick);
  visitor(value.accepted_source_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TrainingDrillInstance &value,
                  Visitor &&visitor) {
  visitor(value.origin_seed);
  visitor(value.origin_state);
  visitor(value.origin_field_order);
  visitor(value.challenge);
  visitor(value.phase);
  visitor(value.frozen_tick);
  visitor(value.performance_bp);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TrainingDrillTerms &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.challenge_id);
  visitor(value.mode);
  visitor(value.tactics_xp);
  visitor(value.actor_snapshot);
  visitor(value.source_sha256);
  visitor(value.group_source);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TrainingGroupSource &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.unit_id);
  visitor(value.troop_type_id);
  visitor(value.roster_sha256);
  visitor(value.group_ids);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryEffect &value,
                  Visitor &&visitor) {
  visitor(value.request_answer);
  visitor(value.action_id);
  visitor(value.training_receipt);
  visitor(value.completed_receipt_ids);
  visitor(value.receipt_id);
  visitor(value.fact_id);
  visitor(value.source_id);
  visitor(value.return_wait_reason);
  visitor(value.battle_id);
  visitor(value.siege_id);
  visitor(value.plan_submitted);
  visitor(value.actor_changes);
  visitor(value.journey_requests);
  visitor(value.success);
  visitor(value.code);
  visitor(value.office_id);
  visitor(value.army_id);
  visitor(value.order_id);
  visitor(value.campaign_id);
  visitor(value.details_ok);
  visitor(value.already_completed);
  visitor(value.journey_request);
  visitor(value.integration_request);
  visitor(value.casualty_source_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryJourneyRequest &value,
                  Visitor &&visitor) {
  visitor(value.army_id);
  visitor(value.destination_settlement_id);
  visitor(value.field_order);
  visitor(value.return_garrison_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryRequestAnswer &value,
                  Visitor &&visitor) {
  visitor(value.receipt_id);
  visitor(value.battle_id);
  visitor(value.request_key);
  visitor(value.request_kind);
  visitor(value.unit_id);
  visitor(value.target_unit_id);
  visitor(value.commander_actor_id);
  visitor(value.reason);
  visitor(value.response_unit_id);
  visitor(value.round);
  visitor(value.tick);
  visitor(value.accepted);
  visitor(value.response_order);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryTrainingReceipt &value,
                  Visitor &&visitor) {
  visitor(value.unit_id);
  visitor(value.provided_half_days);
  visitor(value.fee_paid);
  visitor(value.training_gain);
  visitor(value.released_unprovided_fee);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TroopDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.name);
  visitor(value.category);
  visitor(value.portrait_kind);
  visitor(value.has_name_key);
  visitor(value.movement);
  visitor(value.minimum_range);
  visitor(value.maximum_range);
  visitor(value.tactic_capacity);
  visitor(value.attack);
  visitor(value.defense);
  visitor(value.accuracy);
  visitor(value.movement_costs);
  visitor(value.tactics);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OfficeDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.organization_id);
  visitor(value.garrison_id);
  visitor(value.appointing_role_id);
  visitor(value.command_scope);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryUnitDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.commander_actor_id);
  visitor(value.troop_type_id);
  visitor(value.initial_soldiers);
  visitor(value.equipment);
  visitor(value.initial_training);
  visitor(value.initial_fatigue);
  visitor(value.display_name);
  visitor(value.population_source_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryTacticLearningCatalog &value,
                  Visitor &&visitor) {
  visitor(value.enabled);
  visitor(value.definitions);
  visitor(value.initial_grants);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryTacticInitialGrant &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.actor_id);
  visitor(value.tactic_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryTactic &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.kind);
  visitor(value.name);
  visitor(value.point_cost);
  visitor(value.morale_effect);
  visitor(value.move_bonus);
  visitor(value.status_rounds);
  visitor(value.power_bp);
  visitor(value.accuracy_bonus);
  visitor(value.learned_tactic_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitarySequentialRoundRule &value,
                  Visitor &&visitor) {
  visitor(value.profile);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryRoutRetreatRule &value,
                  Visitor &&visitor) {
  visitor(value.profile);
  visitor(value.blocked_rounds_before_capture);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryRoutRetreatPolicy &value,
                  Visitor &&visitor) {
  visitor(value.rule);
  visitor(value.battlefield_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryRequestPolicy &value,
                  Visitor &&visitor) {
  visitor(value.per_actor_round_limit);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryRecoveryPolicy &value,
                  Visitor &&visitor) {
  visitor(value.profile);
  visitor(value.garrison_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryPlan &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.office_id);
  visitor(value.order_id);
  visitor(value.unit_ids);
  visitor(value.food_quantity);
  visitor(value.money);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryOrderDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.office_id);
  visitor(value.garrison_id);
  visitor(value.issuer_role_id);
  visitor(value.report_role_id);
  visitor(value.target_stronghold_id);
  visitor(value.objective_kind);
  visitor(value.deadline_ticks);
  visitor(value.recipient);
  visitor(value.recipient_organization_id);
  visitor(value.item_id);
  visitor(value.quantity);
  visitor(value.display_name);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitaryOfficerAssignmentPolicy &value,
      Visitor &&visitor) {
  visitor(value.profile);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitaryLearnedTacticDefinition &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.name_key);
  visitor(value.description_key);
  visitor(value.kind);
  visitor(value.teacher_actor_ids);
  visitor(value.practice_challenge_ids);
  visitor(value.practice_min_score);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryGarrisonRestPolicy &value,
                  Visitor &&visitor) {
  visitor(value.profile);
  visitor(value.garrison_ids);
  visitor(value.fatigue_reduction);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryFieldTerrainRule &value,
                  Visitor &&visitor) {
  visitor(value.profile);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryFieldTerrainPolicy &value,
                  Visitor &&visitor) {
  visitor(value.rule);
  visitor(value.battlefields);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryFieldTerrainBinding &value,
                  Visitor &&visitor) {
  visitor(value.approach_battlefield_id);
  visitor(value.field_battlefield_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryFieldCommandPolicy &value,
                  Visitor &&visitor) {
  visitor(value.profile);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryDamageModel &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.side_bp);
  visitor(value.rear_bp);
  visitor(value.forest_ranged_bp);
  visitor(value.hill_melee_bp);
  visitor(value.min_total_bp);
  visitor(value.max_total_bp);
  visitor(value.present);
  visitor(value.empty);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryCommanderLossRule &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.exposure_bp);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryCommandedDamageRule &value,
                  Visitor &&visitor) {
  visitor(value.profile);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryCommandShockRule &value,
                  Visitor &&visitor) {
  visitor(value.profile);
  visitor(value.morale_loss);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryCommandResourcesRule &value,
                  Visitor &&visitor) {
  visitor(value.profile);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryCombatConditionRule &value,
                  Visitor &&visitor) {
  visitor(value.profile);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryCatalog &value,
                  Visitor &&visitor) {
  visitor(value.siege_engineering_policy);
  visitor(value.recovery_policy);
  visitor(value.garrison_rest_policy);
  visitor(value.supply_attrition_policy);
  visitor(value.rout_retreat_policy);
  visitor(value.command_shock_policy);
  visitor(value.combat_condition_policy);
  visitor(value.commanded_damage_policy);
  visitor(value.command_resources_policy);
  visitor(value.sequential_round_policy);
  visitor(value.field_terrain_policy);
  visitor(value.field_command_policy);
  visitor(value.officer_assignment_policy);
  visitor(value.request_policy);
  visitor(value.military);
  visitor(value.march_condition_model);
  visitor(value.scenario_id);
  visitor(value.offices);
  visitor(value.garrisons);
  visitor(value.orders);
  visitor(value.strongholds);
  visitor(value.plans);
  visitor(value.troop_type_ids);
  visitor(value.battle_catalog);
  visitor(value.siege_topologies);
  visitor(value.drill_challenges);
  visitor(value.item_ids);
  visitor(value.estate_display_names);
  visitor(value.siege_integration);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryBattleCatalog &value,
                  Visitor &&visitor) {
  visitor(value.battlefields);
  visitor(value.troop_types);
  visitor(value.objectives);
  visitor(value.supply_attrition);
  visitor(value.rout_retreat_enabled);
  visitor(value.field_terrain_enabled);
  visitor(value.command_shock_rule);
  visitor(value.combat_condition_rule);
  visitor(value.commanded_damage_rule);
  visitor(value.sequential_round_rule);
  visitor(value.command_resources_rule);
  visitor(value.tactic_learning);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EquipmentDefinition &value,
                  Visitor &&visitor) {
  visitor(value.item_id);
  visitor(value.quantity_per_person);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DrillSupplyGoal &value,
                  Visitor &&visitor) {
  visitor(value.group_id);
  visitor(value.text_key);
  visitor(value.text);
  visitor(value.point);
  visitor(value.guard_cells);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DrillScreenPair &value,
                  Visitor &&visitor) {
  visitor(value.front);
  visitor(value.rear);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DrillScreenGoal &value,
                  Visitor &&visitor) {
  visitor(value.front_group_id);
  visitor(value.ranged_group_id);
  visitor(value.text_key);
  visitor(value.text);
  visitor(value.pairs);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DrillPassageGoal &value,
                  Visitor &&visitor) {
  visitor(value.group_id);
  visitor(value.text_key);
  visitor(value.text);
  visitor(value.cells);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DrillGroup &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.kind);
  visitor(value.text_key);
  visitor(value.text);
  visitor(value.origin);
  visitor(value.targets);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DrillDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.prompt_key);
  visitor(value.prompt);
  visitor(value.source_sha256);
  visitor(value.difficulty);
  visitor(value.groups);
  visitor(value.supply);
  visitor(value.passage);
  visitor(value.screen);
  visitor(value.cavalry);
  visitor(value.solution);
  visitor(value.objectives_field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DrillCavalryGoal &value,
                  Visitor &&visitor) {
  visitor(value.group_id);
  visitor(value.text_key);
  visitor(value.text);
  visitor(value.bad_cells);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BattlefieldDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.name);
  visitor(value.objective_id);
  visitor(value.visual_tag);
  visitor(value.cells);
  visitor(value.spawns);
  visitor(value.exits);
  visitor(value.damage_model);
  visitor(value.commander_losses);
  visitor(value.rout_retreat);
  visitor(value.field_terrain);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BattleObjectiveDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.name);
  visitor(value.kind);
  visitor(value.side);
  visitor(value.max_rounds);
  visitor(value.hold_rounds);
  visitor(value.required_units);
  visitor(value.allow_withdrawal);
  visitor(value.point);
}

// native/world/src/resident/domains/military/duty/military_duty_contract.hpp:14;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void
visit(const ::hanmo::resident::DutyRegisteredMilitaryActivity &value,
      Visitor &&visitor) {
  visitor(value.activity_ref);
  visitor(value.activity_schema_version);
}

// native/world/src/resident/domains/military/duty/military_duty_contract.hpp:30;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::DutyServiceWindowRef &value,
                  Visitor &&visitor) {
  visitor(value.duty_action_id);
  visitor(value.service_epoch);
  visitor(value.ordinal);
  visitor(value.opened_at);
  visitor(value.planned_close_at);
}

// native/world/src/resident/domains/military/duty/military_duty_contract.hpp:13;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::DutyStationAction &value,
                  Visitor &&visitor) {
  visitor(value.action_id);
}

// native/world/src/resident/domains/military/duty/military_duty_contract.hpp:12;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::DutyTravelAction &value,
                  Visitor &&visitor) {
  visitor(value.action_id);
  visitor(value.journey_ref);
}

// native/world/src/resident/domains/military/duty/military_duty_contract.hpp:43;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::DutyWindowClosed &value,
                  Visitor &&visitor) {
  visitor(value.result_ref);
}

// native/world/src/resident/domains/military/duty/military_duty_contract.hpp:42;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::DutyWindowOpen &value,
                  Visitor &&visitor) {
}

// native/world/src/resident/domains/military/naval_repair_exact_model.hpp:14;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::ExactNavalRepairResult2 &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.action_id);
  visitor(value.actor_id);
  visitor(value.request_key);
  visitor(value.reason);
  visitor(value.interruption_source_id);
  visitor(value.success);
  visitor(value.actual_interruption_source);
}

// native/world/src/resident/domains/military/dynamic_action_model.hpp:14;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitaryActionStatusDurations2 &value,
      Visitor &&visitor) {
  visitor(value.inspired);
  visitor(value.suppressed);
  visitor(value.hindered);
  visitor(value.exposed);
}

// native/world/src/resident/domains/military/dynamic_action_model.hpp:18;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitaryActionTimingDefinition2 &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.version);
  visitor(value.action_kind);
  visitor(value.prepare_duration);
  visitor(value.recovery_duration);
  visitor(value.commit_kind);
  visitor(value.flight_duration);
  visitor(value.posture_duration);
  visitor(value.status_durations);
  visitor(value.original_troop_or_weapon_ref);
  visitor(value.original_tactic_ref);
  visitor(value.original_damage_leaf);
  visitor(value.geometry_ref);
  visitor(value.resource_basis_ref);
  visitor(value.objective_definition_ref);
}

// native/world/src/resident/domains/military/military_body_footprint.hpp:12;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitaryBodyFootprintDefinition2 &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.version);
  visitor(value.geometry_ref);
  visitor(value.original_body_definition);
  visitor(value.offsets);
}

// native/world/src/resident/domains/military/duty/military_duty_contract.hpp:71;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryDutyAcceptedSource &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.duty_action_id);
  visitor(value.accepted_at);
  visitor(value.original_assignment_source);
  visitor(value.assignment_source_schema);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitaryDutyConditionConsumption &value,
      Visitor &&visitor) {
  visitor(value.common_prefix);
  visitor(value.condition_prefix);
  visitor(value.consumed_heads);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryDutyWorkPatrolRoute &value,
                  Visitor &&visitor) {
  visitor(value.work_acceptance_ref);
  visitor(value.choice_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryDutyAssignmentSource &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.issued_at);
  visitor(value.issuing_actor_id);
  visitor(value.duty_kind);
  visitor(value.authority_ref);
  visitor(value.authority_sources);
  visitor(value.leader_actor_id);
  visitor(value.unit_ids);
  visitor(value.named_guard_actor_ids);
  visitor(value.fleet_ids);
  visitor(value.authorized_until_tick);
  visitor(value.duty_rule_id);
  visitor(value.duty_rule_version);
  visitor(value.qualified_period_policy_ref);
  visitor(value.route_definition);
  visitor(value.supply_authorization);
  visitor(value.original_work_assignment);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitaryDutyLegacyAuthoritySelection &value,
      Visitor &&visitor) {
  visitor(value.office_id);
}

template <class Visitor>
inline void visit(
    const ::hanmo::resident::MilitaryDutyOrganizationAuthoritySelection &value,
    Visitor &&visitor) {
  visitor(value.office_id);
  visitor(value.grant_id);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitaryDutyPrivateAuthoritySelection &value,
      Visitor &&visitor) {
  visitor(value.agreement_id);
  visitor(value.command_assignment_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::IssueMilitaryDutyAssignment &value,
                  Visitor &&visitor) {
  visitor(value.original_work_assignment);
  visitor(value.authority);
  visitor(value.supply_authorization);
  visitor(value.duty_rule_id);
  visitor(value.authorized_until_tick);
  visitor(value.route_choice_ref);
}

// native/world/src/resident/domains/military/duty/military_duty_contract.hpp:74;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitaryDutyActualBindingSource &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.execution_binding);
  visitor(value.original_execution_source);
  visitor(value.original_started_at);
  visitor(value.bound_at);
}

// native/world/src/resident/domains/military/duty/military_duty_contract.hpp:80;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitaryDutyActualMovementEnded &value,
      Visitor &&visitor) {
  visitor(value.actual_terminal_source);
}

// native/world/src/resident/domains/military/duty/military_duty_contract.hpp:79;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitaryDutyAuthorizationEnded &value,
      Visitor &&visitor) {
}

// native/world/src/resident/domains/military/duty/military_duty_contract.hpp:91;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryDutyControlSource &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.duty_action_id);
  visitor(value.operation);
  visitor(value.current_authority_source);
  visitor(value.occurred_at);
  visitor(value.resulting_phase);
  visitor(value.safe_boundary_stop_source);
  visitor(value.continuation_source);
}

// native/world/src/resident/domains/military/duty/military_duty_contract.hpp:85;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryDutyExecution &value,
                  Visitor &&visitor) {
  visitor(value.duty_action_id);
  visitor(value.accepted_source_ref);
  visitor(value.service_epoch);
  visitor(value.next_period_ordinal);
  visitor(value.current_window_ref);
  visitor(value.last_result_ref);
  visitor(value.actual_binding);
  visitor(value.movement_stop);
}

// native/world/src/resident/domains/military/duty/military_duty_contract.hpp:81;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitaryDutyMovementStopSource &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.stopped_at);
  visitor(value.reason);
}

// native/world/src/resident/domains/military/duty/military_duty_contract.hpp:53;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryDutyPeriodFact2 &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.window);
  visitor(value.actual_until);
  visitor(value.execution_binding);
  visitor(value.order_or_assignment_ref);
  visitor(value.duty_kind);
  visitor(value.leader_actor_id);
  visitor(value.leader_present);
  visitor(value.unit_ids);
  visitor(value.actual_participant_refs);
  visitor(value.actual_participant_count);
  visitor(value.eligible_force_member_count);
  visitor(value.route_or_station_source_refs);
  visitor(value.participant_basis_refs);
  visitor(value.supply_fact_refs);
  visitor(value.encounter_result_refs);
  visitor(value.unresolved_conflict_refs);
  visitor(value.provided_half_days);
  visitor(value.disposition);
  visitor(value.reason_ref);
  visitor(value.duty_rule_version);
  visitor(value.authority_ref);
}

// native/world/src/resident/domains/military/duty/military_duty_contract.hpp:31;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryDutyQualification &value,
                  Visitor &&visitor) {
  visitor(value.actual_participant_refs);
  visitor(value.actual_member_ids);
  visitor(value.eligible_force_member_ids);
  visitor(value.leader_present);
  visitor(value.authority_valid);
  visitor(value.route_or_station_valid);
  visitor(value.supplied);
  visitor(value.participant_basis_refs);
  visitor(value.route_or_station_sources);
  visitor(value.supply_sources);
  visitor(value.authority_sources);
  visitor(value.encounter_result_refs);
  visitor(value.unresolved_conflict_refs);
  visitor(value.unavailable_cause);
  visitor(value.condition_consumption);
}

// native/world/src/resident/domains/military/duty/military_duty_contract.hpp:64;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryDutyRule &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.version);
  visitor(value.kind);
  visitor(value.maximum_authorization_half_days);
  visitor(value.minimum_eligible_force_members);
  visitor(value.qualified_period_policy_ref);
  visitor(value.leader_capability_purpose_ref);
  visitor(value.member_capability_purpose_ref);
  visitor(value.requires_leader);
  visitor(value.requires_supply);
  visitor(value.rejects_conflict);
}

// native/world/src/resident/domains/military/duty/military_duty_contract.hpp:41;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryDutyServiceSegment &value,
                  Visitor &&visitor) {
  visitor(value.span);
  visitor(value.qualification);
}

// native/world/src/resident/domains/military/duty/military_duty_contract.hpp:20;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryDutyTerms &value,
                  Visitor &&visitor) {
  visitor(value.duty_action_id);
  visitor(value.order_or_assignment_ref);
  visitor(value.authority_ref);
  visitor(value.duty_kind);
  visitor(value.leader_actor_id);
  visitor(value.unit_ids);
  visitor(value.named_guard_actor_ids);
  visitor(value.fleet_ids);
  visitor(value.execution_binding);
  visitor(value.start_tick);
  visitor(value.until_tick);
  visitor(value.duty_rule_id);
  visitor(value.duty_rule_version);
  visitor(value.route_or_station_definition_ref);
  visitor(value.boundary_refs);
  visitor(value.custody_source_refs);
  visitor(value.supply_authorization_ref);
  visitor(value.qualified_period_policy_ref);
  visitor(value.report_recipient_ref);
  visitor(value.state);
}

// native/world/src/resident/domains/military/duty/military_duty_contract.hpp:44;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryDutyWindowState &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.window);
  visitor(value.last_accounted_at);
  visitor(value.actual_service_units);
  visitor(value.qualification_source_refs);
  visitor(value.min_eligible_force_members);
  visitor(value.leader_present_throughout);
  visitor(value.conflict_source_refs);
  visitor(value.state);
  visitor(value.opening_qualification);
  visitor(value.actual_segments);
  visitor(value.closing_qualification);
}

// native/world/src/resident/domains/military/military_body_footprint.hpp:7;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryHullFootprintRef &value,
                  Visitor &&visitor) {
  visitor(value.hull_id);
}

// native/world/src/resident/domains/military/military_authority_contract.hpp:27;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryParticipationRef &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.owner_activity_id);
  visitor(value.occupation_source_id);
  visitor(value.occupation_version);
}

// native/world/src/resident/domains/military/duty/military_duty_contract.hpp:97;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryStationActionSource &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.duty_action_id);
  visitor(value.actual_action);
  visitor(value.started_at);
  visitor(value.actual_presence_sources);
}

// native/world/src/resident/domains/military/duty/military_duty_contract.hpp:101;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryStationTerminalSource &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.station_source_ref);
  visitor(value.ended_at);
  visitor(value.original_cause);
  visitor(value.authorization_expired);
}

// native/world/src/resident/domains/military/military_body_footprint.hpp:6;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryTroopFootprintRef &value,
                  Visitor &&visitor) {
  visitor(value.troop_type_id);
}

// native/world/src/resident/domains/military/naval_repair_exact_model.hpp:21;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::NewNavalRepairTimeSource2 &value,
                  Visitor &&visitor) {
  visitor(value.timing);
  visitor(value.request_key);
  visitor(value.original_terms);
  visitor(value.original_result);
  visitor(value.original_charge);
  visitor(value.rule_version);
}

// native/world/src/resident/domains/military/training_exact_model.hpp:17;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::OriginalClosingSupplyGate2 &value,
                  Visitor &&visitor) {
  visitor(value.lesson_ordinal);
  visitor(value.gate_at);
  visitor(value.selected_period_ref);
  visitor(value.period_from_tick);
  visitor(value.period_to_tick);
  visitor(value.opening_receipt_ref);
  visitor(value.supply_fact_ref);
  visitor(value.checked_terms_digest);
}

// native/world/src/resident/domains/military/training_exact_model.hpp:49;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::TrainingDrillFreezeSource2 &value,
                  Visitor &&visitor) {
  visitor(value.action_ref);
  visitor(value.frozen_at);
  visitor(value.first_lesson_ref);
  visitor(value.performance_bp);
}

// native/world/src/resident/domains/military/training_exact_model.hpp:22;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::TrainingParticipationSpan2 &value,
                  Visitor &&visitor) {
  visitor(value.action_ref);
  visitor(value.terms_source_ref);
  visitor(value.from);
  visitor(value.to);
  visitor(value.actual_participant_selection_ref);
  visitor(value.unit_and_occupation_source_refs);
  visitor(value.eligibility_change_refs);
}

// native/world/src/resident/domains/military/training_exact_model.hpp:27;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::TrainingPeriodWitness2 &value,
                  Visitor &&visitor) {
  visitor(value.action_ref);
  visitor(value.period_ref);
  visitor(value.intersection_from);
  visitor(value.intersection_to);
  visitor(value.opening_receipt_ref);
  visitor(value.actual_participation_span_refs);
  visitor(value.observed_supply_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryDutyState &value,
                  Visitor &&visitor) {
  visitor(value.terms);
  visitor(value.executions);
  visitor(value.accepted_sources);
  visitor(value.windows);
  visitor(value.period_facts);
  visitor(value.controls);
  visitor(value.station_sources);
  visitor(value.station_terminals);
  visitor(value.assignments);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryActionTimeState &value,
                  Visitor &&visitor) {
  visitor(value.dynamic_action_facts);
  visitor(value.dynamic_logistics_facts);
  visitor(value.dynamic_return_facts);
  visitor(value.dynamic_training_facts);
  visitor(value.training_participation_spans);
  visitor(value.training_period_witnesses);
  visitor(value.drill_freeze_sources);
  visitor(value.naval_repair_time_sources);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryDutyCatalog &value,
                  Visitor &&visitor) {
  visitor(value.rules);
}

template <class Visitor>
inline void visit_payload(const ::hanmo::resident::MilitaryActorConsequence &v,
                          Visitor &&visitor) {
  visit_military_actor_consequence_payload(v, visitor);
}

template <class Visitor>
inline void
visit_payload(const ::hanmo::resident::MilitaryActorConsequenceBuildView &v,
              Visitor &&visitor) {
  visit_military_actor_consequence_payload(v, visitor);
}

template <class Visitor>
inline void
visit_payload(const ::hanmo::resident::MilitaryFatalConsequence &value,
              Visitor &&visitor) {
  visit_MilitaryFatalConsequence_payload(value, visitor);
}

template <class Visitor>
inline void
visit_payload(const ::hanmo::resident::MilitaryFatalConsequenceBuildView &value,
              Visitor &&visitor) {
  visit_MilitaryFatalConsequence_payload(value, visitor);
}

template <class Visitor>
inline void
visit_payload(const ::hanmo::resident::MilitaryCasualtySourceRead &value,
              Visitor &&visitor) {
  visit_MilitaryCasualtySourceRead_payload(value, visitor);
}

template <class Visitor>
inline void
visit_payload(const ::hanmo::resident::MilitaryCasualtySourceBuildView &value,
              Visitor &&visitor) {
  visit_MilitaryCasualtySourceRead_payload(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BattleWear &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  item_condition_fields::fields(fields, const_cast<BattleWear &>(value));
}

template <class Visitor>
inline void
visit_payload(const ::hanmo::resident::MilitaryCampaignAcceptanceTime1 &value,
              Visitor &&visitor) {
  visit_MilitaryCampaignAcceptanceTime1_payload(value, visitor);
}

template <class Visitor>
inline void visit_payload(
    const ::hanmo::resident::MilitaryCampaignAcceptanceBuildView &value,
    Visitor &&visitor) {
  visit_MilitaryCampaignAcceptanceTime1_payload(value, visitor);
}

template <class Visitor>
inline void
visit_payload(const ::hanmo::resident::MilitaryOfficeResignationTime1 &value,
              Visitor &&visitor) {
  visit_MilitaryOfficeResignationTime1_payload(value, visitor);
}

template <class Visitor>
inline void visit_payload(
    const ::hanmo::resident::MilitaryOfficeResignationBuildView &value,
    Visitor &&visitor) {
  visit_MilitaryOfficeResignationTime1_payload(value, visitor);
}

template <class Visitor>
inline void visit_payload(const MilitaryActionAcceptanceConditions1 &value,
                          Visitor &&visitor) {
  visit_military_acceptance_payload(value, visitor);
}

template <class Visitor>
inline void
visit_payload(const MilitaryActionAcceptanceConditionsBuildView &value,
              Visitor &&visitor) {
  visit_military_acceptance_payload(value, visitor);
}

template <class Visitor>
inline void visit_payload(const MilitaryActionEndpointObservation2 &value,
                          Visitor &&visitor) {
  visit_military_endpoint_payload(value, visitor);
}

template <class Visitor>
inline void
visit_payload(const MilitaryActionEndpointObservationBuildView2 &value,
              Visitor &&visitor) {
  visit_military_endpoint_payload(value, visitor);
}
