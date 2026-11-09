// 职责：集中家庭与接续中的出生、照护与抚养访问；仅借用原字段，不改变顺序或失败行为。
// 由中央登记头在 source_owned_fields
// 命名空间内包含；依赖与具名助手已先行可见。

template <class Visitor>
inline void visit(const ::hanmo::resident::BirthArrangementSource &value,
                  Visitor &&visitor) {
  visitor(value.arrangement_id);
  visitor(value.speaker_actor_id);
  visitor(value.listener_actor_id);
  visitor(value.dialogue_id);
  visitor(value.node_id);
  visitor(value.node_kind);
  visitor(value.session_id);
  visitor(value.scenario_id);
  visitor(value.world_id);
  visitor(value.context);
  visitor(value.terms);
  visitor(value.terms_version);
  visitor(value.tick);
  visitor(value.revision);
  visitor(value.speaker_birth_tick);
  visitor(value.listener_birth_tick);
  visitor(value.field_order);
  visitor(value.age_sources);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BirthCancellationResult &value,
                  Visitor &&visitor) {
  visitor(value.result_id);
  visitor(value.arrangement_id);
  visitor(value.terms_version);
  visitor(value.reason);
  visitor(value.death_ref);
  visitor(value.effective_at);
  visitor(value.original_responsibility_refs);
  visitor(value.closed_or_pending_disposition);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BirthCareDisposition &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.care_assignment_ref);
  visitor(value.care_need_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BirthCareTerms &value,
                  Visitor &&visitor) {
  visitor(value.gestating_actor_id);
  visitor(value.caregiver_actor_id);
  visitor(value.payer_actor_id);
  visitor(value.place_id);
  visitor(value.source_inventory_id);
  visitor(value.food_item_id);
  visitor(value.child_prototype_id);
  visitor(value.birth_duration_ticks);
  visitor(value.care_period_ticks);
  visitor(value.food_per_period);
  visitor(value.prepaid_care_periods);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BirthClosure &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.accepted_id);
  visitor(value.actor_id);
  visitor(value.reason);
  visitor(value.source_fact_id);
  visitor(value.tick);
  visitor(value.revision);
  visitor(value.released_food);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BirthDueExecution &value,
                  Visitor &&visitor) {
  visitor(value.source);
  visitor(value.result);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BirthFactRef &value,
                  Visitor &&visitor) {
  visitor(value.birth_fact_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BirthInnateResult &value,
                  Visitor &&visitor) {
  visitor(value.family_id);
  visitor(value.policy_version);
  visitor(value.first);
  visitor(value.second);
  visitor(value.branch);
  visitor(value.draw);
  visitor(value.level);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BirthProposal &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.source);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BirthReply &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.proposal_id);
  visitor(value.actor_id);
  visitor(value.decision);
  visitor(value.tick);
  visitor(value.revision);
  visitor(value.confirmation);
  visitor(value.reserved_food);
  visitor(value.due_tick);
  visitor(value.field_order);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::BirthResponsibilityDisposition &value,
      Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.original_refs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BornActorSource &value,
                  Visitor &&visitor) {
  visitor(value.source_identity);
  visitor(value.actor_id);
  visitor(value.birth_fact_id);
  visitor(value.creation_fact_id);
  visitor(value.prototype_id);
  visitor(value.before_fact_id);
  visitor(value.biological_parent_ids);
  visitor(value.birth_tick);
  visitor(value.observed_tick);
  visitor(value.adult);
  visitor(value.alive);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BornBirthDueResult &value,
                  Visitor &&visitor) {
  visitor(value.birth_fact_ref);
  visitor(value.child_actor_id);
  visitor(value.location_result_ref);
  visitor(value.transport_binding_ref);
  visitor(value.care);
  visitor(value.population_transition_ref);
  visitor(value.provision_binding_ref);
  visitor(value.carrier_load_result_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ChildCareReceipt &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.accepted_id);
  visitor(value.child_actor_id);
  visitor(value.caregiver_actor_id);
  visitor(value.action_id);
  visitor(value.place_id);
  visitor(value.outcome);
  visitor(value.code);
  visitor(value.interruption_actor_id);
  visitor(value.interruption_fact_id);
  visitor(value.started_tick);
  visitor(value.tick);
  visitor(value.revision);
  visitor(value.period_index);
  visitor(value.food_consumed);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ChildCareReliefAgreement &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.source);
  visitor(value.reserved_food);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ChildCareReliefClosure &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.accepted_id);
  visitor(value.actor_id);
  visitor(value.reason);
  visitor(value.source_fact_id);
  visitor(value.tick);
  visitor(value.revision);
  visitor(value.released_food);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ChildCareReliefReceipt &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.accepted_id);
  visitor(value.action_id);
  visitor(value.provider_actor_id);
  visitor(value.child_actor_id);
  visitor(value.place_id);
  visitor(value.outcome);
  visitor(value.code);
  visitor(value.interruption_fact_id);
  visitor(value.started_tick);
  visitor(value.tick);
  visitor(value.revision);
  visitor(value.period_index);
  visitor(value.food_consumed);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ChildCareReliefSource &value,
                  Visitor &&visitor) {
  visitor(value.offer_id);
  visitor(value.provider_actor_id);
  visitor(value.parent_actor_id);
  visitor(value.child_actor_id);
  visitor(value.birth_fact_id);
  visitor(value.original_accepted_id);
  visitor(value.original_stop_fact_id);
  visitor(value.original_stop_kind);
  visitor(value.dialogue_id);
  visitor(value.node_id);
  visitor(value.node_kind);
  visitor(value.session_id);
  visitor(value.scenario_id);
  visitor(value.world_id);
  visitor(value.context);
  visitor(value.terms);
  visitor(value.terms_version);
  visitor(value.tick);
  visitor(value.revision);
  visitor(value.provider_birth_tick);
  visitor(value.parent_birth_tick);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ChildCareReliefStart &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.accepted_id);
  visitor(value.action_id);
  visitor(value.provider_actor_id);
  visitor(value.child_actor_id);
  visitor(value.place_id);
  visitor(value.tick);
  visitor(value.revision);
  visitor(value.duration);
  visitor(value.period_index);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ChildCareReliefState &value,
                  Visitor &&visitor) {
  visitor(value.agreements);
  visitor(value.starts);
  visitor(value.care);
  visitor(value.closures);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ChildCareReliefTerms &value,
                  Visitor &&visitor) {
  visitor(value.place_id);
  visitor(value.source_inventory_id);
  visitor(value.food_item_id);
  visitor(value.period_ticks);
  visitor(value.food_per_period);
  visitor(value.prepaid_periods);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ChildCareStart &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.accepted_id);
  visitor(value.child_actor_id);
  visitor(value.caregiver_actor_id);
  visitor(value.action_id);
  visitor(value.place_id);
  visitor(value.tick);
  visitor(value.revision);
  visitor(value.duration);
  visitor(value.period_index);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ChildMaturationRecord &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.actor_id);
  visitor(value.birth_fact_id);
  visitor(value.tick);
  visitor(value.revision);
  visitor(value.availability_enabled);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyBirthRecord &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.accepted_id);
  visitor(value.actor_id);
  visitor(value.prototype_id);
  visitor(value.display_name);
  visitor(value.place_id);
  visitor(value.tick);
  visitor(value.revision);
  visitor(value.rng_stream);
  visitor(value.rng_backend);
  visitor(value.rng_seed);
  visitor(value.rng_before);
  visitor(value.rng_after);
  visitor(value.biological_parent_ids);
  visitor(value.innate);
  visitor(value.field_order);
  visitor(value.location_result_ref);
  visitor(value.allocation_source_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyBirthState &value,
                  Visitor &&visitor) {
  visitor(value.proposals);
  visitor(value.replies);
  visitor(value.closures);
  visitor(value.births);
  visitor(value.care);
  visitor(value.care_starts);
  visitor(value.maturations);
  visitor(value.field_order);
  visitor(value.birth_due_executions);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ScheduledBirthSource &value,
                  Visitor &&visitor) {
  visitor(value.arrangement_id);
  visitor(value.terms_version);
  visitor(value.due_tick);
  visitor(value.occurrence);
  visitor(value.source_ref);
  visitor(value.selected_policy_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ChildCareReliefDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.provider_actor_id);
  visitor(value.dialogue_id);
  visitor(value.node_id);
  visitor(value.terms_version);
  visitor(value.terms);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ChildCareReliefCatalog &value,
                  Visitor &&visitor) {
  visitor(value.enabled);
  visitor(value.offers);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BirthEndowmentPolicy &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.unweighted);
  visitor(value.single);
  visitor(value.different);
  visitor(value.same);
  visitor(value.families);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BirthArrangementDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.speaker_actor_id);
  visitor(value.listener_actor_id);
  visitor(value.dialogue_id);
  visitor(value.node_id);
  visitor(value.terms_version);
  visitor(value.terms);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BirthArrangementCatalog &value,
                  Visitor &&visitor) {
  visitor(value.enabled);
  visitor(value.arrangements);
  visitor(value.selected_version);
  visitor(value.due_policy_ref);
  visitor(value.dependent_provision_profile_ref);
}

