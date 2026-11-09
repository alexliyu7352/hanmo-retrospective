// 职责：集中政治与条约中的治理方案与执行访问；仅借用原字段，不改变顺序或失败行为。
// 由中央登记头在 source_owned_fields
// 命名空间内包含；依赖与具名助手已先行可见。

template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceAuthorityRecord &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.policy_id);
  visitor(value.issuer_actor_id);
  visitor(value.office_id);
  visitor(value.grant_id);
  visitor(value.organization_id);
  visitor(value.executor_actor_id);
  visitor(value.jurisdiction_id);
  visitor(value.site_id);
  visitor(value.relief_policy_id);
  visitor(value.public_wallet_id);
  visitor(value.public_inventory_id);
  visitor(value.continuity_policy_id);
  visitor(value.authority_epoch);
  visitor(value.jurisdiction_revision);
  visitor(value.issued_tick);
  visitor(value.offer_expires_tick);
  visitor(value.completion_deadline_ticks);
  visitor(value.rule);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::GovernanceContinuationPlayerReceipt &value,
      Visitor &&visitor) {
  visitor(value.source_fact_id);
  visitor(value.actor_id);
  visitor(value.result_id);
  visitor(value.tick);
  visitor(value.request_sequence);
  visitor(value.result_revision);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::GovernanceInvitationReplyPlayerReceipt &value,
      Visitor &&visitor) {
  visitor(value.result_id);
  visitor(value.actor_id);
  visitor(value.request_sequence);
  visitor(value.result_revision);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::GovernanceInvitationReplyRecord &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.authority_fact_id);
  visitor(value.actor_id);
  visitor(value.counterpart_actor_id);
  visitor(value.site_id);
  visitor(value.grant_id);
  visitor(value.kind);
  visitor(value.tick);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::GovernanceInvitationReplyState &value,
      Visitor &&visitor) {
  visitor(value.version);
  visitor(value.replies);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceOutcomePlan &value,
                  Visitor &&visitor) {
  visitor(value.rule_id);
  visitor(value.rule_version);
  visitor(value.executor_actor_id);
  visitor(value.work_completed_tick);
  visitor(value.rites_xp);
  visitor(value.rites_level);
  visitor(value.support_delta);
  visitor(value.support_cap);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceTaskState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.governance_contracts);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::JurisdictionSecurityState &value,
                  Visitor &&visitor) {
  visitor(value.jurisdiction_id);
  visitor(value.registration_fact_id);
  visitor(value.last_fact_id);
  visitor(value.original_jurisdiction_definition);
  visitor(value.security);
  visitor(value.revision);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalGovernanceAgencyAnswer &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.offer_fact_id);
  visitor(value.actor_id);
  visitor(value.tick);
  visitor(value.accept);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalGovernanceAgencyAuthoredOrigin &value,
      Visitor &&visitor) {
  visitor(value.intent_id);
  visitor(value.director_policy_id);
  visitor(value.actor_id);
  visitor(value.step);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalGovernanceAgencyIssue &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.offer_fact_id);
  visitor(value.acceptance_fact_id);
  visitor(value.delegate_actor_id);
  visitor(value.executor_actor_id);
  visitor(value.authority_fact_id);
  visitor(value.tick);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalGovernanceAgencyOffer &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.policy_id);
  visitor(value.organization_id);
  visitor(value.office_id);
  visitor(value.issuer_actor_id);
  visitor(value.delegate_actor_id);
  visitor(value.appointment_grant_id);
  visitor(value.commission_grant_id);
  visitor(value.office_epoch);
  visitor(value.jurisdiction_revision);
  visitor(value.tick);
  visitor(value.response_before_tick);
  visitor(value.valid_through_tick);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalGovernanceAgencyRevocation &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.offer_fact_id);
  visitor(value.issuer_actor_id);
  visitor(value.office_epoch);
  visitor(value.tick);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalGovernanceAgencyState &value,
      Visitor &&visitor) {
  visitor(value.version);
  visitor(value.offers);
  visitor(value.answers);
  visitor(value.revocations);
  visitor(value.issues);
  visitor(value.authored_origins);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalGovernanceContinuation &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.task_id);
  visitor(value.acceptance_fact_id);
  visitor(value.authority_fact_id);
  visitor(value.contract_id);
  visitor(value.executor_actor_id);
  visitor(value.jurisdiction_id);
  visitor(value.site_id);
  visitor(value.original_organization_id);
  visitor(value.public_wallet_id);
  visitor(value.public_inventory_id);
  visitor(value.registration_fact_id);
  visitor(value.approval_fact_id);
  visitor(value.decision_fact_id);
  visitor(value.occupation_fact_id);
  visitor(value.grant_id);
  visitor(value.authority);
  visitor(value.tick);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalGovernanceContinuationState &value,
      Visitor &&visitor) {
  visitor(value.version);
  visitor(value.continuations);
  visitor(value.player_receipts);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticsGovernanceState &value,
                  Visitor &&visitor) {
  visitor(value.authorities);
  visitor(value.plans);
  visitor(value.civil);
  visitor(value.consequences);
  visitor(value.jurisdiction_security);
  visitor(value.security_consequences);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RecordedGovernancePlan &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.authority_fact_id);
  visitor(value.work_fact_id);
  visitor(value.task_id);
  visitor(value.contract_id);
  visitor(value.action_id);
  visitor(value.value);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::ApproveGovernanceWorkReassignment &value,
      Visitor &&visitor) {
  visitor(value.authority);
  visitor(value.work_id);
  visitor(value.new_actor_id);
  visitor(value.expected_executor_predecessor_ref);
  visitor(value.expected_target_revision);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticsGovernanceCatalog &value,
                  Visitor &&visitor) {
  visitor(value.enabled);
  visitor(value.appeasement_rules);
  visitor(value.commission_policies);
  visitor(value.initial_support);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalGovernanceIntentCatalog &value,
      Visitor &&visitor) {
  visitor(value.version);
  visitor(value.intents);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalGovernanceIntent &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.issuer_actor_id);
  visitor(value.executor_actor_id);
  visitor(value.office_id);
  visitor(value.jurisdiction_id);
  visitor(value.commission_policy_id);
  visitor(value.public_inventory_id);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalGovernanceAgencyPolicy &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.jurisdiction_id);
  visitor(value.office_id);
  visitor(value.appointment_grant_id);
  visitor(value.commission_grant_id);
  visitor(value.commission_policy_id);
  visitor(value.public_inventory_id);
  visitor(value.response_ticks);
  visitor(value.mandate_ticks);
  visitor(value.maximum_issues);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalGovernanceAgencyIntentCatalog &value,
      Visitor &&visitor) {
  visitor(value.version);
  visitor(value.intents);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalGovernanceAgencyIntent &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.agency_policy_id);
  visitor(value.principal_actor_id);
  visitor(value.delegate_actor_id);
  visitor(value.executor_actor_id);
  visitor(value.principal_director_policy_id);
  visitor(value.delegate_director_policy_id);
  visitor(value.executor_director_policy_id);
  visitor(value.delegate_accepts);
  visitor(value.executor_accepts);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalGovernanceAgencyCatalog &value,
      Visitor &&visitor) {
  visitor(value.version);
  visitor(value.policies);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceCommissionPolicy &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.rule_id);
  visitor(value.relief_policy_id);
  visitor(value.offer_valid_ticks);
  visitor(value.completion_deadline_ticks);
}

