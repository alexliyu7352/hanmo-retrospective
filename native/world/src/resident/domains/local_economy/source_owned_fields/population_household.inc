// 职责：集中地方经济中的人口、家户与供养访问；仅借用原字段，不改变顺序或失败行为。
// 由中央登记头在 source_owned_fields
// 命名空间内包含；依赖与具名助手已先行可见。

template <class Visitor>
inline void visit(const ::hanmo::resident::CivilianPopulationRef &value,
                  Visitor &&visitor) {
  visitor(value.cohort_id);
  visitor(value.population_revision);
  visitor(value.transport_binding_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CohortHouseholdBuyer &value,
                  Visitor &&visitor) {
  visitor(value.cohort_id);
  visitor(value.population_selection_ref);
  visitor(value.labor_allocation_id);
  visitor(value.workers);
  visitor(value.accepted_household_policy_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::HouseholdGroup &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.wallet_id);
  visitor(value.inventory_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::HouseholdItemCap &value,
                  Visitor &&visitor) {
  visitor(value.item_id);
  visitor(value.quantity_cap);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::HouseholdMobileCustodyAdoption &value,
      Visitor &&visitor) {
  visitor(value.plan_source);
  visitor(value.actual_view);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::HouseholdProvisionClosure &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.registration_id);
  visitor(value.obligation_id);
  visitor(value.tick);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::HouseholdProvisionConfiguration &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.authorization_id);
  visitor(value.actor_id);
  visitor(value.previous_configuration_id);
  visitor(value.authority);
  visitor(value.requested_tick);
  visitor(value.requested_revision);
  visitor(value.effective_tick);
  visitor(value.first_due_tick);
  visitor(value.consumption_limit);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::HouseholdProvisionConfigurationResolution &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.configuration_id);
  visitor(value.kind);
  visitor(value.actor_id);
  visitor(value.authority);
  visitor(value.tick);
  visitor(value.source_revision);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::HouseholdProvisionManagementState &value,
      Visitor &&visitor) {
  visitor(value.version);
  visitor(value.configurations);
  visitor(value.resolutions);
  visitor(value.current_by_authorization);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::HouseholdProvisionRegistration &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.authorization_id);
  visitor(value.tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::HouseholdProvisionState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.registrations);
  visitor(value.closures);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::HouseholdPurchase &value,
                  Visitor &&visitor) {
  visitor(value.purchase_id);
  visitor(value.plan_ref);
  visitor(value.accepted_terms_ref);
  visitor(value.executor);
  visitor(value.execution_binding);
  visitor(value.service_claim_ref);
  visitor(value.retail_receipt_ref);
  visitor(value.holding_inventory_ref);
  visitor(value.delivery_binding);
  visitor(value.state);
  visitor(value.phase_revision);
  visitor(value.last_consumed_period_ref);
  visitor(value.terminal_result_ref);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::HouseholdPurchaseAcceptedTerms &value,
      Visitor &&visitor) {
  visitor(value.source);
  visitor(value.plan);
  visitor(value.seller);
  visitor(value.item_id);
  visitor(value.quantity);
  visitor(value.quoted_total_qian);
  visitor(value.accepted_at);
  visitor(value.presence_source_ref);
  visitor(value.accepted_versions);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::HouseholdPurchaseHandover &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.purchase_id);
  visitor(value.actual_delivery_source);
  visitor(value.occurred_at);
  visitor(value.from_inventory_id);
  visitor(value.to_inventory_id);
  visitor(value.actual_lots);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::HouseholdPurchasePhaseChange &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.purchase_id);
  visitor(value.before);
  visitor(value.after);
  visitor(value.revision_before);
  visitor(value.revision_after);
  visitor(value.actual_cause);
  visitor(value.occurred_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::HouseholdPurchasePlan &value,
                  Visitor &&visitor) {
  visitor(value.plan_id);
  visitor(value.household_id);
  visitor(value.need_or_policy_ref);
  visitor(value.controller_authority_ref);
  visitor(value.allowed_items);
  visitor(value.spend_cap_qian);
  visitor(value.known_market_ref);
  visitor(value.destination_inventory);
  visitor(value.execution_choice);
  visitor(value.approach_travel_ref);
  visitor(value.latest_start_tick);
  visitor(value.delivery_deadline);
  visitor(value.terms_version);
  visitor(value.accepted_policy_ref);
  visitor(value.accepted_policy_revision);
  visitor(value.mobile_custody_adoption);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::HouseholdPurchasePolicy &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.household_id);
  visitor(value.controller_actor_id);
  visitor(value.revision);
  visitor(value.accepted_tick);
  visitor(value.valid_from_tick);
  visitor(value.valid_until_tick);
  visitor(value.controller_authority_ref);
  visitor(value.allowed_items);
  visitor(value.merchandise_cap_qian);
  visitor(value.transport_cap_qian);
  visitor(value.total_cap_qian);
  visitor(value.allowed_market_refs);
  visitor(value.allowed_destination_inventories);
  visitor(value.acceptance_source_ref);
  visitor(value.allow_cohort_buyer);
  visitor(value.closed_source_ref);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::HouseholdPurchaseServiceResult &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.purchase_id);
  visitor(value.serviced_window);
  visitor(value.coverage_source);
  visitor(value.close_receipt);
  visitor(value.period_ref);
  visitor(value.completed_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::HouseholdPurchaseState &value,
                  Visitor &&visitor) {
  visitor(value.policies);
  visitor(value.plans);
  visitor(value.accepted_terms);
  visitor(value.purchases);
  visitor(value.service_results);
  visitor(value.work_periods);
  visitor(value.handovers);
  visitor(value.phase_changes);
  visitor(value.service_segments);
  visitor(value.named_service_events);
  visitor(value.named_service_claim_episodes);
  visitor(value.market_service_definitions);
  visitor(value.market_service_sources);
  visitor(value.public_staffing_offers);
  visitor(value.public_staffing_consents);
  visitor(value.public_staffing_bindings);
  visitor(value.public_staffing_events);
  visitor(value.public_staffing_release_consents);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::HouseholdReliefRecipient &value,
                  Visitor &&visitor) {
  visitor(value.household_group_id);
  visitor(value.cohort_id);
  visitor(value.cash);
  visitor(value.grain);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::HouseholdReliefTransfer &value,
                  Visitor &&visitor) {
  visitor(value.household_group_id);
  visitor(value.cohort_id);
  visitor(value.wallet_id);
  visitor(value.inventory_id);
  visitor(value.cash);
  visitor(value.grain);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::HouseholdServiceEvent2 &value,
                  Visitor &&visitor) {
  visitor(value.event_id);
  visitor(value.purchase_id);
  visitor(value.original_action_ref);
  visitor(value.event_ordinal);
  visitor(value.kind);
  visitor(value.occurred_at);
  visitor(value.cause_ref);
  visitor(value.episode_ordinal);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::HouseholdServiceSegment &value,
                  Visitor &&visitor) {
  visitor(value.purchase_id);
  visitor(value.window);
  visitor(value.actual_coverage_source);
  visitor(value.labor_allocation_refs);
  visitor(value.exertion_source_refs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LaborAllocation &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.cohort_id);
  visitor(value.activity_or_production_id);
  visitor(value.workers);
  visitor(value.from_tick);
  visitor(value.through_tick);
  visitor(value.commitment_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LivelihoodIntent &value,
                  Visitor &&visitor) {
  visitor(value.goal_id);
  visitor(value.item_id);
  visitor(value.target_quantity);
  visitor(value.selected_service_role_id);
  visitor(value.selected_shop_id);
  visitor(value.selected_source_id);
  visitor(value.selected_tick);
  visitor(value.return_place_id);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::NamedBirthPopulationTransition &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.birth_fact_ref);
  visitor(value.child_actor_id);
  visitor(value.occurred_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NamedHouseholdBuyer &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.accepted_service_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NamedServiceClaimEpisode2 &value,
                  Visitor &&visitor) {
  visitor(value.purchase_id);
  visitor(value.ordinal);
  visitor(value.admission_result_ref);
  visitor(value.claim_id);
  visitor(value.admission_receipt_ref);
  visitor(value.close_result_ref);
  visitor(value.close_receipt_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PopulationCohort &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.site_id);
  visitor(value.household_group_id);
  visitor(value.status);
  visitor(value.headcount);
  visitor(value.labor_capacity);
  visitor(value.private_wallet_id);
  visitor(value.private_inventory_id);
  visitor(value.affiliation_id);
  visitor(value.assignment_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PopulationLaborChange &value,
                  Visitor &&visitor) {
  visitor(value.allocation_id);
  visitor(value.before_workers);
  visitor(value.after_workers);
  visitor(value.before_through_tick);
  visitor(value.after_through_tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PopulationLoss &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.cohort_id);
  visitor(value.source_fact_id);
  visitor(value.casualty_fact_id);
  visitor(value.count);
  visitor(value.labor_eligible_count);
  visitor(value.effective_tick);
  visitor(value.member_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PopulationRevisionSource &value,
                  Visitor &&visitor) {
  visitor(value.cohort_id);
  visitor(value.revision_before);
  visitor(value.revision_after);
  visitor(value.actual_source);
  visitor(value.condition_after);
  visitor(value.location_after);
  visitor(value.labor_changes);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PopulationSelection &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.owner_business_ref);
  visitor(value.source);
  visitor(value.selected_persons);
  visitor(value.selected_labor_eligible);
  visitor(value.partition_policy);
  visitor(value.created_tick);
  visitor(value.release_gate);
  visitor(value.state);
  visitor(value.result_transfer_ref);
  visitor(value.purpose);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PopulationSelectionResult &value,
                  Visitor &&visitor) {
  visitor(value.selection_id);
  visitor(value.source_cohort_ref);
  visitor(value.result_cohort_ref);
  visitor(value.transfer_ref);
  visitor(value.condition_ref);
  visitor(value.provision_handoff_ref);
  visitor(value.obligation_continuation_refs);
  visitor(value.location_binding_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PopulationSourceRevision &value,
                  Visitor &&visitor) {
  visitor(value.cohort_id);
  visitor(value.revision);
  visitor(value.condition_source_ref);
  visitor(value.location_binding_ref);
  visitor(value.active_selection_refs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PopulationTransfer &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.from_cohort_id);
  visitor(value.to_cohort_id);
  visitor(value.count);
  visitor(value.labor_eligible_count);
  visitor(value.reason);
  visitor(value.source_fact_id);
  visitor(value.effective_tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PopulationTransportBinding &value,
                  Visitor &&visitor) {
  visitor(value.binding_id);
  visitor(value.transport_unit_id);
  visitor(value.transport_cohort_id);
  visitor(value.source_population_transfer_ref);
  visitor(value.labor_allocation_refs);
  visitor(value.registration_source_ref);
  visitor(value.created_tick);
  visitor(value.state);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PopulationTransportRelease &value,
                  Visitor &&visitor) {
  visitor(value.binding_id);
  visitor(value.actual_handover_source);
  visitor(value.effective_tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PopulationTransportState &value,
                  Visitor &&visitor) {
  visitor(value.source_revisions);
  visitor(value.selections);
  visitor(value.materializations);
  visitor(value.transport_bindings);
  visitor(value.revision_sources);
  visitor(value.releases);
  visitor(value.public_market_labor_origins);
  visitor(value.public_market_labor_transitions);
  visitor(value.version);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateHouseholdRetailSeller &value,
                  Visitor &&visitor) {
  visitor(value.shop_id);
  visitor(value.operation_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ProvisionBinding &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.subject);
  visitor(value.from_tick);
  visitor(value.through_tick);
  visitor(value.payer_inventory);
  visitor(value.obligation_id);
  visitor(value.policy_owner);
  visitor(value.military_source_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PublicHouseholdRetailSeller &value,
                  Visitor &&visitor) {
  visitor(value.market_shop_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RationReceipt &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.obligation_id);
  visitor(value.inventory_id);
  visitor(value.item_id);
  visitor(value.beneficiary);
  visitor(value.tick);
  visitor(value.due_tick);
  visitor(value.required);
  visitor(value.consumed);
  visitor(value.shortage);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::HouseholdProvisionManagementEffects &value,
      Visitor &&visitor) {
  visitor(value.fact_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::HouseholdReserveRule &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.household_id);
  visitor(value.item_id);
  visitor(value.destination_inventory_id);
  visitor(value.minimum_stock);
  visitor(value.minimum_cash_reserve);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::HouseholdPurchaseCatalog &value,
                  Visitor &&visitor) {
  visitor(value.duration_definition_id);
  visitor(value.continuation_rule_id);
  visitor(value.market_services);
  visitor(value.market_service_definitions);
  visitor(value.household_reserve_rules);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::HouseholdProvisionManagementCatalog &value,
      Visitor &&visitor) {
  visitor(value.version);
  visitor(value.authorization_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::HouseholdProvisionCatalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.authorizations);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::HouseholdProvisionAuthorization &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.household_group_id);
  visitor(value.cohort_id);
  visitor(value.obligation_id);
  visitor(value.provision_binding_id);
  visitor(value.inventory_id);
  visitor(value.item_id);
  visitor(value.rate_profile_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CohortWageNeedPolicy &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.obligation_id);
  visitor(value.organization_id);
  visitor(value.semantic_key);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CohortWageNeedCatalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.policies);
}
