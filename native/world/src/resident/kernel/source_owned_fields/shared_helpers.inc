// 职责：共享原有字段回调和具名助手；保持助手定义顺序及原有同名重载集合。
// 由中央登记头在 source_owned_fields
// 命名空间内包含，不单独声明有限访问入口。

// One finite field sequence supplies both payload visits and unevaluated
// default-member type selection. The latter never constructs a payload.
template <class Visitor>
inline void
visit_dynamic_military_actual_effect_read_member_selectors(Visitor &&visitor) {
  visitor([](const auto &value) -> decltype(auto) { return (value.source); });
  visitor(
      [](const auto &value) -> decltype(auto) { return (value.action_ref); });
  visitor(
      [](const auto &value) -> decltype(auto) { return (value.effective_at); });
  visitor([](const auto &value) -> decltype(auto) { return (value.use); });
}

template <class Value, class Visitor>
inline void
visit_dynamic_military_actual_effect_read_members(const Value &value,
                                                  Visitor &&visitor) {
  visit_dynamic_military_actual_effect_read_member_selectors(
      [&](const auto &member) { visitor(member(value)); });
}

template <class Value, class Visitor>
inline void visit_action_elapsed_state_payload(const Value &value,
                                               Visitor &&visitor) {
  visitor(value.action_ref);
  visitor(value.execution_source_ref);
  visitor(value.duration_definition_ref);
  visitor(value.started_at);
  visitor(value.required_units);
  visitor(value.credited_units);
  visitor(value.last_settled_at);
  visitor(value.timing);
  visitor(value.owner_work_remainder);
  visitor(value.owner_cost_state_ref);
}

template <class Value, class Visitor>
inline void visit_action_elapsed_store_payload(const Value &value,
                                               Visitor &&visitor) {
  visitor(value.entries);
  visitor(value.gate_results);
  visitor(value.terminal_bindings);
  visitor(value.fixed_service_transitions);
}

template <class Value, class Visitor>
inline void visit_action_elapsed_terminal_payload(const Value &value,
                                                  Visitor &&visitor) {
  visitor(value.action_ref);
  visitor(value.action_kind);
  visitor(value.actor_id);
  visitor(value.participant_actor_ids);
  visitor(value.original_execution_source_ref);
  visitor(value.timing_kind);
  visitor(value.started_source_ref);
  visitor(value.ended_source_ref);
  visitor(value.terminal_result_ref);
  visitor(value.last_closed_gate_result_ref);
  visitor(value.retained_gate_result_refs);
}

template <class Value, class Visitor>
inline void visit_actor_condition_head_members(const Value &value,
                                               Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.condition_revision);
  visitor(value.initial_condition_seed_ref);
  visitor(value.latest_receipt_ref);
}

template <class Value, class Visitor>
inline void visit_condition_receipt_ref_v2_members(const Value &value,
                                                   Visitor &&visitor) {
  visitor(value.result_id);
}

template <class Value, class Visitor>
inline void visit_s3_player_receipt_payload(const Value &value,
                                            Visitor &&visitor) {
  visitor(value.result_id);
  visitor(value.request_actor_id);
  visitor(value.request_sequence);
  visitor(value.result_revision);
}

template <class Value, class Visitor>
inline void visit_elapsed_suspended_payload(const Value &value,
                                            Visitor &&visitor) {
  visitor(value.reason_source_ref);
}

template <class Value, class Visitor>
inline void visit_elapsed_terminal_payload(const Value &value,
                                           Visitor &&visitor) {
  visitor(value.outcome_ref);
}

template <class Value, class Visitor>
inline void visit_fixed_owner_adopted_payload(const Value &value,
                                              Visitor &&visitor) {
  visitor(value.owner_source_ref);
  visitor(value.owner_source_schema);
}

template <class Value, class Visitor>
inline void visit_FixedOwnerTerminated_members(const Value &value,
                                               Visitor &&visitor) {
  visitor(value.owner_result_ref);
  visitor(value.owner_result_schema);
}

