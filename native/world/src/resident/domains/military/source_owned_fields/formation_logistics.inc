// 职责：集中军旅与战斗中的军队组建、运输供给和返回交接访问；仅借用原字段，不改变顺序或失败行为。
// 由中央登记头在 source_owned_fields
// 命名空间内包含；依赖与具名助手已先行可见。

template <class Visitor>
inline void visit(const ::hanmo::resident::ArmyForm &value, Visitor &&visitor) {
  visitor(value.order_id);
  visitor(value.unit_ids);
  visitor(value.retainer_ids);
  visitor(value.food_quantity);
  visitor(value.money);
  visitor(value.unit_ids_valid);
  visitor(value.retainer_ids_valid);
  visitor(value.food_quantity_valid);
  visitor(value.money_valid);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ArmyFormationTerms &value,
                  Visitor &&visitor) {
  visitor(value.order_id);
  visitor(value.office_id);
  visitor(value.actor_id);
  visitor(value.garrison_id);
  visitor(value.ownership_id);
  visitor(value.place_id);
  visitor(value.organization_id);
  visitor(value.unit_ids);
  visitor(value.commanders);
  visitor(value.member_actor_ids);
  visitor(value.retainers);
  visitor(value.goods);
  visitor(value.money);
  visitor(value.source_inventory_id);
  visitor(value.source_wallet_id);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ArmyLogistics &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.army_id);
  visitor(value.garrison_id);
  visitor(value.food_quantity);
  visitor(value.money);
  visitor(value.garrison_valid);
  visitor(value.food_valid);
  visitor(value.money_valid);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ArmyRecord &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.organization_id);
  visitor(value.commander_actor_id);
  visitor(value.member_actor_ids);
  visitor(value.unit_ids);
  visitor(value.inventory_id);
  visitor(value.wallet_id);
  visitor(value.order_id);
  visitor(value.garrison_id);
  visitor(value.status);
  visitor(value.last_supply_tick);
  visitor(value.retainers);
  visitor(value.prisoner_ids);
  visitor(value.formation_source);
  visitor(value.return_garrison_id);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CampaignArrivalFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.campaign_id);
  visitor(value.army_id);
  visitor(value.actor_id);
  visitor(value.tick);
  visitor(value.stronghold_id);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CampaignObjectiveResult &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.tick);
  visitor(value.source_id);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CampaignRecord &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.template_id);
  visitor(value.actor_id);
  visitor(value.office_id);
  visitor(value.garrison_id);
  visitor(value.issuer_role_id);
  visitor(value.issuer_organization_id);
  visitor(value.status);
  visitor(value.army_id);
  visitor(value.accepted_tick);
  visitor(value.deadline_tick);
  visitor(value.last_tick);
  visitor(value.objective_result);
  visitor(value.settlement_id);
  visitor(value.arrival_fact_id);
  visitor(value.return_receipt_id);
  visitor(value.recipient_army_id);
  visitor(value.return_garrison_id);
  visitor(value.return_wait_reason);
  visitor(value.field_order);
  visitor(value.exact_acceptance);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CampaignSettlementFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.campaign_id);
  visitor(value.army_id);
  visitor(value.actor_id);
  visitor(value.tick);
  visitor(value.reason);
  visitor(value.objective_result);
  visitor(value.return_receipt_id);
  visitor(value.report_context);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GarrisonRecord &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.unit_ids);
  visitor(value.prisoner_ids);
  visitor(value.last_supply_tick);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryActorChange &value,
                  Visitor &&visitor) {
  visit_original_army_return_actor_change_members(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::OriginalArmyReturnActorChangeBuildView &value,
      Visitor &&visitor) {
  visit_original_army_return_actor_change_members(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryDeliveryFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.campaign_id);
  visitor(value.army_id);
  visitor(value.actor_id);
  visitor(value.tick);
  visitor(value.stronghold_id);
  visitor(value.recipient_army_id);
  visitor(value.item_id);
  visitor(value.quantity);
  visitor(value.from_inventory_id);
  visitor(value.to_inventory_id);
  visitor(value.source_owner);
  visitor(value.recipient_owner);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryIntegrationCustody &value,
                  Visitor &&visitor) {
  visitor(value.member_id);
  visitor(value.from_army_id);
  visitor(value.to_army_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryIntegrationFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.proposal_id);
  visitor(value.proposal_key);
  visitor(value.siege_id);
  visitor(value.stronghold_id);
  visitor(value.recipient_army_id);
  visitor(value.recipient_organization_id);
  visitor(value.tick);
  visitor(value.decision_source_ids);
  visitor(value.source_armies);
  visitor(value.recipient_units_before);
  visitor(value.recipient_units_after);
  visitor(value.recipient_members_before);
  visitor(value.recipient_members_after);
  visitor(value.wallets);
  visitor(value.lots);
  visitor(value.custody);
  visitor(value.ended_appointments);
  visitor(value.closed_campaign_ids);
  visitor(value.population_policy_ids);
  visitor(value.population_receipt_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryIntegrationLot &value,
                  Visitor &&visitor) {
  visitor(value.source_inventory_id);
  visitor(value.destination_inventory_id);
  visitor(value.lot_id);
  visitor(value.item_id);
  visitor(value.source_organization_id);
  visitor(value.recipient_organization_id);
  visitor(value.quantity);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryIntegrationWallet &value,
                  Visitor &&visitor) {
  visitor(value.source_wallet_id);
  visitor(value.destination_wallet_id);
  visitor(value.source_organization_id);
  visitor(value.recipient_organization_id);
  visitor(value.amount);
  visitor(value.source_before);
  visitor(value.source_after);
  visitor(value.destination_before);
  visitor(value.destination_after);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryLogisticsAction &value,
                  Visitor &&visitor) {
  visitor(value.action_id);
  visitor(value.terms);
  visitor(value.request);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryLogisticsFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.command);
  visitor(value.actor_id);
  visitor(value.phase);
  visitor(value.start_tick);
  visitor(value.completed_tick);
  visitor(value.success);
  visitor(value.terms);
  visitor(value.result);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryLogisticsResult &value,
                  Visitor &&visitor) {
  visit_MilitaryLogisticsResult_members(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitaryLogisticsResultBuildView &value,
      Visitor &&visitor) {
  visit_MilitaryLogisticsResult_members(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryLogisticsTerms &value,
                  Visitor &&visitor) {
  visitor(value.army_id);
  visitor(value.actor_id);
  visitor(value.garrison_id);
  visitor(value.source_garrison_id);
  visitor(value.ownership_id);
  visitor(value.place_id);
  visitor(value.organization_id);
  visitor(value.authority);
  visitor(value.goods);
  visitor(value.money);
  visitor(value.source_inventory_id);
  visitor(value.source_wallet_id);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryMarchFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.rule);
  visitor(value.tick);
  visitor(value.army_id);
  visitor(value.formation_source);
  visitor(value.commander_actor_id);
  visitor(value.supply_fact_id);
  visitor(value.movement);
  visitor(value.movement_field_order);
  visitor(value.uncut_execution);
  visitor(value.uncut_field_order);
  visitor(value.contact_source_id);
  visitor(value.contact_time);
  visitor(value.contact_time_field_order);
  visitor(value.units);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryMarchUnit &value,
                  Visitor &&visitor) {
  visitor(value.unit_id);
  visitor(value.training);
  visitor(value.fatigue_before);
  visitor(value.fatigue_after);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryMusterFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.actor_id);
  visitor(value.estate_id);
  visitor(value.order_id);
  visitor(value.actor_ids);
  visitor(value.tick);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryReturnAuthority &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.id);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryReturnFact &value,
                  Visitor &&visitor) {
  visit_original_army_return_fact_members(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::OriginalArmyReturnFactBuildView &value,
      Visitor &&visitor) {
  visit_original_army_return_fact_members(value, visitor);
}

template <class Visitor>
inline void
visit_payload(const ::hanmo::resident::OriginalArmyReturnCompletion &value,
              Visitor &&visitor) {
  visit_original_army_return_completion_payload(value, visitor);
}

template <class Visitor>
inline void visit_payload(
    const ::hanmo::resident::OriginalArmyReturnCompletionBuildView &value,
    Visitor &&visitor) {
  visit_original_army_return_completion_payload(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitarySupplyAttritionFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.unit_id);
  visitor(value.supply_fact_id);
  visitor(value.previous_supply_fact_id);
  visitor(value.tick);
  visitor(value.consecutive_shortage_half_days);
  visitor(value.fit_before);
  visitor(value.eligible_fit_before);
  visitor(value.deserted_ids);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitarySupplyAttritionState &value,
                  Visitor &&visitor) {
  visitor(value.facts);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitarySupplyEffectSource2 &value,
                  Visitor &&visitor) {
  visitor(value.original_supply_fact_ref);
  visitor(value.period_ref);
  visitor(value.opening_source_ref);
  visitor(value.occurred_at);
  visitor(value.selected_named_actor_ids);
  visitor(value.shortage_value);
  visitor(value.original_rule_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitarySupplyFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.organization_id);
  visitor(value.ownership_id);
  visitor(value.army_id);
  visitor(value.garrison_id);
  visitor(value.tick);
  visitor(value.item_id);
  visitor(value.mouth_ids);
  visitor(value.required);
  visitor(value.consumed);
  visitor(value.shortage);
  visitor(value.cause_fact_ids);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OccupationAssetManifest &value,
                  Visitor &&visitor) {
  visitor(value.wallets);
  visitor(value.inventories);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OccupationCustodyTransfer &value,
                  Visitor &&visitor) {
  visitor(value.person_id);
  visitor(value.from_army_id);
  visitor(value.to_army_id);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OccupationInventoryManifest &value,
                  Visitor &&visitor) {
  visitor(value.from_owner);
  visitor(value.to_owner);
  visitor(value.public_lots);
  visitor(value.protected_lots);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OccupationPublicLot &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.item_id);
  visitor(value.quantity);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OccupationWalletManifest &value,
                  Visitor &&visitor) {
  visitor(value.from_owner);
  visitor(value.to_owner);
  visitor(value.balance);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PopulationEnrollCommand &value,
                  Visitor &&visitor) {
  visitor(value.authority);
  visitor(value.source_id);
  visitor(value.count);
  visitor(value.labor_count);
  visitor(value.release_allocation_ids);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PopulationEnrollment &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.source_id);
  visitor(value.action_id);
  visitor(value.unit_id);
  visitor(value.actor_id);
  visitor(value.completed_tick);
  visitor(value.terms);
  visitor(value.members);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PopulationMilitaryAction &value,
                  Visitor &&visitor) {
  visitor(value.action_id);
  visitor(value.kind);
  visitor(value.request);
  visitor(value.terms);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PopulationMilitaryActionFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.kind);
  visitor(value.actor_id);
  visitor(value.phase);
  visitor(value.code);
  visitor(value.receipt_id);
  visitor(value.started_tick);
  visitor(value.completed_tick);
  visitor(value.success);
  visitor(value.request);
  visitor(value.terms);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PopulationMilitaryFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.kind);
  visitor(value.source_id);
  visitor(value.action_id);
  visitor(value.military_receipt_id);
  visitor(value.tick);
  visitor(value.movements);
  visitor(value.population_receipt_ids);
  visitor(value.occurred_at);
  visitor(value.military_source_schema);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PopulationMilitaryIntegrationFact &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.military_receipt_id);
  visitor(value.policy_id);
  visitor(value.source_id);
  visitor(value.unit_id);
  visitor(value.from_army_id);
  visitor(value.to_army_id);
  visitor(value.service_consent_source_id);
  visitor(value.tick);
  visitor(value.member_ids);
  visitor(value.old_wage_obligation_ids);
  visitor(value.new_wage_obligation_ids);
  visitor(value.next_due_ticks);
  visitor(value.wage_remainders);
  visitor(value.receiving_wage_remainders_before);
  visitor(value.old_ration_obligation_ids);
  visitor(value.new_ration_obligation_ids);
  visitor(value.ration_provision_binding_ids);
  visitor(value.ration_next_due_ticks);
  visitor(value.ration_remainders);
  visitor(value.receiving_ration_remainders_before);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PopulationMilitaryMovement &value,
                  Visitor &&visitor) {
  visitor(value.member_id);
  visitor(value.enrollment_id);
  visitor(value.from_cohort_id);
  visitor(value.to_cohort_id);
  visitor(value.captor_army_id);
  visitor(value.before);
  visitor(value.after);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PopulationMilitaryState &value,
                  Visitor &&visitor) {
  visitor(value.enrollments);
  visitor(value.actions);
  visitor(value.action_facts);
  visitor(value.facts);
  visitor(value.integration_facts);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PopulationMilitaryTerms &value,
                  Visitor &&visitor) {
  visitor(value.source_id);
  visitor(value.organization_id);
  visitor(value.unit_id);
  visitor(value.garrison_id);
  visitor(value.place_id);
  visitor(value.household_group_id);
  visitor(value.reserve_cohort_id);
  visitor(value.serving_cohort_id);
  visitor(value.actor_id);
  visitor(value.military_office_id);
  visitor(value.military_appointment_id);
  visitor(value.authority);
  visitor(value.count);
  visitor(value.labor_count);
  visitor(value.member_ids);
  visitor(value.release_allocation_ids);
  visitor(value.enrollment_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PopulationReleaseCommand &value,
                  Visitor &&visitor) {
  visitor(value.authority);
  visitor(value.enrollment_id);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PopulationSoldierOrigin &value,
                  Visitor &&visitor) {
  visitor(value.member_id);
  visitor(value.source_cohort_id);
  visitor(value.household_group_id);
  visitor(value.labor_eligible);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RetainerRecord &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.estate_id);
  visitor(value.source_id);
  visitor(value.unit_id);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryFormationWorld3 &value,
                  Visitor &&visitor) {
  visitor(value.s1_force_location_ref);
  visitor(value.binding_revision);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryFormationInBattle3 &value,
                  Visitor &&visitor) {
  visitor(value.session_id);
  visitor(value.session_generation);
  visitor(value.entry_source_ref);
  visitor(value.binding_revision);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryFormationPlayer3 &value,
                  Visitor &&visitor) {
  visitor(value.controller_actor_id);
  visitor(value.control_epoch);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryFormationPrebattle3 &value,
                  Visitor &&visitor) {
  visitor(value.assignment_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryFormationDispatched3 &value,
                  Visitor &&visitor) {
  visitor(value.dispatch_fact_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryFormationAutonomous3 &value,
                  Visitor &&visitor) {
  visitor(value.origin);
  visitor(value.active_mission_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PopulationServiceConsent &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.cohort_id);
  visitor(value.organization_id);
  visitor(value.garrison_id);
  visitor(value.from_tick);
  visitor(value.through_tick);
  visitor(value.max_active_count);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PopulationMilitarySource &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.organization_id);
  visitor(value.organization_office_id);
  visitor(value.military_office_id);
  visitor(value.garrison_id);
  visitor(value.unit_id);
  visitor(value.reserve_cohort_id);
  visitor(value.serving_cohort_id);
  visitor(value.wounded_cohort_id);
  visitor(value.captive_cohort_id);
  visitor(value.displaced_cohort_id);
  visitor(value.home_site_id);
  visitor(value.household_group_id);
  visitor(value.service_consent_source_id);
  visitor(value.wage_obligation_ids);
  visitor(value.capacity);
  visitor(value.initial_training);
  visitor(value.initial_fatigue);
  visitor(value.initial_morale);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PopulationMilitaryIntegrationPolicy &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.source_id);
  visitor(value.receiving_organization_id);
  visitor(value.receiving_garrison_id);
  visitor(value.service_consent_source_id);
  visitor(value.wage_obligation_ids);
  visitor(value.ration_obligation_ids);
  visitor(value.target_stronghold_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PopulationMilitaryCatalog &value,
                  Visitor &&visitor) {
  visitor(value.enabled);
  visitor(value.consents);
  visitor(value.sources);
  visitor(value.integration_policies);
  visitor(value.selected_version);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitarySupplyAttritionPolicy &value,
                  Visitor &&visitor) {
  visitor(value.profile);
  visitor(value.shortage_threshold_half_days);
  visitor(value.desertion_basis_points);
  visitor(value.morale_loss);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GarrisonDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.organization_id);
  visitor(value.place_id);
  visitor(value.inventory_id);
  visitor(value.wallet_id);
  visitor(value.food_item_id);
  visitor(value.army_capacity);
  visitor(value.ration_per_person);
  visitor(value.units);
  visitor(value.display_name);
  visitor(value.training_wallet_id);
  visitor(value.training_drill_id);
  visitor(value.training_fatigue_per_tick);
}

