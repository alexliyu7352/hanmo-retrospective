// Family 出生/照护新增原值的完整拥有字段访问。
// 中央在 source_owned_fields
// 命名空间内包含；这里只借真实成员，不复制或释放原源。

// 完整枚举 MaturityResult 的实际拥有值；来源存在性仍归原业务 validator。
template <class Visitor>
inline void visit(const ::hanmo::resident::MaturityResult &value,
                  Visitor &&visitor) {
  visitor(value.result_id);
  visitor(value.actor_id);
  visitor(value.age_basis_ref);
  visitor(value.calendar_profile_ref);
  visitor(value.calendar_boundary_ordinal);
  visitor(value.effective_tick);
  visitor(value.original_condition_or_life_boundary_ref);
  visitor(value.previous_stage);
  visitor(value.new_stage);
}

// 完整枚举 CareEmptyBodyScope 的实际拥有值；来源存在性仍归原业务 validator。
template <class Visitor>
inline void visit(const ::hanmo::resident::CareEmptyBodyScope &value,
                  Visitor &&visitor) {
  (void)value;
  (void)visitor;
}

// 完整枚举 CareNamedBodyScope 的实际拥有值；来源存在性仍归原业务 validator。
template <class Visitor>
inline void visit(const ::hanmo::resident::CareNamedBodyScope &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.character_origin_actor_key);
  visitor(value.condition_basis);
  visitor(value.death_ref);
}

// 完整枚举 CareResidenceBodyScope 的实际拥有值；来源存在性仍归原业务
// validator。
template <class Visitor>
inline void visit(const ::hanmo::resident::CareResidenceBodyScope &value,
                  Visitor &&visitor) {
  visitor(value.agreement_id);
  visitor(value.intake_line_id);
  visitor(value.actual_scope_source_ref);
  visitor(value.actual_scope_schema);
  visitor(value.body_count);
  visitor(value.known_named_actor_ids);
  visitor(value.unresolved_loss_source_refs);
}

// 完整枚举 FacilityClearanceWorkPeriod2 的实际拥有值；来源存在性仍归原业务
// validator。
template <class Visitor>
inline void visit(const ::hanmo::resident::FacilityClearanceWorkPeriod2 &value,
                  Visitor &&visitor) {
  visitor(value.source_id);
  visitor(value.source_schema);
  visitor(value.execution_ref);
  visitor(value.work_period_ref);
  visitor(value.provided_at);
  visitor(value.worker_capability_inputs);
  visitor(value.worker_condition_receipt_refs);
  visitor(value.field_order);
}

// 完整枚举 FacilityClearanceExecutionBoundary1
// 的实际拥有值；来源存在性仍归原业务 validator。
template <class Visitor>
inline void
visit(const ::hanmo::resident::FacilityClearanceExecutionBoundary1 &value,
      Visitor &&visitor) {
  visitor(value.boundary_id);
  visitor(value.source_schema);
  visitor(value.execution_ref);
  visitor(value.boundary_ordinal);
  visitor(value.previous_boundary_ref);
  visitor(value.kind);
  visitor(value.occurred_at);
  visitor(value.transition_rule_ref);
  visitor(value.cause);
  visitor(value.worker_capability_inputs);
  visitor(value.actual_participation_source_refs);
  visitor(value.facility_use_binding_refs);
  visitor(value.field_order);
}

// 完整枚举 FacilityClearanceLocalProvided 的实际拥有值；来源存在性仍归原业务
// validator。
template <class Visitor>
inline void
visit(const ::hanmo::resident::FacilityClearanceLocalProvided &value,
      Visitor &&visitor) {
  visitor(value.period);
}

// 完整枚举 FacilityClearanceOriginalCarePeriod
// 的实际拥有值；来源存在性仍归原业务 validator。
template <class Visitor>
inline void
visit(const ::hanmo::resident::FacilityClearanceOriginalCarePeriod &value,
      Visitor &&visitor) {
  visitor(value.source_ref);
  visitor(value.source_schema);
}