template <class Value, class Visitor>
inline void visit_FixedServiceTransition_members(const Value &value,
                                                 Visitor &&visitor) {
  visitor(value.transition_id);
  visitor(value.source_schema);
  visitor(value.action_ref);
  visitor(value.transition_ordinal);
  visitor(value.previous_transition_ref);
  visitor(value.kind);
  visitor(value.occurred_at);
  visitor(value.credited_at);
  visitor(value.execution_source_ref);
  visitor(value.execution_source_schema);
  visitor(value.cause);
  visitor(value.input_basis_refs);
  visitor(value.previous_input_basis_refs);
  visitor(value.terminal_disposition);
}

template <class Value, class Visitor>
inline void visit_InArmyLocation_members(const Value &value,
                                         Visitor &&visitor) {
  visitor(value.army_id);
}

// The same actual fields accept an owning row or original borrowed build view.
template <class Boundary, class Visitor>
inline void visit_interaction_session_boundary_payload(const Boundary &value,
                                                       Visitor &&visitor) {
  visitor(value.session_id);
  visitor(value.generation);
  if (value.current_response_fact_ref)
    visitor(*value.current_response_fact_ref);
  if (value.last_consumed_response_fact_ref)
    visitor(*value.last_consumed_response_fact_ref);
}

template <class Value, class Visitor>
inline void visit_MilitaryActionResult_members(const Value &value,
                                               Visitor &&visitor) {
  visitor(value.ok);
  visitor(value.office_id);
  visitor(value.army_id);
  visitor(value.order_id);
  visitor(value.field_order);
}

template <class Value, class Visitor>
inline void visit_original_army_return_actor_change_members(const Value &value,
                                                            Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.status);
  visitor(value.captor_army_id);
  visitor(value.garrison_id);
  visitor(value.army_id);
  visitor(value.field_order);
}

template <class Value, class Visitor>
inline void visit_MilitaryLogisticsResult_members(const Value &value,
                                                  Visitor &&visitor) {
  visitor(value.ok);
  visitor(value.army_id);
  visitor(value.order_id);
  visitor(value.receipt_id);
  visitor(value.actor_changes);
  visitor(value.field_order);
}

template <class Value, class Visitor>
inline void visit_original_army_return_fact_members(const Value &value,
                                                    Visitor &&visitor) {
  visitor(value.id);
  visitor(value.army_id);
  visitor(value.order_id);
  visitor(value.garrison_id);
  visitor(value.source_garrison_id);
  visitor(value.ownership_id);
  visitor(value.actor_id);
  visitor(value.authority);
  visitor(value.tick);
  visitor(value.goods);
  visitor(value.money);
  visitor(value.unit_ids);
  visitor(value.returned_retainers);
  visitor(value.lost_retainers);
  visitor(value.prisoner_ids);
  visitor(value.actor_changes);
  visitor(value.field_order);
}

template <class Value, class Visitor>
inline void visit_original_army_return_completion_payload(const Value &value,
                                                          Visitor &&visitor) {
  visitor(value.success);
  visitor(value.actual_return);
}

template <class Value, class Visitor>
inline void visit_moment_owner_source_members(const Value &value,
                                              Visitor &&visitor) {
  visitor(value.source_ref);
  visitor(value.source_schema);
}

template <class Value, class Visitor>
inline void visit_OriginalMovementAnchor_members(const Value &value,
                                                 Visitor &&visitor) {
  visitor(value.location);
  visitor(value.actual_source);
}

template <class T, class Visitor>
inline void visit_original_period_source(const T &value, Visitor &&visitor) {
  visitor(value.source_ref);
  visitor(value.source_schema);
  visitor(value.processed_tick);
}

template <class Value, class Visitor>
inline void visit_S3ActionFinishedSource_members(const Value &value,
                                                 Visitor &&visitor) {
  visitor(value.action_id);
  visitor(value.original_completion_result_ref);
}

template <class Value, class Visitor>
inline void visit_S3ActionStartedSource_members(const Value &value,
                                                Visitor &&visitor) {
  visitor(value.action_id);
  visitor(value.original_start_result_ref);
}

