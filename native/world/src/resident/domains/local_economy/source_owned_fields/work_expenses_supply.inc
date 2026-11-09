// 职责：集中地方经济中的工作费用、供给与经济来源访问；仅借用原字段，不改变顺序或失败行为。
// 由中央登记头在 source_owned_fields
// 命名空间内包含；依赖与具名助手已先行可见。

template <class Visitor>
inline void visit(const ::hanmo::resident::ActualTransportExpense &value,
                  Visitor &&visitor) {
  visitor(value.source_ref);
  visitor(value.source_schema);
  visitor(value.journey_id);
  visitor(value.segment_or_handling_id);
  visitor(value.cost_rule_id);
  visitor(value.payee_wallet_id);
  visitor(value.destination_id);
  visitor(value.purpose);
  visitor(value.amount);
  visitor(value.occurred_at);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::EconomicConfigurationCommandSource &value,
      Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.unit_id);
  visitor(value.actor_id);
  visitor(value.authority);
  visitor(value.prior_configuration_id);
  visitor(value.request_configuration_id);
  visitor(value.permissions);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::EconomicConfigurationExecutionSource &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.configuration_id);
  visitor(value.production_receipt_id);
  visitor(value.tick);
  visitor(value.due_tick);
  visitor(value.applied);
  visitor(value.code);
  visitor(value.permissions);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EconomicLaborRemainderSource &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.unit_id);
  visitor(value.value);
  visitor(value.previous_source_ref);
  visitor(value.settlement_receipt_ref);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::EconomicOperatingPermissionSource &value,
      Visitor &&visitor) {
  visitor(value.responsibility);
  visitor(value.registration_fact_id);
  visitor(value.approval_fact_id);
  visitor(value.decision_fact_id);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::EconomicOperatingReceiptSource &value,
      Visitor &&visitor) {
  visitor(value.receipt_id);
  visitor(value.permissions);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EconomicPeriodOpening &value,
                  Visitor &&visitor) {
  visitor(value.source_id);
  visitor(value.period_id);
  visitor(value.opening_at);
  visitor(value.through_tick);
  visitor(value.existing_activity_ids);
  visitor(value.existing_action_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EconomicResponsibilityRef &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.id);
  visitor(value.unit_id);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::EconomicSupplyPolicyObservation &value,
      Visitor &&visitor) {
  visitor(value.policy_fact_id);
  visitor(value.observation);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EconomicSupplyPolicyRecord &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.author_policy_id);
  visitor(value.registration_fact_id);
  visitor(value.original_source_key);
  visitor(value.original_source_fact_id);
  visitor(value.grant_id);
  visitor(value.authority);
  visitor(value.policy);
  visitor(value.issued_tick);
  visitor(value.earliest_effective_tick);
  visitor(value.through_tick);
  visitor(value.target_amount);
  visitor(value.public_report);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EconomicSupplyPolicyState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.initialized_tick);
  visitor(value.last_observation_tick);
  visitor(value.policies);
  visitor(value.observations);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EconomicTakenPaymentSource &value,
                  Visitor &&visitor) {
  visitor(value.receipt_id);
  visitor(value.arrear_id);
  visitor(value.registration_fact_id);
  visitor(value.approval_fact_id);
  visitor(value.decision_fact_id);
  visitor(value.scope);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::EconomicTakeoverExecutionState &value,
      Visitor &&visitor) {
  visitor(value.version);
  visitor(value.payments);
  visitor(value.rations);
  visitor(value.production);
  visitor(value.relief);
  visitor(value.configurations);
  visitor(value.configuration_requests);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EconomicTransportExpenseState &value,
                  Visitor &&visitor) {
  visitor(value.payments);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EconomicWorkSourcesState &value,
                  Visitor &&visitor) {
  visitor(value.cycles);
  visitor(value.active_cycles_by_unit);
  visitor(value.segments);
  visitor(value.labor_remainders);
  visitor(value.settlements);
  visitor(value.calendar_origin);
  visitor(value.period_openings);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LocalEconomyState &value,
                  Visitor &&visitor) {
  visitor(value.last_step_tick);
  visitor(value.households);
  visitor(value.cohorts);
  visitor(value.labor_allocations);
  visitor(value.production_units);
  visitor(value.obligations);
  visitor(value.provision_bindings);
  visitor(value.production_bindings);
  visitor(value.arrears);
  visitor(value.payments);
  visitor(value.rations);
  visitor(value.production_receipts);
  visitor(value.population_transfers);
  visitor(value.population_losses);
  visitor(value.obligation_remainders);
  visitor(value.supply_registrations);
  visitor(value.supply_observations);
  visitor(value.membership_wage_bindings);
  visitor(value.membership_terminal_fractions);
  visitor(value.production_management);
  visitor(value.household_provisioning);
  visitor(value.named_birth_population_transitions);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LocalSupplyObservation &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.observation);
  visitor(value.tick);
  visitor(value.publicly_visible);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LocalSupplyRegistration &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.source_key);
  visitor(value.organization_id);
  visitor(value.site_id);
  visitor(value.inventory_id);
  visitor(value.item_id);
  visitor(value.tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PaymentReceipt &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.arrear_id);
  visitor(value.obligation_id);
  visitor(value.from_wallet_id);
  visitor(value.to_wallet_id);
  visitor(value.tick);
  visitor(value.due_tick);
  visitor(value.amount);
  visitor(value.authority_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateMarketAssetBasis &value,
                  Visitor &&visitor) {
  visitor(value.original_shop_ref);
  visitor(value.original_shop_epoch_ref);
  visitor(value.original_premises_ref);
  visitor(value.original_lease_or_title_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateMarketOperationSource &value,
                  Visitor &&visitor) {
  visitor(value.operation_ref);
  visitor(value.original_staff_participation_ref);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PublicMarketAllocationAdjusted &value,
      Visitor &&visitor) {
  visitor(value.original_population_change_ref);
  visitor(value.before_allocation);
  visitor(value.after_allocation);
  visitor(value.effective_span);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PublicMarketAssetBasis &value,
                  Visitor &&visitor) {
  visitor(value.original_asset_ref);
  visitor(value.original_organization_ref);
  visitor(value.original_asset_right_ref);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PublicMarketEligibilityChanged &value,
      Visitor &&visitor) {
  visitor(value.before_population_source_ref);
  visitor(value.after_population_source_ref);
  visitor(value.actual_location_or_condition_source_refs);
  visitor(value.qualification_change_cause_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PublicMarketLaborOrigin &value,
                  Visitor &&visitor) {
  visitor(value.origin_id);
  visitor(value.source_schema);
  visitor(value.binding_ref);
  visitor(value.original_allocation);
  visitor(value.committed_span);
  visitor(value.population_source_ref);
  visitor(value.location_source_ref);
  visitor(value.eligibility_source_refs);
  visitor(value.accepted_labor_authority_ref);
  visitor(value.approved_org_fact_ref);
  visitor(value.accepted_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PublicMarketLaborReleased &value,
                  Visitor &&visitor) {
  visitor(value.original_org_boundary_fact_ref);
  visitor(value.release_reason);
  visitor(value.ended_committed_span);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PublicMarketLaborTransition &value,
                  Visitor &&visitor) {
  visitor(value.transition_id);
  visitor(value.source_schema);
  visitor(value.origin_ref);
  visitor(value.transition_ordinal);
  visitor(value.previous_transition_ref);
  visitor(value.occurred_at);
  visitor(value.change);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PublicMarketStaffingBinding &value,
                  Visitor &&visitor) {
  visitor(value.binding_id);
  visitor(value.offer_ref);
  visitor(value.consent_ref);
  visitor(value.original_org_activity_ref);
  visitor(value.original_labor_allocation_ref);
  visitor(value.accepted_event_ref);
  visitor(value.labor_origin_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PublicMarketStaffingConsent &value,
                  Visitor &&visitor) {
  visitor(value.consent_id);
  visitor(value.offer_ref);
  visitor(value.terms_revision);
  visitor(value.cohort_ref);
  visitor(value.controller_authority_ref);
  visitor(value.local_contact_source_ref);
  visitor(value.consented_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PublicMarketStaffingEvent &value,
                  Visitor &&visitor) {
  visitor(value.event_id);
  visitor(value.binding_ref);
  visitor(value.event_ordinal);
  visitor(value.occurred_at);
  visitor(value.kind);
  visitor(value.cause_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PublicMarketStaffingOffer &value,
                  Visitor &&visitor) {
  visitor(value.offer_id);
  visitor(value.terms);
  visitor(value.employer_acceptance_ref);
  visitor(value.local_contact_source_ref);
  visitor(value.issued_at);
  visitor(value.expires_at);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PublicMarketStaffingReleaseConsent &value,
      Visitor &&visitor) {
  visitor(value.consent_id);
  visitor(value.binding_ref);
  visitor(value.terms_revision);
  visitor(value.release_tick);
  visitor(value.controller_authority_ref);
  visitor(value.local_contact_source_ref);
  visitor(value.consented_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PublicMarketStaffingSource &value,
                  Visitor &&visitor) {
  visitor(value.staffing_binding_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PublicMarketStaffingTerms &value,
                  Visitor &&visitor) {
  visitor(value.terms_ref);
  visitor(value.terms_revision);
  visitor(value.market_definition_ref);
  visitor(value.original_org_activity_terms_ref);
  visitor(value.organization_ref);
  visitor(value.responsible_office_ref);
  visitor(value.cohort_ref);
  visitor(value.workers);
  visitor(value.from_tick);
  visitor(value.until_tick);
  visitor(value.compensation_basis);
  visitor(value.original_opening_authorization_ref);
  visitor(value.original_labor_authority_ref);
  visitor(value.continuity_rule_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PublicReliefCommitment &value,
                  Visitor &&visitor) {
  visitor(value.terms);
  visitor(value.policy_id);
  visitor(value.reserved_tick);
  visitor(value.transfers);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PublicReliefReceipt &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.terms_id);
  visitor(value.task_id);
  visitor(value.contract_id);
  visitor(value.action_id);
  visitor(value.acceptance_fact_id);
  visitor(value.authority_source_fact_id);
  visitor(value.work_fact_id);
  visitor(value.tick);
  visitor(value.cash_transferred);
  visitor(value.grain_transferred);
  visitor(value.recipients);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PublicReliefRelease &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.terms_id);
  visitor(value.tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PublicReliefTerms &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.task_id);
  visitor(value.contract_id);
  visitor(value.acceptance_fact_id);
  visitor(value.authority_source_fact_id);
  visitor(value.jurisdiction_id);
  visitor(value.site_id);
  visitor(value.organization_id);
  visitor(value.public_wallet_id);
  visitor(value.public_inventory_id);
  visitor(value.item_id);
  visitor(value.reservation_id);
  visitor(value.total_cash);
  visitor(value.total_grain);
  visitor(value.recipients);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PublicStaffAcceptedDuty &value,
                  Visitor &&visitor) {
  visitor(value.original_duty_terms_ref);
  visitor(value.explicit_no_wage_consent_ref);
  visitor(value.provision_binding_refs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PublicStaffEmployment &value,
                  Visitor &&visitor) {
  visitor(value.accepted_wage_contract_ref);
  visitor(value.provision_binding_refs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SupplyObservation &value,
                  Visitor &&visitor) {
  visitor(value.source_key);
  visitor(value.organization_id);
  visitor(value.site_id);
  visitor(value.inventory_id);
  visitor(value.item_id);
  visitor(value.source_fact_id);
  visitor(value.target_amount);
  visitor(value.physical_amount);
  visitor(value.source_revision);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TransportExpenseRequest &value,
                  Visitor &&visitor) {
  visitor(value.actual_movement_or_handling_source);
  visitor(value.source_schema);
  visitor(value.authority);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TransportPaymentReceipt &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.request);
  visitor(value.expense);
  visitor(value.payer_wallet_id);
  visitor(value.original_obligation_id);
  visitor(value.original_arrear_id);
  visitor(value.original_payment_receipt_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::WorkRef &value, Visitor &&visitor) {
  visitor(value.owner_domain);
  visitor(value.kind);
  visitor(value.work_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LocalEconomyEffects &value,
                  Visitor &&visitor) {
  visitor(value.receipt_ids);
  visitor(value.fact_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EconomicWorkCatalog &value,
                  Visitor &&visitor) {
  visitor(value.production_profiles);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PublicReliefPolicy &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.site_id);
  visitor(value.organization_id);
  visitor(value.item_id);
  visitor(value.recipients);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PublicReliefCatalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.policies);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LocalSupplySource &value,
                  Visitor &&visitor) {
  visitor(value.source_key);
  visitor(value.organization_id);
  visitor(value.site_id);
  visitor(value.inventory_id);
  visitor(value.item_id);
  visitor(value.target_amount);
  visitor(value.public_report);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LocalEconomyCatalog &value,
                  Visitor &&visitor) {
  visitor(value.enabled);
  visitor(value.households);
  visitor(value.cohorts);
  visitor(value.labor_allocations);
  visitor(value.recipes);
  visitor(value.production_units);
  visitor(value.rates);
  visitor(value.obligations);
  visitor(value.provision_bindings);
  visitor(value.production_bindings);
  visitor(value.supply_sources);
  visitor(value.named_actors_excluded);
  visitor(value.production_management);
  visitor(value.household_provisioning);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::EconomicSupplyPolicyDefinition &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.original_source_key);
  visitor(value.organization_id);
  visitor(value.template_id);
  visitor(value.terms_version);
  visitor(value.unit);
  visitor(value.funding_wallet_id);
  visitor(value.reward_wallet_id);
  visitor(value.accept_role);
  visitor(value.report_role);
  visitor(value.measure);
  visitor(value.target_amount);
  visitor(value.max_claim_quantity);
  visitor(value.purchase_budget_per_unit);
  visitor(value.reward_per_unit);
  visitor(value.deadline_ticks);
  visitor(value.valid_ticks);
  visitor(value.reward_total);
  visitor(value.public_report);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EconomicSupplyPolicyCatalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.policies);
}

// native/world/src/resident/domains/local_economy/material_aid/material_aid_model.hpp:12;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::AidItemContainerRoute &value,
                  Visitor &&visitor) {
  visitor(value.item_id);
  visitor(value.source_inventory_id);
  visitor(value.beneficiary_inventory_id);
}

// native/world/src/resident/domains/local_economy/material_aid/material_aid_model.hpp:13;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::AidMoneyContainerRoute &value,
                  Visitor &&visitor) {
  visitor(value.source_wallet_id);
  visitor(value.beneficiary_wallet_id);
}

// native/world/src/resident/domains/local_economy/material_aid/material_aid_model.hpp:21;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::AidOccurrenceCapReservation &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.schedule_commitment_id);
  visitor(value.occurrence_ordinal);
  visitor(value.unit);
  visitor(value.quantity);
  visitor(value.establishment_result_ref);
  visitor(value.release_result_ref);
}

// native/world/src/resident/domains/local_economy/material_aid/material_aid_model.hpp:9;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::AidOccurrenceKey &value,
                  Visitor &&visitor) {
  visitor(value.treaty_id);
  visitor(value.terms_revision);
  visitor(value.term_id);
  visitor(value.ordinal);
}

// native/world/src/resident/domains/local_economy/material_aid/material_aid_model.hpp:14;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::AidScheduleCommitment &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.schedule_id);
  visitor(value.treaty_term_ref);
  visitor(value.resource_acceptance_ref);
  visitor(value.accepted_at);
  visitor(value.authority_refs);
  visitor(value.occurrence_cap_reservation_refs);
  visitor(value.budget_binding_refs);
  visitor(value.established_occurrence_refs);
  visitor(value.termination_source_ref);
}

// native/world/src/resident/domains/local_economy/material_aid/material_aid_model.hpp:47;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::AidSourceLegRef &value,
                  Visitor &&visitor) {
  visitor(value.source);
  visitor(value.leg_ordinal);
}

// native/world/src/resident/domains/local_economy/supply/supply_policy_contract.hpp:32;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::EconomicSupplyPolicySource &value,
                  Visitor &&visitor) {
  visitor(value.issued);
  visitor(value.effective_tick);
  visitor(value.observed_tick);
  visitor(value.new_contracts_allowed);
  visitor(value.approval_fact_id);
  visitor(value.unavailable_code);
  visitor(value.last_observation_fact_id);
}