// native/world/src/resident/domains/family/care_arrangement.hpp:23; actual
// owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::AcceptedCaregiver &value,
                  Visitor &&visitor) {
  visitor(value.accepted_id);
  visitor(value.requirement_id);
  visitor(value.subject_actor_id);
  visitor(value.provider);
  visitor(value.requirement_transition_ref);
  visitor(value.terms);
  visitor(value.accepted_at);
  visitor(value.acceptance_condition_head);
}

// native/world/src/resident/domains/family/care_requirement_contract.hpp:24;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::BirthDueCareOrigin &value,
                  Visitor &&visitor) {
  visitor(value.birth_ref);
  visitor(value.scheduled_source_ref);
}

// native/world/src/resident/domains/family/care_requirement_contract.hpp:35;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CareAcceptedWindowEnded &value,
                  Visitor &&visitor) {
  visitor(value.accepted_care_terms_ref);
  visitor(value.until_tick);
}

// native/world/src/resident/domains/family/care_requirement_contract.hpp:34;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CareActualSeparation &value,
                  Visitor &&visitor) {
  visitor(value.actual_location_or_binding_transition);
}

// native/world/src/resident/domains/family/care_facility_use.hpp:7; actual
// owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CareArrangementRef &value,
                  Visitor &&visitor) {
  visitor(value.accepted_arrangement_id);
}