template <class Value, class Visitor>
inline void visit_S3ActualTerminationSource_members(const Value &value,
                                                    Visitor &&visitor) {
  visitor(value.original_cause_ref);
  visitor(value.original_action_terminal_ref);
}

template <class Value, class Visitor>
inline void visit_S3CommittedZeroStepSource_members(const Value &value,
                                                    Visitor &&visitor) {
  visitor(value.original_commit_result_ref);
  visitor(value.actual_contact_source_ref);
}

template <class Value, class Visitor>
inline void visit_S3ContactInquiryRecord_members(const Value &value,
                                                 Visitor &&visitor) {
  visitor(value.record_table);
  visitor(value.record_id);
}

template <class Value, class Visitor>
inline void visit_S3EventTime_members(const Value &value, Visitor &&visitor) {
  visitor(value.original_record_ref);
  visitor(value.event_role);
  visitor(value.precision);
}

template <class Value, class Visitor>
inline void visit_S3ExactEventMoment_members(const Value &value,
                                             Visitor &&visitor) {
  visitor(value.occurred_at);
  visitor(value.actual_source);
}

template <class Value, class Visitor>
inline void visit_S3InteractionStepRecord_members(const Value &value,
                                                  Visitor &&visitor) {
  visitor(value.original_session_id);
  visitor(value.original_step_execution_key);
}

template <class Value, class Visitor>
inline void visit_S3PersonalAppointmentRecord_members(const Value &value,
                                                      Visitor &&visitor) {
  visitor(value.record_table);
  visitor(value.record_id);
}

template <class Value, class Visitor>
inline void visit_S3ProfessionalCustodyReturnRecord_members(const Value &value,
                                                            Visitor &&visitor) {
  visitor(value.order_id);
  visitor(value.return_result_id);
}

template <class Value, class Visitor>
inline void visit_S3ProfessionalInstallmentRecord_members(const Value &value,
                                                          Visitor &&visitor) {
  visitor(value.order_id);
  visitor(value.installment_result_id);
}

template <class Value, class Visitor>
inline void
visit_S3ProfessionalResultDeliveryRecord_members(const Value &value,
                                                 Visitor &&visitor) {
  visitor(value.order_id);
  visitor(value.result_delivery_result_id);
}

template <class Value, class Visitor>
inline void visit_S3ReceivedKnowledgeSource_members(const Value &value,
                                                    Visitor &&visitor) {
  visitor(value.original_knowledge_receipt_ref);
}

template <class Value, class Visitor>
inline void visit_S3ResidualCopyDeliveryRecord_members(const Value &value,
                                                       Visitor &&visitor) {
  visitor(value.order_id);
  visitor(value.residual_delivery_result_id);
}

template <class Value, class Visitor>
inline void visit_S3VisitAppointmentMeetingRecord_members(const Value &value,
                                                          Visitor &&visitor) {
  visitor(value.record_table);
  visitor(value.record_id);
}

template <class Value, class Visitor>
inline void visit_selected_injury_result_prefix(const Value &value,
                                                Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.injury_rule_ref);
  visitor(value.result_choice_ref);
}

template <class Value, class Visitor>
inline void visit_SettlementLocation_members(const Value &value,
                                             Visitor &&visitor) {
  visitor(value.settlement_id);
  visitor(value.place_id);
}

template <class D, class K, class I, class Visitor>
inline void visit_source_ref_members(const D &domain, const K &kind,
                                     const I &id, Visitor &&visitor) {
  visitor(domain);
  visitor(kind);
  visitor(id);
}

template <class Value, class Visitor>
inline void visit_task_exact_start_payload(const Value &value,
                                           Visitor &&visitor) {
  visitor(value.id);
  visitor(value.action_id);
  visitor(value.action_kind);
  visitor(value.actor_id);
  visitor(value.participants);
  visitor(value.accepted_action);
  visitor(value.required_half_days);
  visitor(value.started_at);
  visitor(value.catalog_identity);
  visitor(value.terms_revision);
  visitor(value.actual_start_sources);
}

