// 职责：集中空间与旅行中的有限拥有字段访问；仅借用原字段，不改变顺序或失败行为。
// 由中央登记头在 source_owned_fields
// 命名空间内包含；依赖与具名助手已先行可见。

template <class Visitor>
inline void visit(const ::hanmo::resident::ActorCarrierRef &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ActorGroupInitiator &value,
                  Visitor &&visitor) {
  visitor(value.initiator_actor_id);
  visitor(value.authority_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ArmyCarrierRef &value,
                  Visitor &&visitor) {
  visitor(value.army_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BattleMovementWriter &value,
                  Visitor &&visitor) {
  visitor(value.session_id);
  visitor(value.handoff_id);
  visitor(value.generation);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BoundaryCrossingReceipt &value,
                  Visitor &&visitor) {
  visitor(value.receipt_id);
  visitor(value.execution);
  visitor(value.carrier);
  visitor(value.composition_revision);
  visitor(value.manifest_revision);
  visitor(value.boundary);
  visitor(value.entry_at);
  visitor(value.entry_ordinal);
  visitor(value.decision_source_ref);
  visitor(value.payment_refs);
  visitor(value.effect_use_refs);
  visitor(value.safe_exit_binding_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CargoHandlingExecution &value,
                  Visitor &&visitor) {
  visitor(value.execution_id);
  visitor(value.terms_ref);
  visitor(value.business_source);
  visitor(value.batch_ordinal);
  visitor(value.phase);
  visitor(value.started_at);
  visitor(value.timing);
  visitor(value.labor_binding_refs);
  visitor(value.facility_claim_ref);
  visitor(value.progress_source_revision);
  visitor(value.work_result_ref);
  visitor(value.terminal_result_ref);
  visitor(value.waiting_cause_source_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CargoHandlingResult &value,
                  Visitor &&visitor) {
  visitor(value.result_id);
  visitor(value.execution_ref);
  visitor(value.terms_ref);
  visitor(value.business_source);
  visitor(value.batch_ordinal);
  visitor(value.status);
  visitor(value.occurred_at);
  visitor(value.work_result_ref);
  visitor(value.actual_transfer_source_refs);
  visitor(value.actual_resource_effect_refs);
  visitor(value.actual_lot_flow_refs);
  visitor(value.carrier_manifest_change_refs);
  visitor(value.original_business_result_refs);
  visitor(value.released_labor_source_refs);
  visitor(value.released_claim_source_refs);
  visitor(value.reason_source_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CargoHandlingState &value,
                  Visitor &&visitor) {
  visitor(value.terms);
  visitor(value.executions);
  visitor(value.work_results);
  visitor(value.results);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CargoHandlingTerms &value,
                  Visitor &&visitor) {
  visitor(value.terms_id);
  visitor(value.business_source);
  visitor(value.batch_ordinal);
  visitor(value.operation);
  visitor(value.carrier);
  visitor(value.source_inventory_id);
  visitor(value.destination_inventory_id);
  visitor(value.cargo);
  visitor(value.matching_owners);
  visitor(value.source_reservation);
  visitor(value.destination_owner);
  visitor(value.handling_rule_ref);
  visitor(value.accepted_rule_version);
  visitor(value.actual_site_or_anchor_ref);
  visitor(value.carrier_composition_source_ref);
  visitor(value.source_custody_ref);
  visitor(value.receiver_acceptance_source_ref);
  visitor(value.expense_authority_ref);
  visitor(value.cost_terms_ref);
  visitor(value.labor_selection);
  visitor(value.facility_claim_ref);
  visitor(value.accepted_at);
  visitor(value.actual_start_cost_sources);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CargoHandlingWorkResult &value,
                  Visitor &&visitor) {
  visitor(value.result_id);
  visitor(value.execution_ref);
  visitor(value.terms_ref);
  visitor(value.service_started_at);
  visitor(value.service_completed_at);
  visitor(value.timing_source_ref);
  visitor(value.labor_coverage_source_refs);
  visitor(value.facility_use_source_refs);
  visitor(value.cost_source_refs);
  visitor(value.rule_ref);
  visitor(value.rule_version);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CargoSelection &value,
                  Visitor &&visitor) {
  visitor(value.inventory_id);
  visitor(value.lot_id);
  visitor(value.quantity);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CarriedGroupMovement &value,
                  Visitor &&visitor) {
  visitor(value.vessel_binding_refs);
  visitor(value.carrier_group_movement_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CarriedLocation &value,
                  Visitor &&visitor) {
  visitor(value.carrier_kind);
  visitor(value.carrier_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CarriedMovementWriter &value,
                  Visitor &&visitor) {
  visitor(value.carrier_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CarrierPosition &value,
                  Visitor &&visitor) {
  visitor(value.position_id);
  visitor(value.carrier_ref);
  visitor(value.location);
  visitor(value.source_ref);
  visitor(value.revision);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::CivilianDirectJourneyResponsibility &value,
      Visitor &&visitor) {
  visitor(value.current_operator_binding);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::CivilianServiceJourneyResponsibility &value,
      Visitor &&visitor) {
  visitor(value.accepted_scope);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CohortCargoHandlingTiming &value,
                  Visitor &&visitor) {
  visitor(value.scale_ref);
  visitor(value.required_units);
  visitor(value.credited_units);
  visitor(value.last_settled_at);
  visitor(value.participation_source_refs);
  visitor(value.state);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CohortCargoHandlingWorkers &value,
                  Visitor &&visitor) {
  visitor(value.allocation_request_refs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ContactEndpoint &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.anchor);
  visitor(value.trace);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ContactHit &value,
                  Visitor &&visitor) {
  visitor(value.a);
  visitor(value.b);
  visitor(value.time_num);
  visitor(value.time_den);
  visitor(value.first);
  visitor(value.second);
  visitor(value.road);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ContactPoint &value,
                  Visitor &&visitor) {
  visitor(value.x);
  visitor(value.y);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ContactSegment &value,
                  Visitor &&visitor) {
  visitor(value.from);
  visitor(value.to);
  visitor(value.start_cost);
  visitor(value.end_cost);
  visitor(value.point_index);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CustodyExitMovementExecution &value,
                  Visitor &&visitor) {
  visitor(value.escape_action_id);
  visitor(value.exit_terms_id);
  visitor(value.segment_ordinal);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DirectGroupMovement &value,
                  Visitor &&visitor) {
  visitor(value.group_movement_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ExactContactMotionWitness &value,
                  Visitor &&visitor) {
  visitor(value.unit_ref);
  visitor(value.basis_ref);
  visitor(value.original_mapping);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ExactMovementContact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.original_hit);
  visitor(value.window_from);
  visitor(value.window_duration);
  visitor(value.occurred_at);
  visitor(value.first_prefix_id);
  visitor(value.second_prefix_id);
  visitor(value.first_motion_source);
  visitor(value.second_motion_source);
  visitor(value.exact_road_fraction);
  visitor(value.geometry_profile);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ExactTerrainCost &value,
                  Visitor &&visitor) {
  visitor(value.value);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FleetCarrierRef &value,
                  Visitor &&visitor) {
  visitor(value.fleet_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FollowingLocation &value,
                  Visitor &&visitor) {
  visitor(value.party_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FrozenPassageSafeBoundary &value,
                  Visitor &&visitor) {
  visitor(value.original_boundary);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FrozenPassageSegmentEnd &value,
                  Visitor &&visitor) {
  visitor(value.actual_end);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GroupExpenseContactCause &value,
                  Visitor &&visitor) {
  visitor(value.actual_contact);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GroupExpensePrefixCause &value,
                  Visitor &&visitor) {
  visitor(value.first_actual_span);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GroupJourneyStep &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.journey_id);
  visitor(value.ordinal);
  visitor(value.span);
  visitor(value.from_anchor);
  visitor(value.to_anchor);
  visitor(value.actual_progress);
  visitor(value.cost_refs);
  visitor(value.exertion_refs);
  visitor(value.contact_refs);
  visitor(value.outcome);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GroupJourneyTerminal &value,
                  Visitor &&visitor) {
  visitor(value.journey_id);
  visitor(value.actual_terminal_source);
  visitor(value.source_schema);
  visitor(value.occurred_at);
  visitor(value.outcome);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GroupMotionProfile &value,
                  Visitor &&visitor) {
  visitor(value.actual_rule_source);
  visitor(value.source_schema);
  visitor(value.original_profile);
  visitor(value.additional_cost_per_unit);
  visitor(value.fare_payee_wallet_id);
  visitor(value.fare_destination_id);
  visitor(value.original_load_total);
  visitor(value.original_load_people);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GroupMovementBinding &value,
                  Visitor &&visitor) {
  visitor(value.carrier);
  visitor(value.position_ref);
  visitor(value.execution_owner);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GroupMovementControlSource &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.journey_id);
  visitor(value.operation);
  visitor(value.authority_source);
  visitor(value.occurred_at);
  visitor(value.actual_anchor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GroupMovementExecution &value,
                  Visitor &&visitor) {
  visitor(value.journey_id);
  visitor(value.carrier);
  visitor(value.segment_ordinal);
}

template<class Visitor>
inline void visit(const ::hanmo::resident::GroupMilitarySupplyRebindSource& value,Visitor&& visitor){
  visitor(value.id);
  visitor(value.journey_id);
  visitor(value.army_id);
  visitor(value.prefix_ref);
  visitor(value.unit_ref);
  visitor(value.basis_ref);
  visitor(value.cause_fact_ids);
  visitor(value.occurred_at);
  visitor(value.before_composition_revision);
  visitor(value.after_composition_revision);
  visitor(value.before_profile);
  visitor(value.after_profile);
  visitor(value.before_manifest_sources);
  visitor(value.after_manifest_sources);
  visitor(value.disposition); visitor(value.original_mapping);
  visitor(value.original_cursor);
  visitor(value.original_active_elapsed);
  visitor(value.before_budget);
  visitor(value.after_budget);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GroupMovementExecutionState &value,
                  Visitor &&visitor) {
  visitor(value.openings);
  visitor(value.unit_terms);
  visitor(value.expenses);
  visitor(value.controls);
  visitor(value.military_supply_rebinds);
  visitor(value.entry_manifests);
  visitor(value.contact_member_releases);
  visitor(value.member_loss_handoffs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GroupMovementJourney &value,
                  Visitor &&visitor) {
  visitor(value.journey_id);
  visitor(value.carrier);
  visitor(value.position_ref);
  visitor(value.composition_revision);
  visitor(value.initiator);
  visitor(value.sponsor);
  visitor(value.payer_wallet_id);
  visitor(value.expense_authority);
  visitor(value.shipment_refs);
  visitor(value.route_profile);
  visitor(value.movement_execution);
  visitor(value.started_at);
  visitor(value.last_step_at);
  visitor(value.state);
  visitor(value.stop_reason);
  visitor(value.expense_source_refs);
  visitor(value.movement_source_refs);
  visitor(value.movement_prefix_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GroupMovementOpeningSource &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.journey_id);
  visitor(value.carrier_origin);
  visitor(value.composition_revision);
  visitor(value.occurred_at);
  visitor(value.original_anchor);
  visitor(value.motion_profile);
  visitor(value.manifest_sources);
  visitor(value.named_participants);
  visitor(value.military_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GroupMovementUnitTerms &value,
                  Visitor &&visitor) {
  visitor(value.unit_ref);
  visitor(value.journey_id);
  visitor(value.motion_profile);
  visitor(value.fare_amount);
  visitor(value.actual_start_source);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GroupTransportExpenseSource &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.journey_id);
  visitor(value.prefix_ref);
  visitor(value.unit_ref);
  visitor(value.occurred_at);
  visitor(value.actual_cause);
  visitor(value.original_payment_receipt);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::HeldMovementMap &value,
                  Visitor &&visitor) {
  visitor(value.held_cursor);
  visitor(value.actual_hold_source);
  visitor(value.effective_at);
  visitor(value.resume_rate);
  visitor(value.resume_budget_rate);
  visitor(value.resume_rate_source);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::HouseholdPolicyGroupInitiator &value,
                  Visitor &&visitor) {
  visitor(value.accepted_household_policy_ref);
  visitor(value.policy_revision);
  visitor(value.purchase_plan_ref);
  visitor(value.purchase_ref);
  visitor(value.phase);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::HouseholdPurchaseExpenseAuthority &value,
      Visitor &&visitor) {
  visitor(value.household_id);
  visitor(value.accepted_policy_ref);
  visitor(value.policy_revision);
  visitor(value.purchase_plan_ref);
  visitor(value.purchase_ref);
  visitor(value.phase);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::InArmyLocation &value,
                  Visitor &&visitor) {
  visit_InArmyLocation_members(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::InArmyLocationBuildView &value,
                  Visitor &&visitor) {
  visit_InArmyLocation_members(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::InventorySpatialBinding &value,
                  Visitor &&visitor) {
  visitor(value.inventory_id);
  visitor(value.placement);
  visitor(value.source_ref);
  visitor(value.revision);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::JourneyPurposeBinding &value,
                  Visitor &&visitor) {
  visitor(value.journey_id);
  visitor(value.purpose);
  visitor(value.original_responsibility);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryJourneyResponsibility &value,
                  Visitor &&visitor) {
  visitor(value.original_intent_or_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryMarchExpenseAuthority &value,
                  Visitor &&visitor) {
  visitor(value.accepted_order_source);
  visitor(value.army_id);
  visitor(value.campaign_id);
  visitor(value.payer_wallet_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MobileInventoryAdoptionView &value,
                  Visitor &&visitor) {
  visitor(value.inventory_id);
  visitor(value.vehicle_ref);
  visitor(value.original_container_source);
  visitor(value.spatial_source);
  visitor(value.capacity_source);
  visitor(value.adopted_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MovementActive &value,
                  Visitor &&visitor) {
  (void)value;
  (void)visitor;
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MovementActivitySegment &value,
                  Visitor &&visitor) {
  visitor(value.span);
  visitor(value.cursor_from);
  visitor(value.cursor_to);
  visitor(value.rate);
  visitor(value.budget_rate);
  visitor(value.rate_source);
  visitor(value.basis_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MovementArrivalSource &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.prefix_id);
  visitor(value.unit_ref);
  visitor(value.execution);
  visitor(value.occurred_at);
  visitor(value.actual_anchor);
  visitor(value.basis_ref);
  visitor(value.cursor);
  visitor(value.reached_points);
  visitor(value.original_point_count);
  visitor(value.destination_settlement_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MovementBasisTransitionSource &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.prefix_id);
  visitor(value.unit_ref);
  visitor(value.execution);
  visitor(value.occurred_at);
  visitor(value.old_basis_ref);
  visitor(value.old_cursor);
  visitor(value.new_basis_ref);
  visitor(value.actual_anchor);
  visitor(value.original_reason_source);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MovementClosed &value,
                  Visitor &&visitor) {
  visitor(value.original_result_ref);
  visitor(value.actual_anchor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::MovementContactContinuationSource &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.prefix_id);
  visitor(value.contact_ref);
  visitor(value.closed_unit_ref);
  visitor(value.new_unit_ref);
  visitor(value.actual_resolution_source);
  visitor(value.resolution_schema);
  visitor(value.occurred_at);
  visitor(value.held);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::MovementContactInstallationSource &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.contact_ref);
  visitor(value.actual_business_source);
  visitor(value.business_source_schema);
  visitor(value.occurred_at);
}

// 独立边界扩展只拥有这一条真实单 prefix 源，不复制运动或军队根。
template <class Visitor>
inline void visit(const ::hanmo::resident::MovementBoundaryContact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.prefix_id);
  visitor(value.original_field_source);
  visitor(value.field_hash);
  visitor(value.prediction_span);
  visitor(value.entry_fraction);
  visitor(value.occurred_at);
  visitor(value.motion_source);
  visitor(value.actual_anchor);
}

// 原续行记录的真实九字段；复制与留存共用，不改变 wire 或来源校验。
template <class Visitor>
inline void visit(const ::hanmo::resident::MovementBoundaryContinuationSource &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.prefix_id);
  visitor(value.closed_unit_ref);
  visitor(value.new_unit_ref);
  visitor(value.boundary_contact);
  visitor(value.field_contact);
  visitor(value.terminal_segment);
  visitor(value.occurred_at);
  visitor(value.held);
}

// 独立 local-move 来源的真实持值遍历；删除记账后迁入 source_owned_fields 同一位置。
template <class Visitor>
inline void visit(const ::hanmo::resident::LocalMoveSource &value, Visitor &&visitor) {
  visitor(value.id); visitor(value.actor_id); visitor(value.occurred_at);
  visitor(value.before_prefix.common_fact_count); visitor(value.before_prefix.last_common_fact);
  visitor(value.before_head); visitor(value.from); visitor(value.to);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MovementExactState &value,
                  Visitor &&visitor) {
  visitor(value.local_moves);
  visitor(value.boundary_continuations);
  visitor(value.boundary_contacts);
  visitor(value.passage_modifier_adoptions);
  visitor(value.patrol_final_motions);
  visitor(value.patrol_checkpoint_arrivals);
  visitor(value.cargo_handling);
  visitor(value.personal_controls);
  visitor(value.contact_continuations);
  visitor(value.unit_boundaries);
  visitor(value.contact_installations);
  visitor(value.personal_terminals);
  visitor(value.prefixes);
  visitor(value.units);
  visitor(value.spatial_bindings);
  visitor(value.contacts);
  visitor(value.credit_dispositions);
  visitor(value.arrivals);
  visitor(value.expenses);
  visitor(value.exertions);
  visitor(value.exertion_bases);
  visitor(value.unit_closes);
  visitor(value.basis_transitions);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MovementExpenseSource &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.prefix_id);
  visitor(value.unit_ref);
  visitor(value.payer_wallet_id);
  visitor(value.execution);
  visitor(value.occurred_at);
  visitor(value.amount);
  visitor(value.balance_before);
  visitor(value.balance_after);
  visitor(value.original_cost_rule_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MovementHeld &value,
                  Visitor &&visitor) {
  visitor(value.original_reason_source);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MovementPaymentConsumed &value,
                  Visitor &&visitor) {
  visitor(value.actual_expense_source);
  visitor(value.payer_wallet_id);
  visitor(value.amount);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MovementPaymentNotConsumed &value,
                  Visitor &&visitor) {
  (void)value;
  (void)visitor;
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MovementPrefixState &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.execution);
  visitor(value.basis);
  visitor(value.cursor);
  visitor(value.activity_world_map);
  visitor(value.last_settled_at);
  visitor(value.unit_ref);
  visitor(value.lifecycle);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MovementUnitBoundarySource &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.prefix_id);
  visitor(value.unit_ref);
  visitor(value.basis_ref);
  visitor(value.execution);
  visitor(value.occurred_at);
  visitor(value.cursor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MovementUnitCloseSource &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.prefix_id);
  visitor(value.unit_ref);
  visitor(value.execution);
  visitor(value.kind);
  visitor(value.occurred_at);
  visitor(value.cursor);
  visitor(value.actual_anchor);
  visitor(value.actual_cause);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MovementUnitFinalized &value,
                  Visitor &&visitor) {
  visitor(value.original_close_source);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MovementUnitOpen &value,
                  Visitor &&visitor) {
  (void)value;
  (void)visitor;
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MovementUnitState &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.execution);
  visitor(value.ordinal);
  visitor(value.active_elapsed);
  visitor(value.budget);
  visitor(value.payment);
  visitor(value.effort_basis_refs);
  visitor(value.naval_finalized_source);
  visitor(value.status);
  visitor(value.activity_spans);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NamedCargoHandlingTiming &value,
                  Visitor &&visitor) {
  visitor(value.action_ref);
  visitor(value.action_elapsed_source_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NamedCargoHandlingWorkers &value,
                  Visitor &&visitor) {
  visitor(value.lead_actor_id);
  visitor(value.participant_actor_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NoTerrainBudget &value,
                  Visitor &&visitor) {
  (void)value;
  (void)visitor;
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OnRouteLocation &value,
                  Visitor &&visitor) {
  visitor(value.edge_id);
  visitor(value.origin);
  visitor(value.progress);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OrdinaryJourneyPurpose &value,
                  Visitor &&visitor) {
  (void)value;
  (void)visitor;
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OriginalMovementAnchor &value,
                  Visitor &&visitor) {
  visit_OriginalMovementAnchor_members(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitaryActionPlacementAnchorBuildView &value,
      Visitor &&visitor) {
  visit_OriginalMovementAnchor_members(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::ActorSpatialProjectionAnchorBuildView &value,
      Visitor &&visitor) {
  visit_OriginalMovementAnchor_members(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OverloadSafetyJourneyPurpose &value,
                  Visitor &&visitor) {
  visitor(value.incident_id);
  visitor(value.response_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PartyCarrierRef &value,
                  Visitor &&visitor) {
  visitor(value.party_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PassageBoundaryRef &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.id);
  visitor(value.map_id);
  visitor(value.from_site_id);
  visitor(value.to_site_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PassageModifierAdoption &value,
                  Visitor &&visitor) {
  visitor(value.adoption_id);
  visitor(value.execution);
  visitor(value.segment_ref);
  visitor(value.source_id);
  visitor(value.source_revision);
  visitor(value.effect_id);
  visitor(value.effect_revision);
  visitor(value.modifier_rule_version);
  visitor(value.admitted_at);
  visitor(value.frozen_completion_boundary);
  visitor(value.actual_removed_contribution);
  visitor(value.effect_use_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PatrolCheckpointArrival2 &value,
                  Visitor &&visitor) {
  visitor(value.source_fact_id);
  visitor(value.journey_id);
  visitor(value.final_motion_source_ref);
  visitor(value.actor_or_group_ref);
  visitor(value.checkpoint_ref);
  visitor(value.arrived_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PatrolFinalMotionSource &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.prefix_id);
  visitor(value.unit_ref);
  visitor(value.basis_ref);
  visitor(value.execution);
  visitor(value.occurred_at);
  visitor(value.cursor);
  visitor(value.actual_anchor);
  visitor(value.final_contact_source);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PersonalMovementControlSource &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.actor_id);
  visitor(value.action_id);
  visitor(value.prefix_id);
  visitor(value.unit_ref);
  visitor(value.basis_ref);
  visitor(value.operation);
  visitor(value.occurred_at);
  visitor(value.cursor);
  visitor(value.original_execution_source);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PersonalMovementExecution &value,
                  Visitor &&visitor) {
  visitor(value.action_id);
  visitor(value.journey_id);
  visitor(value.segment_ordinal);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PersonalMovementTerminalSource &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.prefix_id);
  visitor(value.actual_action);
  visitor(value.opening_source);
  visitor(value.actual_close_source);
  visitor(value.occurred_at);
  visitor(value.kind);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PointI &value, Visitor &&visitor) {
  visitor(value.x);
  visitor(value.y);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PrivateMandateExpenseAuthority &value,
      Visitor &&visitor) {
  visitor(value.mandate_id);
  visitor(value.accepted_terms_revision);
  visitor(value.authority_epoch);
  visitor(value.payer_wallet_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PublicBudgetExpenseAuthority &value,
                  Visitor &&visitor) {
  visitor(value.execution_ref);
  visitor(value.payer_wallet_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RemovedSurgeBarrier &value,
                  Visitor &&visitor) {
  visitor(value.blocks_entry);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RemovedViolentCurrent &value,
                  Visitor &&visitor) {
  visitor(value.speed_bp);
  visitor(value.extra_fatigue_units);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RescueParams &value,
                  Visitor &&visitor) {
  visitor(value.origin);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RoadContactEndpoint &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.anchor);
  visitor(value.trace);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RoadContactEndpoints &value,
                  Visitor &&visitor) {
  visitor(value.first);
  visitor(value.second);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RoadFractionalAnchor &value,
                  Visitor &&visitor) {
  visitor(value.edge_id);
  visitor(value.origin);
  visitor(value.progress);
  visitor(value.actual_source);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RoadMovementBasis &value,
                  Visitor &&visitor) {
  visitor(value.basis_ref);
  visitor(value.original_inputs);
  visitor(value.start_anchor);
  visitor(value.end_anchor);
  visitor(value.exact_start_progress);
  visitor(value.exact_end_progress);
  visitor(value.continued);
  visitor(value.catalog_rule_ref);
  visitor(value.original_unit_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RoadTravelMotion &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.action_id);
  visitor(value.edge_id);
  visitor(value.origin);
  visitor(value.start);
  visitor(value.end);
  visitor(value.moving);
  visitor(value.params);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RoadTravelParams &value,
                  Visitor &&visitor) {
  visitor(value.destination);
  visitor(value.via);
  visitor(value.path);
  visitor(value.edge_index);
  visitor(value.payer_wallet_id);
  visitor(value.returning);
  visitor(value.paused);
  visitor(value.reverse_first);
  visitor(value.has_terrain);
  visitor(value.has_paused);
  visitor(value.has_reverse_first);
  visitor(value.has_encounter_id);
  visitor(value.has_met_actions);
  visitor(value.encounter_id);
  visitor(value.met_actions);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RunningMovementMap &value,
                  Visitor &&visitor) {
  visitor(value.world_from);
  visitor(value.cursor_at_from);
  visitor(value.rate);
  visitor(value.budget_rate);
  visitor(value.rate_source);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SelfFundedExpenseAuthority &value,
                  Visitor &&visitor) {
  visitor(value.funding_terms_id);
  visitor(value.principal_actor_id);
  visitor(value.payer_wallet_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SettlementLocation &value,
                  Visitor &&visitor) {
  visit_SettlementLocation_members(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SettlementLocationBuildView &value,
                  Visitor &&visitor) {
  visit_SettlementLocation_members(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ShoreAssaultMovementExecution &value,
                  Visitor &&visitor) {
  visitor(value.landing_operation_id);
  visitor(value.crossing_action_id);
  visitor(value.segment_ordinal);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SpatialCarrierState &value,
                  Visitor &&visitor) {
  visitor(value.transport_units);
  visitor(value.positions);
  visitor(value.bindings);
  visitor(value.journeys);
  visitor(value.steps);
  visitor(value.terminals);
  visitor(value.inventory_bindings);
  visitor(value.crossings);
  visitor(value.journey_purpose_bindings);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SpatialWriterBinding &value,
                  Visitor &&visitor) {
  visitor(value.subject_kind);
  visitor(value.subject_id);
  visitor(value.writer);
  visitor(value.binding_source);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StableMovementWriter &value,
                  Visitor &&visitor) {
  visitor(value.anchor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TerrainContinuousAnchor &value,
                  Visitor &&visitor) {
  visitor(value.map_id);
  visitor(value.origin);
  visitor(value.x_bits);
  visitor(value.y_bits);
  visitor(value.actual_prefix_close_source);
  visitor(value.geometry_profile_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TerrainCostBasis &value,
                  Visitor &&visitor) {
  visitor(value.basis_ref);
  visitor(value.original_inputs);
  visitor(value.start_anchor);
  visitor(value.end_anchor);
  visitor(value.parameter_limit);
  visitor(value.pieces);
  visitor(value.actual_consumed_total);
  visitor(value.reached_points);
  visitor(value.original_point_count);
  visitor(value.original_end_credit);
  visitor(value.original_closing_policy);
  visitor(value.continued);
  visitor(value.catalog_rule_ref);
  visitor(value.original_unit_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TerrainCostPiece &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.segment);
  visitor(value.cost_from);
  visitor(value.cost_to);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::TerrainCreditDispositionSource &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.prefix_id);
  visitor(value.unit_ref);
  visitor(value.basis_ref);
  visitor(value.execution);
  visitor(value.cursor);
  visitor(value.consumed);
  visitor(value.before_credit);
  visitor(value.after_credit);
  visitor(value.discarded);
  visitor(value.kind);
  visitor(value.original_close_source);
  visitor(value.occurred_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TerrainExecution &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.action_id);
  visitor(value.plan_id);
  visitor(value.map_id);
  visitor(value.action_start_tick);
  visitor(value.source_tick);
  visitor(value.route_index_before);
  visitor(value.route_index_after);
  visitor(value.travel_credit_before);
  visitor(value.travel_credit_after);
  visitor(value.from_point);
  visitor(value.to_point);
  visitor(value.result_location);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TerrainJourneyFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.actor_id);
  visitor(value.action_id);
  visitor(value.map_id);
  visitor(value.mode);
  visitor(value.tick);
  visitor(value.budget);
  visitor(value.starting_load);
  visitor(value.origin);
  visitor(value.destination);
  visitor(value.points);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TerrainTravelParams &value,
                  Visitor &&visitor) {
  visitor(value.plan_id);
  visitor(value.map_id);
  visitor(value.origin);
  visitor(value.payer_wallet_id);
  visitor(value.mode);
  visitor(value.route_index);
  visitor(value.budget);
  visitor(value.travel_credit);
  visitor(value.money_per_tick);
  visitor(value.fatigue_per_tick);
  visitor(value.points);
  visitor(value.destination);
  visitor(value.paused);
  visitor(value.encounter_id);
  visitor(value.has_encounter_id);
  visitor(value.has_met_actions);
  visitor(value.met_actions);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TerrainUnitBudget &value,
                  Visitor &&visitor) {
  visitor(value.opening_credit);
  visitor(value.funded);
  visitor(value.spent);
  visitor(value.discarded);
  visitor(value.disposition_sources);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TransportEntryManifestSource &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.execution);
  visitor(value.entry_ordinal);
  visitor(value.boundary);
  visitor(value.occurred_at);
  visitor(value.manifest);
  visitor(value.inventory_sources);
  visitor(value.population_sources);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TransportInventoryManifest &value,
                  Visitor &&visitor) {
  visitor(value.entry_evidence);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TransportManifestRead &value,
                  Visitor &&visitor) {
  visitor(value.carrier);
  visitor(value.manifest_revision);
  visitor(value.composition_revision);
  visitor(value.at_tick);
  visitor(value.actual_anchor);
  visitor(value.source_refs);
  visitor(value.named_actor_refs);
  visitor(value.vehicle_refs);
  visitor(value.inventory_refs);
  visitor(value.civilian_population_refs);
  visitor(value.military_force_refs);
  visitor(value.vessel_placements);
  visitor(value.persons);
  visitor(value.working_crew);
  visitor(value.weight);
  visitor(value.read_stamp);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TransportPopulationManifest &value,
                  Visitor &&visitor) {
  visitor(value.population_ref);
  visitor(value.population_revision);
  visitor(value.actual);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TransportPopulationSource &value,
                  Visitor &&visitor) {
  visitor(value.source_ref);
  visitor(value.cohort_id);
  visitor(value.persons);
  visitor(value.working_crew);
  visitor(value.named_actor_ids);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::TransportRegistrationAuthority &value,
      Visitor &&visitor) {
  visitor(value.business_source);
  visitor(value.subject);
  visitor(value.initiator);
  visitor(value.accepted_scope_revision);
  visitor(value.labor_until_tick);
  visitor(value.accepted_selection_ids);
  visitor(value.named_participants);
  visitor(value.vehicle_refs);
  visitor(value.inventory_ids);
  visitor(value.custody_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TransportRegistrationSource &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.transport_unit_id);
  visitor(value.accepted_scope);
  visitor(value.occurred_at);
  visitor(value.departure_anchor);
  visitor(value.selections);
  visitor(value.actual_vehicle_takeover_refs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TransportRegistrationState &value,
                  Visitor &&visitor) {
  visitor(value.registrations);
  visitor(value.releases);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::TransportSelectionRegistration &value,
      Visitor &&visitor) {
  visitor(value.selection_id);
  visitor(value.departure_anchor_ref);
  visitor(value.release_allocation_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TransportUnitCarrierRef &value,
                  Visitor &&visitor) {
  visitor(value.transport_unit_ref);
  visitor(value.composition_revision);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TransportUnitRef &value,
                  Visitor &&visitor) {
  visitor(value.subject);
  visitor(value.transport_unit_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TransportUnitReleaseSource &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.transport_unit_id);
  visitor(value.handover_site_id);
  visitor(value.receiver_custody_ref);
  visitor(value.composition_revision);
  visitor(value.occurred_at);
  visitor(value.actual_anchor);
  visitor(value.accepted_handover_source);
  visitor(value.released_composition);
  visitor(value.cargo_handover_refs);
  visitor(value.participant_release_refs);
  visitor(value.vehicle_release_refs);
  visitor(value.obligation_disposition_refs);
  visitor(value.population_binding_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TransportUnitState &value,
                  Visitor &&visitor) {
  visitor(value.transport_unit_id);
  visitor(value.subject);
  visitor(value.registration_source_ref);
  visitor(value.composition_revision);
  visitor(value.population_binding_refs);
  visitor(value.cohort_allocation_refs);
  visitor(value.named_participants);
  visitor(value.vehicle_refs);
  visitor(value.inventory_ids);
  visitor(value.custody_ref);
  visitor(value.position_ref);
  visitor(value.active_journey_id);
  visitor(value.state);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TravelCommand &value,
                  Visitor &&visitor) {
  visitor(value.destination);
  visitor(value.via);
  visitor(value.has_destination);
  visitor(value.has_via);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TravelExertionBasis &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.unit_ref);
  visitor(value.execution);
  visitor(value.original_action);
  visitor(value.opened_at);
  visitor(value.actual_participants);
  visitor(value.original_fatigue_per_unit);
  visitor(value.original_rule_source);
  visitor(value.actual_basis);
  visitor(value.opened_cursor);
  visitor(value.accepted_funding_plan);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TravelExertionSource &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.prefix_id);
  visitor(value.unit_ref);
  visitor(value.execution);
  visitor(value.actual_span);
  visitor(value.active_elapsed);
  visitor(value.original_fatigue_per_unit);
  visitor(value.actual_participants);
  visitor(value.original_close_source);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TravelItinerary &value,
                  Visitor &&visitor) {
  visitor(value.index);
  visitor(value.ready_tick);
  visitor(value.travelling);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TravelMotion &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.action_id);
  visitor(value.map_id);
  visitor(value.origin);
  visitor(value.params);
  visitor(value.segments);
  visitor(value.budget);
  visitor(value.end_credit);
  visitor(value.reached_points);
  visitor(value.end);
  visitor(value.moving);
  visitor(value.stationary);
  visitor(value.stationary_location);
  visitor(value.read_versions);
  visitor(value.cost_piece_kinds);
  visitor(value.actual_consumed_total);
  visitor(value.original_point_count);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TravelProfile &value,
                  Visitor &&visitor) {
  visitor(value.mode);
  visitor(value.payer_wallet_id);
  visitor(value.load_weight);
  visitor(value.naval_budget);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TravelState &value,
                  Visitor &&visitor) {
  visitor(value.actions);
  visitor(value.road_actions);
  visitor(value.rescues);
  visitor(value.journey_facts);
  visitor(value.itineraries);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VesselManifestPlacement &value,
                  Visitor &&visitor) {
  visitor(value.vessel_id);
  visitor(value.placement_source);
  visitor(value.named_actor_ids);
  visitor(value.inventory_ids);
  visitor(value.population_sources);
  visitor(value.persons);
  visitor(value.weight);
  visitor(value.maximum_persons);
  visitor(value.maximum_weight);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::WildernessLocation &value,
                  Visitor &&visitor) {
  visitor(value.map_id);
  visitor(value.origin);
  visitor(value.point);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::WorldMovementWriter &value,
                  Visitor &&visitor) {
  visitor(value.prefix_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MobileInventoryDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.inventory_id);
  visitor(value.initial_site_id);
  visitor(value.version);
  visitor(value.container_maximum_weight);
  visitor(value.initialized_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TravelSettlement &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.entry_place);
  visitor(value.map_xy);
  visitor(value.world_map_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TravelScheduleDefinition &value,
                  Visitor &&visitor) {
  visitor(value.destinations);
  visitor(value.dwell_ticks);
  visitor(value.start_tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TravelRoad &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.origin);
  visitor(value.destination);
  visitor(value.duration_ticks);
  visitor(value.cost_per_tick);
  visitor(value.fatigue_per_tick);
  visitor(value.blocked_from_tick);
  visitor(value.blocked_until_tick);
  visitor(value.shape);
  visitor(value.world_map_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TravelPlace &value,
                  Visitor &&visitor) {
  visitor(value.settlement_id);
  visitor(value.passages);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TravelCatalog &value,
                  Visitor &&visitor) {
  visitor(value.cargo_handling_rules);
  visitor(value.mobile_inventory_definitions);
  visitor(value.world_map_id);
  visitor(value.road_encounters);
  visitor(value.maps);
  visitor(value.settlements);
  visitor(value.places);
  visitor(value.roads);
  visitor(value.authored_plans);
  visitor(value.route_cache);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::terrain::TerrainMap &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.width);
  visitor(value.height);
  visitor(value.cell_size);
  visitor(value.cells);
  visitor(value.costs);
  visitor(value.budgets);
  visitor(value.money_per_tick);
  visitor(value.fatigue_per_tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::terrain::Route &value,
                  Visitor &&visitor) {
  visitor(value.points);
  visitor(value.segment_costs);
  visitor(value.cost);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PlaceDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.settlement_id);
  visitor(value.passages);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ExactTimeProfile &value,
                  Visitor &&visitor) {
  visitor(value.profile_id);
  visitor(value.scale_ref);
  visitor(value.macro_providers);
  visitor(value.supply_report_subscriptions);
  visitor(value.complete);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ExactSupplyReportSubscription &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.report_source_keys);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ExactPhaseSelection &value,
                  Visitor &&visitor) {
  visitor(value.provider);
  visitor(value.profile_id);
  visitor(value.version);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ExactPhaseRegistration &value,
                  Visitor &&visitor) {
  visitor(value.provider);
  visitor(value.profile_id);
  visitor(value.version);
  visitor(value.stages);
  visitor(value.calendar_stages);
  visitor(value.schedule_kind);
  visitor(value.interval_writer);
  visitor(value.period_source_owner);
  visitor(value.period_closure_kind);
  visitor(value.effect_writer_keys);
  visitor(value.interval_after);
  visitor(value.opening_after);
  visitor(value.closing_after);
  visitor(value.causal_edges);
  visitor(value.validate_author_selection);
  visitor(value.validate_world_sources);
  visitor(value.predict);
  visitor(value.apply_due);
  visitor(value.adopt_interval_from);
  visitor(value.provide_interval);
  visitor(value.apply_calendar_stage);
  visitor(value.open_period);
  visitor(value.close_period);
  visitor(value.read_period_sources);
  visitor(value.validate_applied_source);
  visitor(value.derived_deadline_kinds);
  visitor(value.predict_derived_deadlines);
  visitor(value.apply_derived_deadline);
  visitor(value.validate_derived_deadline);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ExactPhaseGraph &value,
                  Visitor &&visitor) {
  visitor(value.providers);
  visitor(value.causal_edges);
  visitor(value.ordered_stages);
  visitor(value.interval_order);
  visitor(value.opening_order);
  visitor(value.closing_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ExactActionTimeSelection &value,
                  Visitor &&visitor) {
  visitor(value.action_kind);
  visitor(value.profile_id);
  visitor(value.version);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::terrain::Cell &value,
                  Visitor &&visitor) {
  visitor(value.terrain);
  visitor(value.crossing);
  visitor(value.blocked_flag);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CargoHandlingRule &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.version);
  visitor(value.allowed_carrier_kinds);
  visitor(value.allowed_operations);
  visitor(value.allowed_business_kinds);
  visitor(value.allowed_cargo_classes);
  visitor(value.handling_half_days);
  visitor(value.required_workers);
  visitor(value.labor_modes);
  visitor(value.required_capability_purpose_ref);
  visitor(value.facility_requirement_ref);
  visitor(value.cost_rule_ref);
  visitor(value.interruption_rule_ref);
}

// native/world/src/resident/domains/travel/passage_model.hpp:10; actual owning
// declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::ActorPassageRequester &value,
                  Visitor &&visitor) {
  visitor(value.acting_actor_id);
}

// native/world/src/resident/domains/travel/travel_dependent_state.hpp:6; actual
// owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::BirthDependentBindingChange &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.actual_birth_source);
  visitor(value.occurred_at);
  visitor(value.child_actor_id);
  visitor(value.delivery_actor_id);
  visitor(value.birth_arrangement_id);
  visitor(value.actual_placement);
  visitor(value.before_binding_refs);
  visitor(value.after_binding_refs);
}

// native/world/src/resident/domains/travel/birth_placement.hpp:12; actual
// owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::BirthPlacement &value,
                  Visitor &&visitor) {
  visitor(value.location);
  visitor(value.resolved_anchor);
  visitor(value.placement_kind);
  visitor(value.carrier_ref);
  visitor(value.safe_boundary_ref);
  visitor(value.placement_basis_ref);
}

// native/world/src/resident/domains/travel/travel_dependent_state.hpp:16;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::CareDependentBindingChange &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.actual_handover_source);
  visitor(value.occurred_at);
  visitor(value.child_actor_id);
  visitor(value.carer_actor_id);
  visitor(value.actual_location);
  visitor(value.resolved_anchor);
  visitor(value.before_binding_refs);
  visitor(value.after_binding_refs);
}

// native/world/src/resident/domains/travel/passage_model.hpp:11; actual owning
// declaration, no codec validation.
template <class Visitor>
inline void
visit(const ::hanmo::resident::HouseholdPolicyPassageRequester &value,
      Visitor &&visitor) {
  visitor(value.household_id);
  visitor(value.accepted_household_policy_ref);
  visitor(value.policy_revision);
  visitor(value.purchase_plan_ref);
  visitor(value.purchase_ref);
  visitor(value.phase);
}

// native/world/src/resident/domains/travel/passage_model.hpp:16; actual owning
// declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::OrdinaryPassageContext &value,
                  Visitor &&visitor) {}

// native/world/src/resident/domains/travel/passage_model.hpp:9; actual owning
// declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::PassageWindow &value,
                  Visitor &&visitor) {
  visitor(value.from_tick);
  visitor(value.until_tick);
}

// native/world/src/resident/domains/travel/passage_model.hpp:17; actual owning
// declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::SafeExitPassageContext &value,
                  Visitor &&visitor) {
  visitor(value.binding_id);
  visitor(value.line_refs);
  visitor(value.path_id);
  visitor(value.next_boundary_ordinal);
}

// native/world/src/resident/domains/travel/birth_placement.hpp:24; actual
// owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::TravelDependentBinding &value,
                  Visitor &&visitor) {
  visitor(value.binding_id);
  visitor(value.dependent_actor_id);
  visitor(value.carrier_actor_id);
  visitor(value.transport_ref);
  visitor(value.source_birth_ref);
  visitor(value.attached_at);
  visitor(value.policy);
  visitor(value.revision);
}

template <class Visitor>
inline void
visit_payload(const ::hanmo::resident::MilitaryActionPlacement &value,
              Visitor &&visitor) {
  visit_MilitaryActionPlacement_payload(value, visitor);
}

template <class Visitor>
inline void
visit_payload(const ::hanmo::resident::MilitaryActionPlacementBuildView &value,
              Visitor &&visitor) {
  visit_MilitaryActionPlacement_payload(value, visitor);
}

template <class Visitor>
inline void visit_payload(const BoundSpatialLocationWrite &value,
                          Visitor &&visitor) {
  visit_bound_spatial_location_write_payload(value, visitor);
}

template <class Visitor>
inline void visit_payload(const BoundSpatialLocationWriteBuildView &value,
                          Visitor &&visitor) {
  visit_bound_spatial_location_write_payload(value, visitor);
}

template <class Visitor>
inline void visit_payload(const ActualSpatialSourceRef &value,
                          Visitor &&visitor) {
  visit_actual_spatial_source_payload(value, visitor);
}

template <class Visitor>
inline void visit_payload(const ActualSpatialSourceBuildView &value,
                          Visitor &&visitor) {
  visit_actual_spatial_source_payload(value, visitor);
}

template <class Visitor>
inline void visit_payload(const ActualActorSpatialWriteRead &value,
                          Visitor &&visitor) {
  visit_actual_actor_spatial_write_payload(value, visitor);
}

template <class Visitor>
inline void visit_payload(const ActualActorSpatialWriteReadBuildView &value,
                          Visitor &&visitor) {
  visit_actual_actor_spatial_write_payload(value, visitor);
}