template <class Visitor>
inline void visit(const MilitaryReturnDetachmentRelease1 &value,
                  Visitor &&visitor) {
  visit_military_return_detachment_release_payload(value, visitor);
}

template <class Visitor>
inline void visit_payload(const MilitaryReturnDetachmentReleases1 &value,
                          Visitor &&visitor) {
  visit_military_return_detachment_releases_payload(value, visitor);
}

template <class Visitor>
inline void
visit_payload(const MilitaryReturnDetachmentReleasesBuildView &value,
              Visitor &&visitor) {
  visit_military_return_detachment_releases_payload(value, visitor);
}

template <class Visitor>
inline void visit_payload(const MilitaryReturnCargo &value, Visitor &&visitor) {
  visit_military_return_cargo_payload(value, visitor);
}

template <class Visitor>
inline void visit_payload(const MilitaryReturnCargoBuildView &value,
                          Visitor &&visitor) {
  visit_military_return_cargo_payload(value, visitor);
}

template <class Visitor>
inline void
visit_payload(const ::hanmo::resident::MilitaryFormationSourceView &value,
              Visitor &&visitor) {
  visit_MilitaryFormationSourceView_payload(value, visitor);
}

template <class Visitor>
inline void
visit_payload(const ::hanmo::resident::MilitaryFormationSourceBuildView &value,
              Visitor &&visitor) {
  visit_MilitaryFormationSourceView_payload(value, visitor);
}