// native/world/src/resident/domains/family/care_arrangement.hpp:9; actual
// owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CareArrangementTerms &value,
                  Visitor &&visitor) {
  visitor(value.handover_half_days);
  visitor(value.from_tick);
  visitor(value.until_tick);
  visitor(value.allowed_place_ids);
  visitor(value.allowed_dependency_refs);
  visitor(value.provision_arrangement_refs);
  visitor(value.fee_arrangement_refs);
  visitor(value.guardianship_permission_refs);
}

// native/world/src/resident/domains/family/care_requirement_contract.hpp:37;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void
visit(const ::hanmo::resident::CareAuthorityOrArrangementEnded &value,
      Visitor &&visitor) {
  visitor(value.actual_ended_result);
}

// native/world/src/resident/domains/family/care_coverage.hpp:7; actual owning
// declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CareBinding &value,
                  Visitor &&visitor) {
  visitor(value.binding_id);
  visitor(value.child_actor_id);
  visitor(value.carer_actor_id);
  visitor(value.terms_ref);
  visitor(value.accepted_ref);
  visitor(value.started_tick);
  visitor(value.until_tick);
  visitor(value.care_activity_ref);
  visitor(value.placement_ref);
  visitor(value.phase);
}

// native/world/src/resident/domains/family/care_period.hpp:6; actual owning
// declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CareCalendarWindow &value,
                  Visitor &&visitor) {
  visitor(value.from_tick);
  visitor(value.to_tick);
}

// native/world/src/resident/domains/family/care_coverage.hpp:17; actual owning
// declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CareCoverageLost &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.requirement_id);
  visitor(value.old_binding_ref);
  visitor(value.cause_ref);
  visitor(value.effective_tick);
}

// native/world/src/resident/domains/family/care_requirement_contract.hpp:39;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CareCoverageLostEvent &value,
                  Visitor &&visitor) {
  visitor(value.requirement_id);
  visitor(value.prior_source_episode);
  visitor(value.next_source_episode);
  visitor(value.old_binding_ref);
  visitor(value.lost_fact_id);
  visitor(value.cause);
}

