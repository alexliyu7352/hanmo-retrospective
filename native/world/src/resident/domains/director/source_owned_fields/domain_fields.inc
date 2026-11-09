// 职责：集中角色决策中的有限拥有字段访问；仅借用原字段，不改变顺序或失败行为。
// 由中央登记头在 source_owned_fields
// 命名空间内包含；依赖与具名助手已先行可见。

template <class Visitor>
inline void visit(const ::hanmo::resident::ActorDirectorAdmission &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.admission_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ActorExecutionBinding &value,
                  Visitor &&visitor) {
  visitor(value.original_business_ref);
  visitor(value.execution_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::AwaitOfficerBinding &value,
                  Visitor &&visitor) {
  visitor(value.approved_goal_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CohortDirectorAdmission &value,
                  Visitor &&visitor) {
  visitor(value.accepted_household_policy_ref);
  visitor(value.source_revision);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CohortHouseholdRoutineBinding &value,
                  Visitor &&visitor) {
  visitor(value.need_source_ref);
  visitor(value.execution_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CohortPurchaseBinding &value,
                  Visitor &&visitor) {
  visitor(value.purchase_plan_ref);
  visitor(value.execution_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DirectorAdoptionResult2 &value,
                  Visitor &&visitor) {
  visitor(value.subject);
  visitor(value.registration_epoch);
  visitor(value.decision_seq);
  visitor(value.plan_id);
  visitor(value.typed_proposal_kind);
  visitor(value.source_policy_ref);
  visitor(value.status);
  visitor(value.actual_outcome_refs);
  visitor(value.reviewed_tick);
  visitor(value.next_review_tick);
  visitor(value.accepted_command);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DirectorRegistration &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.policy_id);
  visitor(value.admission_id);
  visitor(value.reason);
  visitor(value.epoch);
  visitor(value.entered_tick);
  visitor(value.changed_tick);
  visitor(value.lifecycle);
  visitor(value.obligations_only);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DirectorRegistration2 &value,
                  Visitor &&visitor) {
  visitor(value.subject);
  visitor(value.policy_id);
  visitor(value.admission_id);
  visitor(value.reason);
  visitor(value.epoch);
  visitor(value.entered_tick);
  visitor(value.changed_tick);
  visitor(value.lifecycle);
  visitor(value.obligations_only);
  visitor(value.admission_source);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DirectorState &value,
                  Visitor &&visitor) {
  visitor(value.subject2);
  visitor(value.schedules);
  visitor(value.plans);
  visitor(value.fairness_cursor);
  visitor(value.last_boundary_tick);
  visitor(value.regional_life);
  visitor(value.named_membership);
  visitor(value.dynamic_subjects);
  visitor(value.registrations);
  visitor(value.archived_plans);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DirectorSubjects2State &value,
                  Visitor &&visitor) {
  visitor(value.command_binding_version);
  visitor(value.wake_sources);
  visitor(value.wake_cache_ready);
  visitor(value.registrations);
  visitor(value.subject_plan_bindings);
  visitor(value.subject_continuations);
  visitor(value.adoption_results);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DirectorWakeRecord2 &value,
                  Visitor &&visitor) {
  visitor(value.wake);
  visitor(value.observed_at);
  visitor(value.consumed_decision_seq);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FinitePlan &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.template_id);
  visitor(value.goal_ref);
  visitor(value.subject);
  visitor(value.step_index);
  visitor(value.next_review_tick);
  visitor(value.phase);
  visitor(value.retry_reason);
  visitor(value.current_action_id);
  visitor(value.org_activity_id);
  visitor(value.commitment_ids);
  visitor(value.known_report_ids);
  visitor(value.livelihood);
  visitor(value.pending_life_action_id);
  visitor(value.last_life_fact_id);
  visitor(value.policy_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::InstitutionalExecutionBinding &value,
                  Visitor &&visitor) {
  visitor(value.approved_responsibility_ref);
  visitor(value.execution_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OrganizationDirectorAdmission &value,
                  Visitor &&visitor) {
  visitor(value.approved_responsibility_ref);
  visitor(value.source_revision);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ReviewSchedule &value,
                  Visitor &&visitor) {
  visitor(value.subject);
  visitor(value.next_tick);
  visitor(value.decision_seq);
  visitor(value.waiting_since);
  visitor(value.last_decision_tick);
  visitor(value.wake_reasons);
  visitor(value.last_outcome);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SubjectContinuation &value,
                  Visitor &&visitor) {
  visitor(value.prior_subject);
  visitor(value.current_subject);
  visitor(value.original_continuation_ref);
  visitor(value.old_registration_epoch);
  visitor(value.new_registration_epoch);
  visitor(value.plan_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SubjectPlanBinding &value,
                  Visitor &&visitor) {
  visitor(value.plan_id);
  visitor(value.subject);
  visitor(value.registration_epoch);
  visitor(value.binding);
  visitor(value.policy_source_ref);
  visitor(value.last_source_revision);
  visitor(value.continuation_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SubjectWake2 &value,
                  Visitor &&visitor) {
  visitor(value.subject);
  visitor(value.kind);
  visitor(value.actual_source);
  visitor(value.source_schema);
  visitor(value.source_revision);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NamedRecruitmentPost &value,
                  Visitor &&visitor) {
  visitor(value.office_id);
  visitor(value.terms_id);
  visitor(value.candidate_actor_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DirectorPolicy &value,
                  Visitor &&visitor) {
  visitor(value.organization_id);
  visitor(value.templates);
  visitor(value.request_arrear_after_ticks);
  visitor(value.decline_work_after_ticks);
  visitor(value.leave_after_ticks);
  visitor(value.report_source_keys);
  visitor(value.private_livelihood);
  visitor(value.named_membership_policy_id);
  visitor(value.recruitment_posts);
  visitor(value.political_career);
  visitor(value.diplomatic_response);
  visitor(value.diplomatic_delegation_policy_id);
  visitor(value.supply_renewal_intent_id);
  visitor(value.shop_registry_intent_id);
  visitor(value.governance_intent_id);
  visitor(value.governance_agency_intent_id);
  visitor(value.office_resignation_response_intent_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DirectorCatalog &value,
                  Visitor &&visitor) {
  visitor(value.governance_intents);
  visitor(value.governance_agency_intents);
  visitor(value.office_resignation_responses);
  visitor(value.subordinate_appointments);
  visitor(value.shop_registry_intents);
  visitor(value.personal_appointment_attendance);
  visitor(value.visit_reception);
  visitor(value.supply_renewals);
  visitor(value.military_appointments);
  visitor(value.field_appointments);
  visitor(value.review_interval_ticks);
  visitor(value.retry_delay_ticks);
  visitor(value.decision_budget);
  visitor(value.candidates_per_subject);
  visitor(value.subjects);
  visitor(value.regional_life);
  visitor(value.named_membership);
  visitor(value.politics);
  visitor(value.diplomatic_response);
  visitor(value.diplomatic_delegation);
  visitor(value.treaty_passage);
  visitor(value.exclude_selected_player);
  visitor(value.dynamic_subjects);
  visitor(value.subjects2);
  visitor(value.command_acceptance_bindings);
  visitor(value.policies);
  visitor(value.admissions);
  visitor(value.authored_task_continuation);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DirectorAdmission &value,
                  Visitor &&visitor) {
  visitor(value.policy_id);
  visitor(value.organization_id);
  visitor(value.kind);
  visitor(value.minimum_age_years);
  visitor(value.excluded_actor_ids);
}

// 原提案转移值沿同一字段访问预留深复制；不引入第二份状态或默认费用。
template <class Visitor>
inline void visit(const ::hanmo::resident::DirectorTransition &value,
                  Visitor &&visitor) {
  visitor(value.expected_revision);
  visitor(value.decision_seq);
  visitor(value.next_review_tick);
  visitor(value.next_step_index);
  visitor(value.template_id);
  visitor(value.goal_ref);
  visitor(value.phase);
  visitor(value.current_action_id);
  visitor(value.known_report_ids);
  visitor(value.commitment_ids);
  visitor(value.wait_reason);
  visitor(value.livelihood);
  visitor(value.pending_life_action_id);
  visitor(value.last_life_fact_id);
  visitor(value.registration_epoch);
}