// native/world/src/resident/domains/politics/governance_targets/governance_program.hpp:10;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CompletedSkillRule &value,
                  Visitor &&visitor) {
  visitor(value.rule_ref);
}

// native/world/src/resident/domains/politics/governance_targets/governance_program.hpp:9;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::FixedApprovedDelta &value,
                  Visitor &&visitor) {
  visitor(value.security_delta_points);
}

// native/world/src/resident/domains/politics/governance_targets/governance_program.hpp:38;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceAgencyExecutor &value,
                  Visitor &&visitor) {
  visitor(value.agency_ref);
}

// native/world/src/resident/domains/politics/governance_targets/governance_program.hpp:8;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceAgronomy &value,
                  Visitor &&visitor) {
  visitor(value.production_unit_id);
  visitor(value.improvement_policy_id);
}

// native/world/src/resident/domains/politics/governance_targets/governance_program.hpp:70;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceAgronomyApplication &value,
                  Visitor &&visitor) {
  visitor(value.production_improvement_receipt);
}

// native/world/src/resident/domains/politics/governance_targets/governance_program.hpp:80;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceAppliedEffectSource &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.program_id);
  visitor(value.outcome_source_id);
  visitor(value.application);
  visitor(value.applied_at);
}

// native/world/src/resident/domains/politics/governance_targets/governance_program.hpp:17;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceCashRelief &value,
                  Visitor &&visitor) {
  visitor(value.currency_id);
  visitor(value.target_amount);
  visitor(value.max_support_delta);
}