// native/world/src/resident/domains/family/care_coverage.hpp:13; actual owning
// declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CareCoverageSatisfied &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.requirement_id);
  visitor(value.source_episode);
  visitor(value.binding_ref);
  visitor(value.handover_result_ref);
  visitor(value.effective_tick);
}

// native/world/src/resident/domains/family/care_facility_use.hpp:9; actual
// owning declaration, no codec validation.
template <class Visitor>
inline void
visit(const ::hanmo::resident::CareFacilityBorrowedPhysicalHolder &value,
      Visitor &&visitor) {
  visitor(value.physical_holder_source_ref);
  visitor(value.physical_holder_source_schema);
  visitor(value.placement_claim_id);
  visitor(value.authorized_use_source_ref);
  visitor(value.authorized_use_source_schema);
}

// native/world/src/resident/domains/family/care_facility_use.hpp:8; actual
// owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CareFacilityOwnClaim &value,
                  Visitor &&visitor) {
  visitor(value.original_claim_id);
  visitor(value.holder);
}

// native/world/src/resident/domains/family/care_facility_use.hpp:15; actual
// owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CareFacilityUseBinding &value,
                  Visitor &&visitor) {
  visitor(value.binding_id);
  visitor(value.owner_business_ref);
  visitor(value.accepted_terms_ref);
  visitor(value.usage_rule_ref);
  visitor(value.slot_ref);
  visitor(value.mode);
  visitor(value.created_source_ref);
  visitor(value.created_source_schema);
  visitor(value.created_at);
  visitor(value.field_order);
}

// native/world/src/resident/domains/family/care_family_activity.hpp:15; actual
// owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CareFamilyActivity &value,
                  Visitor &&visitor) {
  visitor(value.activity_id);
  visitor(value.accepted_care_terms_ref);
  visitor(value.child_actor_id);
  visitor(value.carer_actor_id);
  visitor(value.actual_handover_execution_ref);
  visitor(value.actual_start_work_period_ref);
  visitor(value.actual_placement_result_ref);
  visitor(value.started_at);
  visitor(value.compatibility_rule_ref);
  visitor(value.adopted_participation_refs);
  visitor(value.ended_by);
}

// native/world/src/resident/domains/family/care_family_activity.hpp:12; actual
// owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CareFamilyActivityEnd &value,
                  Visitor &&visitor) {
  visitor(value.end_id);
  visitor(value.ended_at);
  visitor(value.cause);
}

// native/world/src/resident/domains/family/care_handover_execution.hpp:11;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void
visit(const ::hanmo::resident::CareHandoverAcceptedObligation &value,
      Visitor &&visitor) {
  visitor(value.original_obligation_ref);
  visitor(value.original_obligation_source_schema);
  visitor(value.accepted_rule_ref);
}

// native/world/src/resident/domains/family/care_handover_execution.hpp:7;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CareHandoverActorCommand &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.command_binding);
  visitor(value.route_basis);
  visitor(value.accepted_terms_ref);
  visitor(value.authority_source_refs);
}

// native/world/src/resident/domains/family/care_handover_execution.hpp:20;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void
visit(const ::hanmo::resident::CareHandoverAuthorityOrTermsChanged &value,
      Visitor &&visitor) {
  visitor(value.source_ref);
  visitor(value.source_schema);
}

// native/world/src/resident/domains/family/care_requirement_contract.hpp:27;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CareHandoverCompletedEvent &value,
                  Visitor &&visitor) {
  visitor(value.handover_id);
  visitor(value.requirement_id);
  visitor(value.source_episode);
  visitor(value.binding_id);
  visitor(value.satisfied_fact_id);
  visitor(value.completion_work_period_ref);
  visitor(value.actual_participation_refs);
  visitor(value.placement_result_ref);
  visitor(value.fee_result_refs);
}

// native/world/src/resident/domains/family/care_handover_execution.hpp:15;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CareHandoverConditionChanged &value,
                  Visitor &&visitor) {
  visitor(value.condition_receipt_ref);
}

// native/world/src/resident/domains/family/care_handover_execution.hpp:16;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CareHandoverDeathChanged &value,
                  Visitor &&visitor) {
  visitor(value.death_ref);
}