// native/world/src/resident/domains/local_economy/supply/supply_policy_contract.hpp:13;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void
visit(const ::hanmo::resident::EconomicSupplyPolicyTermsSource &value,
      Visitor &&visitor) {
  visitor(value.source_identity);
  visitor(value.policy_fact_id);
  visitor(value.author_policy_id);
  visitor(value.registration_fact_id);
  visitor(value.original_source_key);
  visitor(value.original_source_fact_id);
  visitor(value.policy);
  visitor(value.issued_tick);
  visitor(value.earliest_effective_tick);
  visitor(value.through_tick);
  visitor(value.target_amount);
  visitor(value.issuing_authority);
  visitor(value.issuing_grant_id);
  visitor(value.maximum_contract_commitment);
}

// native/world/src/resident/domains/local_economy/governance_work/governance_work_expense_contract.hpp:70;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceWorkChargeSource &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  governance_work_expense_fields::fields(
      fields, const_cast<GovernanceWorkChargeSource &>(value));
}

// native/world/src/resident/domains/local_economy/governance_work/governance_work_expense_contract.hpp:38;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceWorkExpenseApproval &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  governance_work_expense_fields::fields(
      fields, const_cast<GovernanceWorkExpenseApproval &>(value));
}

// native/world/src/resident/domains/local_economy/governance_work/governance_work_expense_contract.hpp:48;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceWorkExpenseTerms &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  governance_work_expense_fields::fields(
      fields, const_cast<GovernanceWorkExpenseTerms &>(value));
}