template <class Value, class Visitor>
inline void visit_TaskExactTerminalSource_payload(const Value &value,
                                                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.start_source_id);
  visitor(value.action_id);
  visitor(value.finished_at);
  visitor(value.effect);
  visitor(value.task_status_after);
  visitor(value.settlement_id_after);
  visitor(value.actual_result_sources);
  visitor(value.disposition);
  visitor(value.participant_death);
}

// Borrow the existing original Repair/unique-transfer field callbacks. They
// only pass members to this const consumer; they never assign the source.
// actor_route's actor argument is contextual and is already a separate row
// member, so retained storage visits only the actual route value here.
template <class Visitor> struct RepairOwnedFieldCallback {
  Visitor &visitor;
  template <class T> void operator()(const char *, const T &field) {
    visitor(field);
  }
  void actor_route(const char *, const LifeActorRoute &route,
                   const StableId &) {
    visitor(route);
  }
};

// Preserve the two original algorithms' adjacent-rule visitation order while
// sharing the actual catalog member definition. No allocation or rule choice.
enum class ActorFoundationCatalogTraversal { fresh_copy, retained_graph };

template <class Visitor>
inline void visit_actor_foundation_catalog(
    const ::hanmo::resident::ActorFoundationCatalog &value,
    ActorFoundationCatalogTraversal traversal, Visitor &&visitor) {
  visitor(value.identity);
  visitor(value.scenario_id);
  visitor(value.construction_profile);
  visitor(value.provenance_policy);
  visitor(value.rest_rule);
  visitor(value.rest_purpose);
  visitor(value.provider_purpose);
  visitor(value.actors);
  visitor(value.travel_rule);
  visitor(value.work_or_study_rule);
  visitor(value.study_capability_rule);
  visitor(value.military_death_rule);
  visitor(value.natural_age_death_rule);
  visitor(value.sea_death_rule);
  visitor(value.original_military_wound_rule);
  visitor(value.injury_capability_map);
  auto shortage = [&] {
    visitor(value.original_military_supply_shortage_rule);
  };
  auto long_rest = [&] { visitor(value.original_long_rest_rule); };
  if (traversal == ActorFoundationCatalogTraversal::retained_graph) {
    long_rest();
    shortage();
  } else {
    shortage();
    long_rest();
  }
  visitor(value.normal_work_efficiency_rule);
  visitor(value.capability_purpose_bindings);
  visitor(value.original_growth);
}

template <class Value, class Visitor>
inline void visit_pending_retirement_cancelled_payload(const Value &value,
                                                       Visitor &&visitor) {
  visitor(value.pending_id);
  visitor(value.generation);
  visitor(value.original_withdrawn_preparation);
}

template <class Value, class Visitor>
inline void visit_MilitaryTacticalFormationBirth3_payload(const Value &value,
                                                          Visitor &&visitor) {
  visitor(value.original_admission_source);
  visitor(value.created_at);
  visitor(value.actual_before_common_prefix);
  visitor(value.original_source_refs);
}

template <class Value, class Visitor>
inline void visit_MilitaryTacticalFormation3_payload(const Value &value,
                                                     Visitor &&visitor) {
  visitor(value.formation_id);
  visitor(value.source_army_ref);
  visitor(value.roster_revision);
  visitor(value.naval_carrier_ref);
  visitor(value.spatial_binding);
  visitor(value.member_unit_refs);
  visitor(value.officer_assignment_ref);
  visitor(value.superior_formation_ref);
  visitor(value.mission_terms_ref);
  visitor(value.control);
  visitor(value.active_intent_ref);
  visitor(value.last_applied_order_ordinal);
  visitor(value.control_revision);
  visitor(value.command_binding_revision);
  visitor(value.intent_revision);
  visitor(value.birth);
}

template <class Value, class Visitor>
inline void visit_DirectForceIntentReceipt3_payload(const Value &value,
                                                    Visitor &&visitor) {
  visitor(value.formation_id);
  visitor(value.mission_ref);
  visitor(value.previous_intent_revision);
  visitor(value.intent_revision);
  visitor(value.accepted_at);
  visitor(value.accepted_intent_refs);
}