// native/world/src/resident/domains/family/care_handover_execution.hpp:23;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CareHandoverDependencyChanged &value,
                  Visitor &&visitor) {
  visitor(value.change);
  visitor(value.continuation_rule_ref);
}

// native/world/src/resident/domains/family/care_handover_execution.hpp:41;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CareHandoverExecution &value,
                  Visitor &&visitor) {
  visitor(value.handover_id);
  visitor(value.requirement_id);
  visitor(value.accepted_care_terms_ref);
  visitor(value.original_action_ref);
  visitor(value.execution_binding);
  visitor(value.handover_rule_ref);
  visitor(value.actual_placement_result_refs);
  visitor(value.completion_event_ref);
  visitor(value.terminal_source_ref);
  visitor(value.work_period_sources);
  visitor(value.handover_execution_boundaries);
}

// native/world/src/resident/domains/family/care_handover_execution.hpp:28;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void
visit(const ::hanmo::resident::CareHandoverExecutionBoundary1 &value,
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

// native/world/src/resident/domains/family/care_handover_execution.hpp:19;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CareHandoverFacilityChanged &value,
                  Visitor &&visitor) {
  visitor(value.source_ref);
  visitor(value.source_schema);
}

// native/world/src/resident/domains/family/care_handover_execution.hpp:18;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CareHandoverOccupationChanged &value,
                  Visitor &&visitor) {
  visitor(value.source_ref);
  visitor(value.source_schema);
}

// native/world/src/resident/domains/family/care_handover_execution.hpp:24;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CareHandoverOriginalOutcome &value,
                  Visitor &&visitor) {
  visitor(value.outcome_source_ref);
  visitor(value.outcome_source_schema);
}

// native/world/src/resident/domains/family/care_handover_execution.hpp:17;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CareHandoverPositionChanged &value,
                  Visitor &&visitor) {
  visitor(value.source_ref);
  visitor(value.source_schema);
}

// native/world/src/resident/domains/family/care_handover_execution.hpp:36;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CareHandoverWorkPeriod2 &value,
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

// native/world/src/resident/domains/family/care_requirement_contract.hpp:32;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CareHolderDeath &value,
                  Visitor &&visitor) {
  visitor(value.death_ref);
}

// native/world/src/resident/domains/family/care_requirement_contract.hpp:42;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CareLawfulGoalWithdrawal &value,
                  Visitor &&visitor) {
  visitor(value.actual_withdrawal);
}

// native/world/src/resident/domains/family/care_requirement_contract.hpp:41;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CareNoLongerDependent &value,
                  Visitor &&visitor) {
  visitor(value.actual_applicability_result);
}

// native/world/src/resident/domains/family/care_requirement_contract.hpp:36;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CareParticipationConflict &value,
                  Visitor &&visitor) {
  visitor(value.actual_occupation_change);
  visitor(value.compatibility_rule_ref);
}

// native/world/src/resident/domains/family/care_period.hpp:10; actual owning
// declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CarePeriod &value,
                  Visitor &&visitor) {
  visitor(value.child);
  visitor(value.caregiver);
  visitor(value.place);
  visitor(value.period);
  visitor(value.activity_ref);
  visitor(value.food_support_ref);
  visitor(value.outcome);
}

// native/world/src/resident/domains/family/care_period.hpp:23; actual owning
// declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CarePeriodSource &value,
                  Visitor &&visitor) {
  visitor(value.care_period_ref);
  visitor(value.binding_ref);
  visitor(value.calendar_period_ref);
  visitor(value.participation);
  visitor(value.actual_complete_source_ref);
  visitor(value.provision_result_refs);
  visitor(value.condition_result_refs);
}

// native/world/src/resident/domains/family/care_family_activity.hpp:8; actual
// owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CarePersonalStop &value,
                  Visitor &&visitor) {
  visitor(value.actual_stop_result);
  visitor(value.actual_command);
}

// native/world/src/resident/domains/family/care_arrangement.hpp:19; actual
// owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CareProviderIdentity &value,
                  Visitor &&visitor) {
  visitor(value.provider);
  visitor(value.carer_actor_id);
  visitor(value.institutional_assignment_ref);
}

// native/world/src/resident/domains/family/care_requirement_contract.hpp:33;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CareRequiredCapacityLost &value,
                  Visitor &&visitor) {
  visitor(value.capacity_transition_ref);
  visitor(value.condition_receipt_ref);
  visitor(value.purpose_ref);
}