// native/world/src/resident/domains/politics/governance_targets/governance_program.hpp:44;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void
visit(const ::hanmo::resident::GovernanceContinuityDefinition &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.original_grant_policy);
}

// native/world/src/resident/domains/politics/governance_targets/governance_program.hpp:62;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceExecutionBinding &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.program_id);
  visitor(value.offered_task_ref);
  visitor(value.task_acceptance_ref);
  visitor(value.execution_ref);
  visitor(value.occurred_at);
}

// native/world/src/resident/domains/politics/governance_targets/governance_program.hpp:41;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void
visit(const ::hanmo::resident::GovernanceExecutorQualification &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.skill_id);
  visitor(value.minimum_level);
}

// native/world/src/resident/domains/politics/governance_targets/governance_program.hpp:73;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceFacilityApplication &value,
                  Visitor &&visitor) {
  visitor(value.facility_activation_receipt);
}

// native/world/src/resident/domains/politics/governance_targets/governance_program.hpp:16;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceFacilityBuild &value,
                  Visitor &&visitor) {
  visitor(value.project_spec_id);
  visitor(value.site_use_ref);
  visitor(value.asset_owner);
}

// native/world/src/resident/domains/politics/governance_targets/governance_program.hpp:72;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void
visit(const ::hanmo::resident::GovernanceFortificationApplication &value,
      Visitor &&visitor) {
  visitor(value.fortification_repair_result);
}

// native/world/src/resident/domains/politics/governance_targets/governance_program.hpp:15;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceFortificationRepair &value,
                  Visitor &&visitor) {
  visitor(value.stronghold_id);
  visitor(value.topology_id);
  visitor(value.node_id);
  visitor(value.target_hp);
}

// native/world/src/resident/domains/politics/governance_targets/governance_program.hpp:18;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceGrainRelief &value,
                  Visitor &&visitor) {
  visitor(value.item_id);
  visitor(value.target_quantity);
  visitor(value.max_support_delta);
}

// native/world/src/resident/domains/politics/governance_targets/governance_program.hpp:75;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceIndustryApplication &value,
                  Visitor &&visitor) {
  visitor(value.production_improvement_receipt);
}

// native/world/src/resident/domains/politics/governance_targets/governance_program.hpp:20;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceIndustrySupport &value,
                  Visitor &&visitor) {
  visitor(value.production_unit_id);
  visitor(value.investment_policy_id);
  visitor(value.investment_contract_ref);
}

// native/world/src/resident/domains/politics/governance_targets/governance_program.hpp:38;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceNamedExecutor &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
}

// native/world/src/resident/domains/politics/governance_targets/governance_program.hpp:7;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::GovernancePacification &value,
                  Visitor &&visitor) {
  visitor(value.jurisdiction_id);
  visitor(value.relief_terms_id);
  visitor(value.support_delta_points);
}