template <class Visitor>
inline void
visit_payload(const ::hanmo::resident::MilitaryCompletedReturnSource &value,
              Visitor &&visitor) {
  visit_MilitaryCompletedReturnSource_payload(value, visitor);
}

template <class Visitor>
inline void visit_payload(
    const ::hanmo::resident::MilitaryCompletedReturnSourceBuildView &value,
    Visitor &&visitor) {
  visit_MilitaryCompletedReturnSource_payload(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryFifoTransferEffect1 &value,
                  Visitor &&visitor) {
  visit_MilitaryFifoTransferEffect1_members(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitaryFifoTransferEffectBuildView &value,
      Visitor &&visitor) {
  visit_MilitaryFifoTransferEffect1_members(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryWalletDeltaEffect1 &value,
                  Visitor &&visitor) {
  visit_MilitaryWalletDeltaEffect1_members(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitaryWalletDeltaEffectBuildView &value,
      Visitor &&visitor) {
  visit_MilitaryWalletDeltaEffect1_members(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitaryWalletReservationEffect1 &value,
      Visitor &&visitor) {
  visit_MilitaryWalletReservationEffect1_members(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitaryWalletReservationEffectBuildView &value,
      Visitor &&visitor) {
  visit_MilitaryWalletReservationEffect1_members(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryWalletCreationEffect1 &value,
                  Visitor &&visitor) {
  visit_MilitaryWalletCreationEffect1_members(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitaryWalletCreationEffectBuildView &value,
      Visitor &&visitor) {
  visit_MilitaryWalletCreationEffect1_members(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitaryInventoryCreationEffect1 &value,
      Visitor &&visitor) {
  visit_MilitaryInventoryCreationEffect1_members(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitaryInventoryCreationEffectBuildView &value,
      Visitor &&visitor) {
  visit_MilitaryInventoryCreationEffect1_members(value, visitor);
}

template <class Visitor>
inline void
visit_payload(const ::hanmo::resident::MilitaryActionResourceEffect1 &value,
              Visitor &&visitor) {
  visit_military_action_resource_payload(value, visitor);
}

template <class Visitor>
inline void visit_payload(
    const ::hanmo::resident::MilitaryActionResourceEffectBuildView &value,
    Visitor &&visitor) {
  visit_military_action_resource_payload(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitaryReturnResourceOpening1 &value,
      Visitor &&visitor) {
  visit_military_return_resource_opening_members(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitaryReturnResourceOpeningBuildView &value,
      Visitor &&visitor) {
  visit_military_return_resource_opening_members(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitaryInventoryReservationEffect1 &value,
      Visitor &&visitor) {
  visit_military_inventory_reservation_effect_members(value, visitor);
}

template <class Visitor>
inline void visit(
    const ::hanmo::resident::MilitaryInventoryReservationEffectBuildView &value,
    Visitor &&visitor) {
  visit_military_inventory_reservation_effect_members(value, visitor);
}

template <class Visitor>
inline void
visit_payload(const ::hanmo::resident::MilitaryFormationReservations1 &value,
              Visitor &&visitor) {
  visit_military_formation_reservations_payload(value, visitor);
}

template <class Visitor>
inline void visit_payload(
    const ::hanmo::resident::MilitaryFormationReservationsBuildView &value,
    Visitor &&visitor) {
  visit_military_formation_reservations_payload(value, visitor);
}
