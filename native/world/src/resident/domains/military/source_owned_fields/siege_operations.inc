// 职责：集中军旅与战斗中的围城工程、整编和据点访问；仅借用原字段，不改变顺序或失败行为。
// 由中央登记头在 source_owned_fields
// 命名空间内包含；依赖与具名助手已先行可见。

template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitarySiegeEngineeringPreparation &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.siege_id);
  visitor(value.topology_id);
  visitor(value.previous_preparation_id);
  visitor(value.from_tick);
  visitor(value.to_tick);
  visitor(value.round);
  visitor(value.units);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitarySiegeEngineeringState &value,
                  Visitor &&visitor) {
  visitor(value.preparations);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitarySiegeEngineeringUnit &value,
                  Visitor &&visitor) {
  visitor(value.unit_id);
  visitor(value.army_id);
  visitor(value.commander_actor_id);
  visitor(value.authored_actor_id);
  visitor(value.command_source);
  visitor(value.fit_count);
  visitor(value.experience);
  visitor(value.level);
  visitor(value.causal_fact_ids);
  visitor(value.field_order);
  visitor(value.source_field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitarySiegeLifecycleFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.siege_id);
  visitor(value.topology_id);
  visitor(value.actor_id);
  visitor(value.cause_id);
  visitor(value.previous_fact_id);
  visitor(value.kind);
  visitor(value.tick);
  visitor(value.before);
  visitor(value.after);
  visitor(value.strategic_command);
  visitor(value.termination_reason);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitarySiegeLifecycleState &value,
                  Visitor &&visitor) {
  visitor(value.facts);
  visitor(value.field_order);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitarySiegeParticipationSnapshot &value,
      Visitor &&visitor) {
  visitor(value.phase);
  visitor(value.started_tick);
  visitor(value.last_tick);
  visitor(value.round);
  visitor(value.armies);
  visitor(value.unit_nodes);
  visitor(value.withdrawn_army_ids);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitarySiegeReleaseFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.siege_id);
  visitor(value.actor_id);
  visitor(value.army_id);
  visitor(value.authority_kind);
  visitor(value.result_kind);
  visitor(value.tick);
  visitor(value.result_tick);
  visitor(value.winner_side);
  visitor(value.terms);
  visitor(value.reason);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SiegeAssetEffect &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.army_id);
  visitor(value.item_id);
  visitor(value.quantity);
  visitor(value.source_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SiegeBattleRequest &value,
                  Visitor &&visitor) {
  visitor(value.source_id);
  visitor(value.node_id);
  visitor(value.battlefield_id);
  visitor(value.unit_ids);
  visitor(value.origin_nodes);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SiegeEngineUse &value,
                  Visitor &&visitor) {
  visitor(value.inventory_id);
  visitor(value.item_id);
  visitor(value.quantity);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SiegeEngineWork &value,
                  Visitor &&visitor) {
  visitor(value.node_id);
  visitor(value.remaining);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SiegeEvent &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.unit_id);
  visitor(value.node_id);
  visitor(value.stronghold_id);
  visitor(value.campaign_id);
  visitor(value.terms);
  visitor(value.defender_actor_id);
  visitor(value.reason);
  visitor(value.damage);
  visitor(value.remaining_hp);
  visitor(value.army_ids);
  visitor(value.battle_source_ids);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SiegeIntegrationActorAction &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.command);
  visitor(value.actor_id);
  visitor(value.proposal_id);
  visitor(value.proposal_key);
  visitor(value.decision);
  visitor(value.decision_source_id);
  visitor(value.policy_reason);
  visitor(value.tick);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SiegeIntegrationAppointment &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.holder_actor_id);
  visitor(value.appointment_id);
  visitor(value.enabled);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SiegeIntegrationArmy &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.organization_id);
  visitor(value.campaign_id);
  visitor(value.formation_source_id);
  visitor(value.commander_actor_id);
  visitor(value.office_id);
  visitor(value.garrison_id);
  visitor(value.inventory_id);
  visitor(value.wallet_id);
  visitor(value.location);
  visitor(value.unit_ids);
  visitor(value.member_actor_ids);
  visitor(value.prisoner_ids);
  visitor(value.retainers);
  visitor(value.units);
  visitor(value.officers);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SiegeIntegrationAttempt &value,
                  Visitor &&visitor) {
  visitor(value.proposal_id);
  visitor(value.status);
  visitor(value.reason);
  visitor(value.completion_source_id);
  visitor(value.decision_source_ids);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SiegeIntegrationOfficer &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.army_id);
  visitor(value.formation_source_id);
  visitor(value.unit_ids);
  visitor(value.appointment_sources);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SiegeIntegrationProposal &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.proposal_key);
  visitor(value.siege_id);
  visitor(value.campaign_id);
  visitor(value.stronghold_id);
  visitor(value.proposer_actor_id);
  visitor(value.actor_action_source_id);
  visitor(value.created_tick);
  visitor(value.recipient);
  visitor(value.defender_armies);
  visitor(value.required_officers);
  visitor(value.terms);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SiegeIntegrationRetainer &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.estate_id);
  visitor(value.source_id);
  visitor(value.unit_id);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SiegeIntegrationState &value,
                  Visitor &&visitor) {
  visitor(value.proposals);
  visitor(value.actor_actions);
  visitor(value.decisions);
  visitor(value.attempts);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SiegeIntegrationTerms &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.preserve_unit_command);
  visitor(value.preserve_private_property);
  visitor(value.transfer_current_public_assets);
  visitor(value.transfer_prisoner_custody);
  visitor(value.recipient_future_service_responsibility);
  visitor(value.retain_original_arrears);
  visitor(value.population_policy_ids);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SiegeIntegrationUnit &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.source_garrison_id);
  visitor(value.organization_id);
  visitor(value.army_id);
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
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SiegeMaterialFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.tick);
  visitor(value.source_id);
  visitor(value.effect);
  visitor(value.effect_field_order);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SiegeOfficerDecision &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.proposal_id);
  visitor(value.proposal_key);
  visitor(value.actor_id);
  visitor(value.decision);
  visitor(value.actor_action_source_id);
  visitor(value.legal_basis);
  visitor(value.policy_reason);
  visitor(value.tick);
  visitor(value.officer);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SiegeOrder &value,
                  Visitor &&visitor) {
  visitor(value.action);
  visitor(value.node_id);
  visitor(value.engine_use);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SiegeRecord &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.campaign_id);
  visitor(value.stronghold_id);
  visitor(value.topology_id);
  visitor(value.phase);
  visitor(value.started_tick);
  visitor(value.last_tick);
  visitor(value.round);
  visitor(value.armies);
  visitor(value.unit_nodes);
  visitor(value.node_hp);
  visitor(value.inner_hold_ticks);
  visitor(value.unsupplied_ticks);
  visitor(value.blockaded);
  visitor(value.engine_work);
  visitor(value.pending_nodes);
  visitor(value.ai_plans);
  visitor(value.seen_receipt_ids);
  visitor(value.withdrawn_army_ids);
  visitor(value.result);
  visitor(value.last_events);
  visitor(value.plan_submitted);
  visitor(value.pending_orders);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SiegeResult &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.winner_side);
  visitor(value.tick);
  visitor(value.terms);
  visitor(value.reason);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SiegeStrategicCommand &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.army_id);
  visitor(value.terms);
  visitor(value.unexpected_fields);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StrongholdRecord &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.owner_organization_id);
  visitor(value.occupation_id);
  visitor(value.siege_id);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SiegeIntegrationEffect &value,
                  Visitor &&visitor) {
  visitor(value.proposal_id);
  visitor(value.proposal_key);
  visitor(value.actor_action_source_id);
  visitor(value.decision_source_id);
  visitor(value.status);
  visitor(value.completion_source_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SiegeIntegrationPropose &value,
                  Visitor &&visitor) {
  visitor(value.siege_id);
  visitor(value.terms);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StrongholdDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.garrison_id);
  visitor(value.settlement_id);
  visitor(value.topology_id);
  visitor(value.public_inventory_ids);
  visitor(value.public_wallet_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SiegeTopology &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.name);
  visitor(value.attacker_entry_id);
  visitor(value.defender_entry_id);
  visitor(value.inner_id);
  visitor(value.nodes);
  visitor(value.edges);
  visitor(value.supply_node_ids);
  visitor(value.rules);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SiegeRuleDefinition &value,
                  Visitor &&visitor) {
  visitor(value.hold_ticks);
  visitor(value.blockade_units);
  visitor(value.starvation_ticks);
  visitor(value.max_ticks);
  visitor(value.gate_damage);
  visitor(value.repair_amount);
  visitor(value.engine_bonus);
  visitor(value.engine_ticks);
  visitor(value.surrender_morale);
  visitor(value.wall_defense_bp);
  visitor(value.engine_item_id);
  visitor(value.engine_materials);
  visitor(value.repair_materials);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SiegeNode &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.kind);
  visitor(value.name);
  visitor(value.battlefield_id);
  visitor(value.max_hp);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SiegeMaterial &value,
                  Visitor &&visitor) {
  visitor(value.item_id);
  visitor(value.quantity);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SiegeIntegrationOfficerPolicy &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.allowed_recipient_organization_ids);
  visitor(value.allowed_terms);
  visitor(value.minimum_willingness);
  visitor(value.require_unit_command);
  visitor(value.require_private_property);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SiegeIntegrationCatalog &value,
                  Visitor &&visitor) {
  visitor(value.enabled);
  visitor(value.officer_policies);
  visitor(value.default_terms);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SiegeEdge &value,
                  Visitor &&visitor) {
  visitor(value.from);
  visitor(value.to);
  visitor(value.blocker_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitarySiegeEngineeringRule &value,
                  Visitor &&visitor) {
  visitor(value.profile);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitarySiegeEngineeringPolicy &value,
      Visitor &&visitor) {
  visitor(value.rule);
  visitor(value.topology_ids);
}

// 职责：遍历原节点六槽拥有字段，不添加来源认证或 revision 政策。
template <class Visitor>
inline void visit(const ::hanmo::resident::FortificationNodeState &value,
                  Visitor &&visitor) {
  visitor(value.stronghold_id);
  visitor(value.topology_id);
  visitor(value.node_id);
  visitor(value.integrity);
  visitor(value.revision);
  visitor(value.last_change_source_id);
}
// 职责：集合唯一拥有原行；复制/保留复用同一字段图，不编码第二份键。
template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryFortificationNodes &value,
                  Visitor &&visitor) {
  visitor(value.rows);
}