// 完整枚举 FacilityClearanceWorkGate 的实际拥有值；来源存在性仍归原业务
// validator。
template <class Visitor>
inline void visit(const ::hanmo::resident::FacilityClearanceWorkGate &value,
                  Visitor &&visitor) {
  visitor(value.payload);
  visitor(value.field_order);
}

// 完整枚举 ClearanceCareHandoverObligation 的实际拥有值；来源存在性仍归原业务
// validator。
template <class Visitor>
inline void
visit(const ::hanmo::resident::ClearanceCareHandoverObligation &value,
      Visitor &&visitor) {
  visitor(value.accepted_care_terms_ref);
  visitor(value.care_handover_ref);
}

// 完整枚举 ClearanceMedicalObligation 的实际拥有值；来源存在性仍归原业务
// validator。
template <class Visitor>
inline void visit(const ::hanmo::resident::ClearanceMedicalObligation &value,
                  Visitor &&visitor) {
  visitor(value.case_id);
  visitor(value.referral_or_original_termination_ref);
  visitor(value.accepted_patient_or_care_authority_ref);
}

// 完整枚举 ClearanceResidenceObligation 的实际拥有值；来源存在性仍归原业务
// validator。
template <class Visitor>
inline void visit(const ::hanmo::resident::ClearanceResidenceObligation &value,
                  Visitor &&visitor) {
  visitor(value.agreement_id);
  visitor(value.intake_line_ref);
  visitor(value.original_end_or_change_intent_ref);
  visitor(value.accepted_residence_terms_ref);
  visitor(value.care_or_departure_authority_ref);
}

// 完整枚举 ClearanceLawfulDispositionObligation
// 的实际拥有值；来源存在性仍归原业务 validator。
template <class Visitor>
inline void
visit(const ::hanmo::resident::ClearanceLawfulDispositionObligation &value,
      Visitor &&visitor) {
  visitor(value.original_placement_source_ref);
  visitor(value.death_or_population_loss_refs);
  visitor(value.original_accepted_disposition_clause_ref);
  visitor(value.executor_or_care_authority_ref);
  visitor(value.authorized_site_ref);
}

// 完整枚举 ClearanceLocalWorkOrigin 的实际拥有值；来源存在性仍归原业务
// validator。
template <class Visitor>
inline void visit(const ::hanmo::resident::ClearanceLocalWorkOrigin &value,
                  Visitor &&visitor) {
  (void)value;
  (void)visitor;
}

// 完整枚举 ClearanceCareWorkOrigin 的实际拥有值；来源存在性仍归原业务
// validator。
template <class Visitor>
inline void visit(const ::hanmo::resident::ClearanceCareWorkOrigin &value,
                  Visitor &&visitor) {
  visitor(value.handover_id);
}

// 完整枚举 ClearanceSeaWorkOrigin 的实际拥有值；来源存在性仍归原业务
// validator。
template <class Visitor>
inline void visit(const ::hanmo::resident::ClearanceSeaWorkOrigin &value,
                  Visitor &&visitor) {
  visitor(value.action_ref);
  visitor(value.care_transport_binding_ref);
  visitor(value.terms_source_ref);
  visitor(value.terms_source_schema);
  visitor(value.action_elapsed_ref);
  visitor(value.result_source_ref);
  visitor(value.result_source_schema);
}

// 完整枚举 CareAggregatePhysicalPosition 的实际拥有值；来源存在性仍归原业务
// validator。
template <class Visitor>
inline void visit(const ::hanmo::resident::CareAggregatePhysicalPosition &value,
                  Visitor &&visitor) {
  visitor(value.placement_claim_id);
  visitor(value.affected_unit_refs);
  visitor(value.position_rule_ref);
}

// 完整枚举 CareExactPhysicalPosition 的实际拥有值；来源存在性仍归原业务
// validator。
template <class Visitor>
inline void visit(const ::hanmo::resident::CareExactPhysicalPosition &value,
                  Visitor &&visitor) {
  visitor(value.placement_claim_id);
  visitor(value.occupied_unit_refs);
  visitor(value.position_source_refs);
}