// native/world/src/resident/domains/family/care_requirement_contract.hpp:10;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CareRequirement &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.subject_actor_id);
  visitor(value.revision);
  visitor(value.source_episode);
  visitor(value.created_tick);
  visitor(value.current_transition_ref);
  visitor(value.rule_version);
  visitor(value.care_basis);
  visitor(value.accepted_care_terms_ref);
  visitor(value.current_care_binding_ref);
  visitor(value.state);
}

// native/world/src/resident/domains/family/care_requirement_contract.hpp:44;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CareRequirementClosedEvent &value,
                  Visitor &&visitor) {
  visitor(value.requirement_id);
  visitor(value.source_episode);
  visitor(value.final_state);
  visitor(value.cause);
}

// native/world/src/resident/domains/family/care_requirement_contract.hpp:26;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CareRequirementCreatedEvent &value,
                  Visitor &&visitor) {
  visitor(value.requirement_id);
  visitor(value.source_episode);
  visitor(value.origin);
}

// native/world/src/resident/domains/family/care_requirement_contract.hpp:51;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CareRequirementRule &value,
                  Visitor &&visitor) {
  visitor(value.rule_ref);
  visitor(value.service_kind);
  visitor(value.dependency_rule_ref);
  visitor(value.cancellation_rule_ref);
  visitor(value.disclosure_rule_ref);
  visitor(value.state_need_policy_ref);
}

// native/world/src/resident/domains/family/care_requirement_contract.hpp:17;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CareRequirementTransition &value,
                  Visitor &&visitor) {
  visitor(value.transition_id);
  visitor(value.requirement_id);
  visitor(value.revision_before);
  visitor(value.revision_after);
  visitor(value.source_episode_before);
  visitor(value.source_episode_after);
  visitor(value.kind);
  visitor(value.previous_transition_ref);
  visitor(value.cause_ref);
}

// native/world/src/resident/domains/family/care_requirement_contract.hpp:40;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CareSubjectDeath &value,
                  Visitor &&visitor) {
  visitor(value.death_ref);
}

// native/world/src/resident/domains/family/care_requirement_contract.hpp:46;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CareTemporalEvent &value,
                  Visitor &&visitor) {
  visitor(value.event_id);
  visitor(value.original_transition_ref);
  visitor(value.occurred_at);
  visitor(value.rule_ref);
  visitor(value.body);
}

// native/world/src/resident/domains/family/care_period.hpp:18; actual owning
// declaration, no codec validation.
template <class Visitor>
inline void
visit(const ::hanmo::resident::CompatibleFamilyActivityParticipation &value,
      Visitor &&visitor) {
  visitor(value.family_activity_ref);
  visitor(value.compatibility_rule_ref);
  visitor(value.actual_participation_ref);
}

// native/world/src/resident/domains/family/care_requirement_contract.hpp:25;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void
visit(const ::hanmo::resident::DependentCareApplicabilityResultRef &value,
      Visitor &&visitor) {
  visitor(value.original_result);
}

// native/world/src/resident/domains/family/care_requirement_contract.hpp:8;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::DependentCareRuleRef &value,
                  Visitor &&visitor) {
  visitor(value.rule_ref);
}

// native/world/src/resident/domains/family/dependent_provision_profile.hpp:6;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::DependentProvisionProfile &value,
                  Visitor &&visitor) {
  visitor(value.profile_id);
  visitor(value.version);
  visitor(value.age_stage);
  visitor(value.food_requirement_profile_ref);
  visitor(value.care_requirement_profile_ref);
  visitor(value.transport_cost_policy_ref);
  visitor(value.infant_weight_units);
  visitor(value.weight_unit_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DependentProvisionCatalog &value,
                  Visitor &&visitor) {
  visitor(value.identity);
  visitor(value.birth_profile_id);
  visitor(value.profiles);
}

// native/world/src/resident/domains/family/care_period.hpp:15; actual owning
// declaration, no codec validation.
template <class Visitor>
inline void
visit(const ::hanmo::resident::OriginalCareActionParticipation &value,
      Visitor &&visitor) {
  visitor(value.original_action_ref);
  visitor(value.provided_period_ref);
}
