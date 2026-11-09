// 职责：集中地方经济中的生产、设施与容量访问；仅借用原字段，不改变顺序或失败行为。
// 由中央登记头在 source_owned_fields
// 命名空间内包含；依赖与具名助手已先行可见。

template <class Visitor>
inline void visit(const ::hanmo::resident::AccumulatedProductionState2 &value,
                  Visitor &&visitor) {
  visitor(value.cycle_source_ref);
  visitor(value.unit_id);
  visitor(value.start_tick);
  visitor(value.due_tick);
  visitor(value.last_accounted_at);
  visitor(value.active_input_source_refs);
  visitor(value.capacity_unit_sum);
  visitor(value.labor_unit_numerator);
  visitor(value.labor_denominator);
  visitor(value.original_labor_remainder_ref);
  visitor(value.last_segment_key);
  visitor(value.settlement_receipt_ref);
  visitor(value.active_input);
  visitor(value.active_labor_allocation_refs);
  visitor(value.active_anchor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::AppliedProductionSegment &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.source);
  visitor(value.close_cause);
  visitor(value.capacity_units_added);
  visitor(value.labor_numerator_added);
  visitor(value.actual_worker_halfdays);
  visitor(value.actual_labor_allocation_refs);
  visitor(value.actual_anchor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::EconomicProductionSettlementSource &value,
      Visitor &&visitor) {
  visitor(value.receipt_id);
  visitor(value.cycle_source_ref);
  visitor(value.capacity_limit);
  visitor(value.labor_limit);
  visitor(value.actual_worker_halfdays);
  visitor(value.labor_remainder);
  visitor(value.remainder_before_ref);
  visitor(value.remainder_after_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FacilityClaim2 &value,
                  Visitor &&visitor) {
  visitor(value.claim_id);
  visitor(value.slot_ref);
  visitor(value.holder);
  visitor(value.from);
  visitor(value.until);
  visitor(value.units);
  visitor(value.capacity_revision);
  visitor(value.usage_right_ref);
  visitor(value.source_result_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FacilityClaimMutationReceipt &value,
                  Visitor &&visitor) {
  visitor(value.claim_set_revision);
  visitor(value.occurred_at);
  visitor(value.source_result_ref);
  visitor(value.source_schema);
  visitor(value.closed_claims);
  visitor(value.admitted_claims);
  visitor(value.capacity_source_refs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FacilityClaimReceiptRef &value,
                  Visitor &&visitor) {
  visitor(value.slot_ref);
  visitor(value.claim_set_revision);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FacilityClaimsState &value,
                  Visitor &&visitor) {
  visitor(value.claims);
  visitor(value.slot_histories);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FacilitySlotHistory &value,
                  Visitor &&visitor) {
  visitor(value.slot_ref);
  visitor(value.base);
  visitor(value.mutations);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FacilitySlotRef &value,
                  Visitor &&visitor) {
  visitor(value.provider_domain);
  visitor(value.facility_id);
  visitor(value.slot_kind);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FacilitySourceRef &value,
                  Visitor &&visitor) {
  visitor(value.source_ref);
  visitor(value.source_schema);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MarketNotConditioned &value,
                  Visitor &&visitor) {
  visitor(value.asset_definition_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MarketOriginalCondition &value,
                  Visitor &&visitor) {
  visitor(value.condition_ref);
  visitor(value.condition_version);
  visitor(value.original_change_source_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MarketServiceCapacitySource &value,
                  Visitor &&visitor) {
  visitor(value.source_id);
  visitor(value.source_schema);
  visitor(value.slot_ref);
  visitor(value.source_revision);
  visitor(value.previous_source_ref);
  visitor(value.occurred_at);
  visitor(value.definition_ref);
  visitor(value.asset_basis);
  visitor(value.opening_basis);
  visitor(value.staffing_bindings);
  visitor(value.condition_basis);
  visitor(value.usage_right_basis);
  visitor(value.change_cause);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MarketServiceDefinition &value,
                  Visitor &&visitor) {
  visitor(value.definition_ref);
  visitor(value.slot_ref);
  visitor(value.place_ref);
  visitor(value.asset_source_ref);
  visitor(value.physical_service_slots);
  visitor(value.opening_intervals);
  visitor(value.max_claim_half_days);
  visitor(value.staffing_rule);
  visitor(value.usage_rule_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MarketServiceFacilityChanged &value,
                  Visitor &&visitor) {
  visitor(value.original_condition_result_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MarketServiceInitialized &value,
                  Visitor &&visitor) {
  visitor(value.original_asset_initialization_ref);
  visitor(value.original_opening_acceptance_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MarketServiceOpeningBasis &value,
                  Visitor &&visitor) {
  visitor(value.original_opening_rule_ref);
  visitor(value.accepted_opening_source_ref);
  visitor(value.committed_intervals);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MarketServiceOpeningChanged &value,
                  Visitor &&visitor) {
  visitor(value.original_opening_change_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MarketServiceRightChanged &value,
                  Visitor &&visitor) {
  visitor(value.original_right_change_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MarketServiceSourceTables &value,
                  Visitor &&visitor) {
  visitor(value.public_sources);
  visitor(value.private_sources);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MarketServiceStaffingChanged &value,
                  Visitor &&visitor) {
  visitor(value.original_staffing_event_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MarketServiceUsageRightBasis &value,
                  Visitor &&visitor) {
  visitor(value.usage_rule_ref);
  visitor(value.original_right_ref);
  visitor(value.original_acceptance_or_approval_ref);
  visitor(value.valid_intervals);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ProductionBinding &value,
                  Visitor &&visitor) {
  visitor(value.unit_id);
  visitor(value.policy_owner);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ProductionConfigurationRecord &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.unit_id);
  visitor(value.actor_id);
  visitor(value.requested_tick);
  visitor(value.effective_tick);
  visitor(value.first_due_tick);
  visitor(value.authority);
  visitor(value.targets);
  visitor(value.prior_workers);
  visitor(value.reservation_allocation_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ProductionConfigurationResult &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.configuration_id);
  visitor(value.kind);
  visitor(value.tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ProductionContext &value,
                  Visitor &&visitor) {
  visitor(value.capacity);
  visitor(value.region_factor_bp);
  visitor(value.season_factor_bp);
  visitor(value.season_phase);
  visitor(value.season_profile_id);
  visitor(value.asset_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ProductionDisruptionOutput &value,
                  Visitor &&visitor) {
  visitor(value.inventory_id);
  visitor(value.item_id);
  visitor(value.original_potential);
  visitor(value.unproduced);
  visitor(value.adjusted_potential);
  visitor(value.stored);
  visitor(value.capacity_shortfall);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::ProductionDisruptionRegistration &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.cause_id);
  visitor(value.tick);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::ProductionDisruptionSettlement &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.production_receipt_id);
  visitor(value.unit_id);
  visitor(value.tick);
  visitor(value.due_tick);
  visitor(value.remaining_yield_bp);
  visitor(value.registration_fact_ids);
  visitor(value.outputs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ProductionDisruptionState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.registrations);
  visitor(value.settlements);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ProductionLaborTarget &value,
                  Visitor &&visitor) {
  visitor(value.allocation_id);
  visitor(value.workers);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ProductionManagementState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.configurations);
  visitor(value.results);
  visitor(value.pending_by_unit);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ProductionQuantity &value,
                  Visitor &&visitor) {
  visitor(value.inventory_id);
  visitor(value.item_id);
  visitor(value.quantity);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ProductionReceipt &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.unit_id);
  visitor(value.recipe_id);
  visitor(value.tick);
  visitor(value.due_tick);
  visitor(value.allocated_workers);
  visitor(value.effective_workers);
  visitor(value.batches);
  visitor(value.inputs);
  visitor(value.outputs);
  visitor(value.unrealized_output);
  visitor(value.cause_fact_ids);
  visitor(value.production_context);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ProductionSegmentSource &value,
                  Visitor &&visitor) {
  visitor(value.unit_id);
  visitor(value.cycle_source_ref);
  visitor(value.from);
  visitor(value.to);
  visitor(value.condition_source_refs);
  visitor(value.asset_use_source_refs);
  visitor(value.participation_refs);
  visitor(value.actual_supply_refs);
  visitor(value.batch_capacity);
  visitor(value.labor_rate_numerator);
  visitor(value.labor_rate_denominator);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ProductionUnit &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.site_id);
  visitor(value.owner);
  visitor(value.input_inventory_ids);
  visitor(value.output_inventory_id);
  visitor(value.labor_allocation_ids);
  visitor(value.capacity);
  visitor(value.recipe_id);
  visitor(value.next_due_tick);
  visitor(value.last_settlement_id);
  visitor(value.labor_remainder);
  visitor(value.factor_remainder);
  visitor(value.input_remainders);
  visitor(value.output_remainders);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::HouseholdMarketServiceDefinition &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.market_ref);
  visitor(value.place_id);
  visitor(value.custody_inventory_id);
  visitor(value.service_slot);
  visitor(value.custody_terms_ref);
  visitor(value.service_usage_right_ref);
  visitor(value.handling_rule_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EconomicProductionProfile &value,
                  Visitor &&visitor) {
  visitor(value.unit_id);
  visitor(value.work_kind);
  visitor(value.rule_version);
  visitor(value.worker_halfdays_per_batch);
  visitor(value.labor_denominator);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RecipeQuantity &value,
                  Visitor &&visitor) {
  visitor(value.item_id);
  visitor(value.numerator);
  visitor(value.denominator);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ProductionSeasonProfile &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.origin_tick);
  visitor(value.phase_ticks);
  visitor(value.factors_bp);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ProductionRecipe &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.period_ticks);
  visitor(value.labor_per_batch);
  visitor(value.region_factor_bp);
  visitor(value.season_factor_bp);
  visitor(value.inputs);
  visitor(value.outputs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ProductionManagementCatalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.assets);
  visitor(value.seasons);
  visitor(value.plans);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ProductionDisruptionCause &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.source_key);
  visitor(value.site_id);
  visitor(value.reason_key);
  visitor(value.from_tick);
  visitor(value.through_tick);
  visitor(value.remaining_yield_bp);
  visitor(value.unit_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ProductionDisruptionCatalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.profile);
  visitor(value.combination_policy);
  visitor(value.causes);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ProductionCapacityPlan &value,
                  Visitor &&visitor) {
  visitor(value.unit_id);
  visitor(value.season_profile_id);
  visitor(value.asset_uses);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ProductionAssetUse &value,
                  Visitor &&visitor) {
  visitor(value.asset_id);
  visitor(value.units);
  visitor(value.batches_per_unit);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ProductionAsset &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.site_id);
  visitor(value.source_key);
  visitor(value.owner);
  visitor(value.kind);
  visitor(value.capacity_units);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::FacilityHolderProfileSelection &value,
      Visitor &&visitor) {
  visitor(value.holder);
  visitor(value.profile_id);
  visitor(value.version);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FacilityHolderPhaseSelection &value,
                  Visitor &&visitor) {
  visitor(value.selection);
  visitor(value.until_stage);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FacilityHolderKind &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.owner_domain);
  visitor(value.activity_kind);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FacilityClaimsCatalog &value,
                  Visitor &&visitor) {
  visitor(value.max_claim_half_days);
  visitor(value.selected_slots);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FacilityActivationReceiptRef &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields,
                       const_cast<FacilityActivationReceiptRef &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::InstalledPhysicalUnitSource &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields,
                       const_cast<InstalledPhysicalUnitSource &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ActivatedPhysicalUnitSource &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields,
                       const_cast<ActivatedPhysicalUnitSource &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FacilityPhysicalUnitRef &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<FacilityPhysicalUnitRef &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FacilityBaseSlotCapacity &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  facility_condition_fields::fields(
      fields, const_cast<FacilityBaseSlotCapacity &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ProductionFacilityCondition &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  facility_condition_fields::fields(
      fields, const_cast<ProductionFacilityCondition &>(value));
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::ProductionFacilityConditionSeed &value,
      Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  facility_condition_fields::fields(
      fields, const_cast<ProductionFacilityConditionSeed &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FacilityAttackResultRef &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  facility_condition_fields::fields(
      fields, const_cast<FacilityAttackResultRef &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CompletedFacilityRepairRef &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  facility_condition_fields::fields(
      fields, const_cast<CompletedFacilityRepairRef &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FacilitySlotCapacityChange &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  facility_condition_fields::fields(
      fields, const_cast<FacilitySlotCapacityChange &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FacilityConditionChange &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  facility_condition_fields::fields(
      fields, const_cast<FacilityConditionChange &>(value));
}

// native/world/src/resident/domains/local_economy/governance_work/governance_work_expense_contract.hpp:21;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void
visit(const ::hanmo::resident::ProductionImprovementAppliedRef &value,
      Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  governance_work_expense_fields::fields(
      fields, const_cast<ProductionImprovementAppliedRef &>(value));
}

// native/world/src/resident/domains/local_economy/governance_work/governance_work_expense_contract.hpp:22;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::WorkFacilityActivatedRef &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  governance_work_expense_fields::fields(
      fields, const_cast<WorkFacilityActivatedRef &>(value));
}

// native/world/src/resident/domains/local_economy/economic_facility_sources.hpp;
// original owning fields, no semantic admission.
template <class Visitor>
inline void visit(const ::hanmo::resident::FacilitySeedRowRef &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(fields,
                                   const_cast<FacilitySeedRowRef &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FacilityAttackToolWear &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  item_condition_fields::fields(fields,
                                const_cast<FacilityAttackToolWear &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OriginalConditionBaseSlot &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(
      fields, const_cast<OriginalConditionBaseSlot &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::InstalledServiceUnits &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(fields,
                                   const_cast<InstalledServiceUnits &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ActivatedServiceUnits &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(fields,
                                   const_cast<ActivatedServiceUnits &>(value));
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::FacilityPhysicalUnitDefinition &value,
      Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(
      fields, const_cast<FacilityPhysicalUnitDefinition &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::AggregateFacilityLayout &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(
      fields, const_cast<AggregateFacilityLayout &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::IndependentFacilityUnits &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(
      fields, const_cast<IndependentFacilityUnits &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SharedFacilitySafetyZone &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(
      fields, const_cast<SharedFacilitySafetyZone &>(value));
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::EconomicFacilitySlotDefinition &value,
      Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(
      fields, const_cast<EconomicFacilitySlotDefinition &>(value));
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::FacilityInitializationAdoptionIdentity &value,
      Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(
      fields, const_cast<FacilityInitializationAdoptionIdentity &>(value));
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PhysicalFacilityInstalledEvidence &value,
      Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(
      fields, const_cast<PhysicalFacilityInstalledEvidence &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::InstalledPhysicalFacility &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(
      fields, const_cast<InstalledPhysicalFacility &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ActivatedPhysicalFacility &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(
      fields, const_cast<ActivatedPhysicalFacility &>(value));
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PhysicalServiceFacilityExtension &value,
      Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(
      fields, const_cast<PhysicalServiceFacilityExtension &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FacilityPhysicalClaimBinding &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(
      fields, const_cast<FacilityPhysicalClaimBinding &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FacilityRetentionBegin &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(fields,
                                   const_cast<FacilityRetentionBegin &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FacilityRetentionContinue &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(
      fields, const_cast<FacilityRetentionContinue &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FacilityRetentionClear &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(fields,
                                   const_cast<FacilityRetentionClear &>(value));
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::FacilityRetentionRestoredService &value,
      Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(
      fields, const_cast<FacilityRetentionRestoredService &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FacilityRetentionTransition &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(
      fields, const_cast<FacilityRetentionTransition &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FacilityNotConditioned &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(fields,
                                   const_cast<FacilityNotConditioned &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FacilityProductionCondition &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(
      fields, const_cast<FacilityProductionCondition &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OriginalFacilityOpenRule &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(
      fields, const_cast<OriginalFacilityOpenRule &>(value));
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::AcceptedFacilityOpeningIntervals &value,
      Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(
      fields, const_cast<AcceptedFacilityOpeningIntervals &>(value));
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::EconomicFacilityUsageRightBasis &value,
      Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(
      fields, const_cast<EconomicFacilityUsageRightBasis &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FacilityInitializedCause &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(
      fields, const_cast<FacilityInitializedCause &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FacilityActivatedCause &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(fields,
                                   const_cast<FacilityActivatedCause &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FacilityConditionChangedCause &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(
      fields, const_cast<FacilityConditionChangedCause &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FacilityOpeningChangedCause &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(
      fields, const_cast<FacilityOpeningChangedCause &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FacilityRightChangedCause &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(
      fields, const_cast<FacilityRightChangedCause &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FacilityRetentionChangedCause &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(
      fields, const_cast<FacilityRetentionChangedCause &>(value));
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::EconomicFacilityCapacitySource &value,
      Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(
      fields, const_cast<EconomicFacilityCapacitySource &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EconomicFacilitySourcesState &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(
      fields, const_cast<EconomicFacilitySourcesState &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EconomicFacilityInitialSeed &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(
      fields, const_cast<EconomicFacilityInitialSeed &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EconomicFacilityOpenRule &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(
      fields, const_cast<EconomicFacilityOpenRule &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EconomicFacilityUsagePolicy &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(
      fields, const_cast<EconomicFacilityUsagePolicy &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EconomicFacilityCatalog &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(
      fields, const_cast<EconomicFacilityCatalog &>(value));
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::ProductionFacilityConditionsState &value,
      Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(
      fields, const_cast<ProductionFacilityConditionsState &>(value));
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::ProductionFacilityConditionsCatalog &value,
      Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(
      fields, const_cast<ProductionFacilityConditionsCatalog &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::AdmitWithinCapacity &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(fields,
                                   const_cast<AdmitWithinCapacity &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NormalWithRetentions &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(fields,
                                   const_cast<NormalWithRetentions &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EntireSlotNoNewAdmission &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  economic_facility_fields::fields(
      fields, const_cast<EntireSlotNoNewAdmission &>(value));
}
