// 职责：集中人物基础与身体条件中的有限拥有字段访问；仅借用原字段，不改变顺序或失败行为。
// 由中央登记头在 source_owned_fields
// 命名空间内包含；依赖与具名助手已先行可见。

template <class Visitor>
inline void visit(const ::hanmo::resident::ActorActionWorkBinding &value,
                  Visitor &&visitor) {
  visitor(value.action_id);
  visitor(value.action_kind);
  visitor(value.source_owner_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ActorAgeBasisRecord &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.scenario_origin_ref);
  visitor(value.age_basis);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ActorAttributes &value,
                  Visitor &&visitor) {
  visitor(value.administration);
  visitor(value.charisma);
  visitor(value.intellect);
  visitor(value.leadership);
  visitor(value.martial);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ActorCapabilityResult &value,
                  Visitor &&visitor) {
  visitor(value.allowed);
  visitor(value.efficiency_bp);
  visitor(value.limiting_condition_refs);
  visitor(value.rule_ref);
  visitor(value.observed_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ActorCombat &value,
                  Visitor &&visitor) {
  visitor(value.weapon_id);
  visitor(value.heart_id);
  visitor(value.armor_id);
  visitor(value.learned);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ActorConditionHead &value,
                  Visitor &&visitor) {
  visit_actor_condition_head_members(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::CharacterConditionAppliedPrefix &value,
      Visitor &&visitor) {
  visitor(value.applied_receipt_count);
  visitor(value.last_applied_receipt);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ActorConstructionReceipt &value,
                  Visitor &&visitor) {
  visitor(value.scenario_origin_ref);
  visitor(value.construction_profile_ref);
  visitor(value.constructed_actor_ids);
  visitor(value.initial_source_refs);
  visitor(value.initialization_result_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ActorCoreView &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.action_id);
  visitor(value.wallet_id);
  visitor(value.inventory_id);
  visitor(value.alive);
  visitor(value.available);
  visitor(value.hp);
  visitor(value.fatigue);
  visitor(value.location);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ActorDomainBinding &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.scenario_origin_ref);
  visitor(value.identity_definition_ref);
  visitor(value.age_basis_ref);
  visitor(value.condition_seed_refs);
  visitor(value.placement_ref);
  visitor(value.activity_ref);
  visitor(value.custody_ref);
  visitor(value.service_refs);
  visitor(value.office_refs);
  visitor(value.property_business_refs);
  visitor(value.initial_knowledge_refs);
  visitor(value.appearance_source_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ActorExtendedSkills &value,
                  Visitor &&visitor) {
  visitor(value.naval);
  visitor(value.espionage);
  visitor(value.fortification);
  visitor(value.agriculture);
  visitor(value.medicine);
  visitor(value.forging);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ActorFoundationAt &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.legacy_tick);
  visitor(value.exact_moment);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ActorFoundationRuleRef &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.version);
  visitor(value.catalog_digest);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ActorSkills &value,
                  Visitor &&visitor) {
  visitor(value.archery);
  visitor(value.cavalry);
  visitor(value.commerce);
  visitor(value.debate);
  visitor(value.infantry);
  visitor(value.martial);
  visitor(value.negotiation);
  visitor(value.rites);
  visitor(value.tactics);
  visitor(value.transport);
  visitor(value.extended);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ActorStartAgeBasisRef &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ActorStartContextStateV2 &value,
                  Visitor &&visitor) {
  visitor(value.scenario_origins);
  visitor(value.actor_age_bases);
  visitor(value.initial_condition_seeds);
  visitor(value.actor_domain_bindings);
  visitor(value.construction_receipts);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::AllocationInnateParentInput &value,
                  Visitor &&visitor) {
  visitor(value.parent_actor_id);
  visitor(value.family_id);
  visitor(value.parent_birth_ref);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::AllocationPopulationDefinitionInput &value,
      Visitor &&visitor) {
  visitor(value.naming_definition_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::AllocationPrototypeInput &value,
                  Visitor &&visitor) {
  visitor(value.prototype_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::AppearanceFeatures &value,
                  Visitor &&visitor) {
  visitor(value.face_shape);
  visitor(value.eyes);
  visitor(value.brows);
  visitor(value.nose);
  visitor(value.mouth);
  visitor(value.hair);
  visitor(value.beard);
  visitor(value.skin_palette);
  visitor(value.build);
  visitor(value.present);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::AuthoredActorAgeBasis &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.birth_tick);
  visitor(value.birth_source_ref);
  visitor(value.years);
  visitor(value.as_of_tick);
  visitor(value.calendar_year_ordinal);
  visitor(value.source);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::AuthoredActorFoundationDefinition &value,
      Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.scenario_origin_ref);
  visitor(value.identity_definition_ref);
  visitor(value.alive);
  visitor(value.available);
  visitor(value.age_basis);
  visitor(value.condition);
  visitor(value.placement);
  visitor(value.appearance_source_ref);
  visitor(value.attributes);
  visitor(value.skills);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::AuthoredActorFoundationState &value,
                  Visitor &&visitor) {
  visitor(value.actor_start_context);
  visitor(value.character_origin_records);
  visitor(value.character_condition_extensions);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::AuthoredCharacterSource &value,
                  Visitor &&visitor) {
  visitor(value.actor_start_context_ref);
  visitor(value.authored_actor_id);
  visitor(value.prototype_ref);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::AuthoredInitialCapabilityBasis &value,
      Visitor &&visitor) {
  visitor(value.catalog_identity);
  visitor(value.prototype_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::AuthoredStartPlacement &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.place_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BirthConditionSeedSource &value,
                  Visitor &&visitor) {
  visitor(value.birth_record_ref);
  visitor(value.allocation_source_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BirthInitialCapabilityBasis &value,
                  Visitor &&visitor) {
  visitor(value.birth_record_ref);
  visitor(value.allocation_source_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BirthInnateSelectionRef &value,
                  Visitor &&visitor) {
  visitor(value.birth_ref);
  visitor(value.family_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BornCharacterSource &value,
                  Visitor &&visitor) {
  visitor(value.birth_record_ref);
  visitor(value.biological_parent_refs);
  visitor(value.innate_result_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CapacityTransitionV2 &value,
                  Visitor &&visitor) {
  visitor(value.transition_id);
  visitor(value.actor_id);
  visitor(value.purpose);
  visitor(value.from);
  visitor(value.to);
  visitor(value.effective_at);
  visitor(value.expected_recovery_at);
  visitor(value.cause_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CharacterAgeBasisRef &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.actor_start);
  visitor(value.birth);
  visitor(value.creation);
  visitor(value.population);
  visitor(value.legacy);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::CharacterConditionExtensionsStateV1 &value,
      Visitor &&visitor) {
  visitor(value.actor_condition_heads);
  visitor(value.episodes);
  visitor(value.applied_receipts);
  visitor(value.effect_contributions);
  visitor(value.recovery_resolutions);
  visitor(value.capacity_transitions);
  visitor(value.combat_return_bindings);
  visitor(value.instant_limits);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CharacterEfficiencyComponent &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.original_source_ref);
  visitor(value.rule_ref);
  visitor(value.cap_bp);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::CharacterNormalWorkEfficiencyRule &value,
      Visitor &&visitor) {
  visitor(value.rule_ref);
  visitor(value.fatigue_cutoffs);
  visitor(value.fatigue_caps);
  visitor(value.injury_caps);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CharacterOriginRecordV2 &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.origin_kind);
  visitor(value.origin_fact_ref);
  visitor(value.created_at);
  visitor(value.age_basis_ref);
  visitor(value.actor_start_context_ref);
  visitor(value.source);
  visitor(value.initial_capability_basis);
  visitor(value.initial_condition_seed_ref);
  visitor(value.provenance_policy_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CharacterOriginRecordsStateV1 &value,
                  Visitor &&visitor) {
  visitor(value.origins);
  visitor(value.registration_sources);
  visitor(value.allocation_sources);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CharacterPurposeBinding &value,
                  Visitor &&visitor) {
  visitor(value.purpose_ref);
  visitor(value.rule_ref);
  visitor(value.reducer);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ConditionPurposeCapacityCause &value,
                  Visitor &&visitor) {
  visitor(value.condition_receipt_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ConditionReceiptRefV2 &value,
                  Visitor &&visitor) {
  visit_condition_receipt_ref_v2_members(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ConditionReceiptV2 &value,
                  Visitor &&visitor) {
  visitor(value.result_id);
  visitor(value.actor_id);
  visitor(value.business_key);
  visitor(value.cause);
  visitor(value.rule_ref);
  visitor(value.effective_at);
  visitor(value.condition_revision_before);
  visitor(value.condition_revision_after);
  visitor(value.scalar_changes);
  visitor(value.selected_injury_result);
  visitor(value.created_effect_keys);
  visitor(value.recovery_resolution_refs);
  visitor(value.life_transition_ref);
  visitor(value.capacity_transition_refs);
  visitor(value.capability_application);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ConditionScalarChange &value,
                  Visitor &&visitor) {
  visitor(value.field);
  visitor(value.before);
  visitor(value.after);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CreatedCharacterSource &value,
                  Visitor &&visitor) {
  visitor(value.creation_result_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CreatedConditionSeedSource &value,
                  Visitor &&visitor) {
  visitor(value.creation_result_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CreatedInitialCapabilityBasis &value,
                  Visitor &&visitor) {
  visitor(value.creation_result_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CreatedRegistrationSource &value,
                  Visitor &&visitor) {
  visitor(value.draft);
  visitor(value.creation_rule_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CreationAgeBasisRef &value,
                  Visitor &&visitor) {
  visitor(value.registration_result_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CreationRegistrationRef &value,
                  Visitor &&visitor) {
  visitor(value.registration_result_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DeclaredAgeAtRegistration &value,
                  Visitor &&visitor) {
  visitor(value.years);
  visitor(value.as_of_tick);
  visitor(value.calendar_profile_ref);
  visitor(value.calendar_year_ordinal);
  visitor(value.source_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FrozenBodyScalars &value,
                  Visitor &&visitor) {
  visitor(value.alive);
  visitor(value.hp);
  visitor(value.fatigue);
  visitor(value.injury);
  visitor(value.field_order);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::FrozenFacilityCapabilityInputs &value,
      Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.purpose_ref);
  visitor(value.captured_at);
  visitor(value.condition_head_snapshot);
  visitor(value.body_scalars);
  visitor(value.active_condition_inputs);
  visitor(value.selected_rule_refs);
  visitor(value.result);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::InitialCapabilityAllocation &value,
                  Visitor &&visitor) {
  visitor(value.allocation_result_id);
  visitor(value.actor_id);
  visitor(value.allocation_kind);
  visitor(value.original_source_ref);
  visitor(value.allocation_rule_ref);
  visitor(value.base_attributes);
  visitor(value.initial_skill_xp);
  visitor(value.selected_input_refs);
  visitor(value.actual_selection_result_refs);
  visitor(value.applied_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::InitialConditionSeed &value,
                  Visitor &&visitor) {
  visitor(value.seed_key);
  visitor(value.actor_id);
  visitor(value.source);
  visitor(value.hp);
  visitor(value.fatigue);
  visitor(value.injury);
  visitor(value.episodes);
  visitor(value.activity_limit_profile);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::InitialConditionSeedRef &value,
                  Visitor &&visitor) {
  visitor(value.seed_key);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LegacyAgeBasisRef &value,
                  Visitor &&visitor) {
  visitor(value.legacy_actor_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LegacyInitialCapabilityBasis &value,
                  Visitor &&visitor) {
  visitor(value.legacy_origin_kind);
  visitor(value.legacy_source_fact_refs);
  visitor(value.legacy_catalog_identity);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::LifeTerminationConditionBusinessKey &value,
      Visitor &&visitor) {
  visitor(value.cause_tag);
  visitor(value.source_id);
  visitor(value.actor_id);
  visitor(value.source_ordinal);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeTerminationConditionCause &value,
                  Visitor &&visitor) {
  visitor(value.source);
  visitor(value.source_schema);
  visitor(value.source_ordinal);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryAvailabilityCauseV2 &value,
                  Visitor &&visitor) {
  visitor(value.casualty_source);
  visitor(value.source_schema);
  visitor(value.source_ordinal);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitaryCasualtyConditionCause &value,
      Visitor &&visitor) {
  visitor(value.disposition);
  visitor(value.casualty_fact_id);
  visitor(value.battle_id);
  visitor(value.actor_id);
  visitor(value.occurrence_at);
  visitor(value.source_schema);
  visitor(value.source_ordinal);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NamedConditionSeedSource &value,
                  Visitor &&visitor) {
  visitor(value.population_origin_ref);
  visitor(value.registration_result_ref);
  visitor(value.allocation_source_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NamedRegistrationSource &value,
                  Visitor &&visitor) {
  visitor(value.population_origin_ref);
  visitor(value.allocation_source_ref);
  visitor(value.registration_rule_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OriginalLongRestConditionRule &value,
                  Visitor &&visitor) {
  visitor(value.rule_ref);
  visitor(value.personal_injury_recovery);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OriginalMilitaryWoundRule &value,
                  Visitor &&visitor) {
  visitor(value.rule_ref);
  visitor(value.injury_rule_ref);
  visitor(value.result_choice_ref);
  visitor(value.original_casualty_rule_ref);
  visitor(value.selected_applied);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OriginalRestProvidedCause &value,
                  Visitor &&visitor) {
  visitor(value.action_id);
  visitor(value.rest_kind);
  visitor(value.provider_actor_id);
  visitor(value.site_id);
  visitor(value.provided_half_days);
  visitor(value.service_fulfillment_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PopulationCharacterSource &value,
                  Visitor &&visitor) {
  visitor(value.population_origin_ref);
  visitor(value.registration_result_ref);
  visitor(value.allocation_source_ref);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PopulationInitialCapabilityBasis &value,
      Visitor &&visitor) {
  visitor(value.population_origin_ref);
  visitor(value.allocation_source_ref);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PopulationRegistrationAgeBasisRef &value,
      Visitor &&visitor) {
  visitor(value.registration_result_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PopulationSelectionResultRef &value,
                  Visitor &&visitor) {
  visitor(value.origin_ref);
  visitor(value.selection_result_id);
  visitor(value.source_ordinal);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::ProvisionShortfallConditionBusinessKey &value,
      Visitor &&visitor) {
  visitor(value.original_supply_ref);
  visitor(value.source_schema);
  visitor(value.source_ordinal);
  visitor(value.actor_id);
  visitor(value.purpose);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::ProvisionShortfallConditionCause &value,
      Visitor &&visitor) {
  visitor(value.original_supply_ref);
  visitor(value.source_schema);
  visitor(value.source_ordinal);
  visitor(value.actor_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RegistrationExactBirth &value,
                  Visitor &&visitor) {
  visitor(value.birth_tick);
  visitor(value.source_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RegistrationSourceV2 &value,
                  Visitor &&visitor) {
  visitor(value.registration_result_id);
  visitor(value.actor_id);
  visitor(value.registration_kind);
  visitor(value.registered_at);
  visitor(value.age_basis);
  visitor(value.source);
  visitor(value.initial_condition_seed_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RestConditionBusinessKey &value,
                  Visitor &&visitor) {
  visitor(value.action_id);
  visitor(value.actor_id);
  visitor(value.completion_ordinal);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ScenarioOriginRecord &value,
                  Visitor &&visitor) {
  visitor(value.ref);
  visitor(value.construction_profile_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ScenarioOriginRef &value,
                  Visitor &&visitor) {
  visitor(value.catalog_digest);
  visitor(value.scenario_id);
  visitor(value.row_key);
  visitor(value.initial_tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SelectedInjuryResult &value,
                  Visitor &&visitor) {
  visit_selected_injury_result_prefix(value, visitor);
  visitor(value.actual_decision_source_ref);
  visitor(value.effect_items);
}

// The original deterministic producer has no actual decision source. The view
// borrows its optional existing effect vector before any fresh destination
// copy.
template <class Visitor>
inline void
visit(const ::hanmo::resident::SelectedMilitaryInjuryResultBuildView &value,
      Visitor &&visitor) {
  visit_selected_injury_result_prefix(value, visitor);
  if (value.effect_items)
    visitor(*value.effect_items);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TravelConditionBusinessKey &value,
                  Visitor &&visitor) {
  visitor(value.unit_ref);
  visitor(value.actor_id);
  visitor(value.exertion_source_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TravelExertionConditionCause &value,
                  Visitor &&visitor) {
  visitor(value.exertion_source_id);
  visitor(value.actor_id);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::UnimplementedActorFoundationRecord &value,
      Visitor &&visitor) {
  (void)value;
  (void)visitor;
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VerifiedLegacyCharacterSource &value,
                  Visitor &&visitor) {
  visitor(value.legacy_kind);
  visitor(value.legacy_catalog_identity);
  visitor(value.legacy_source_fact_refs);
  visitor(value.migration_rule_ref);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::WorkOrStudyConditionBusinessKey &value,
      Visitor &&visitor) {
  visitor(value.action_id);
  visitor(value.actor_id);
  visitor(value.ordinal);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::WorkOrStudyConditionCause &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.action_id);
  visitor(value.actor_id);
  visitor(value.ordinal);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ActorNameDefinition &value,
                  Visitor &&visitor) {
  visitor(value.name_key);
  visitor(value.presentation_profile_id);
  visitor(value.portrait_kind);
}

// native/world/src/resident/domains/actor_foundation/model.hpp:180; actual
// owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::ConditionEffectContribution &value,
                  Visitor &&visitor) {
  visitor(value.identity);
  visitor(value.current_state);
  visitor(value.last_resolution_ref);
}

// native/world/src/resident/domains/actor_foundation/model.hpp:170; actual
// owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::ConditionEffectIdentity &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.episode_id);
  visitor(value.origin_result);
  visitor(value.effect_key);
}

// native/world/src/resident/domains/actor_foundation/model.hpp:173; actual
// owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::ConditionEpisodeV2 &value,
                  Visitor &&visitor) {
  visitor(value.episode_id);
  visitor(value.actor_id);
  visitor(value.condition_rule_ref);
  visitor(value.origin);
  visitor(value.started_at);
  visitor(value.stage_ref);
  visitor(value.stage_revision);
  visitor(value.next_calendar_due);
  visitor(value.terminal_source_ref);
}

// native/world/src/resident/domains/actor_foundation/model.hpp:187; actual
// owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::RecoveryEffectTarget &value,
                  Visitor &&visitor) {
  visitor(value.effect_identity);
  visitor(value.gate_ref);
  visitor(value.state_before);
  visitor(value.state_after);
  visitor(value.remaining_effect_amount);
}

// native/world/src/resident/domains/actor_foundation/model.hpp:167; actual
// owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::RecoveryResolutionRefV2 &value,
                  Visitor &&visitor) {
  visitor(value.result_id);
}

// native/world/src/resident/domains/actor_foundation/model.hpp:192; actual
// owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::RecoveryResolutionV2 &value,
                  Visitor &&visitor) {
  visitor(value.result_id);
  visitor(value.actor_id);
  visitor(value.actual_recovery_source);
  visitor(value.rule_ref);
  visitor(value.effective_at);
  visitor(value.target_effects);
  visitor(value.condition_revision_before);
  visitor(value.condition_revision_after);
}

// native/world/src/resident/domains/actor_foundation/model.hpp:186; actual
// owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::RestProvidedRecoverySource &value,
                  Visitor &&visitor) {
  visitor(value.condition_receipt_ref);
}

// native/world/src/resident/domains/actor_foundation/model.hpp; original owning
// fields, no semantic admission.
template <class Visitor>
inline void visit(const ::hanmo::resident::InjuryActivityRestriction &value,
                  Visitor &&visitor) {
  visitor(value.registered_purpose);
  visitor(value.restriction_ref);
}

// native/world/src/resident/domains/actor_foundation/model.hpp; original owning
// fields, no semantic admission.
template <class Visitor>
inline void visit(const ::hanmo::resident::InjuryAttributeReduction &value,
                  Visitor &&visitor) {
  visitor(value.attribute_id);
  visitor(value.reducer_ref);
  visitor(value.unit_id);
  visitor(value.amount);
}

// native/world/src/resident/domains/actor_foundation/model.hpp; original owning
// fields, no semantic admission.
template <class Visitor>
inline void visit(const ::hanmo::resident::InjuryCapabilityMap &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.entries);
}

// native/world/src/resident/domains/actor_foundation/model.hpp; original owning
// fields, no semantic admission.
template <class Visitor>
inline void visit(const ::hanmo::resident::InjuryCapabilityMapEntry &value,
                  Visitor &&visitor) {
  visitor(value.injury_rule_ref);
  visitor(value.result_choice_ref);
  visitor(value.original_casualty_rule_ref);
  visitor(value.cause_adapter);
  visitor(value.selection);
  visitor(value.stage_ref);
  visitor(value.effect_items);
}

// native/world/src/resident/domains/actor_foundation/model.hpp; original owning
// fields, no semantic admission.
template <class Visitor>
inline void visit(const ::hanmo::resident::InjuryEffectItem &value,
                  Visitor &&visitor) {
  visitor(value.effect_key);
  visitor(value.effect_payload);
  visitor(value.retention);
  visitor(value.original_effect_rule_ref);
}

// native/world/src/resident/domains/actor_foundation/model.hpp; original owning
// fields, no semantic admission.
template <class Visitor>
inline void visit(const ::hanmo::resident::PermanentInjuryEffect &value,
                  Visitor &&visitor) {
}

// native/world/src/resident/domains/actor_foundation/model.hpp; original owning
// fields, no semantic admission.
template <class Visitor>
inline void visit(const ::hanmo::resident::RecoverableInjuryEffect &value,
                  Visitor &&visitor) {
  visitor(value.original_recovery_gate_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ActorConditionExecutionValues &value,
                  Visitor &&visitor) {
  visitor(value.hp);
  visitor(value.fatigue);
  visitor(value.injury);
  visitor(value.alive);
  visitor(value.available);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::AuthoredConditionCurrent &value,
                  Visitor &&visitor) {
  visitor(value.hp);
  visitor(value.fatigue);
  visitor(value.injury);
  visitor(value.alive);
  visitor(value.revision);
  visitor(value.latest);
  visitor(value.last_at);
}

// 遍历 FH 冻结原引用与普通深值；不保留第二份条件或恢复记录。
template <class Visitor>
inline void visit(const ::hanmo::resident::FrozenConditionEffectBinding &value,
                  Visitor &&visitor) {
  visitor(value.origin_result);
  visitor(value.effect_key);
  visitor(value.current_state);
  visitor(value.last_resolution_ref);
  visitor(value.field_order);
}
// 遍历 FH 冻结原引用与普通深值；不保留第二份条件或恢复记录。
template <class Visitor>
inline void visit(const ::hanmo::resident::FrozenActiveConditionInput &value,
                  Visitor &&visitor) {
  visitor(value.episode_id);
  visitor(value.condition_rule_ref);
  visitor(value.stage_ref);
  visitor(value.stage_revision);
  visitor(value.origin);
  visitor(value.effect_bindings);
  visitor(value.field_order);
}

// 具名看管的显式物理6拥有字段；旧variant标签和既有线序不变。
template<class Visitor> inline void visit(const ::hanmo::resident::NamedMilitaryCaptureAvailabilityCause &value, Visitor&& visitor){
  visitor(value.term_source);
  visitor(value.source_schema);
  visitor(value.predecessor_transition_id);
}
template<class Visitor> inline void visit(const ::hanmo::resident::NamedMilitaryReleaseAvailabilityCause &value, Visitor&& visitor){
  visitor(value.release_source);
  visitor(value.source_schema);
  visitor(value.capture_transition_id);
}

// 原有 variant 分支的唯一真实拥有字段；不新增载荷或替代来源。
template <class Visitor>
inline void visit(const ::hanmo::resident::CharacterInjurySourceRefV1 &value,
                  Visitor &&visitor) {
  visitor(value.result_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CharacterInjuryConditionBusinessKeyV1 &value,
                  Visitor &&visitor) {
  visitor(value.source_ref);
  visitor(value.actor_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GatherFatigueConditionCauseV1 &value,
                  Visitor &&visitor) {
  visitor(value.receipt_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GatherFatigueConditionBusinessKeyV1 &value,
                  Visitor &&visitor) {
  visitor(value.receipt_id);
  visitor(value.actor_id);
}