template <class Value, class Visitor>
inline void visit_MilitaryTacticalMissionRecord3_payload(const Value &value,
                                                         Visitor &&visitor) {
  visitor(value.id);
  visitor(value.formation_ref);
  visitor(value.intent_revision);
  visitor(value.adopted_at);
  visitor(value.previous_intent_ref);
  visitor(value.original_direct_request);
  visitor(value.actual_before_common_prefix);
  visitor(value.control_binding_source);
  visitor(value.original_solver_definition_ref);
  visitor(value.original_solver_definition_version);
  visitor(value.original_scope_sources);
  visitor(value.original_executions);
  visitor(value.accepted_intents);
  visitor(value.actual_command_source);
}

template <class Value, class Visitor>
inline void visit_MilitaryPlayerDirectIntent3_payload(const Value &value,
                                                      Visitor &&visitor) {
  visitor(value.controller_actor_id);
  visitor(value.control_epoch);
  visitor(value.actual_command_source);
}

template <class Value, class Visitor>
inline void visit_MilitaryAcceptedIntent3_payload(const Value &value,
                                                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.session_ref);
  visitor(value.generation);
  visitor(value.formation_ref);
  visitor(value.mission_ref);
  visitor(value.unit_ref);
  visitor(value.accepted_at);
  visitor(value.action_instance_ordinal);
  visitor(value.original_order);
  visitor(value.timing_definition_ref);
  visitor(value.timing_definition_version);
  visitor(value.control_binding_source);
  visitor(value.mission_scope_source);
  visitor(value.adoption);
}

// native/world/src/resident/domains/military/military_action_time_records.hpp:6;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit_military_endpoint_optional(
    const std::optional<::hanmo::resident::MilitaryActionEndpointObservation2>
        &value,
    Visitor &&visitor) {
  visitor(value);
}

template <class Visitor>
inline void visit_military_endpoint_optional(
    const std::optional<::hanmo::resident::MilitaryActionEndpointObservation2>
        *value,
    Visitor &&visitor) {
  if (value)
    visitor(*value);
}

template <class Value, class Visitor>
inline void visit_DynamicMilitaryActionFact2_members(const Value &value,
                                                     Visitor &&visitor) {
  visitor(value.id);
  visitor(value.command);
  visitor(value.actor_id);
  visitor(value.started_at);
  visitor(value.completed_at);
  visitor(value.phase);
  visitor(value.success);
  visitor(value.original_terms);
  visitor(value.original_context);
  visitor(value.elapsed_source_ref);
  visitor(value.result);
  visitor(value.terminal_source_ref);
  visitor(value.rule_version);
  visitor(value.actual_resource_effect);
  visitor(value.acceptance_conditions);
  visitor(value.acceptance_resource_reservations);
  visit_military_endpoint_optional(value.acceptance_observation, visitor);
  visit_military_endpoint_optional(value.completion_observation, visitor);
}

// native/world/src/resident/domains/military/military_action_time_records.hpp:13;
// actual owning declaration, no codec validation.
template <class Value, class Visitor>
inline void visit_DynamicMilitaryLogisticsFact2_members(const Value &value,
                                                        Visitor &&visitor) {
  visitor(value.id);
  visitor(value.command);
  visitor(value.actor_id);
  visitor(value.started_at);
  visitor(value.completed_at);
  visitor(value.phase);
  visitor(value.success);
  visitor(value.original_terms);
  visitor(value.original_context);
  visitor(value.elapsed_source_ref);
  visitor(value.result);
  visitor(value.terminal_source_ref);
  visitor(value.rule_version);
  visitor(value.acceptance_conditions);
}

