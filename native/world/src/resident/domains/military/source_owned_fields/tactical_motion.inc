// 职责：集中军旅与战斗中的战术编组、指令和局部运动访问；仅借用原字段，不改变顺序或失败行为。
// 由中央登记头在 source_owned_fields
// 命名空间内包含；依赖与具名助手已先行可见。

template <class Visitor>
inline void visit(const ::hanmo::resident::HexPoint &value, Visitor &&visitor) {
  visitor(value.q);
  visitor(value.r);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LocalEntryConnector &value,
                  Visitor &&visitor) {
  visitor(value.admission_source_ref);
  visitor(value.chart_ref);
  visitor(value.portal_id);
  visitor(value.connector_path_ref);
  visitor(value.piece_ordinal);
  visitor(value.from_qr);
  visitor(value.to_qr);
  visitor(value.topology_anchor_source_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LocalExitConnector &value,
                  Visitor &&visitor) {
  visitor(value.exit_source_ref);
  visitor(value.chart_ref);
  visitor(value.portal_id);
  visitor(value.connector_path_ref);
  visitor(value.piece_ordinal);
  visitor(value.from_qr);
  visitor(value.to_qr);
  visitor(value.topology_anchor_source_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LocalHexMotionBasis2 &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.session_ref);
  visitor(value.carrier_ref);
  visitor(value.local_body_ref);
  visitor(value.execution_ref);
  visitor(value.intent_source_ref);
  visitor(value.geometry_definition_ref);
  visitor(value.map_version);
  visitor(value.terrain_rule_ref);
  visitor(value.unit_or_vessel_capability_source_ref);
  visitor(value.chart_ref);
  visitor(value.started_at);
  visitor(value.segments);
  visitor(value.route_terminal);
  visitor(value.original_unit_cost_source_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LocalHexNeighbor &value,
                  Visitor &&visitor) {
  visitor(value.root_edge_ref);
  visitor(value.from_vertex);
  visitor(value.to_vertex);
  visitor(value.directional_cost);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LocalMotionCursor2 &value,
                  Visitor &&visitor) {
  visitor(value.execution_ref);
  visitor(value.local_body_ref);
  visitor(value.basis_ref);
  visitor(value.segment_ordinal);
  visitor(value.credited_edge_work);
  visitor(value.last_settled_at);
  visitor(value.rate_source_ref);
  visitor(value.rate_work_per_half_day);
  visitor(value.phase);
  visitor(value.phase_source_ref);
  visitor(value.reserved_topology_refs);
  visitor(value.closed_prefix_sources);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LocalMotionPrefixClose2 &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.session_ref);
  visitor(value.execution_ref);
  visitor(value.local_body_ref);
  visitor(value.root_edge_ref);
  visitor(value.parent_basis_ref);
  visitor(value.parent_segment_ordinal);
  visitor(value.parent_u_start);
  visitor(value.parent_u_end);
  visitor(value.parent_required_work);
  visitor(value.credited_work_at_close);
  visitor(value.closed_root_u);
  visitor(value.closed_at);
  visitor(value.topology_anchor_source_ref);
  visitor(value.reservation_source_refs);
  visitor(value.original_cost_unit_ref);
  visitor(value.actual_work_cost_source_refs);
  visitor(value.change_source_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LocalMotionSegment &value,
                  Visitor &&visitor) {
  visitor(value.ordinal);
  visitor(value.geometry);
  visitor(value.required_edge_work);
  visitor(value.terrain_cost_basis_ref);
  visitor(value.original_cost_unit_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LocalMotionSegmentRef &value,
                  Visitor &&visitor) {
  visitor(value.basis_ref);
  visitor(value.segment_ordinal);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LocalPartialEdgeContinuation &value,
                  Visitor &&visitor) {
  visitor(value.root_edge_ref);
  visitor(value.parent_segment_ref);
  visitor(value.parent_prefix_close_ref);
  visitor(value.change);
  visitor(value.root_u_start);
  visitor(value.root_u_end);
  visitor(value.directional_cost);
  visitor(value.topology_anchor_source_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryLocalMotionState &value,
                  Visitor &&visitor) {
  visitor(value.bases);
  visitor(value.cursors);
  visitor(value.prefix_closes);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryLocalUnitRef &value,
                  Visitor &&visitor) {
  visitor(value.unit_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryLocalVesselRef &value,
                  Visitor &&visitor) {
  visitor(value.vessel_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryMotionSource &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.action_id);
  visitor(value.plan_id);
  visitor(value.stationary);
  visitor(value.location);
  visitor(value.route_index);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryRouteCorrespondence2 &value,
                  Visitor &&visitor) {
  visitor(value.old_segment_ordinal);
  visitor(value.new_segment_ordinal);
  visitor(value.old_parameter_from);
  visitor(value.old_parameter_to);
  visitor(value.new_parameter_from);
  visitor(value.new_parameter_to);
  visitor(value.old_from);
  visitor(value.old_to);
  visitor(value.new_from);
  visitor(value.new_to);
  visitor(value.chart_ref);
  visitor(value.original_parameter_source);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitarySpatialChart2 &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.session_ref);
  visitor(value.geometry_definition_ref);
  visitor(value.world_geometry_ref);
  visitor(value.world_length_unit_ref);
  visitor(value.local_axis_unit_ref);
  visitor(value.matrix);
  visitor(value.translation);
  visitor(value.domain_polygon);
  visitor(value.portal_rows);
  visitor(value.creation_source_ref);
  visitor(value.rule_version);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitarySpatialHandoff2 &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.carrier_ref);
  visitor(value.from_binding);
  visitor(value.to_binding);
  visitor(value.old_execution_ref);
  visitor(value.new_execution_ref);
  visitor(value.at);
  visitor(value.entry_or_exit_source_ref);
  visitor(value.chart_ref);
  visitor(value.portal_id);
  visitor(value.old_basis_ref);
  visitor(value.old_cursor);
  visitor(value.new_anchor);
  visitor(value.remaining_route_correspondence);
  visitor(value.cost_and_work_disposition_refs);
  visitor(value.closed_writer_source_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitarySpatialHandoffState &value,
                  Visitor &&visitor) {
  visitor(value.charts);
  visitor(value.handoffs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitarySpatialMatrix &value,
                  Visitor &&visitor) {
  visitor(value.a11);
  visitor(value.a12);
  visitor(value.a21);
  visitor(value.a22);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitarySpatialPoint &value,
                  Visitor &&visitor) {
  visitor(value.x);
  visitor(value.y);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitarySpatialPortal &value,
                  Visitor &&visitor) {
  visitor(value.portal_id);
  visitor(value.world_entry_exit_source_kind);
  visitor(value.world_boundary_ref);
  visitor(value.local_boundary_segment);
  visitor(value.allowed_carrier_kinds);
  visitor(value.orientation_mapping);
  visitor(value.connector_path_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitarySpatialSegment &value,
                  Visitor &&visitor) {
  visitor(value.from);
  visitor(value.to);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitarySpatialTranslation &value,
                  Visitor &&visitor) {
  visitor(value.b1);
  visitor(value.b2);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryTacticalService2 &value,
                  Visitor &&visitor) {
  visitor(value.work_binding_ref);
  visitor(value.started_at);
  visitor(value.last_settled_at);
  visitor(value.required_units);
  visitor(value.credited_units);
  visitor(value.actual_service);
  visitor(value.closed_by_source);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryTacticalServiceSpan2 &value,
                  Visitor &&visitor) {
  visitor(value.span);
  visitor(value.participation_sources);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryTacticalWorkBinding &value,
                  Visitor &&visitor) {
  visitor(value.session_ref);
  visitor(value.generation);
  visitor(value.action_ref);
  visitor(value.stage);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RepriceLocalMotionSuffix &value,
                  Visitor &&visitor) {
  visitor(value.cost_change_source_ref);
  visitor(value.continuing_intent_source_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ReverseLocalMotionAtPrefix &value,
                  Visitor &&visitor) {
  visitor(value.accepted_intent_source_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SignedSpatialRational &value,
                  Visitor &&visitor) {
  visitor(value.sign);
  visitor(value.magnitude);
}

// K4 fixed 0543 original local boundary source members. No schema admission.
template <class Visitor>
inline void visit(const ::hanmo::resident::BodyMotionBoundaryInput3 &value,
                  Visitor &&visitor) {
  visitor(value.local_body_ref);
  visitor(value.execution_ref);
  visitor(value.basis_ref);
  visitor(value.segment_ordinal);
  visitor(value.prior_phase_source);
  visitor(value.prior_motion_source);
  visitor(value.credited_work_before);
  visitor(value.from_at);
  visitor(value.rate_source_ref);
  visitor(value.rate_work_per_half_day);
  visitor(value.phase_before);
  visitor(value.reserved_topology_refs);
  visitor(value.heading);
  visitor(value.heading_source_ref);
  visitor(value.shape_source);
  visitor(value.spatial_binding_ref);
  visitor(value.qualification_source_refs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LocalMotionEdgeCompletion3 &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.session_ref);
  visitor(value.generation);
  visitor(value.actual_before_common_prefix);
  visitor(value.input);
  visitor(value.completed_at);
  visitor(value.credited_work_at_completion);
  visitor(value.actual_work_cost_source_refs);
  visitor(value.reservation_release_sources);
  visitor(value.arrived_topology_source);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LocalMotionBlockedCell3 &value,
                  Visitor &&visitor) {
  visitor(value.battlefield_ref);
  visitor(value.map_version);
  visitor(value.cell);
  visitor(value.terrain_cost_source);
  visitor(value.capability_source);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LocalMotionChartBoundary3 &value,
                  Visitor &&visitor) {
  visitor(value.chart_ref);
  visitor(value.geometry_definition_source);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LocalMotionForbiddenRegion3 &value,
                  Visitor &&visitor) {
  visitor(value.definition_source);
  visitor(value.region_ordinal);
  visitor(value.capability_source);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LocalMotionContactBody3 &value,
                  Visitor &&visitor) {
  visitor(value.body_ordinal);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LocalMotionContactObstacle3 &value,
                  Visitor &&visitor) {
  visitor(value.obstacle_ordinal);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::LocalMotionSimultaneousContact3 &value,
      Visitor &&visitor) {
  visitor(value.first_body_ordinal);
  visitor(value.other);
  visitor(value.closed_parameter_window);
  visitor(value.first_inward_parameter);
  visitor(value.original_relation_source);
  visitor(value.consequence_kind);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LocalMotionContact3 &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.session_ref);
  visitor(value.generation);
  visitor(value.actual_before_common_prefix);
  visitor(value.prediction_window);
  visitor(value.occurred_at);
  visitor(value.body_inputs);
  visitor(value.obstacle_inputs);
  visitor(value.simultaneous_contacts);
  visitor(value.actual_work_cost_source_refs);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitaryContactParameterWindow &value,
      Visitor &&visitor) {
  visitor(value.first);
  visitor(value.last);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitaryBattlefieldDestination3 &value,
      Visitor &&visitor) {
  visitor(value.arena_ref);
  visitor(value.geometry_profile_ref);
  visitor(value.position);
  visitor(value.heading);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryWorldDestination3 &value,
                  Visitor &&visitor) {
  visitor(value.registered_s1_destination_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryMoveTo3 &value,
                  Visitor &&visitor) {
  visitor(value.destination);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryHoldPosition3 &value,
                  Visitor &&visitor) {
  visitor(value.destination);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryEngageKnownForce3 &value,
                  Visitor &&visitor) {
  visitor(value.known_target_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitarySupportFormation3 &value,
                  Visitor &&visitor) {
  visitor(value.allied_formation_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryWithdrawToward3 &value,
                  Visitor &&visitor) {
  visitor(value.approved_exit_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryRescueEmergency3 &value,
                  Visitor &&visitor) {
  visitor(value.known_emergency_ref);
  visitor(value.rescue_plan_ref);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitaryTacticalFormationBirth3 &value,
      Visitor &&visitor) {
  visit_MilitaryTacticalFormationBirth3_payload(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitaryTacticalFormationBirthBuildView3 &value,
      Visitor &&visitor) {
  visit_MilitaryTacticalFormationBirth3_payload(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryTacticalFormation3 &value,
                  Visitor &&visitor) {
  visit_MilitaryTacticalFormation3_payload(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitaryTacticalFormationBuildView3 &value,
      Visitor &&visitor) {
  visit_MilitaryTacticalFormation3_payload(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryForceWorldContext3 &value,
                  Visitor &&visitor) {
  (void)value;
  (void)visitor;
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryForceBattleContext3 &value,
                  Visitor &&visitor) {
  visitor(value.session_ref);
  visitor(value.generation);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryForceCommandStamp3 &value,
                  Visitor &&visitor) {
  visitor(value.world);
  visitor(value.moment);
  visitor(value.formation_id);
  visitor(value.control_revision);
  visitor(value.spatial_binding_revision);
  visitor(value.player_control_epoch);
  visitor(value.context);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DirectForceIntent3 &value,
                  Visitor &&visitor) {
  visitor(value.stamp);
  visitor(value.formation_id);
  visitor(value.expected_intent_revision);
  visitor(value.intent);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DirectForceIntentReceipt3 &value,
                  Visitor &&visitor) {
  visit_DirectForceIntentReceipt3_payload(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::DirectForceIntentReceiptBuildView3 &value,
      Visitor &&visitor) {
  visit_DirectForceIntentReceipt3_payload(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitaryMissionOriginalExecution3 &value,
      Visitor &&visitor) {
  visitor(value.unit_ref);
  visitor(value.execution_source);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitaryTacticalMissionRecord3 &value,
      Visitor &&visitor) {
  visit_MilitaryTacticalMissionRecord3_payload(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitaryTacticalMissionBuildView3 &value,
      Visitor &&visitor) {
  visit_MilitaryTacticalMissionRecord3_payload(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryPlayerDirectIntent3 &value,
                  Visitor &&visitor) {
  visit_MilitaryPlayerDirectIntent3_payload(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitaryPlayerDirectIntentBuildView3 &value,
      Visitor &&visitor) {
  visit_MilitaryPlayerDirectIntent3_payload(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitaryCommittedDecisionIntent3 &value,
      Visitor &&visitor) {
  visitor(value.decision_source_ref);
  visitor(value.controller_ref);
  visitor(value.control_binding_generation);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryDeliveredOrderIntent3 &value,
                  Visitor &&visitor) {
  visitor(value.order_ref);
  visitor(value.actual_response_source);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryAcceptedIntent3 &value,
                  Visitor &&visitor) {
  visit_MilitaryAcceptedIntent3_payload(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitaryAcceptedDirectIntentBuildView3 &value,
      Visitor &&visitor) {
  visit_MilitaryAcceptedIntent3_payload(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::HexEntry &value, Visitor &&visitor) {
  visitor(value.side);
  visitor(value.position);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::HexCell &value, Visitor &&visitor) {
  visitor(value.q);
  visitor(value.r);
  visitor(value.terrain);
  visitor(value.blocks_movement);
  visitor(value.blocks_sight);
  visitor(value.height);
  visitor(value.cover);
  visitor(value.field_order);
}

// 遍历方向半空间的实际拥有字段，供原复制和保留机制共用。
// 只陈列既有字段，不补几何值、作者来源或领域合法性。
template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryOrientationHalfspace &value,
                  Visitor &&visitor) {
  visitor(value.normal);
  visitor(value.maximum);
}

// 遍历遭遇方向候选的实际拥有字段，供原复制和保留机制共用。
// 只陈列既有字段，不补几何值、作者来源或领域合法性。
template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryEncounterOrientation &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.tangent_region);
  visitor(value.matrix);
  visitor(value.local_entry);
}

// 遍历固定场地图定义的实际拥有字段，供原复制和保留机制共用。
// 只陈列既有字段，不补几何值、作者来源或领域合法性。
template <class Visitor>
inline void visit(const ::hanmo::resident::LockedArenaChartDefinition &value,
                  Visitor &&visitor) {
  visitor(value.matrix);
  visitor(value.translation);
}

// 遍历遭遇地图定义的实际拥有字段，供原复制和保留机制共用。
// 只陈列既有字段，不补几何值、作者来源或领域合法性。
template <class Visitor>
inline void visit(const ::hanmo::resident::EncounterChartDefinition &value,
                  Visitor &&visitor) {
  visitor(value.orientation_choices);
}

// 遍历原物理几何定义的实际拥有字段，供原复制和保留机制共用。
// 只陈列既有字段，不补几何值、作者来源或领域合法性。
template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitarySpatialGeometryDefinition2 &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.world_geometry_ref);
  visitor(value.world_length_unit_ref);
  visitor(value.local_axis_unit_ref);
  visitor(value.domain_polygon);
  visitor(value.portal_rows);
  visitor(value.author_mode);
  visitor(value.actual_definition_source);
}

// 遍历原物理几何目录的实际拥有字段，供原复制和保留机制共用。
// 只陈列既有字段，不补几何值、作者来源或领域合法性。
template <class Visitor>
inline void
visit(const ::hanmo::resident::MilitarySpatialDefinitionCatalog &value,
      Visitor &&visitor) {
  visitor(value.definitions);
}