// native/world/src/resident/domains/politics/governance_targets/governance_program.hpp:69;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void
visit(const ::hanmo::resident::GovernancePacificationApplication &value,
      Visitor &&visitor) {
  visitor(value.actual_work_result);
  visitor(value.public_relief_receipt);
  visitor(value.civil_consequence);
}

// native/world/src/resident/domains/politics/governance_targets/governance_program.hpp:14;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::GovernancePatrol &value,
                  Visitor &&visitor) {
  visitor(value.jurisdiction_id);
  visitor(value.unit_ids);
  visitor(value.gain);
}

// native/world/src/resident/domains/politics/governance_targets/governance_program.hpp:71;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::GovernancePatrolApplication &value,
                  Visitor &&visitor) {
  visitor(value.actual_work_result);
  visitor(value.security_consequence);
}

// native/world/src/resident/domains/politics/governance_targets/governance_program.hpp:27;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::GovernancePatrolWorkTerms &value,
                  Visitor &&visitor) {
  visitor(value.permitted_leader_actor_ids);
  visitor(value.route);
  visitor(value.minimum_available_people);
  visitor(value.completion_deadline);
  visitor(value.supply_order_ref);
  visitor(value.rule_id);
  visitor(value.rule_version);
}

// native/world/src/resident/domains/politics/governance_targets/governance_program.hpp:48;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceProgramApproval &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.spec_ref);
  visitor(value.target);
  visitor(value.executor);
  visitor(value.budget_execution);
  visitor(value.budget_period);
  visitor(value.payer_wallet_id);
  visitor(value.contract_debtor);
  visitor(value.issuer);
  visitor(value.authority_basis_ref);
  visitor(value.expected_target_revision);
  visitor(value.issued_at);
  visitor(value.valid_until);
}

// native/world/src/resident/domains/politics/governance_targets/governance_program.hpp:79;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceProgramOutcome &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.program_id);
  visitor(value.target_kind);
  visitor(value.execution_ref);
  visitor(value.actual_result_ref);
  visitor(value.application_refs);
  visitor(value.status);
  visitor(value.remaining_requirement);
  visitor(value.continuity);
  visitor(value.occurred_at);
  visitor(value.patrol_unavailable);
}

// native/world/src/resident/domains/politics/governance_targets/governance_program.hpp:32;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceProgramSpec &value,
                  Visitor &&visitor) {
  visitor(value.spec_ref);
  visitor(value.target);
  visitor(value.resource_recipe_id);
  visitor(value.executor_qualification_id);
  visitor(value.required_work_half_days);
  visitor(value.scope);
  visitor(value.completion_policy);
  visitor(value.continuity_policy_ref);
  visitor(value.expense_policy_ref);
  visitor(value.offer_valid_half_days);
  visitor(value.approved_work_terms);
}

// native/world/src/resident/domains/politics/governance_targets/governance_program.hpp:21;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceRecruit &value,
                  Visitor &&visitor) {
  visitor(value.recruitment_order_id);
}

// native/world/src/resident/domains/politics/governance_targets/governance_program.hpp:76;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceRecruitApplication &value,
                  Visitor &&visitor) {
  visitor(value.population_military_result);
}

// native/world/src/resident/domains/politics/governance_targets/governance_program.hpp:19;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceRelief &value,
                  Visitor &&visitor) {
  visitor(value.relief_terms_ids);
  visitor(value.effect_period);
  visitor(value.effect_measure);
}

// native/world/src/resident/domains/politics/governance_targets/governance_program.hpp:74;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceReliefApplication &value,
                  Visitor &&visitor) {
  visitor(value.actual_receipts);
  visitor(value.civil_consequence);
}

// native/world/src/resident/domains/politics/governance_targets/governance_program.hpp:29;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void
visit(const ::hanmo::resident::GovernanceSupervisionActionDuration &value,
      Visitor &&visitor) {
  visitor(value.half_days);
}