// native/world/src/resident/domains/local_economy/governance_work/governance_work_fee_policy.hpp:6;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceWorkFeePolicy &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  governance_work_expense_fields::fields(
      fields, const_cast<GovernanceWorkFeePolicy &>(value));
}

// native/world/src/resident/domains/local_economy/governance_work/governance_work_expense_contract.hpp:75;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceWorkPaymentSource &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  governance_work_expense_fields::fields(
      fields, const_cast<GovernanceWorkPaymentSource &>(value));
}

// native/world/src/resident/domains/local_economy/governance_work/governance_work_expense_contract.hpp:84;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void
visit(const ::hanmo::resident::GovernanceWorkTerminationSettlement &value,
      Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  governance_work_expense_fields::fields(
      fields, const_cast<GovernanceWorkTerminationSettlement &>(value));
}

// native/world/src/resident/domains/local_economy/material_aid/material_aid_model.hpp:58;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::MaterialAidDeadlineResult &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.obligation_id);
  visitor(value.occurred_at);
  visitor(value.compensation_obligation_ref);
}

// native/world/src/resident/domains/local_economy/material_aid/material_aid_model.hpp:40;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::MaterialAidEstablishment &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.schedule_commitment_id);
  visitor(value.occurrence_ordinal);
  visitor(value.original_obligation_ref);
  visitor(value.established_at);
  visitor(value.converted_reservation_refs);
}