// native/world/src/resident/domains/military/military_action_time_records.hpp:22;
// actual owning declaration, no codec validation.
template <class Value, class Visitor>
inline void visit_DynamicMilitaryReturnFact2_members(const Value &value,
                                                     Visitor &&visitor) {
  visitor(value.id);
  visitor(value.army_id);
  visitor(value.order_id);
  visitor(value.garrison_id);
  visitor(value.source_garrison_id);
  visitor(value.ownership_id);
  visitor(value.actor_id);
  visitor(value.authority);
  visitor(value.completed_at);
  visitor(value.goods);
  visitor(value.money);
  visitor(value.unit_ids);
  visitor(value.returned_retainers);
  visitor(value.lost_retainers);
  visitor(value.prisoner_ids);
  visitor(value.actor_changes);
  visitor(value.logistics_action_ref);
  visitor(value.completion_source_ref);
  visitor(value.actual_resource_effect);
  visitor(value.actual_detachment_releases);
}

template <class Value, class Visitor>
inline void
visit_military_return_detachment_release_payload(const Value &value,
                                                 Visitor &&visitor) {
  visitor(value.estate_id);
  visitor(value.actor_id);
  visitor(value.source_id);
  visitor(value.detached_before);
  visitor(value.detached_after);
}

template <class Value, class Visitor>
inline void
visit_military_return_detachment_releases_payload(const Value &value,
                                                  Visitor &&visitor) {
  visitor(value.releases);
}

template <class Value, class Visitor>
inline void visit_military_return_cargo_payload(const Value &value,
                                                Visitor &&visitor) {
  visitor(value.goods);
  visitor(value.money);
}

// The exact original seven fresh PIN strings, shared by actual row and borrowed
// builder.
template <class C1, class C2, class C3, class C4, class S1, class S2, class S3,
          class Visitor>
inline void visit_source_pin_parts(const C1 &cd, const C2 &ck, const C3 &ci,
                                   const C4 &cs, const S1 &sd, const S2 &sk,
                                   const S3 &si, SourcePinReason reason,
                                   Visitor &&visitor) {
  visitor(cd);
  visitor(ck);
  visitor(ci);
  visitor(cs);
  visit_source_ref_members(sd, sk, si, visitor);
  visitor(reason);
}

template <class Value, class Visitor>
inline void visit_military_actor_consequence_payload(const Value &v,
                                                     Visitor &&visitor) {
  visitor(v.actor_id);
  visitor(v.status);
  visitor(v.casualty_id);
  visitor(v.battle_id);
  visitor(v.original_casualty_rule_ref);
  visitor(v.occurred_at);
  visitor(v.source_ordinal);
  visitor(v.effect_source);
}

template <class Value, class Visitor>
inline void visit_land_consequence_payload(const Value &value,
                                           Visitor &&visitor) {
  visitor(value.effect_group_source);
  visitor(value.source_schema);
  visitor(value.session_ref);
  visitor(value.generation);
  visitor(value.effective_at);
  visitor(value.started_at);
  visitor(value.actual_contact_source);
  visitor(value.original_casualty_rule_ref);
  visitor(value.army_ids);
  visitor(value.unit_ids);
  visitor(value.strike_snapshot);
  visitor(value.complete_strikes);
  visitor(value.actual_supports);
  visitor(value.casualty_rule);
  visitor(value.merged);
  visitor(value.commander_consequences);
  visitor(value.original_consequence_morale);
  visitor(value.command_shock_causes);
  visitor(value.dispositions);
}

template <class Value, class Visitor>
inline void visit_MilitaryFatalConsequence_payload(const Value &value,
                                                   Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.casualty_id);
  visitor(value.battle_id);
  visitor(value.occurred_at);
  visitor(value.source_ordinal);
}

template <class Value, class Visitor>
inline void visit_MilitaryCasualtySourceRead_payload(const Value &value,
                                                     Visitor &&visitor) {
  visitor(value.source_ref);
  visitor(value.schema);
  visitor(value.session_ref);
  visitor(value.battle_id);
  visitor(value.occurred_at);
  visitor(value.roster_receipts);
  visitor(value.actor_changes);
  visitor(value.member_effect_links);
}

template <class Value, class Visitor>
inline void visit_DynamicMilitaryCommanderLossRead_payload(const Value &value,
                                                           Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.loss);
  visitor(value.actual_sources);
}