// native/world/src/resident/domains/politics/governance_targets/governance_program.hpp:55;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void
visit(const ::hanmo::resident::GovernanceSupervisionExecutionApproval &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.work_id);
  visitor(value.authorization);
  visitor(value.execution_ordinal);
  visitor(value.preceding_terminal_ref);
  visitor(value.quote_ref);
  visitor(value.spec_ref);
  visitor(value.service_limit_half_days);
  visitor(value.issuer);
  visitor(value.authority_basis_ref);
  visitor(value.budget_execution);
  visitor(value.budget_period);
  visitor(value.expected_target_revision);
  visitor(value.issued_at);
  visitor(value.source_schema);
  visitor(value.successor_acceptance_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceProgramCatalog &value,
                  Visitor &&visitor) {
  visitor(value.identity);
  visitor(value.specs);
  visitor(value.patrol_gain_rules);
  visitor(value.schema_version);
  visitor(value.executor_qualifications);
  visitor(value.continuity_definitions);
  visitor(value.supervision_execution_quotes);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceProgramState &value,
                  Visitor &&visitor) {
  visitor(value.approvals);
  visitor(value.execution_bindings);
  visitor(value.outcome_sources);
  visitor(value.applied_effect_sources);
  visitor(value.schema_version);
  visitor(value.supervision_execution_approvals);
  visitor(value.work_reassignment_approvals);
  visitor(value.work_deadline_terminations);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::GovernanceWorkReassignmentApproval &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.work_id);
  visitor(value.authorization);
  visitor(value.work_acceptance_ref);
  visitor(value.executor_predecessor_ref);
  visitor(value.preceding_terminal_ref);
  visitor(value.kind);
  visitor(value.old_actor_id);
  visitor(value.new_actor_id);
  visitor(value.spec_ref);
  visitor(value.issuer);
  visitor(value.authority_basis_ref);
  visitor(value.budget_execution);
  visitor(value.budget_period);
  visitor(value.expected_target_revision);
  visitor(value.issued_at);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::GovernanceWorkDeadlineTermination &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.work_id);
  visitor(value.authorization);
  visitor(value.work_acceptance_ref);
  visitor(value.spec_ref);
  visitor(value.absolute_deadline);
  visitor(value.effective_at);
}

// native/world/src/resident/domains/politics/governance_targets/governance_program.hpp:46;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void
visit(const ::hanmo::resident::GovernanceSupervisionExecutionQuote &value,
      Visitor &&visitor) {
  visitor(value.quote_ref);
  visitor(value.program_spec_ref);
  visitor(value.time_use);
}

// native/world/src/resident/domains/politics/governance_targets/governance_program.hpp:30;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void
visit(const ::hanmo::resident::GovernanceSupervisionWorkTerms &value,
      Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.permitted_supervisor_actor_ids);
  visitor(value.economic_work_ref);
  visitor(value.completion_deadline);
  visitor(value.target_site_or_anchor_ref);
  visitor(value.required_skill_ref);
  visitor(value.rule_id);
  visitor(value.rule_version);
  visitor(value.initial_execution_time_use);
}

// native/world/src/resident/domains/politics/governance_targets/governance_program.hpp:22;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceTrain &value,
                  Visitor &&visitor) {
  visitor(value.training_order_id);
}

// native/world/src/resident/domains/politics/governance_targets/governance_program.hpp:77;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceTrainApplication &value,
                  Visitor &&visitor) {
  visitor(value.military_training_receipt);
}

// native/world/src/resident/domains/politics/politics_security_types.hpp:8;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::JurisdictionSecurityInitial &value,
                  Visitor &&visitor) {
  visitor(value.jurisdiction_id);
  visitor(value.original_jurisdiction_definition);
  visitor(value.security);
}

// native/world/src/resident/domains/politics/governance_targets/governance_program.hpp:12;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::PatrolGainRule &value,
                  Visitor &&visitor) {
  visitor(value.rule_ref);
  visitor(value.base_gain);
  visitor(value.skill_id);
  visitor(value.cap);
}