// native/world/src/resident/domains/local_economy/material_aid/material_aid_model.hpp:51;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::MaterialAidReceipt &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.obligation_id);
  visitor(value.issue_result_id);
  visitor(value.actual_receipt_ref);
  visitor(value.occurred_at);
}

// native/world/src/resident/domains/local_economy/material_aid/material_aid_model.hpp:48;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::MaterialAidResourceIssue &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.obligation_id);
  visitor(value.actual_issue_ref);
  visitor(value.occurred_at);
}

// native/world/src/resident/domains/local_economy/material_aid/material_aid_model.hpp:54;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::MaterialAidScheduleRelease &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.schedule_commitment_id);
  visitor(value.actual_termination_ref);
  visitor(value.occurred_at);
  visitor(value.released_reservation_refs);
}

// native/world/src/resident/domains/local_economy/governance_work/governance_work_expense_contract.hpp:20;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::PatrolSecurityAppliedRef &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  governance_work_expense_fields::fields(
      fields, const_cast<PatrolSecurityAppliedRef &>(value));
}

// native/world/src/resident/domains/local_economy/governance_work/governance_work_expense_contract.hpp:43;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::ProposedWorkExpenseTerms &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  governance_work_expense_fields::fields(
      fields, const_cast<ProposedWorkExpenseTerms &>(value));
}