// These original source views borrow original_terms/original. Only their own
// copied values are owned here; never traverse the referent.
template <class Value, class Visitor>
inline void visit_MilitaryFormationSourceView_payload(const Value &value,
                                                      Visitor &&visitor) {
  visitor(value.completed_source);
  visitor(value.original_action_source);
  visitor(value.completed_at);
  visitor(value.original_completion_serial);
}

template <class Value, class Visitor>
inline void visit_MilitaryCompletedReturnSource_payload(const Value &value,
                                                        Visitor &&visitor) {
  visitor(value.actual_return);
  visitor(value.actual_application);
}

template <class Value, class Visitor>
inline void visit_MilitaryActionPlacement_payload(const Value &value,
                                                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.occurred_at);
  visitor(value.actual_application);
  visitor(value.original_location_result);
  visitor(value.actual_anchor);
}

template <class Value, class Visitor>
inline void visit_interaction_action_confirmation_payload(const Value &value,
                                                          Visitor &&visitor) {
  visitor(value.committed_result_id);
  visitor(value.session_id);
  visitor(value.session_generation);
  visitor(value.dialogue_id);
  visitor(value.action_node_id);
  visitor(value.confirmed_command);
  visitor(value.action_id);
  visitor(value.confirmation_at);
}

template <class Value, class Visitor>
inline void visit_MilitaryCampaignAcceptanceTime1_payload(const Value &value,
                                                          Visitor &&visitor) {
  visitor(value.accepted_at);
  visitor(value.actual_acceptance_source);
  visitor(value.original_context);
  visitor(value.office_appointment_source);
}

template <class Value, class Visitor>
inline void visit_MilitaryOfficeResignationTime1_payload(const Value &value,
                                                         Visitor &&visitor) {
  visitor(value.resigned_at);
  visitor(value.actual_resignation_source);
  visitor(value.original_terms);
  visitor(value.original_context);
  visitor(value.office_appointment_source);
}

template <class Value, class Visitor>
inline void visit_TaskExactParticipantDeath_members(const Value &value,
                                                    Visitor &&visitor) {
  visitor(value.death_ref);
  visitor(value.role);
}

template <class Value, class Visitor>
inline void visit_MilitaryFifoTransferEffect1_members(const Value &value,
                                                      Visitor &&visitor) {
  visitor(value.request);
  visitor(value.source_before);
  visitor(value.source_after);
  visitor(value.destination_before);
  visitor(value.destination_after);
  visitor(value.actual_flows);
  visitor(value.next_instance_before);
  visitor(value.next_instance_after);
}

template <class Value, class Visitor>
inline void visit_MilitaryWalletDeltaEffect1_members(const Value &value,
                                                     Visitor &&visitor) {
  visitor(value.request);
  visitor(value.before);
  visitor(value.after);
}

template <class Value, class Visitor>
inline void visit_MilitaryWalletReservationEffect1_members(const Value &value,
                                                           Visitor &&visitor) {
  visitor(value.request);
  visitor(value.before);
  visitor(value.after);
}

template <class Value, class Visitor>
inline void visit_MilitaryWalletCreationEffect1_members(const Value &value,
                                                        Visitor &&visitor) {
  visitor(value.request);
  visitor(value.after);
}

template <class Value, class Visitor>
inline void visit_MilitaryInventoryCreationEffect1_members(const Value &value,
                                                           Visitor &&visitor) {
  visitor(value.request);
  visitor(value.after);
}

template <class Value, class Visitor>
inline void visit_military_action_resource_payload(const Value &value,
                                                   Visitor &&visitor) {
  visitor(value.operations);
  visitor(value.return_opening);
}

template <class Value, class Visitor>
inline void visit_military_return_resource_opening_members(const Value &value,
                                                           Visitor &&visitor) {
  visitor(value.source_inventory);
  visitor(value.destination_inventory);
}

template <class Value, class Visitor>
inline void visit_prepared_fixed_service_payload(const Value &value,
                                                 Visitor &&visitor) {
  visitor(value.entry);
  visitor(value.transition);
  visitor(value.terminal);
  visitor(value.replay);
}

