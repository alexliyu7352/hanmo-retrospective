// 职责：集中公共运行状态中的共享时间、资源与身份访问；仅借用原字段，不改变顺序或失败行为。
// 由中央登记头在 source_owned_fields
// 命名空间内包含；依赖与具名助手已先行可见。

template <class Visitor>
inline void visit(const ::hanmo::resident::ActionCoreView &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.kind);
  visitor(value.actor_id);
  visitor(value.participants);
  visitor(value.start_tick);
  visitor(value.remaining_ticks);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ActionElapsedEntryRef &value,
                  Visitor &&visitor) {
  visitor(value.action_ref);
}

template <class Visitor>
inline void visit_payload(const ::hanmo::resident::ActionElapsedState &value,
                          Visitor &&visitor) {
  visit_action_elapsed_state_payload(value, visitor);
}

template <class Visitor>
inline void
visit_payload(const ::hanmo::resident::ActionElapsedStateBuildView &value,
              Visitor &&visitor) {
  visit_action_elapsed_state_payload(value, visitor);
}

template <class Visitor>
inline void visit_payload(const ::hanmo::resident::ActionElapsedStore &value,
                          Visitor &&visitor) {
  visit_action_elapsed_store_payload(value, visitor);
}

template <class Visitor>
inline void
visit_payload(const ::hanmo::resident::ActionElapsedStoreBuildView &value,
              Visitor &&visitor) {
  visit_action_elapsed_store_payload(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ActionElapsedTerminalBinding &value,
                  Visitor &&visitor) {
  visit_action_elapsed_terminal_payload(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::ActionElapsedTerminalBuildView &value,
      Visitor &&visitor) {
  visit_action_elapsed_terminal_payload(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ActualActorWorkRole &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.role);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CatalogIdentity &value,
                  Visitor &&visitor) {
  visitor(value.content_id);
  visitor(value.content_version);
  visitor(value.content_digest);
  visitor(value.ruleset_id);
  visitor(value.schema_sha256);
  visitor(value.pack_set_digest);
  visitor(value.module_versions);
  visitor(value.artifact_sha256);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ClosedFacilityClaim &value,
                  Visitor &&visitor) {
  visitor(value.claim_id);
  visitor(value.reason);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CommonState &value,
                  Visitor &&visitor) {
  visitor(value.identity);
  visitor(value.metadata);
  visitor(value.next_instance_id);
  visitor(value.root_field_order);
  visitor(value.battle_order);
  visitor(value.action_field_order);
  visitor(value.actors);
  visitor(value.actions);
  visitor(value.parties);
  visitor(value.roles);
  visitor(value.wallets);
  visitor(value.inventories);
  visitor(value.facts);
  visitor(value.rng);
  visitor(value.request_sequences);
  visitor(value.results);
  visitor(value.history);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CompatibilityFields &value,
                  Visitor &&visitor) {
  visitor(value.profile);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicFactView &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.kind);
  visitor(value.rule_version);
  visitor(value.tick);
  visitor(value.cause_fact_ids);
  visitor(value.affair_ids);
  visitor(value.participants);
  visitor(value.supply);
  visitor(value.obligation);
  visitor(value.subject_id);
  visitor(value.outcome);
  visitor(value.publicly_visible);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ElapsedRunning &value,
                  Visitor &&visitor) {
  visitor(value.completion_due_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ElapsedSuspended &value,
                  Visitor &&visitor) {
  visit_elapsed_suspended_payload(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ElapsedSuspendedBuildView &value,
                  Visitor &&visitor) {
  visit_elapsed_suspended_payload(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ElapsedTerminal &value,
                  Visitor &&visitor) {
  visit_elapsed_terminal_payload(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ElapsedTerminalBuildView &value,
                  Visitor &&visitor) {
  visit_elapsed_terminal_payload(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FactParticipant &value,
                  Visitor &&visitor) {
  visitor(value.subject);
  visitor(value.role);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FactRecord &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.kind);
  visitor(value.domain);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FixedAppliedInput &value,
                  Visitor &&visitor) {
  visitor(value.source_ref);
  visitor(value.source_schema);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FixedEligibilityChanged &value,
                  Visitor &&visitor) {
  visitor(value.original_cause_ref);
  visitor(value.original_cause_schema);
  visitor(value.transition_rule_ref);
  visitor(value.original_owner_execution_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FixedLockedRuleInput &value,
                  Visitor &&visitor) {
  visitor(value.catalog_identity);
  visitor(value.rule_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FixedOwnerAdopted &value,
                  Visitor &&visitor) {
  visit_fixed_owner_adopted_payload(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FixedOwnerAdoptedBuildView &value,
                  Visitor &&visitor) {
  visit_fixed_owner_adopted_payload(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FixedOwnerStateChanged &value,
                  Visitor &&visitor) {
  visitor(value.owner_source_ref);
  visitor(value.owner_source_schema);
  visitor(value.transition_rule_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FixedOwnerTerminated &value,
                  Visitor &&visitor) {
  visit_FixedOwnerTerminated_members(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FixedOwnerTerminatedBuildView &value,
                  Visitor &&visitor) {
  visit_FixedOwnerTerminated_members(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FixedServiceTransition &value,
                  Visitor &&visitor) {
  visit_FixedServiceTransition_members(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::FixedServiceTerminalTransitionBuildView &value,
      Visitor &&visitor) {
  visit_FixedServiceTransition_members(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::FixedServiceAdoptionTransitionBuildView &value,
      Visitor &&visitor) {
  visit_FixedServiceTransition_members(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::HistoryRecord &value,
                  Visitor &&visitor) {
  visitor(value.tick);
  visitor(value.kind);
  visitor(value.actor_id);
  visitor(value.result_id);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::InitialEmptyFacilityClaims &value,
                  Visitor &&visitor) {
  (void)value;
  (void)visitor;
}

template <class Visitor>
inline void visit(const ::hanmo::resident::InventoryEntryEvidence &value,
                  Visitor &&visitor) {
  visitor(value.source_schema);
  visitor(value.entry_source);
  visitor(value.inventory_id);
  visitor(value.inventory_revision);
  visitor(value.occurred_at);
  visitor(value.world_id);
  visitor(value.item_catalog_digest);
  visitor(value.lots);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::InventoryEntryLotEvidence &value,
                  Visitor &&visitor) {
  visitor(value.lot_id);
  visitor(value.item_id);
  visitor(value.quantity);
  visitor(value.item_definition_weight);
  visitor(value.item_instance_id);
  visitor(value.lot_properties_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::InventoryView &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.owner);
  visitor(value.capacity);
  visitor(value.revision);
  visitor(value.lots);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeActivityView &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.action_id);
  visitor(value.actor_id);
  visitor(value.provider_actor_id);
  visitor(value.activity);
  visitor(value.kind);
  visitor(value.phase);
  visitor(value.start_tick);
  visitor(value.settlement_tick);
  visitor(value.completed_tick);
  visitor(value.success);
  visitor(value.code);
  visitor(value.item_id);
  visitor(value.quantity);
  visitor(value.amount);
  visitor(value.payer_wallet_id);
  visitor(value.payee_wallet_id);
  visitor(value.from_inventory_id);
  visitor(value.to_inventory_id);
  visitor(value.participant_actor_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeHalfDayStageGateResult &value,
                  Visitor &&visitor) {
  visitor(value.work_period_ref);
  visitor(value.completed_halfday_ordinal);
  visitor(value.completed_stage_ordinals);
  visitor(value.original_stage_result_refs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeHalfDayStageGateState &value,
                  Visitor &&visitor) {
  visitor(value.gate_profile_ref);
  visitor(value.stage_rule_source_ref);
  visitor(value.earned_service_units);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeWorkPeriodSourceRef &value,
                  Visitor &&visitor) {
  visitor(value.source_ref);
  visitor(value.source_schema);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LotView &value, Visitor &&visitor) {
  visitor(value.id);
  visitor(value.item_id);
  visitor(value.reserved_by);
  visitor(value.quantity);
  visitor(value.owner);
  visitor(value.extra);
  visitor(value.field_order);
  visitor(value.item_instance_id);
  visitor(value.lot_properties_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MomentOwnerSource &value,
                  Visitor &&visitor) {
  visit_moment_owner_source_members(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MomentOwnerSourceBuildView &value,
                  Visitor &&visitor) {
  visit_moment_owner_source_members(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MomentSpan &value,
                  Visitor &&visitor) {
  visitor(value.from);
  visitor(value.to);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OriginalHalfDayGate &value,
                  Visitor &&visitor) {
  visitor(value.original_execution_binding);
  visitor(value.original_gate_profile_ref);
  visitor(value.last_consumed_period_ref);
  visitor(value.next_period_ordinal);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OriginalHalfDayGateResult &value,
                  Visitor &&visitor) {
  visitor(value.execution_binding);
  visitor(value.period_source_ref);
  visitor(value.gate_ordinal);
  visitor(value.provided_work_units);
  visitor(value.completed_before);
  visitor(value.completed_after);
  visitor(value.processed_tick);
  visitor(value.stage_result_refs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OriginalPeriodSource &value,
                  Visitor &&visitor) {
  visit_original_period_source(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OriginalPeriodSourceBuildView &value,
                  Visitor &&visitor) {
  visit_original_period_source(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::Owner &value, Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.id);
  visitor(value.extra);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OwnerPeriodWorkBinding &value,
                  Visitor &&visitor) {
  visitor(value.domain);
  visitor(value.activity_kind);
  visitor(value.activity_id);
  visitor(value.registered_period_profile);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PreparedCatalog &value,
                  Visitor &&visitor) {
  visitor(value.exact_time);
  visitor(value.complete_exact_time_runtime);
  visitor(value.actor_foundation);
  visitor(value.growth_permanent);
  visitor(value.life_age_checks);
  visitor(value.care_root_capability_selection);
  visitor(value.production_cycles);
  visitor(value.private_transport_funding_selected);
  visitor(value.social_knowledge_common_cut_selected);
  visitor(value.actor_social_occupancy_selected);
  visitor(value.actor_social_occupancy_initial);
  visitor(value.identity);
  visitor(value.political_takeover);
  visitor(value.task_takeover_execution_enabled);
  visitor(value.task_static_assets_enabled);
  visitor(value.task_extensions);
  visitor(value.personal_appointments);
  visitor(value.contact_inquiries);
  visitor(value.visit_message_replies);
  visitor(value.visit_appointment_invitations);
  visitor(value.visit_appointment_intents);
  visitor(value.visit_appointment_attendance);
  visitor(value.visit_message_receipts_enabled);
  visitor(value.visit_reply_intents);
  visitor(value.born_personal_dialogues);
  visitor(value.known_personal_dialogues);
  visitor(value.task_message_handoffs_enabled);
  visitor(value.task_message_handoff_version);
  visitor(value.task_message_notices);
  visitor(value.task_message_returns);
  visitor(value.task_personal_messages);
  visitor(value.task_personal_reports);
  visitor(value.task_personal_acceptances);
  visitor(value.task_personal_cancellations);
  visitor(value.treaty_passage);
  visitor(value.political_military_appointments);
  visitor(value.political_field_appointments);
  visitor(value.occupation_fiscal);
  visitor(value.occupation_fiscal_settlement_enabled);
  visitor(value.occupation_fiscal_needs_enabled);
  visitor(value.fiscal_taxpayer_continuity_enabled);
  visitor(value.dynamic_profile);
  visitor(value.regional_life);
  visitor(value.named_membership);
  visitor(value.player_plans);
  visitor(value.governance_tasks_enabled);
  visitor(value.task_offer_responses);
  visitor(value.envoy_tasks_enabled);
  visitor(value.party_invitations_enabled);
  visitor(value.task_offer_response_version);
  visitor(value.local_economy);
  visitor(value.household_purchases);
  visitor(value.household_retail_settlements);
  visitor(value.selected_household_version);
  visitor(value.population_military);
  visitor(value.organization);
  visitor(value.organization_lifecycle);
  visitor(value.governance_program_catalog);
  visitor(value.affairs);
  visitor(value.director);
  visitor(value.common);
  visitor(value.travel);
  visitor(value.military);
  visitor(value.field_battle);
  visitor(value.public_supply_inspection);
  visitor(value.dynamic_military_actions);
  visitor(value.military_spatial_definitions);
  visitor(value.military_session_definitions);
  visitor(value.military_duty_catalog);
  visitor(value.item_services);
  visitor(value.repair_policies);
  visitor(value.repair_service_rules);
  visitor(value.craft_recipes);
  visitor(value.economic_facilities);
  visitor(value.production_facility_conditions);
  visitor(value.governance_work_fees);
  visitor(value.public_budget_policies);
  visitor(value.selected_military_occupation_schema);
  visitor(value.naval);
  visitor(value.interaction);
  visitor(value.immutable_interaction_flags);
  visitor(value.social);
  visitor(value.estate);
  visitor(value.tasks);
  visitor(value.task_personal_advances);
  visitor(value.family);
  visitor(value.control_continuation);
  visitor(value.family_life);
  visitor(value.family_legacy);
  visitor(value.family_bequest);
  visitor(value.family_inheritance);
  visitor(value.family_marriage);
  visitor(value.family_apprenticeship);
  visitor(value.family_disciple_legacy);
  visitor(value.family_biological_legacy);
  visitor(value.family_collateral_legacy);
  visitor(value.family_voluntary_paths);
  visitor(value.family_birth);
  visitor(value.dependent_provision_catalog);
  visitor(value.family_care_relief);
  visitor(value.family_lineage);
  visitor(value.estate_employment);
  visitor(value.economic_supply_policies_enabled);
  visitor(value.economic_supply_policies);
  visitor(value.personal);
  visitor(value.growth);
  visitor(value.personal_mastery_initial);
  visitor(value.personal_recovery);
  visitor(value.growth_tactic_lessons);
  visitor(value.growth_market_history);
  visitor(value.private_shops);
  visitor(value.private_shop_household_custody);
  visitor(value.political_treaty_terms);
  visitor(value.political_relations);
  visitor(value.field_group_passage);
  visitor(value.treaty_authority_mapping);
  visitor(value.private_shop_daily_rent);
  visitor(value.private_shop_command_results);
  visitor(value.private_shop_liquidations);
  visitor(value.political_shop_licensing);
  visitor(value.political_shop_licensing_receipts);
  visitor(value.political_governance_continuations);
  visitor(value.political_governance_invitation_replies);
  visitor(value.political_head_handovers);
  visitor(value.political_successor_rounds);
  visitor(value.political_term_handovers);
  visitor(value.political_office_receptions);
  visitor(value.political_office_resignations);
  visitor(value.political_subordinate_appointments);
  visitor(value.political_dismissal_sources);
  visitor(value.political_subordinate_appointment_intents);
  visitor(value.political_governance_agencies);
  visitor(value.political_successions);
  visitor(value.political_shop_registry_successions);
  visitor(value.task_shop_orders_enabled);
  visitor(value.task_shop_order_contacts_enabled);
  visitor(value.diplomacy);
  visitor(value.politics_governance);
  visitor(value.politics);
  visitor(value.political_fiscal);
  visitor(value.fiscal_settlement);
  visitor(value.public_relief);
  visitor(value.household_provision_management);
  visitor(value.production_disruptions);
  visitor(value.place_world_templates);
  visitor(value.world_template);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ProfileDeliveryEntry &value,
                  Visitor &&visitor) {
  visitor(value.delivery_id);
  visitor(value.source_world_id);
  visitor(value.source_tick);
  visitor(value.mode);
  visitor(value.kind);
  visitor(value.subject_id);
  visitor(value.acquisition_source);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ProfileDeliveryOutbox &value,
                  Visitor &&visitor) {
  visitor(value.entries);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ReadVersion &value,
                  Visitor &&visitor) {
  visitor(value.resource);
  visitor(value.version);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::Reservation &value,
                  Visitor &&visitor) {
  visitor(value.action_id);
  visitor(value.amount);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ReservationRef &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.id);
  visitor(value.domain_id);
  visitor(value.activity_kind);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ResourceKey &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::Revision &value, Visitor &&visitor) {
  visitor(value.revision);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RngStream &value,
                  Visitor &&visitor) {
  visitor(value.seed);
  visitor(value.state);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ScopeRef &value, Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.id);
}

template <class Visitor>
inline void visit_payload(const ::hanmo::resident::SelectedResponseTail &value,
                          Visitor &&visitor) {
  visitor(value.session_id);
  visitor(value.session_generation);
  visitor(value.response_fact_ref);
  visitor(value.next_node_or_terminal);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SharedWorkElapsedState &value,
                  Visitor &&visitor) {
  visitor(value.action_ref);
  visitor(value.execution);
  visitor(value.gate);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CommonActualFactPrefix &value,
                  Visitor &&visitor) {
  visitor(value.common_fact_count);
  visitor(value.last_common_fact);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SourceFactTerms &value,
                  Visitor &&visitor) {
  visitor(value.source);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SourceRef &value,
                  Visitor &&visitor) {
  visit_source_ref_members(value.domain, value.kind, value.id, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SourceRefBuildView &value,
                  Visitor &&visitor) {
  visit_source_ref_members(value.domain, value.kind, value.id, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SourceTermsKey &value,
                  Visitor &&visitor) {
  visitor(value.key);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SubjectRef &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TimeScaleRef &value,
                  Visitor &&visitor) {
  visitor(value.definition_id);
  visitor(value.version);
  visitor(value.units_per_half_day);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::WalletView &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.owner);
  visitor(value.balance);
  visitor(value.reserved);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::WorldIdentity &value,
                  Visitor &&visitor) {
  visitor(value.world_id);
  visitor(value.execution_epoch);
  visitor(value.revision);
  visitor(value.tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::WorldMetadata &value,
                  Visitor &&visitor) {
  visitor(value.ruleset_id);
  visitor(value.content_id);
  visitor(value.content_version);
  visitor(value.content_digest);
  visitor(value.engine_build);
  visitor(value.rng_backend);
  visitor(value.scenario_id);
  visitor(value.player_actor_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::WorldTimelineState &value,
                  Visitor &&visitor) {
  visitor(value.scale_ref);
  visitor(value.sub_half_day);
  visitor(value.period_id);
  visitor(value.opening_receipt_ref);
  visitor(value.close_receipt_ref);
  visitor(value.boundary_state);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ActionStarted &value,
                  Visitor &&visitor) {
  visitor(value.action_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::AdvanceDetails &value,
                  Visitor &&visitor) {
  visitor(value.completed_actions);
  visitor(value.suspended_actors);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::Done &value, Visitor &&visitor) {
  (void)value;
  (void)visitor;
}

template <class Visitor>
inline void visit_payload(const ::hanmo::resident::ReceiptDetails &value,
                          Visitor &&visitor) {
  visitor(value.data);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RequiredContainer &value,
                  Visitor &&visitor) {
  visitor(value.owner);
  visitor(value.capacity);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CompleteExactTimeSelection &value,
                  Visitor &&visitor) {
  visitor(value.phase_providers);
  visitor(value.action_time_profiles);
  visitor(value.facility_claims);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CompleteExactTimeRuntime &value,
                  Visitor &&visitor) {
  visitor(value.phase_graph);
  visitor(value.action_times);
  visitor(value.facility_claims);
  visitor(value.facility_holder_profiles);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CommonCatalog &value,
                  Visitor &&visitor) {
  visitor(value.items);
  visitor(value.places);
  visitor(value.roles);
  visitor(value.organization_ids);
  visitor(value.actor_ids);
  visitor(value.required_wallets);
  visitor(value.required_inventories);
  visitor(value.actor_initial_birth);
  visitor(value.personal_skill_kinds);
  visitor(value.actor_names);
  visitor(value.texts);
  visitor(value.extended_skills);
  visitor(value.family_birth_identity_enabled);
  visitor(value.item_instances_enabled);
  visitor(value.lot_properties_enabled);
  visitor(value.selected_resource_operations_enabled);
  visitor(value.domain_activity_reservations_enabled);
  visitor(value.carrier_location_enabled);
  visitor(value.protected_market_inventory_ids);
  visitor(value.personal_loadout_protection_enabled);
  visitor(value.private_shop_freight_protection_enabled);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::WorkServiceIntervalProof &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  craft_fields::fields(fields, const_cast<WorkServiceIntervalProof &>(value));
}

// native/world/src/resident/checkpoint/checkpoint_envelope.hpp:10; actual
// owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CheckpointIdentity &value,
                  Visitor &&visitor) {
  visitor(value.world_id);
  visitor(value.revision);
  visitor(value.tick);
  visitor(value.catalog);
}

// native/world/src/resident/kernel/world_timeline.hpp; original owning fields,
// no semantic admission.
template <class Visitor>
inline void visit(const ::hanmo::resident::FixedServiceTransitionRef &value,
                  Visitor &&visitor) {
  visitor(value.transition_id);
  visitor(value.source_schema);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ConsumerRef &v, Visitor &&visitor) {
  visitor(v.domain);
  visitor(v.kind);
  visitor(v.id);
  visitor(v.slot);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SourcePin &v, Visitor &&visitor) {
  visit_source_pin_parts(v.consumer.domain, v.consumer.kind, v.consumer.id,
                         v.consumer.slot, v.source.domain, v.source.kind,
                         v.source.id, v.reason, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SourcePinBuildView &v,
                  Visitor &&visitor) {
  visit_source_pin_parts(v.consumer_domain, v.consumer_kind, v.consumer_id,
                         v.consumer_slot, v.source_domain, v.source_kind,
                         v.source_id, v.reason, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LegacyFactBoundary &value,
                  Visitor &&visitor) {
  visitor(value.original_tick);
  visitor(value.original_source_schema);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OwnerFactMoment &value,
                  Visitor &&visitor) {
  visitor(value.source);
  visitor(value.source_schema);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicFactViewV2 &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.kind);
  visitor(value.rule_version);
  visitor(value.occurred_at);
  visitor(value.time_basis);
  visitor(value.cause_fact_ids);
  visitor(value.affair_ids);
  visitor(value.participants);
  visitor(value.supply);
  visitor(value.obligation);
  visitor(value.subject_id);
  visitor(value.outcome);
  visitor(value.publicly_visible);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicActionResultV2 &value,
                  Visitor &&visitor) {
  visitor(value.action_id);
  visitor(value.action_kind);
  visitor(value.source_fact_id);
  visitor(value.code);
  visitor(value.completed_at);
  visitor(value.time_basis);
  visitor(value.success);
  visitor(value.participant_actor_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeWorkServiceSourceRef &value,
                  Visitor &&visitor) {
  visitor(value.source_ref);
  visitor(value.source_schema);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeWorkServiceSegment &value,
                  Visitor &&visitor) {
  visitor(value.actual_span);
  visitor(value.provided_service_units);
  visitor(value.actual_service_source_refs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeWorkPeriodView &value,
                  Visitor &&visitor) {
  visitor(value.ref);
  visitor(value.execution_binding);
  visitor(value.gate_profile);
  visitor(value.stage_rule_source_ref);
  visitor(value.completed_halfday_ordinal);
  visitor(value.applied_at);
  visitor(value.service_segments);
  visitor(value.consumed_service_units);
  visitor(value.actual_actor_roles);
  visitor(value.actual_labor_allocations);
  visitor(value.facility_claim_refs);
  visitor(value.resource_effect_refs);
  visitor(value.exertion_source_refs);
  visitor(value.original_stage_result_refs);
  visitor(value.completed_stage_ordinals);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TransferLots &value,
                  Visitor &&visitor) {
  visitor(value.source_id);
  visitor(value.destination_id);
  visitor(value.item_id);
  visitor(value.quantity);
  visitor(value.owners);
  visitor(value.reservation);
  visitor(value.new_owner);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::WalletDelta &value,
                  Visitor &&visitor) {
  visitor(value.wallet_id);
  visitor(value.delta);
  visitor(value.require_spendable);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SetReservation &value,
                  Visitor &&visitor) {
  visitor(value.wallet_id);
  visitor(value.action_id);
  visitor(value.amount);
  visitor(value.erase);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CreateWallet &value,
                  Visitor &&visitor) {
  visitor(value.wallet);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CreateInventory &value,
                  Visitor &&visitor) {
  visitor(value.inventory);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ResourceLotFlowDraft &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.source_inventory_id);
  visitor(value.source_lot_id);
  visitor(value.destination_inventory_id);
  visitor(value.destination_lot_id);
  visitor(value.item_id);
  visitor(value.quantity);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DomainReadVersion &value,
                  Visitor &&visitor) {
  visitor(value.domain);
  visitor(value.kind);
  visitor(value.id);
  visitor(value.revision);
}

template <class Visitor>
inline void visit_payload(const PreparedFixedServiceChange &value,
                          Visitor &&visitor) {
  visit_prepared_fixed_service_payload(value, visitor);
}

template <class Visitor>
inline void visit_payload(const PreparedFixedServiceChangeBuildView &value,
                          Visitor &&visitor) {
  visit_prepared_fixed_service_payload(value, visitor);
}

template <class Visitor>
inline void visit_payload(const ActionTimeOwnerProof &value,
                          Visitor &&visitor) {
  visit_action_time_proof_payload(value, visitor);
}

template <class Visitor>
inline void visit_payload(const ActionTimeOwnerProofBuildView &value,
                          Visitor &&visitor) {
  visit_action_time_proof_payload(value, visitor);
}

template <class Visitor>
inline void visit_payload(const ActualServiceRemaining &value,
                          Visitor &&visitor) {
  visit_service_remaining_payload(value, visitor);
}

template <class Visitor>
inline void visit_payload(const ActualServiceRemainingBuildView &value,
                          Visitor &&visitor) {
  visit_service_remaining_payload(value, visitor);
}

template <class Visitor>
inline void visit_payload(const ActualAuthorizationRemaining &value,
                          Visitor &&visitor) {
  visit_authorization_remaining_payload(value, visitor);
}

template <class Visitor>
inline void visit_payload(const ActualAuthorizationRemainingBuildView &value,
                          Visitor &&visitor) {
  visit_authorization_remaining_payload(value, visitor);
}

// Actual original ReserveLots request and immutable C05 acceptance evidence.
template <class Visitor>
inline void visit(const ::hanmo::resident::ReserveLots &value,
                  Visitor &&visitor) {
  visitor(value.inventory_id);
  visitor(value.item_id);
  visitor(value.quantity);
  visitor(value.owners);
  visitor(value.action_id);
}

template <class Visitor>
inline void visit_payload(const ActualServiceInterval &value,
                          Visitor &&visitor) {
  visit_actual_service_interval_payload(value, visitor);
}

template <class Visitor>
inline void visit_payload(const ActualServiceIntervalBuildView &value,
                          Visitor &&visitor) {
  visit_actual_service_interval_payload(value, visitor);
}

template <class Visitor>
inline void visit_payload(const ActualServiceIntervalView &value,
                          Visitor &&visitor) {
  visit_actual_service_interval_view_payload(value, visitor);
}

template <class Visitor>
inline void visit_payload(const ActualServiceIntervalViewBuildView &value,
                          Visitor &&visitor) {
  visit_actual_service_interval_view_payload(value, visitor);
}
