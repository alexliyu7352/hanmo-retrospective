// 职责：集中事务需求中的有限拥有字段访问；仅借用原字段，不改变顺序或失败行为。
// 由中央登记头在 source_owned_fields
// 命名空间内包含；依赖与具名助手已先行可见。

template <class Visitor>
inline void visit(const ::hanmo::resident::AffairsState &value,
                  Visitor &&visitor) {
  visitor(value.exact_claims);
  visitor(value.state_needs);
  visitor(value.affairs);
  visitor(value.needs);
  visitor(value.claims);
  visitor(value.contributions);
  visitor(value.obligation_needs);
  visitor(value.fiscal_needs);
  visitor(value.cohort_wage_needs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CareCoverageGoal &value,
                  Visitor &&visitor) {
  visitor(value.care_requirement_id);
  visitor(value.subject_actor_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CareGoalSatisfaction &value,
                  Visitor &&visitor) {
  visitor(value.source);
  visitor(value.source_schema);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ContributionUse &value,
                  Visitor &&visitor) {
  visitor(value.receipt_id);
  visitor(value.transfer_fact_id);
  visitor(value.contract_id);
  visitor(value.claim_id);
  visitor(value.need_id);
  visitor(value.counted_amount);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MedicalGoalSatisfaction &value,
                  Visitor &&visitor) {
  visitor(value.source);
  visitor(value.source_schema);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MedicalResolutionGoal &value,
                  Visitor &&visitor) {
  visitor(value.medical_need_id);
  visitor(value.patient_actor_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::Need &value, Visitor &&visitor) {
  visitor(value.id);
  visitor(value.affair_id);
  visitor(value.source_domain);
  visitor(value.source_key);
  visitor(value.beneficiary);
  visitor(value.target_inventory_id);
  visitor(value.item_id);
  visitor(value.unit);
  visitor(value.measure);
  visitor(value.revision);
  visitor(value.created_tick);
  visitor(value.deadline_tick);
  visitor(value.target_amount);
  visitor(value.fulfilled_amount);
  visitor(value.status);
  visitor(value.receipt_ids);
  visitor(value.claim_ids);
  visitor(value.source_fact_id);
  visitor(value.policy_id);
  visitor(value.physical_amount);
  visitor(value.source_revision);
  visitor(value.last_observed_tick);
  visitor(value.obligation_id);
  visitor(value.arrear_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NeedClaim &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.need_id);
  visitor(value.assignee);
  visitor(value.accepted_amount);
  visitor(value.fulfilled_amount);
  visitor(value.accepted_tick);
  visitor(value.expires_tick);
  visitor(value.status);
  visitor(value.contract_id);
  visitor(value.accepted_source);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NeedPolicy &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.source_domain);
  visitor(value.source_key);
  visitor(value.semantic_key);
  visitor(value.organization_id);
  visitor(value.site_id);
  visitor(value.inventory_id);
  visitor(value.item_id);
  visitor(value.unit);
  visitor(value.template_id);
  visitor(value.terms_version);
  visitor(value.funding_wallet_id);
  visitor(value.reward_wallet_id);
  visitor(value.measure);
  visitor(value.max_claim_quantity);
  visitor(value.purchase_budget_per_unit);
  visitor(value.reward_per_unit);
  visitor(value.deadline_ticks);
  visitor(value.accept_role);
  visitor(value.report_role);
  visitor(value.cancellation_policy_id);
  visitor(value.reward_total);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StateClaimDeadlineGateRef &value,
                  Visitor &&visitor) {
  visitor(value.claim_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StateNeed &value,
                  Visitor &&visitor) {
  visitor(value.need_id);
  visitor(value.affair_id);
  visitor(value.goal_ref);
  visitor(value.episode);
  visitor(value.source_episode);
  visitor(value.source_revision);
  visitor(value.opened_tick);
  visitor(value.deadline);
  visitor(value.phase);
  visitor(value.active_claim_id);
  visitor(value.closing_source_ref);
  visitor(value.last_reconcile_source_ref);
  visitor(value.previous_episode_ref);
  visitor(value.deadline_closed_at_tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StateNeedClaim &value,
                  Visitor &&visitor) {
  visitor(value.claim_id);
  visitor(value.need_ref);
  visitor(value.assignee);
  visitor(value.executor_actor_id);
  visitor(value.task_contract_id);
  visitor(value.accepted_source_revision);
  visitor(value.accepted_tick);
  visitor(value.claim_until_tick);
  visitor(value.phase);
  visitor(value.release_reason);
  visitor(value.acceptance_ref);
  visitor(value.deadline_closed_at_tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StateNeedDeadline &value,
                  Visitor &&visitor) {
  visitor(value.tick);
  visitor(value.inclusive);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StateNeedDeadlineClosure &value,
                  Visitor &&visitor) {
  visitor(value.need_id);
  visitor(value.episode);
  visitor(value.claim_id);
  visitor(value.deadline_gate_ref);
  visitor(value.deadline_closed_at_tick);
  visitor(value.closing_source_ref);
  visitor(value.rule_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StateNeedDeadlineGateRef &value,
                  Visitor &&visitor) {
  visitor(value.need_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StateNeedDeadlineOpening &value,
                  Visitor &&visitor) {
  visitor(value.need_ref);
  visitor(value.claim);
  visitor(value.rule_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StateNeedOpeningClaim &value,
                  Visitor &&visitor) {
  visitor(value.claim_id);
  visitor(value.task_contract_id);
  visitor(value.acceptance_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StateNeedPeriodOpening &value,
                  Visitor &&visitor) {
  visitor(value.period_id);
  visitor(value.opened_at);
  visitor(value.through_tick);
  visitor(value.opening_needs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StateNeedRef &value,
                  Visitor &&visitor) {
  visitor(value.need_id);
  visitor(value.episode);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StateNeedState &value,
                  Visitor &&visitor) {
  visitor(value.needs);
  visitor(value.claims);
  visitor(value.deadline_closures);
  visitor(value.exact_period_openings);
  visitor(value.period_openings);
  visitor(value.current_episodes);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::WorldAffair &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.semantic_key);
  visitor(value.episode);
  visitor(value.principal);
  visitor(value.cause_fact_ids);
  visitor(value.phase);
  visitor(value.opened_tick);
  visitor(value.closed_tick);
  visitor(value.need_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StateNeedPolicy &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.owner_rule);
  visitor(value.task_template);
  visitor(value.disclosure);
  visitor(value.fee_reward_terms);
  visitor(value.cancellation_rule);
  visitor(value.goal_kind);
  visitor(value.claim_window);
  visitor(value.deadline);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StateNeedCatalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.policies);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ObligationNeedPolicy &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.obligation_id);
  visitor(value.organization_id);
  visitor(value.semantic_key);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::AffairsCatalog &value,
                  Visitor &&visitor) {
  visitor(value.policies);
  visitor(value.obligation_needs);
  visitor(value.obligation_policies);
  visitor(value.fiscal_need_policies);
  visitor(value.cohort_wage_need_policies);
  visitor(value.state_need_policies);
}