template <class Value, class Visitor>
inline void visit_military_acceptance_payload(const Value &value,
                                              Visitor &&visitor) {
  visitor(value.participant_heads);
}

template <class Value, class Visitor>
inline void visit_military_endpoint_payload(const Value &value,
                                            Visitor &&visitor) {
  visitor(value.common_prefix);
  visitor(value.condition_prefix);
  visitor(value.participant_heads);
}

template <class T, class Visitor>
inline void visit_action_time_proof_payload(const T &value, Visitor &&visitor) {
  visitor(value.action_ref);
  visitor(value.action_kind);
  visitor(value.actor_id);
  visitor(value.participant_actor_ids);
  visitor(value.original_execution_source);
  visitor(value.source_schema);
  visitor(value.started_source);
  visitor(value.phase);
  visitor(value.retained_wait_source);
  visitor(value.ended_source);
  visitor(value.terminal_result_ref);
  visitor(value.retained_gate_result_refs);
  visitor(value.suspended_source);
}

template <class T, class Visitor>
inline void visit_service_remaining_payload(const T &value, Visitor &&visitor) {
  visitor(value.remaining);
  visitor(value.actual_basis);
  visitor(value.source_schema);
}

template <class T, class Visitor>
inline void visit_authorization_remaining_payload(const T &value,
                                                  Visitor &&visitor) {
  visitor(value.until_tick);
  visitor(value.original_terms_source);
  visitor(value.source_schema);
}

template <class Value, class Visitor>
inline void
visit_military_inventory_reservation_effect_members(const Value &value,
                                                    Visitor &&visitor) {
  visitor(value.request);
  visitor(value.before);
  visitor(value.after);
  visitor(value.actual_flows);
  visitor(value.next_instance_before);
  visitor(value.next_instance_after);
}

template <class Visitor>
inline void visit_military_formation_inventory_opening(
    const std::optional<::hanmo::resident::InventoryView> &value,
    Visitor &&visitor) {
  visitor(value);
}

template <class Visitor>
inline void visit_military_formation_inventory_opening(
    const ::hanmo::resident::InventoryView *value, Visitor &&visitor) {
  if (value)
    visitor(*value);
}

template <class Value, class Visitor>
inline void visit_military_formation_reservations_payload(const Value &value,
                                                          Visitor &&visitor) {
  visitor(value.inventory_reservations);
  visitor(value.wallet_reservation);
  visit_military_formation_inventory_opening(value.empty_inventory_opening,
                                             visitor);
}

template <class Value, class Visitor>
inline void visit_bound_spatial_location_write_payload(const Value &value,
                                                       Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.actual_source);
  visitor(value.source_schema);
  visitor(value.actual_anchor);
  visitor(value.capture_identity);
  visitor(value.capture_sequence);
  visitor(value.prefix_witness);
  visitor(value.write_identity);
}

template <class Value, class Visitor>
inline void visit_actual_spatial_source_payload(const Value &value,
                                                Visitor &&visitor) {
  visitor(value.source);
  visitor(value.schema);
}

template <class Value, class Visitor>
inline void visit_actual_actor_spatial_write_payload(const Value &value,
                                                     Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.occurred_at);
  visitor(value.actual_anchor);
  visitor(value.immutable_source_head);
}

template <class Value, class Visitor>
inline void visit_actual_service_interval_payload(const Value &value,
                                                  Visitor &&visitor) {
  visitor(value.from);
  visitor(value.until);
  visitor(value.opening_transition_ref);
  visitor(value.closing_transition_ref);
  visitor(value.input_basis_refs);
}

template <class Value, class Visitor>
inline void visit_actual_service_interval_view_payload(const Value &value,
                                                       Visitor &&visitor) {
  visitor(value.action_ref);
  visitor(value.intervals);
  visitor(value.credited_before);
  visitor(value.credited_after);
  visitor(value.covered_from);
  visitor(value.covered_until);
  visitor(value.complete);
}
