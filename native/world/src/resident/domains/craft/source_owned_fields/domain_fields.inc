// 职责：集中制作与物品中的有限拥有字段访问；仅借用原字段，不改变顺序或失败行为。
// 由中央登记头在 source_owned_fields
// 命名空间内包含；依赖与具名助手已先行可见。

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftCriterion &value,
                  Visitor &&visitor) {
  visitor(value.acceptance_kind);
  visitor(value.acceptance_version);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftWorkGate &value,
                  Visitor &&visitor) {
  visitor(value.execution_binding);
  visitor(value.gate_ordinal);
  visitor(value.gate_opened_at);
  visitor(value.last_accounted_at);
  visitor(value.earned_service_units);
  visitor(value.pace_for_gate);
  visitor(value.accepted_pace_source_ref);
  visitor(value.rush_units_committed);
  visitor(value.last_closed_gate_result_ref);
  visitor(value.pending_stage_ordinal);
  visitor(value.terminal_result_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftWorkGateResult &value,
                  Visitor &&visitor) {
  visitor(value.result_id);
  visitor(value.execution_binding);
  visitor(value.gate_ordinal);
  visitor(value.actual_service_source_refs);
  visitor(value.work_units_before);
  visitor(value.work_units_added);
  visitor(value.work_units_after);
  visitor(value.reached_stage_ordinals);
  visitor(value.material_effect_refs);
  visitor(value.fee_receipt_refs);
  visitor(value.exertion_source_refs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LotPropertiesRef &value,
                  Visitor &&visitor) {
  visitor(value.properties_id);
}

template <class Visitor>
inline void visit(const ItemConditionHead &value, Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_fields::fields(fields, const_cast<ItemConditionHead &>(value));
}

template <class Visitor>
inline void visit(const OriginalUniqueItemTransferEffect &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  unique_item_transfer_fields::fields(
      fields, const_cast<OriginalUniqueItemTransferEffect &>(value));
}

template <class Visitor>
inline void visit(const OriginalUniqueItemTransferEffectBuildView &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  unique_item_transfer_fields::fields(fields, value);
}

template <class Visitor>
inline void visit(const ItemConditionHeadBuildView &value, Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_fields::fields(fields, value);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ItemDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.weight);
  visitor(value.base_price);
  visitor(value.weapon);
  visitor(value.armor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RepairPolicyRef &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_fields::fields(fields, const_cast<RepairPolicyRef &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RepairMaterialRequirement &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_fields::fields(fields, const_cast<RepairMaterialRequirement &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RepairPolicy &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_fields::fields(fields, const_cast<RepairPolicy &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RepairPolicyCatalog &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_fields::fields(fields, const_cast<RepairPolicyCatalog &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RepairMaterialSelection &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_fields::fields(fields, const_cast<RepairMaterialSelection &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RepairReservedMaterial &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_fields::fields(fields, const_cast<RepairReservedMaterial &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RepairAcceptedTerms &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_fields::fields(fields, const_cast<RepairAcceptedTerms &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RepairAcceptedSource &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_fields::fields(fields, const_cast<RepairAcceptedSource &>(value));
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::RepairUnstartedInterruptionSource &value,
      Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_fields::fields(fields,
                        const_cast<RepairUnstartedInterruptionSource &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RepairTerms &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_fields::fields(fields, const_cast<RepairTerms &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RepairJob &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_fields::fields(fields, const_cast<RepairJob &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RepairControlSource &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_fields::fields(fields, const_cast<RepairControlSource &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RepairCompletedSource &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_fields::fields(fields, const_cast<RepairCompletedSource &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RepairInterruptedSource &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_fields::fields(fields, const_cast<RepairInterruptedSource &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RepairJobsState &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_fields::fields(fields, const_cast<RepairJobsState &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftRecipeRef &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<CraftRecipeRef &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftStageMaterial &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<CraftStageMaterial &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftStageSpec &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<CraftStageSpec &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftMaterialOption &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<CraftMaterialOption &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftMaterialRequirement &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<CraftMaterialRequirement &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftMaterialFitRule &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<CraftMaterialFitRule &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftOutputDefinition &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<CraftOutputDefinition &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftRecipeVariant &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<CraftRecipeVariant &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftRecipeDef &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<CraftRecipeDef &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftRecipeCatalog &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<CraftRecipeCatalog &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftQualityRange &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<CraftQualityRange &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftBrief &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<CraftBrief &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftQualityInputs &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<CraftQualityInputs &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftQualityValue &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<CraftQualityValue &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ItemMaterialization &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<ItemMaterialization &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftMaterialSelection &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<CraftMaterialSelection &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftTerms &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<CraftTerms &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftReservedMaterial &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<CraftReservedMaterial &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftConsumedMaterial &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<CraftConsumedMaterial &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftStageResult &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<CraftStageResult &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftServiceSpan &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<CraftServiceSpan &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftServiceSpanBuildView &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, value);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftGateApplied &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<CraftGateApplied &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftQualityResult &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<CraftQualityResult &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftCompletionResult &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<CraftCompletionResult &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftPaceChange &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<CraftPaceChange &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftControlSource &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<CraftControlSource &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftRecoveryTerms &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<CraftRecoveryTerms &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftRecoveryOutput &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<CraftRecoveryOutput &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftReleasedMaterial &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<CraftReleasedMaterial &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftCancellationSource &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<CraftCancellationSource &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftCancellationResult &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<CraftCancellationResult &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftJob &value, Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<CraftJob &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftOccupancyJobRef &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<CraftOccupancyJobRef &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftOccupancyRepairRef &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<CraftOccupancyRepairRef &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftWipContent &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<CraftWipContent &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RepairItemContent &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<RepairItemContent &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftFacilityOccupancyEvent &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields,
                       const_cast<CraftFacilityOccupancyEvent &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftFacilityClaimUntil &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<CraftFacilityClaimUntil &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftFacilityCapacityChanged &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields,
                       const_cast<CraftFacilityCapacityChanged &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftFacilityUnchangedService &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields,
                       const_cast<CraftFacilityUnchangedService &>(value));
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::CraftFacilityReadmittedService &value,
      Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields,
                       const_cast<CraftFacilityReadmittedService &>(value));
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::CraftFacilitySuspendedWithoutPlacement &value,
      Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(
      fields, const_cast<CraftFacilitySuspendedWithoutPlacement &>(value));
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::CraftFacilityRetainedPlacement &value,
      Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields,
                       const_cast<CraftFacilityRetainedPlacement &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftFacilityClearedPlacement &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields,
                       const_cast<CraftFacilityClearedPlacement &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftFacilityBoundarySource &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields,
                       const_cast<CraftFacilityBoundarySource &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CraftJobsState &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<CraftJobsState &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ItemInstance &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  item_service_fields::fields(fields, const_cast<ItemInstance &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LotProperties &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  item_service_fields::fields(fields, const_cast<LotProperties &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ItemPropertyPolicy &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  item_service_fields::fields(fields, const_cast<ItemPropertyPolicy &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ItemOutputKey &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  item_service_fields::fields(fields, const_cast<ItemOutputKey &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ItemOutputAdoption &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  item_service_fields::fields(fields, const_cast<ItemOutputAdoption &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ItemTerminationResult &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  item_service_fields::fields(fields,
                              const_cast<ItemTerminationResult &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ItemInstancesState &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  item_service_fields::fields(fields, const_cast<ItemInstancesState &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LotPropertiesState &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  item_service_fields::fields(fields, const_cast<LotPropertiesState &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ItemServiceCatalog &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  item_service_fields::fields(fields, const_cast<ItemServiceCatalog &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GatherToolWear &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  item_condition_fields::fields(fields, const_cast<GatherToolWear &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::InfusionFailureWear &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  item_condition_fields::fields(fields,
                                const_cast<InfusionFailureWear &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ItemConditionResultView &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  item_condition_fields::fields(fields,
                                const_cast<ItemConditionResultView &>(value));
}

template <class Visitor>
inline void visit_payload(const ActualItemRepairCompletionBuildView &value,
                          Visitor &&visitor) {
  auto field = [&](const auto &, const auto &member) { visitor(member); };
  item_condition_output_fields::fields(field, value);
}
