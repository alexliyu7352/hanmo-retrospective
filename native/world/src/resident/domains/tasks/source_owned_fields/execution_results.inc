// 职责：集中任务与履约中的执行、验工与结果依据访问；仅借用原字段，不改变顺序或失败行为。
// 由中央登记头在 source_owned_fields
// 命名空间内包含；依赖与具名助手已先行可见。

template <class Visitor>
inline void visit(const ::hanmo::resident::AcceptedFollowThroughStopped &value,
                  Visitor &&visitor) {
  visitor(value.completed_work_result_ref);
  visitor(value.failed_follow_through_refs);
  visitor(value.source_terminal_refs);
  visitor(value.fulfilled_goal_refs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ContractRecord &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.source_binding);
  visitor(value.issuer);
  visitor(value.assignee);
  visitor(value.executor_actor_id);
  visitor(value.org_activity_id);
  visitor(value.current_task_id);
  visitor(value.historical_task_ids);
  visitor(value.frozen_terms);
  visitor(value.status);
  visitor(value.settlement_id);
  visitor(value.restitution_refs);
  visitor(value.delivered_quantity);
  visitor(value.eligible_delivered_quantity);
  visitor(value.paid_reward);
  visitor(value.refunded_budget);
  visitor(value.refunded_reward);
  visitor(value.execution_stopped);
  visitor(value.stop_reason);
  visitor(value.accepted_authority);
  visitor(value.cargo_disposition_ids);
  visitor(value.deadline_closure);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EnvoyHandoverInterruption &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.task_id);
  visitor(value.contract_id);
  visitor(value.acceptance_fact_id);
  visitor(value.action_id);
  visitor(value.action_kind);
  visitor(value.provider_actor_id);
  visitor(value.place_id);
  visitor(value.reason);
  visitor(value.started_tick);
  visitor(value.stopped_tick);
  visitor(value.completed_work_ticks);
  visitor(value.cause_fact_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EnvoyTaskAcceptance &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.task_id);
  visitor(value.contract_id);
  visitor(value.mission_authority_fact_id);
  visitor(value.proposal_id);
  visitor(value.proposal_fact_id);
  visitor(value.terms_revision);
  visitor(value.issuer_organization_id);
  visitor(value.target_organization_id);
  visitor(value.envoy_actor_id);
  visitor(value.target_office_id);
  visitor(value.delivery_site_id);
  visitor(value.report_office_id);
  visitor(value.report_site_id);
  visitor(value.continuity_policy_id);
  visitor(value.display_name);
  visitor(value.accepted_revision);
  visitor(value.accepted_tick);
  visitor(value.delivery_expires_tick);
  visitor(value.return_report_required);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EnvoyTaskAction &value,
                  Visitor &&visitor) {
  visitor(value.action_id);
  visitor(value.kind);
  visitor(value.context);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EnvoyTaskCompletion &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.task_id);
  visitor(value.contract_id);
  visitor(value.acceptance_fact_id);
  visitor(value.delivery_fact_id);
  visitor(value.report_action_id);
  visitor(value.result);
  visitor(value.completed_tick);
  visitor(value.reason);
  visitor(value.diplomatic_report_fact_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EnvoyTaskContract &value,
                  Visitor &&visitor) {
  visitor(value.acceptance);
  visitor(value.action);
  visitor(value.delivery);
  visitor(value.completion);
  visitor(value.pending_code);
  visitor(value.interruptions);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EnvoyTaskDelivery &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.task_id);
  visitor(value.contract_id);
  visitor(value.action_id);
  visitor(value.acceptance_fact_id);
  visitor(value.diplomatic_presentation_fact_id);
  visitor(value.completed_tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EnvoyTaskState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.envoy_contracts);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FrozenObjectiveBase &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.task_id);
  visitor(value.task_acceptance_source);
  visitor(value.task_terms);
  visitor(value.criterion);
  visitor(value.goal);
  visitor(value.required_participant);
  visitor(value.required_recipient);
  visitor(value.professional_target);
  visitor(value.allowed_professional_kinds);
  visitor(value.required_source_terms);
  visitor(value.craft);
  visitor(value.allow_natural_event);
  visitor(value.allow_partial);
  visitor(value.record_contribution);
  visitor(value.use_policy);
  visitor(value.acceptance_purpose);
  visitor(value.completion_gate);
  visitor(value.report_required);
  visitor(value.report_recipient);
  visitor(value.report_gate);
  visitor(value.observer);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceTaskContract &value,
                  Visitor &&visitor) {
  visitor(value.acceptance);
  visitor(value.action_id);
  visitor(value.work);
  visitor(value.completion);
  visitor(value.interruption);
  visitor(value.execution_note_code);
  visitor(value.pending_code);
  visitor(value.last_settlement_attempt_tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceWorkCompletion &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.task_id);
  visitor(value.contract_id);
  visitor(value.action_id);
  visitor(value.acceptance_fact_id);
  visitor(value.outcome_plan_id);
  visitor(value.started_tick);
  visitor(value.completed_tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceWorkInterruption &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.task_id);
  visitor(value.contract_id);
  visitor(value.action_id);
  visitor(value.acceptance_fact_id);
  visitor(value.reason);
  visitor(value.stopped_tick);
  visitor(value.completed_work_ticks);
  visitor(value.cause_fact_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LegacyAppeaseWork &value,
                  Visitor &&visitor) {
  visitor(value.work_completion_fact_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LegacyTaskEvidenceRef &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ObjectiveAcceptanceReceipt &value,
                  Visitor &&visitor) {
  visitor(value.acceptance_id);
  visitor(value.task_id);
  visitor(value.objective_id);
  visitor(value.source);
  visitor(value.criterion);
  visitor(value.actual_participant);
  visitor(value.recipient);
  visitor(value.professional_target);
  visitor(value.completed_tick);
  visitor(value.measure);
  visitor(value.accepted);
  visitor(value.contribution_use_ref);
  visitor(value.source_settlement_key);
  visitor(value.source_owner);
  visitor(value.completed_at);
  visitor(value.accepted_at);
  visitor(value.retained_result_refs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ObjectiveContributionUse &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.acceptance_id);
  visitor(value.objective_id);
  visitor(value.task_id);
  visitor(value.source_owner);
  visitor(value.settlement_key);
  visitor(value.purpose);
  visitor(value.policy);
  visitor(value.measure);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ObjectiveCraftBinding &value,
                  Visitor &&visitor) {
  visitor(value.order_ref);
  visitor(value.job_id);
  visitor(value.item_id);
  visitor(value.from_owner);
  visitor(value.to_owner);
  visitor(value.acceptor_actor_id);
  visitor(value.allow_partial);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ObjectiveMeasured &value,
                  Visitor &&visitor) {
  visitor(value.amount);
  visitor(value.unit_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ObjectivePredicate &value,
                  Visitor &&visitor) {
  visitor(value.predicate_source);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ObjectivePredicateGoal &value,
                  Visitor &&visitor) {
  visitor(value.source_domain);
  visitor(value.source_kind);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ObjectiveQuantityGoal &value,
                  Visitor &&visitor) {
  visitor(value.amount);
  visitor(value.unit_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ObjectiveReceiptRef &value,
                  Visitor &&visitor) {
  visitor(value.source_domain);
  visitor(value.receipt_kind);
  visitor(value.fact_id);
  visitor(value.activity_id);
  visitor(value.contribution_use_id);
  visitor(value.rule_version);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ObjectiveReportReceipt &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.task_id);
  visitor(value.original_report_source);
  visitor(value.recipient);
  visitor(value.received_at);
  visitor(value.accepted_objective_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PatrolResult &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ProfessionalActorTarget &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ProfessionalBatchTarget &value,
                  Visitor &&visitor) {
  visitor(value.batch_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ProfessionalCaseTarget &value,
                  Visitor &&visitor) {
  visitor(value.case_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ProfessionalCriterion &value,
                  Visitor &&visitor) {
  visitor(value.criterion_kind);
  visitor(value.criterion_version);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ProfessionalExpeditionTarget &value,
                  Visitor &&visitor) {
  visitor(value.expedition_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ProfessionalItemTarget &value,
                  Visitor &&visitor) {
  visitor(value.instance_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ProfessionalObserverScope &value,
                  Visitor &&visitor) {
  visitor(value.observer);
  visitor(value.registered_scope_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ProfessionalOutcomeSource &value,
                  Visitor &&visitor) {
  visitor(value.ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ProfessionalRitualTarget &value,
                  Visitor &&visitor) {
  visitor(value.ritual_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ProfessionalSiteTarget &value,
                  Visitor &&visitor) {
  visitor(value.site_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ProfessionalWorkTarget &value,
                  Visitor &&visitor) {
  visitor(value.work_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::S3WorkCommission &value,
                  Visitor &&visitor) {
  visitor(value.work_id);
  visitor(value.accepted_task_ref);
  visitor(value.work_acceptance_source_ref);
  visitor(value.expense_terms_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StateNeedObjectiveBinding &value,
                  Visitor &&visitor) {
  visitor(value.need_ref);
  visitor(value.goal_ref);
  visitor(value.goal_source_episode);
  visitor(value.criterion);
  visitor(value.required_actor_role);
  visitor(value.claim_id);
  visitor(value.disclosure_policy_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StateNeedTaskContract &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.task_id);
  visitor(value.offer_id);
  visitor(value.acceptance_id);
  visitor(value.escrow_wallet_id);
  visitor(value.assignee);
  visitor(value.executor_actor_id);
  visitor(value.objective);
  visitor(value.money);
  visitor(value.completion_gate);
  visitor(value.cancel_policy);
  visitor(value.report_recipient);
  visitor(value.report_required);
  visitor(value.accepted_at);
  visitor(value.claim_until_tick);
  visitor(value.accepted_goal_revision);
  visitor(value.phase);
  visitor(value.closing_cause);
  visitor(value.contribution_id);
  visitor(value.report_receipt);
  visitor(value.settlement_source);
  visitor(value.closing_source);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SupervisedResult &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SupervisedWorkPlan &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.supervisor_actor_id);
  visitor(value.economic_work_ref);
  visitor(value.required_half_days);
  visitor(value.completion_deadline);
  visitor(value.target_site_or_anchor_ref);
  visitor(value.required_skill_ref);
  visitor(value.expense_terms_ref);
  visitor(value.continuity_policy_ref);
  visitor(value.rule_id);
  visitor(value.rule_version);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SupervisionExecutionTerms &value,
                  Visitor &&visitor) {
  visitor(value.work_id);
  visitor(value.execution_ordinal);
  visitor(value.original_authorization_ref);
  visitor(value.original_approved_execution_terms_ref);
  visitor(value.action_id);
  visitor(value.kind);
  visitor(value.service_limit_half_days);
  visitor(value.actual_start_source_ref);
  visitor(value.actual_terminal_result_ref);
  visitor(value.initial_successor_acceptance_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskCompletionEvidence &value,
                  Visitor &&visitor) {
  visitor(value.task_id);
  visitor(value.template_id);
  visitor(value.settlement_id);
  visitor(value.reported_tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskContractPaymentLeg &value,
                  Visitor &&visitor) {
  visitor(value.purpose);
  visitor(value.from_wallet_id);
  visitor(value.to_wallet_id);
  visitor(value.amount);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskContractPaymentSource &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.responsibility_id);
  visitor(value.task_id);
  visitor(value.registration_fact_id);
  visitor(value.decision_fact_id);
  visitor(value.tick);
  visitor(value.legs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskExactExecutionState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.starts);
  visitor(value.terminals);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskExactQuantityFields &value,
                  Visitor &&visitor) {
  visitor(value.assignee);
  visitor(value.original_acceptance_action_ref);
  visitor(value.deadline_gate);
  visitor(value.objective_completed_at);
  visitor(value.reported_at);
  visitor(value.original_acceptance_result_ref);
  visitor(value.original_report_result_ref);
}

template <class Visitor>
inline void visit_payload(const ::hanmo::resident::TaskExactStartSource &value,
                          Visitor &&visitor) {
  visit_task_exact_start_payload(value, visitor);
}

template <class Visitor>
inline void
visit_payload(const ::hanmo::resident::TaskExactStartBuildView &value,
              Visitor &&visitor) {
  visit_task_exact_start_payload(value, visitor);
}

template <class Visitor>
inline void
visit_payload(const ::hanmo::resident::TaskExactTerminalSource &value,
              Visitor &&visitor) {
  visit_TaskExactTerminalSource_payload(value, visitor);
}

template <class Visitor>
inline void
visit_payload(const ::hanmo::resident::TaskExactTerminalBuildView &value,
              Visitor &&visitor) {
  visit_TaskExactTerminalSource_payload(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskMessageReturnResult &value,
                  Visitor &&visitor) {
  visitor(value.result_id);
  visitor(value.request_actor_id);
  visitor(value.command_kind);
  visitor(value.request_sequence);
  visitor(value.result_revision);
  visitor(value.from_tick);
  visitor(value.result_tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskObjectiveBaseState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.objectives);
  visitor(value.acceptances);
  visitor(value.contribution_uses);
  visitor(value.reports);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskPersonalMessageResult &value,
                  Visitor &&visitor) {
  visitor(value.result_id);
  visitor(value.request_actor_id);
  visitor(value.command_kind);
  visitor(value.request_sequence);
  visitor(value.result_revision);
  visitor(value.from_tick);
  visitor(value.result_tick);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::TaskPublicAccountExecutionState &value,
      Visitor &&visitor) {
  visitor(value.version);
  visitor(value.payments);
  visitor(value.static_payments);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskQuantityDeadlineClosure &value,
                  Visitor &&visitor) {
  visitor(value.source_id);
  visitor(value.task_id);
  visitor(value.original_acceptance_start_ref);
  visitor(value.frozen_deadline_gate);
  visitor(value.original_period_opening);
  visitor(value.closed_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskShopOrderContactResult &value,
                  Visitor &&visitor) {
  visitor(value.result_id);
  visitor(value.request_actor_id);
  visitor(value.request_sequence);
  visitor(value.result_revision);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskWorkExecutionState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.work_task_offers);
  visitor(value.work_task_acceptances);
  visitor(value.acceptances);
  visitor(value.episodes);
  visitor(value.period_receipts);
  visitor(value.actual_starts);
  visitor(value.leader_changes);
  visitor(value.results);
  visitor(value.supervision_starts);
  visitor(value.supervision_window_closures);
  visitor(value.supervision_terminals);
  visitor(value.supervision_execution_terms);
  visitor(value.work_reassignment_acceptances);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::WorkAcceptanceSource &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.work_id);
  visitor(value.authorization);
  visitor(value.original_task_acceptance);
  visitor(value.expense_terms_ref);
  visitor(value.accepted_at);
  visitor(value.earliest_service_at);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::WorkReassignmentAcceptanceSource &value,
      Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.work_id);
  visitor(value.reassignment_approval_ref);
  visitor(value.work_acceptance_ref);
  visitor(value.new_actor_id);
  visitor(value.acceptance_contact);
  visitor(value.accepted_at);
  visitor(value.earliest_service_at);
  visitor(value.accepted_period_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::WorkActualStartSource &value,
                  Visitor &&visitor) {
  visitor(value.start_fact_id);
  visitor(value.work_id);
  visitor(value.authorization);
  visitor(value.execution);
  visitor(value.actual_basis);
  visitor(value.started_at);
  visitor(value.source_schema);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::WorkAuthorizationRef &value,
                  Visitor &&visitor) {
  visitor(value.program_ref);
  visitor(value.target_line_id);
  visitor(value.terms_revision);
  visitor(value.accepted_task_id);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::WorkCommissionTaskTerminalSource &value,
      Visitor &&visitor) {
  visitor(value.task_terminal_fact_id);
  visitor(value.accepted_scope_ref);
  visitor(value.accepted_terms_ref);
  visitor(value.work_result_ref);
  visitor(value.termination_basis);
  visitor(value.preserved_fulfilment_refs);
  visitor(value.remaining_financial_or_return_refs);
  visitor(value.effective_game_time);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::WorkCommonActionBinding &value,
                  Visitor &&visitor) {
  visitor(value.action_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::WorkCriterion &value,
                  Visitor &&visitor) {
  visitor(value.criterion_version);
  visitor(value.authorization);
  visitor(value.work_kind);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::WorkEconomicPeriodSource &value,
                  Visitor &&visitor) {
  visitor(value.work_ref);
  visitor(value.window);
  visitor(value.source_parts);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::WorkEpisode &value,
                  Visitor &&visitor) {
  visitor(value.work_id);
  visitor(value.acceptance_source_ref);
  visitor(value.authorization);
  visitor(value.frozen_plan);
  visitor(value.phase);
  visitor(value.current_execution);
  visitor(value.accepted_period_refs);
  visitor(value.leader_change_fact_refs);
  visitor(value.original_execution_sources);
  visitor(value.last_interruption_ref);
  visitor(value.terminal_result_ref);
  visitor(value.actual_start_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::WorkEvidenceSource &value,
                  Visitor &&visitor) {
  visitor(value.source);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::WorkExecutionState &value,
                  Visitor &&visitor) {
  visitor(value.work_unit_id);
  visitor(value.total_work_units);
  visitor(value.completed_work_units);
  visitor(value.last_processed_period);
  visitor(value.phase);
  visitor(value.running_policy_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::WorkExecutionStopped &value,
                  Visitor &&visitor) {
  visitor(value.stopped_work_result_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::WorkLeaderChange &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.work_id);
  visitor(value.old_actor_id);
  visitor(value.new_actor_id);
  visitor(value.authority_source);
  visitor(value.actual_consent_source);
  visitor(value.actual_arrival_source);
  visitor(value.replacement_at);
  visitor(value.new_execution_source);
  visitor(value.original_execution_source);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::WorkMilitaryActivityBinding &value,
                  Visitor &&visitor) {
  visitor(value.military_activity_ref);
  visitor(value.source_schema);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::WorkMilitaryPeriodSource &value,
                  Visitor &&visitor) {
  visitor(value.fact_ref);
  visitor(value.source_schema);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::WorkMilitaryStartBasis &value,
                  Visitor &&visitor) {
  visitor(value.period_fact_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::WorkPeriodReceipt2 &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.work_id);
  visitor(value.kind);
  visitor(value.window);
  visitor(value.actual_until);
  visitor(value.leader_actor_id);
  visitor(value.source);
  visitor(value.execution_source);
  visitor(value.actual_anchor_sources);
  visitor(value.credited_half_days);
  visitor(value.disposition);
  visitor(value.reason);
  visitor(value.reason_source);
  visitor(value.rule_version);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::WorkPeriodRef &value,
                  Visitor &&visitor) {
  visitor(value.binding);
  visitor(value.period_ordinal);
  visitor(value.from_tick);
  visitor(value.to_tick);
  visitor(value.source_schema_version);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::WorkPeriodView &value,
                  Visitor &&visitor) {
  visitor(value.ref);
  visitor(value.actual_actor_roles);
  visitor(value.actual_labor_allocations);
  visitor(value.facility_claim_refs);
  visitor(value.work_unit_id);
  visitor(value.units_before);
  visitor(value.units_added);
  visitor(value.units_after);
  visitor(value.resource_effect_refs);
  visitor(value.exertion_source_refs);
  visitor(value.disposition);
  visitor(value.reason);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::WorkProgramAuthorizationScope &value,
                  Visitor &&visitor) {
  visitor(value.program_ref);
  visitor(value.target_line_id);
  visitor(value.terms_revision);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::WorkResult &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.work_id);
  visitor(value.task_id);
  visitor(value.authorization);
  visitor(value.kind);
  visitor(value.plan_rule_version);
  visitor(value.first_work_at);
  visitor(value.finished_at);
  visitor(value.credited_half_days);
  visitor(value.required_half_days);
  visitor(value.checkpoint_completion_refs);
  visitor(value.period_summary_refs);
  visitor(value.actual_leader_refs);
  visitor(value.encounter_result_refs);
  visitor(value.actual_stage_result_refs);
  visitor(value.expense_receipt_refs);
  visitor(value.disposition);
  visitor(value.stop_reason);
  visitor(value.actual_stop_source);
  visitor(value.skill_input);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::WorkResultSkillInput &value,
                  Visitor &&visitor) {
  visitor(value.condition_before);
  visitor(value.skill_id);
  visitor(value.qualification);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::WorkSupervisionStartBasis &value,
                  Visitor &&visitor) {
  visitor(value.work_period_receipt_ref);
  visitor(value.economic_period_refs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::WorkTaskAcceptanceSource &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.task_id);
  visitor(value.offered_task_ref);
  visitor(value.executor_actor_id);
  visitor(value.acceptance_contact);
  visitor(value.accepted_at);
  visitor(value.earliest_service_at);
  visitor(value.accepted_period_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::WorkTaskOfferSource &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.authorization);
  visitor(value.executor_actor_id);
  visitor(value.presenting_actor_id);
  visitor(value.presentation_contact);
  visitor(value.offered_at);
  visitor(value.valid_until);
}

// native/world/src/resident/domains/tasks/work_execution/work_follow_through.hpp:22;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::FollowThroughTerminalView &value,
                  Visitor &&visitor) {
  visitor(value.original_obligation_ref);
  visitor(value.accepted_goal_ref);
  visitor(value.work_result_ref);
  visitor(value.source_terminal_id);
  visitor(value.source_schema);
  visitor(value.actual_cause_ref);
  visitor(value.continuity_source_ref);
  visitor(value.preserved_actual_result_refs);
  visitor(value.effective_game_time);
}

// native/world/src/resident/domains/tasks/work_execution/work_follow_through.hpp:39;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::WorkCommissionTerminal &value,
                  Visitor &&visitor) {
  visitor(value.task_id);
  visitor(value.accepted_task_ref);
  visitor(value.work_id);
  visitor(value.task_terminal_fact_id);
  visitor(value.source_schema);
}

// native/world/src/resident/kernel/fixed_service_work_period.hpp:7; actual
// owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::WorkPeriodRef2 &value,
                  Visitor &&visitor) {
  visitor(value.binding);
  visitor(value.period_ordinal);
  visitor(value.from);
  visitor(value.until);
  visitor(value.source_schema_version);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TaskExactParticipantDeath &value,
                  Visitor &&visitor) {
  visit_TaskExactParticipantDeath_members(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::TaskExactParticipantDeathBuildView &value,
      Visitor &&visitor) {
  visit_TaskExactParticipantDeath_members(value, visitor);
}
