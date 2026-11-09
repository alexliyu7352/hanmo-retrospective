// 职责：集中任务与履约中的任务受领、契约与消息访问；仅借用原字段，不改变顺序或失败行为。
// 由中央登记头在 source_owned_fields
// 命名空间内包含；依赖与具名助手已先行可见。

template <class Visitor>
inline void visit(const ::hanmo::resident::DeliveryFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.task_id);
  visitor(value.activity_id);
  visitor(value.source_action_id);
  visitor(value.from_inventory_id);
  visitor(value.to_inventory_id);
  visitor(value.beneficiary);
  visitor(value.item_id);
  visitor(value.quantity);
  visitor(value.completed_tick);
  visitor(value.transfer_fact_id);
  visitor(value.completed_at);
  visitor(value.original_completion_result_ref);
  visitor(value.quantity_source_kind);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EscortArrivalFact &value,
                  Visitor &&visitor) {
  visitor(value.task_id);
  visitor(value.actor_id);
  visitor(value.escort_actor_id);
  visitor(value.place_id);
  visitor(value.tick);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EscortProgress &value,
                  Visitor &&visitor) {
  visitor(value.departed);
  visitor(value.arrival_tick);
  visitor(value.battle_ids);
  visitor(value.failure);
  visitor(value.failed_tick);
  visitor(value.field_order);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::FacilityApplicationUnavailable &value,
      Visitor &&visitor) {
  visitor(value.asset_ref);
  visitor(value.application_terminal_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FrozenTaskTerms &value,
                  Visitor &&visitor) {
  visitor(value.definition);
  visitor(value.agreed_quantity);
  visitor(value.purchase_budget_cap);
  visitor(value.reward_total);
  visitor(value.deadline_tick);
  visitor(value.target_inventory_id);
  visitor(value.item_id);
  visitor(value.quantity_unit);
  visitor(value.source_wallet_id);
  visitor(value.reward_wallet_id);
  visitor(value.payee_wallet_id);
  visitor(value.beneficiary);
  visitor(value.terms_version);
  visitor(value.payment_policy);
  visitor(value.unused_budget_policy);
  visitor(value.late_reward_policy);
  visitor(value.cancellation_policy_id);
  visitor(value.personal_advance_allowed);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceTaskAcceptance &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.task_id);
  visitor(value.contract_id);
  visitor(value.authority_source_fact_id);
  visitor(value.organization_id);
  visitor(value.executor_actor_id);
  visitor(value.jurisdiction_id);
  visitor(value.site_id);
  visitor(value.rule_id);
  visitor(value.rule_version);
  visitor(value.relief_terms_id);
  visitor(value.reservation_id);
  visitor(value.display_name);
  visitor(value.kind);
  visitor(value.accepted_revision);
  visitor(value.accepted_tick);
  visitor(value.work_ticks);
  visitor(value.deadline_tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceTaskCompletion &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.task_id);
  visitor(value.contract_id);
  visitor(value.action_id);
  visitor(value.acceptance_fact_id);
  visitor(value.work_completion_fact_id);
  visitor(value.outcome_plan_id);
  visitor(value.resource_receipt_ids);
  visitor(value.result);
  visitor(value.completed_tick);
  visitor(value.reason);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::ImprovementApplicationUnavailable &value,
      Visitor &&visitor) {
  visitor(value.work_ref);
  visitor(value.application_terminal_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LegacyClaimAcceptedTick &value,
                  Visitor &&visitor) {
  visitor(value.accepted_tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LegacyGoalCriterion &value,
                  Visitor &&visitor) {
  visitor(value.goal_kind);
  visitor(value.rule_version);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OfferInstance &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.template_id);
  visitor(value.source_need_id);
  visitor(value.need_revision_at_creation);
  visitor(value.issuer);
  visitor(value.accept_scope);
  visitor(value.report_scope);
  visitor(value.funding_wallet_id);
  visitor(value.reward_wallet_id);
  visitor(value.terms_version);
  visitor(value.available_from_tick);
  visitor(value.expires_tick);
  visitor(value.status);
  visitor(value.definition);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OfferRef &value, Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OutcomeApplication &value,
                  Visitor &&visitor) {
  visitor(value.task_id);
  visitor(value.objective_id);
  visitor(value.program_ref);
  visitor(value.target_line_id);
  visitor(value.work_result_ref);
  visitor(value.application_kind);
  visitor(value.original_application_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OutcomeFactRef &value,
                  Visitor &&visitor) {
  visitor(value.source_domain);
  visitor(value.source_kind);
  visitor(value.fact_id);
  visitor(value.version);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PatrolApplicationUnavailable &value,
                  Visitor &&visitor) {
  visitor(value.program_ref);
  visitor(value.target_line_id);
  visitor(value.application_terminal_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PatrolCircuit &value,
                  Visitor &&visitor) {
  visitor(value.checkpoint_refs);
  visitor(value.allowed_passage_refs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PatrolPlan &value,
                  Visitor &&visitor) {
  visitor(value.jurisdiction_id);
  visitor(value.authority_scope);
  visitor(value.leader_actor_id);
  visitor(value.unit_ids);
  visitor(value.route);
  visitor(value.required_half_days);
  visitor(value.minimum_available_people);
  visitor(value.completion_deadline);
  visitor(value.supply_order_ref);
  visitor(value.expense_terms_ref);
  visitor(value.continuity_policy_ref);
  visitor(value.rule_id);
  visitor(value.rule_version);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PredicateContributionUse &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.contract_id);
  visitor(value.claim_id);
  visitor(value.need_id);
  visitor(value.episode);
  visitor(value.criterion);
  visitor(value.satisfaction_ref);
  visitor(value.actual_contribution_ref);
  visitor(value.applied_result_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ReportBack &value,
                  Visitor &&visitor) {
  visitor(value.task_id);
  visitor(value.report_requirement_id);
  visitor(value.work_result_ref);
  visitor(value.report_content_ref);
  visitor(value.original_reporting_obligation_ref);
  visitor(value.addressee);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ReportBackUnavailable &value,
                  Visitor &&visitor) {
  visitor(value.reporting_obligation_ref);
  visitor(value.report_terminal_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RuntimeOfferResponseSource &value,
                  Visitor &&visitor) {
  visitor(value.report_id);
  visitor(value.fact_id);
  visitor(value.source_key);
  visitor(value.observation_tick);
  visitor(value.learned_tick);
  visitor(value.local_public);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RuntimeTaskOfferResponse &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.offer_id);
  visitor(value.need_id);
  visitor(value.affair_id);
  visitor(value.terms_version);
  visitor(value.session_id);
  visitor(value.need_revision);
  visitor(value.response_tick);
  visitor(value.context);
  visitor(value.source);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RuntimeTaskOfferResponses &value,
                  Visitor &&visitor) {
  visitor(value.responses);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::Shipment &value, Visitor &&visitor) {
  visitor(value.id);
  visitor(value.activity_id);
  visitor(value.source_inventory_id);
  visitor(value.cargo_inventory_id);
  visitor(value.destination_inventory_id);
  visitor(value.cargo_owner);
  visitor(value.carrier_ref);
  visitor(value.route_or_action_id);
  visitor(value.status);
  visitor(value.receipt_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SourceBinding &value,
                  Visitor &&visitor) {
  visitor(value.affair_id);
  visitor(value.need_id);
  visitor(value.claim_id);
  visitor(value.contract_id);
  visitor(value.source_revision);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StateNeedOffer &value,
                  Visitor &&visitor) {
  visitor(value.offer_ref);
  visitor(value.need_ref);
  visitor(value.goal_summary);
  visitor(value.terms_ref);
  visitor(value.claim_window);
  visitor(value.completion_gate);
  visitor(value.fee_reward_terms);
  visitor(value.cancel_policy);
  visitor(value.original_disclosure_ref);
  visitor(value.report_recipient);
  visitor(value.report_required);
  visitor(value.objective);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StateNeedTaskState &value,
                  Visitor &&visitor) {
  visitor(value.offers);
  visitor(value.contracts);
  visitor(value.predicate_contributions);
  visitor(value.settlements);
  visitor(value.closures);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StateTaskClosure &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.contract_id);
  visitor(value.cause);
  visitor(value.actual_service_stop_ref);
  visitor(value.closed_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StateTaskMoneyTerms &value,
                  Visitor &&visitor) {
  visitor(value.original_terms_ref);
  visitor(value.terms_revision);
  visitor(value.funding_wallet_id);
  visitor(value.reward_wallet_id);
  visitor(value.payee_wallet_id);
  visitor(value.issuer);
  visitor(value.maximum_service_fee);
  visitor(value.success_reward);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StateTaskSettlement &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.contract_id);
  visitor(value.cause);
  visitor(value.service_charge_sources);
  visitor(value.paid_service_fee);
  visitor(value.paid_success_reward);
  visitor(value.refunded_service_fee);
  visitor(value.refunded_reward);
  visitor(value.settled_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StationWatch &value,
                  Visitor &&visitor) {
  visitor(value.station_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SupervisionActionStartSource &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.action_id);
  visitor(value.work_id);
  visitor(value.actor_id);
  visitor(value.work_acceptance_ref);
  visitor(value.started_at);
  visitor(value.until_tick);
  visitor(value.accepted_presence_source);
  visitor(value.condition_at_start);
  visitor(value.original_start_sources);
  visitor(value.skill_qualification);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::SupervisionActionTerminalSource &value,
      Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.start_source_ref);
  visitor(value.ended_at);
  visitor(value.disposition);
  visitor(value.actual_release_cause);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::SupervisionWindowClosureSource &value,
      Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.start_source_ref);
  visitor(value.closed_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskAction &value,
                  Visitor &&visitor) {
  visitor(value.offer_id);
  visitor(value.task_id);
  visitor(value.context);
  visitor(value.reward);
  visitor(value.field_order);
  visitor(value.source_kind);
  visitor(value.delivery_quantity);
  visitor(value.restitution);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskCargoDispositionFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.battle_id);
  visitor(value.encounter_id);
  visitor(value.source_result_id);
  visitor(value.contract_id);
  visitor(value.task_id);
  visitor(value.carrier_actor_id);
  visitor(value.recipient_actor_id);
  visitor(value.from_inventory_id);
  visitor(value.to_inventory_id);
  visitor(value.item_id);
  visitor(value.from_owner);
  visitor(value.to_owner);
  visitor(value.tick);
  visitor(value.source_quantity_before);
  visitor(value.source_quantity_after);
  visitor(value.transferred_quantity);
  visitor(value.destroyed_quantity);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskCargoExposure &value,
                  Visitor &&visitor) {
  visitor(value.battle_id);
  visitor(value.encounter_id);
  visitor(value.contract_id);
  visitor(value.task_id);
  visitor(value.carrier_actor_id);
  visitor(value.recipient_actor_id);
  visitor(value.recipient_prototype_id);
  visitor(value.source_inventory_id);
  visitor(value.destination_inventory_id);
  visitor(value.policy);
  visitor(value.carrier_anchor);
  visitor(value.recipient_anchor);
  visitor(value.created_tick);
  visitor(value.source_inventory_revision);
  visitor(value.destination_inventory_revision);
  visitor(value.destination_capacity);
  visitor(value.destination_weight);
  visitor(value.frozen_quantity);
  visitor(value.transfer_quantity);
  visitor(value.destroy_quantity);
  visitor(value.lots);
  visitor(value.resolution);
  visitor(value.source_result_id);
  visitor(value.disposition_fact_id);
  visitor(value.resolution_reason);
  visitor(value.resolved_tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskCargoLot &value,
                  Visitor &&visitor) {
  visitor(value.lot_id);
  visitor(value.quantity);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskCargoPolicy &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.version);
  visitor(value.task_template_id);
  visitor(value.item_id);
  visitor(value.outcome);
  visitor(value.recipient);
  visitor(value.allocation);
  visitor(value.unavailable_recipient);
  visitor(value.transfer_cap);
  visitor(value.destroy_cap);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskCarrierAcceptanceContact &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.provider_actor_id);
  visitor(value.session_id);
  visitor(value.place_id);
  visitor(value.role_id);
  visitor(value.dialogue_id);
  visitor(value.node_id);
  visitor(value.tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskCarrierAgreement &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.accepted_scope_revision);
  visitor(value.terms);
  visitor(value.contact);
  visitor(value.accepted_at);
  visitor(value.stopped_source);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskCarrierReportTerms &value,
                  Visitor &&visitor) {
  visitor(value.report_id);
  visitor(value.sender_actor_id);
  visitor(value.carrier_actor_id);
  visitor(value.recipient_actor_id);
  visitor(value.actual_carrier_disclosure);
  visitor(value.earliest_delivery_tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskCarrierSelectionTerms &value,
                  Visitor &&visitor) {
  visitor(value.selection_id);
  visitor(value.releasable_allocation_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskCarrierTerms &value,
                  Visitor &&visitor) {
  visitor(value.original_terms_source);
  visitor(value.original_business_source);
  visitor(value.terms_revision);
  visitor(value.principal);
  visitor(value.carrier_subject);
  visitor(value.initiator);
  visitor(value.provider_actor_id);
  visitor(value.role_id);
  visitor(value.dialogue_id);
  visitor(value.accept_node_id);
  visitor(value.population_selections);
  visitor(value.named_participants);
  visitor(value.vehicle_refs);
  visitor(value.inventory_ids);
  visitor(value.custody_ref);
  visitor(value.destination_site_id);
  visitor(value.expense_authority);
  visitor(value.cargo_authority_refs);
  visitor(value.vehicle_authority_refs);
  visitor(value.participant_consent_refs);
  visitor(value.payment_source_refs);
  visitor(value.passage_source_refs);
  visitor(value.labor_until_tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskClaimEstablishedRef &value,
                  Visitor &&visitor) {
  visitor(value.source);
  visitor(value.source_schema);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskClaimEstablishedSource &value,
                  Visitor &&visitor) {
  visitor(value.source_id);
  visitor(value.task_id);
  visitor(value.contract_id);
  visitor(value.claim_id);
  visitor(value.acceptance_action_id);
  visitor(value.established_at);
  visitor(value.committed_result_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskClaimEstablishmentState &value,
                  Visitor &&visitor) {
  visitor(value.claim_establishment_sources);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.template_id);
  visitor(value.accept_role);
  visitor(value.report_role);
  visitor(value.funding_wallet);
  visitor(value.on_unavailable);
  visitor(value.display_name);
  visitor(value.issuer);
  visitor(value.budget);
  visitor(value.reward);
  visitor(value.accept_ticks);
  visitor(value.deadline_ticks);
  visitor(value.goal);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskEffect &value,
                  Visitor &&visitor) {
  visitor(value.success);
  visitor(value.task_id);
  visitor(value.code);
  visitor(value.on_unavailable);
  visitor(value.retry);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskExtensionFact &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.policy_id);
  visitor(value.task_id);
  visitor(value.offer_id);
  visitor(value.actor_id);
  visitor(value.issuer_actor_id);
  visitor(value.session_id);
  visitor(value.place_id);
  visitor(value.role_id);
  visitor(value.dialogue_id);
  visitor(value.node_id);
  visitor(value.accepted_tick);
  visitor(value.granted_tick);
  visitor(value.original_deadline_tick);
  visitor(value.deadline_tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskExtensionState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.facts);
  visitor(value.by_task);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskGoal &value, Visitor &&visitor) {
  visitor(value.item_id);
  visitor(value.message_id);
  visitor(value.receiver_role);
  visitor(value.receiver_actor);
  visitor(value.actor_id);
  visitor(value.destination_place_id);
  visitor(value.objective_id);
  visitor(value.quantity);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskMessageHandoff &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.task_id);
  visitor(value.offer_id);
  visitor(value.source_action_id);
  visitor(value.original_issuer);
  visitor(value.carrier_actor_id);
  visitor(value.recipient_actor_id);
  visitor(value.item_id);
  visitor(value.source_lot_id);
  visitor(value.recipient_lot_id);
  visitor(value.from_inventory_id);
  visitor(value.to_inventory_id);
  visitor(value.place_id);
  visitor(value.role_id);
  visitor(value.tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskMessageHandoffState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.receipts);
  visitor(value.observed_by);
  visitor(value.reports);
  visitor(value.reports_observed_by);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskMessageNoticeBinding &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.task_id);
  visitor(value.offer_id);
  visitor(value.accept_action_id);
  visitor(value.carrier_actor_id);
  visitor(value.source_lot_id);
  visitor(value.carrier_inventory_id);
  visitor(value.tick);
  visitor(value.source);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskMessageNoticeState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.bindings);
  visitor(value.task_to_binding);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskMessageReport &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.task_id);
  visitor(value.handoff_fact_id);
  visitor(value.source_action_id);
  visitor(value.settlement_id);
  visitor(value.carrier_actor_id);
  visitor(value.report_actor_id);
  visitor(value.place_id);
  visitor(value.role_id);
  visitor(value.tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskMessageReturnCollection &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.start_fact_id);
  visitor(value.task_id);
  visitor(value.action_id);
  visitor(value.reply_lot_id);
  visitor(value.reason);
  visitor(value.tick);
  visitor(value.success);
  visitor(value.actual_result);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskMessageReturnDelivery &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.task_id);
  visitor(value.collection_fact_id);
  visitor(value.report_start_fact_id);
  visitor(value.source_action_id);
  visitor(value.report_fact_id);
  visitor(value.settlement_id);
  visitor(value.original_lot_id);
  visitor(value.recipient_lot_id);
  visitor(value.from_inventory_id);
  visitor(value.to_inventory_id);
  visitor(value.tick);
  visitor(value.actual_result);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::TaskMessageReturnReportInterruption &value,
      Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.report_start_fact_id);
  visitor(value.task_id);
  visitor(value.action_id);
  visitor(value.reason);
  visitor(value.tick);
  visitor(value.effect_task_id_present);
  visitor(value.on_unavailable);
  visitor(value.retry);
  visitor(value.actual_result);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskMessageReturnReportStart &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.task_id);
  visitor(value.collection_fact_id);
  visitor(value.action_id);
  visitor(value.original_lot_id);
  visitor(value.from_inventory_id);
  visitor(value.to_inventory_id);
  visitor(value.carrier_actor_id);
  visitor(value.sender_actor_id);
  visitor(value.session_id);
  visitor(value.dialogue_id);
  visitor(value.node_id);
  visitor(value.context);
  visitor(value.started_tick);
  visitor(value.actual_result);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskMessageReturnStart &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.task_id);
  visitor(value.offer_id);
  visitor(value.policy_id);
  visitor(value.handoff_fact_id);
  visitor(value.action_id);
  visitor(value.carrier_actor_id);
  visitor(value.recipient_actor_id);
  visitor(value.sender_actor_id);
  visitor(value.reply_item_id);
  visitor(value.carrier_inventory_id);
  visitor(value.session_id);
  visitor(value.dialogue_id);
  visitor(value.node_id);
  visitor(value.context);
  visitor(value.started_tick);
  visitor(value.actual_result);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskMessageReturnState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.starts);
  visitor(value.collections);
  visitor(value.deliveries);
  visitor(value.report_starts);
  visitor(value.report_interruptions);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskOfferResponse &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.offer_id);
  visitor(value.response_tick);
  visitor(value.discovered_tick);
  visitor(value.context);
  visitor(value.source);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskOfferResponseState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.responses);
  visitor(value.runtime);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskPersonalAcceptanceEnd &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.start_fact_id);
  visitor(value.task_id);
  visitor(value.action_id);
  visitor(value.original_lot_id);
  visitor(value.reason);
  visitor(value.tick);
  visitor(value.success);
  visitor(value.effect_task_id_present);
  visitor(value.on_unavailable);
  visitor(value.retry);
  visitor(value.actual_result);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::TaskPersonalAcceptanceKnowledge &value,
      Visitor &&visitor) {
  visitor(value.discovered_tick);
  visitor(value.source_kind);
  visitor(value.source_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskPersonalAcceptanceStart &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.task_id);
  visitor(value.offer_id);
  visitor(value.policy_id);
  visitor(value.action_id);
  visitor(value.carrier_actor_id);
  visitor(value.sender_actor_id);
  visitor(value.funding_wallet_id);
  visitor(value.budget_wallet_id);
  visitor(value.carrier_inventory_id);
  visitor(value.message_item_id);
  visitor(value.sender_declaration);
  visitor(value.known_offer);
  visitor(value.started_tick);
  visitor(value.budget);
  visitor(value.actual_result);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskPersonalAcceptanceState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.starts);
  visitor(value.endings);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskPersonalAdvancePayment &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.contract_id);
  visitor(value.task_id);
  visitor(value.lender_actor_id);
  visitor(value.from_wallet_id);
  visitor(value.to_wallet_id);
  visitor(value.amount);
  visitor(value.tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskPersonalAdvancePolicy &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.template_id);
  visitor(value.task_template_id);
  visitor(value.version);
  visitor(value.maximum_cash);
  visitor(value.repayment_policy);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskPersonalAdvanceRepayment &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.contract_id);
  visitor(value.task_id);
  visitor(value.lender_actor_id);
  visitor(value.to_wallet_id);
  visitor(value.escrow_wallet_id);
  visitor(value.issuer_wallet_id);
  visitor(value.payment_fact_ids);
  visitor(value.amount);
  visitor(value.from_escrow);
  visitor(value.from_issuer);
  visitor(value.tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskPersonalAdvanceState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.terms);
  visitor(value.payments);
  visitor(value.repayments);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskPersonalAdvanceTerms &value,
                  Visitor &&visitor) {
  visitor(value.contract_id);
  visitor(value.task_id);
  visitor(value.claim_id);
  visitor(value.terms_version);
  visitor(value.policy);
  visitor(value.lender_actor_id);
  visitor(value.personal_wallet_id);
  visitor(value.escrow_wallet_id);
  visitor(value.repayment_wallet_id);
  visitor(value.original_issuer);
  visitor(value.frozen_tick);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::TaskPersonalCancellationRecord &value,
      Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.task_id);
  visitor(value.offer_id);
  visitor(value.policy_id);
  visitor(value.carrier_actor_id);
  visitor(value.sender_actor_id);
  visitor(value.settlement_id);
  visitor(value.asset_receipt_fact_id);
  visitor(value.previous_task_status);
  visitor(value.sender_declaration);
  visitor(value.tick);
  visitor(value.refunded_budget);
  visitor(value.actual_result);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskPersonalCancellationState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.cancellations);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskPersonalMessageEnd &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.start_fact_id);
  visitor(value.task_id);
  visitor(value.action_id);
  visitor(value.handoff_fact_id);
  visitor(value.reason);
  visitor(value.tick);
  visitor(value.success);
  visitor(value.effect_task_id_present);
  visitor(value.on_unavailable);
  visitor(value.retry);
  visitor(value.actual_result);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskPersonalMessageStart &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.task_id);
  visitor(value.offer_id);
  visitor(value.policy_id);
  visitor(value.action_id);
  visitor(value.carrier_actor_id);
  visitor(value.recipient_actor_id);
  visitor(value.item_id);
  visitor(value.original_lot_id);
  visitor(value.from_inventory_id);
  visitor(value.to_inventory_id);
  visitor(value.recipient_declaration);
  visitor(value.started_tick);
  visitor(value.actual_result);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskPersonalMessageState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.starts);
  visitor(value.endings);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskPersonalReportEnd &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.start_fact_id);
  visitor(value.task_id);
  visitor(value.action_id);
  visitor(value.report_fact_id);
  visitor(value.settlement_id);
  visitor(value.reason);
  visitor(value.tick);
  visitor(value.success);
  visitor(value.effect_task_id_present);
  visitor(value.on_unavailable);
  visitor(value.retry);
  visitor(value.actual_result);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskPersonalReportStart &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.task_id);
  visitor(value.offer_id);
  visitor(value.policy_id);
  visitor(value.action_id);
  visitor(value.handoff_fact_id);
  visitor(value.carrier_actor_id);
  visitor(value.sender_actor_id);
  visitor(value.sender_declaration);
  visitor(value.started_tick);
  visitor(value.reward);
  visitor(value.actual_result);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskPersonalReportState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.starts);
  visitor(value.endings);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskRecord &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.offer_id);
  visitor(value.assignee);
  visitor(value.status);
  visitor(value.budget_wallet_id);
  visitor(value.settlement_id);
  visitor(value.serial);
  visitor(value.accepted_tick);
  visitor(value.deadline_tick);
  visitor(value.objective_completed_tick);
  visitor(value.reported_tick);
  visitor(value.late);
  visitor(value.escort);
  visitor(value.field_order);
  visitor(value.offer_ref);
  visitor(value.contract_id);
  visitor(value.accepted_at);
  visitor(value.record_kind);
  visitor(value.exact_quantity);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskReportCarriage &value,
                  Visitor &&visitor) {
  visitor(value.agreement_id);
  visitor(value.terms);
  visitor(value.dispatch);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskReportCarriageDispatch &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.action_id);
  visitor(value.original_execution_source);
  visitor(value.sender_knowledge_source);
  visitor(value.permission);
  visitor(value.sent_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskReportHandoverSource &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.carriage_id);
  visitor(value.actual_arrival);
  visitor(value.contact_source);
  visitor(value.dialogue_node_id);
  visitor(value.dialogue_phase);
  visitor(value.handed_over_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskServiceTerminalState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.work_commission_terminals);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskShopOrderAction &value,
                  Visitor &&visitor) {
  visitor(value.action_id);
  visitor(value.task_id);
  visitor(value.context);
  visitor(value.session_id);
  visitor(value.dialogue_id);
  visitor(value.node_id);
  visitor(value.started_tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskShopOrderBinding &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.task_id);
  visitor(value.order_id);
  visitor(value.order_issue_fact_id);
  visitor(value.reservation_id);
  visitor(value.terms);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskShopOrderContactStart &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.task_id);
  visitor(value.order_id);
  visitor(value.acceptance_fact_id);
  visitor(value.order_issue_fact_id);
  visitor(value.proprietor_actor_id);
  visitor(value.client_actor_id);
  visitor(value.action_id);
  visitor(value.session_id);
  visitor(value.dialogue_id);
  visitor(value.node_id);
  visitor(value.context);
  visitor(value.started_tick);
  visitor(value.actual_result);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskShopOrderContactState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.starts);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::TaskShopOrderEstateCancellation &value,
      Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.task_id);
  visitor(value.settlement_fact_id);
  visitor(value.settlement_id);
  visitor(value.request);
  visitor(value.tick);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::TaskShopOrderEstateCancellationState &value,
      Visitor &&visitor) {
  visitor(value.version);
  visitor(value.cancellations);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::TaskShopOrderEstatePlayerReceipt &value,
      Visitor &&visitor) {
  visitor(value.result_id);
  visitor(value.request_sequence);
  visitor(value.result_revision);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskShopOrderInterruption &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.source);
  visitor(value.tick);
  visitor(value.code);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskShopOrderLotLeg &value,
                  Visitor &&visitor) {
  visitor(value.item_id);
  visitor(value.source_lot_id);
  visitor(value.destination_lot_id);
  visitor(value.source_owner);
  visitor(value.destination_owner);
  visitor(value.quantity);
  visitor(value.source_before);
  visitor(value.source_after);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskShopOrderSettlement &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.task_id);
  visitor(value.order_id);
  visitor(value.source_action_id);
  visitor(value.settlement_id);
  visitor(value.outcome);
  visitor(value.termination);
  visitor(value.from_inventory_id);
  visitor(value.to_inventory_id);
  visitor(value.from_wallet_id);
  visitor(value.to_wallet_id);
  visitor(value.from_container_owner);
  visitor(value.to_container_owner);
  visitor(value.tick);
  visitor(value.action_started_tick);
  visitor(value.delivered_quantity);
  visitor(value.paid_price);
  visitor(value.refunded_price);
  visitor(value.lots);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskShopOrderState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.estate_cancellations);
  visitor(value.contacts);
  visitor(value.bindings);
  visitor(value.settlements);
  visitor(value.actions);
  visitor(value.interruptions);
  visitor(value.by_proprietor);
  visitor(value.by_order);
  visitor(value.by_task);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskState &value,
                  Visitor &&visitor) {
  visitor(value.service_terminals);
  visitor(value.work_execution);
  visitor(value.objective_base);
  visitor(value.exact_execution);
  visitor(value.transport_agreements);
  visitor(value.state_need_tasks);
  visitor(value.exact_quantities);
  visitor(value.dynamic_tasks_version);
  visitor(value.claim_establishment);
  visitor(value.tasks);
  visitor(value.actions);
  visitor(value.facts);
  visitor(value.offer_instances);
  visitor(value.contracts);
  visitor(value.deliveries);
  visitor(value.transfers);
  visitor(value.shipments);
  visitor(value.cargo_exposures);
  visitor(value.cargo_dispositions);
  visitor(value.offer_responses);
  visitor(value.personal_advances);
  visitor(value.takeover_execution);
  visitor(value.message_handoffs);
  visitor(value.message_notices);
  visitor(value.static_assets);
  visitor(value.deadline_extensions);
  visitor(value.message_returns);
  visitor(value.personal_messages);
  visitor(value.personal_reports);
  visitor(value.personal_cancellations);
  visitor(value.personal_acceptances);
  visitor(value.shop_orders);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskStaticAssetLeg &value,
                  Visitor &&visitor) {
  visitor(value.item_id);
  visitor(value.source_lot_id);
  visitor(value.destination_lot_id);
  visitor(value.source_owner);
  visitor(value.destination_owner);
  visitor(value.reservation);
  visitor(value.quantity);
  visitor(value.source_quantity_before);
  visitor(value.source_quantity_after);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskStaticAssetReceipt &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.task_id);
  visitor(value.offer_id);
  visitor(value.assignee_actor_id);
  visitor(value.stage);
  visitor(value.source_action_id);
  visitor(value.original_issuer);
  visitor(value.from_container_owner);
  visitor(value.to_container_owner);
  visitor(value.from_inventory_id);
  visitor(value.to_inventory_id);
  visitor(value.tick);
  visitor(value.legs);
  visitor(value.retained_task_goods);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskStaticAssetRemainder &value,
                  Visitor &&visitor) {
  visitor(value.inventory_id);
  visitor(value.lot_id);
  visitor(value.item_id);
  visitor(value.reservation);
  visitor(value.quantity);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskStaticAssetState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.receipts);
  visitor(value.observed_by);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskStaticPaymentLeg &value,
                  Visitor &&visitor) {
  visitor(value.purpose);
  visitor(value.from_wallet_id);
  visitor(value.to_wallet_id);
  visitor(value.amount);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskStaticPaymentSource &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.task_id);
  visitor(value.source_action_id);
  visitor(value.registration_fact_id);
  visitor(value.decision_fact_id);
  visitor(value.stage);
  visitor(value.tick);
  visitor(value.legs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskTransferFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.contract_id);
  visitor(value.task_id);
  visitor(value.source_action_id);
  visitor(value.from_inventory_id);
  visitor(value.to_inventory_id);
  visitor(value.item_id);
  visitor(value.quantity);
  visitor(value.completed_tick);
  visitor(value.from_owner);
  visitor(value.to_owner);
  visitor(value.purpose);
  visitor(value.completed_at);
  visitor(value.original_completion_result_ref);
  visitor(value.quantity_source_kind);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskTransportAgreementState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.agreements);
  visitor(value.report_carriages);
  visitor(value.report_handover_sources);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskPersonalReportPolicy &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.offer_id);
  visitor(value.personal_template_id);
  visitor(value.sender_say_node_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskPersonalReportCatalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.policies);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskPersonalMessagePolicy &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.offer_id);
  visitor(value.personal_template_id);
  visitor(value.recipient_say_node_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskPersonalMessageCatalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.policies);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::TaskPersonalCancellationPolicy &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.offer_id);
  visitor(value.personal_template_id);
  visitor(value.sender_say_node_id);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::TaskPersonalCancellationCatalog &value,
      Visitor &&visitor) {
  visitor(value.version);
  visitor(value.policies);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskPersonalAdvanceCatalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.policies);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskPersonalAcceptancePolicy &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.offer_id);
  visitor(value.personal_template_id);
  visitor(value.sender_say_node_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskPersonalAcceptanceCatalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.policies);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskMessageReturnPolicy &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.offer_id);
  visitor(value.reply_item_id);
  visitor(value.recipient_role_id);
  visitor(value.dialogue_id);
  visitor(value.node_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskMessageReturnCatalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.policies);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskMessageNoticeCatalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.bindings);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskMessageAddress &value,
                  Visitor &&visitor) {
  visitor(value.offer_id);
  visitor(value.recipient_role_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskExtensionPolicy &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.offer_id);
  visitor(value.dialogue_id);
  visitor(value.node_id);
  visitor(value.additional_ticks);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskExtensionCatalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.policies);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskDeathNoticeBindingPolicy &value,
                  Visitor &&visitor) {
  visitor(value.offer_id);
  visitor(value.decedent_actor_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskCatalog &value,
                  Visitor &&visitor) {
  visitor(value.offers);
  visitor(value.presentation_names);
  visitor(value.templates);
  visitor(value.personal_acceptances);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::TaskAuthoredContinuationCatalog &value,
      Visitor &&visitor) {
  visitor(value.version);
  visitor(value.message_addresses);
}

// native/world/src/resident/domains/tasks/task_report_carrier_catalog.hpp:7;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void
visit(const ::hanmo::resident::TaskReportCarrierOfferDefinition &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.rule_version);
  visitor(value.carrier_actor_id);
  visitor(value.role_id);
  visitor(value.dialogue_id);
  visitor(value.accept_node_id);
  visitor(value.destination_site_id);
  visitor(value.allowed_sender_actor_ids);
  visitor(value.allowed_recipient_actor_ids);
  visitor(value.allowed_report_categories);
}