// native/world/src/resident/domains/local_economy/governance_work/governance_work_expense_contract.hpp:15;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::WorkActualStartRef &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  governance_work_expense_fields::fields(
      fields, const_cast<WorkActualStartRef &>(value));
}

// native/world/src/resident/domains/local_economy/governance_work/governance_work_expense_contract.hpp:36;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::WorkAdvanceArrangement &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  governance_work_expense_fields::fields(
      fields, const_cast<WorkAdvanceArrangement &>(value));
}

// native/world/src/resident/domains/local_economy/governance_work/governance_work_expense_contract.hpp:69;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::WorkChargeKey &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  governance_work_expense_fields::fields(fields,
                                         const_cast<WorkChargeKey &>(value));
}

// native/world/src/resident/domains/local_economy/governance_work/governance_work_expense_contract.hpp:19;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::WorkCompletedRef &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  governance_work_expense_fields::fields(fields,
                                         const_cast<WorkCompletedRef &>(value));
}

// native/world/src/resident/domains/local_economy/governance_work/governance_work_expense_contract.hpp:33;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::WorkCostCoverage &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  governance_work_expense_fields::fields(fields,
                                         const_cast<WorkCostCoverage &>(value));
}

// native/world/src/resident/domains/local_economy/governance_work/governance_work_expense_contract.hpp:17;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::WorkEconomicMeasurementRef &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  governance_work_expense_fields::fields(
      fields, const_cast<WorkEconomicMeasurementRef &>(value));
}

// native/world/src/resident/domains/local_economy/governance_work/governance_work_expense_contract.hpp:26;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::WorkFeeLine &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  governance_work_expense_fields::fields(fields,
                                         const_cast<WorkFeeLine &>(value));
}

// native/world/src/resident/domains/local_economy/governance_work/governance_work_expense_contract.hpp:16;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::WorkMeasuredReceiptRef &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  governance_work_expense_fields::fields(
      fields, const_cast<WorkMeasuredReceiptRef &>(value));
}

// native/world/src/resident/domains/local_economy/governance_work/governance_work_expense_contract.hpp:24;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::WorkReportReceivedRef &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  governance_work_expense_fields::fields(
      fields, const_cast<WorkReportReceivedRef &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OriginalGovernanceWorkCharge &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  obligation_source_owned_fields::fields(
      fields, const_cast<OriginalGovernanceWorkCharge &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceWorkExpenseState &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  governance_work_expense_fields::fields(
      fields, const_cast<GovernanceWorkExpenseState &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GovernanceWorkFeeCatalog &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  governance_work_expense_fields::fields(
      fields, const_cast<GovernanceWorkFeeCatalog &>(value));
}
