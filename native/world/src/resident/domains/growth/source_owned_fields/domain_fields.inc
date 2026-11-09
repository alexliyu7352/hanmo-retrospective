// 职责：集中成长与修业中的有限拥有字段访问；仅借用原字段，不改变顺序或失败行为。
// 由中央登记头在 source_owned_fields
// 命名空间内包含；依赖与具名助手已先行可见。

template <class Visitor>
inline void visit(const ::hanmo::resident::StudyBeforeHeadBuildView &value,
                  Visitor &&visitor) {
  visit_actor_condition_head_members(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ActorUnlockEvidenceV1 &value,
                  Visitor &&visitor) {
  visitor(value.evidence);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ActorUnlockEvidenceV2 &value,
                  Visitor &&visitor) {
  visitor(value.evidence);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::AttributeMilestone &value,
                  Visitor &&visitor) {
  visitor(value.milestone_result_id);
  visitor(value.actor_id);
  visitor(value.attribute_id);
  visitor(value.milestone_rule_ref);
  visitor(value.original_outcome_ref);
  visitor(value.unique_milestone_id);
  visitor(value.delta);
  visitor(value.applied_at);
  visitor(value.capability_application);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::AttributeMilestoneRef &value,
                  Visitor &&visitor) {
  visitor(value.milestone_result_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::AttributeMilestoneRule &value,
                  Visitor &&visitor) {
  visitor(value.rule_ref);
  visitor(value.unique_milestone_id);
  visitor(value.attribute_id);
  visitor(value.criterion);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BargainFact &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.basis_sha256);
  visitor(value.consumed_action_id);
  visitor(value.terms);
  visitor(value.instance);
  visitor(value.receipt);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BargainMarketBasis &value,
                  Visitor &&visitor) {
  visitor(value.base_price);
  visitor(value.stock);
  visitor(value.elasticity_bp);
  visitor(value.target_stock);
  visitor(value.event_bp);
  visitor(value.available_quantity);
  visitor(value.shop_wallet_id);
  visitor(value.shop_inventory_id);
  visitor(value.actor_inventory_id);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BargainPrepared &value,
                  Visitor &&visitor) {
  visitor(value.mode);
  visitor(value.quote_id);
  visitor(value.basis_sha256);
  visitor(value.terms);
  visitor(value.definition);
  visitor(value.before);
  visitor(value.next);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BargainQuoteContext &value,
                  Visitor &&visitor) {
  visitor(value.quote_id);
  visitor(value.quote_sha256);
  visitor(value.market_revision);
  visitor(value.valid_tick);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BargainTerms &value,
                  Visitor &&visitor) {
  visitor(value.request);
  visitor(value.base_quote);
  visitor(value.market_basis);
  visitor(value.source_sha256);
  visitor(value.challenge_id);
  visitor(value.skill_level);
  visitor(value.auxiliary_attribute);
  visitor(value.quote_context);
  visitor(value.field_order);
  visitor(value.snapshot_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CampTrainingExperienceV1 &value,
                  Visitor &&visitor) {
  visitor(value.evidence);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::CanonicalCardAcquisitionIdentity &value,
      Visitor &&visitor) {
  visitor(value.world_id);
  visitor(value.actor_id);
  visitor(value.card_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CardAcquisitionReceiptRefV2 &value,
                  Visitor &&visitor) {
  visitor(value.receipt_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CardAcquisitionReceiptV2 &value,
                  Visitor &&visitor) {
  visitor(value.receipt_id);
  visitor(value.actor_id);
  visitor(value.card_id);
  visitor(value.definition_version);
  visitor(value.category);
  visitor(value.acquired_at);
  visitor(value.mode);
  visitor(value.acquisition_basis);
  visitor(value.applied_local_effect_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CardAcquisitionState &value,
                  Visitor &&visitor) {
  visitor(value.card_acquisitions);
  visitor(value.legacy_card_source_mappings);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ChallengeFrozen &value,
                  Visitor &&visitor) {
  visitor(value.field);
  visitor(value.kind);
  visitor(value.challenge_id);
  visitor(value.mode);
  visitor(value.source_sha256);
  visitor(value.skill_level);
  visitor(value.auxiliary_attribute);
  visitor(value.context);
  visitor(value.field_order);
  visitor(value.snapshot_order);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::ConditionReceiptRefV2BuildView &value,
      Visitor &&visitor) {
  visit_condition_receipt_ref_v2_members(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EquipmentChoice &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.defense);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::ExistingGrowthCardAcquisitionSource &value,
      Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.card_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FrozenLessonCapability &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.purpose_ref);
  visitor(value.condition_revision);
  visitor(value.origin_ref);
  visitor(value.attribute_milestone_refs);
  visitor(value.condition_source_refs);
  visitor(value.registered_efficiency_components);
  visitor(value.frozen_efficiency_bp);
  visitor(value.admission_rule_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GroupLessonResultRef &value,
                  Visitor &&visitor) {
  visitor(value.course_id);
  visitor(value.period_ordinal);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthAction &value,
                  Visitor &&visitor) {
  visitor(value.request);
  visitor(value.quote);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthActorUnlockEvidence &value,
                  Visitor &&visitor) {
  visitor(value.card_id);
  visitor(value.target_actor_id);
  visitor(value.earner_actor_id);
  visitor(value.scenario_id);
  visitor(value.world_id);
  visitor(value.acquired_tick);
  visitor(value.tasks);
  visitor(value.envoy);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthCampTrainingExperience &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.actor_id);
  visitor(value.garrison_id);
  visitor(value.place_id);
  visitor(value.unit_id);
  visitor(value.scenario_id);
  visitor(value.world_id);
  visitor(value.started_tick);
  visitor(value.completed_tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthCapabilityApplication &value,
                  Visitor &&visitor) {
  visitor(value.revision_before);
  visitor(value.previous);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthCapabilityRevision &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.revision);
  visitor(value.last_change);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthCardAcquisition &value,
                  Visitor &&visitor) {
  visitor(value.card_id);
  visitor(value.actor_id);
  visitor(value.source_id);
  visitor(value.source_kind);
  visitor(value.acquired_tick);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthCardsState &value,
                  Visitor &&visitor) {
  visitor(value.owned);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthChallengeCondition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.kind);
  visitor(value.text_key);
  visitor(value.matched);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthChallengeDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.kind);
  visitor(value.source_sha256);
  visitor(value.difficulty);
  visitor(value.conditions);
  visitor(value.nodes);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthChallengeInstance &value,
                  Visitor &&visitor) {
  visitor(value.seed_origin);
  visitor(value.seed);
  visitor(value.actor_id);
  visitor(value.source_id);
  visitor(value.phase);
  visitor(value.frozen);
  visitor(value.choices);
  visitor(value.result);
  visitor(value.consumed);
  visitor(value.field_order);
  visitor(value.state_order);
  visitor(value.binding_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthChallengeNode &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.topic_key);
  visitor(value.stance_key);
  visitor(value.prompt_key);
  visitor(value.options);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthChallengeOption &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.text_key);
  visitor(value.kind);
  visitor(value.method);
  visitor(value.points);
  visitor(value.requires_choices);
  visitor(value.supply_condition_id);
  visitor(value.terms_condition_id);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthChallengeRequirement &value,
                  Visitor &&visitor) {
  visitor(value.node_id);
  visitor(value.option_id);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthChallengeResult &value,
                  Visitor &&visitor) {
  visitor(value.completion);
  visitor(value.score);
  visitor(value.auto_score);
  visitor(value.performance_bp);
  visitor(value.request_delta);
  visitor(value.discount_bp);
  visitor(value.relation_discount_bp);
  visitor(value.challenge_discount_bp);
  visitor(value.raw_score);
  visitor(value.choices);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthChoice &value,
                  Visitor &&visitor) {
  visitor(value.step_id);
  visitor(value.option_id);
  visitor(value.points);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthCommand &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.skill_id);
  visitor(value.item_id);
  visitor(value.weapon_id);
  visitor(value.armor_id);
  visitor(value.activity);
  visitor(value.shop_id);
  visitor(value.side);
  visitor(value.payer_wallet_id);
  visitor(value.action_id);
  visitor(value.step_id);
  visitor(value.option_id);
  visitor(value.estate_id);
  visitor(value.challenge_mode);
  visitor(value.ticks);
  visitor(value.quantity);
  visitor(value.duration);
  visitor(value.quoted_total);
  visitor(value.market_revision);
  visitor(value.context);
  visitor(value.bargain_quote_id);
  visitor(value.field_order);
  visitor(value.trade_field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthEnvoyRoundTripEvidence &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.task_id);
  visitor(value.contract_id);
  visitor(value.acceptance_fact_id);
  visitor(value.mission_authority_fact_id);
  visitor(value.proposal_id);
  visitor(value.delivery_fact_id);
  visitor(value.presentation_fact_id);
  visitor(value.completion_fact_id);
  visitor(value.report_fact_id);
  visitor(value.scenario_id);
  visitor(value.world_id);
  visitor(value.accepted_tick);
  visitor(value.delivered_tick);
  visitor(value.completed_tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthFactFixedInputs &value,
                  Visitor &&visitor) {
  visitor(value.terms_action_ref);
  visitor(value.teaching_or_study_acceptance_ref);
  visitor(value.original_training_or_study_rule_ref);
  visitor(value.original_quiet_or_rest_basis);
  visitor(value.actual_learning_or_rest_period_refs);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthMarketCaptureState &value,
                  Visitor &&visitor) {
  visitor(value.open_scopes);
  visitor(value.unbound_business);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthMarketHistoryState &value,
                  Visitor &&visitor) {
  visitor(value.movements);
  visitor(value.field_order);
  visitor(value.version);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthPermanentCatalog &value,
                  Visitor &&visitor) {
  visitor(value.identity);
  visitor(value.practice_rules);
  visitor(value.milestone_rules);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthPermanentState &value,
                  Visitor &&visitor) {
  visitor(value.milestones);
  visitor(value.practice_awards);
  visitor(value.capability_revisions);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthQuote &value,
                  Visitor &&visitor) {
  visitor(value.command);
  visitor(value.duration);
  visitor(value.cost);
  visitor(value.wage);
  visitor(value.expected_xp);
  visitor(value.target_level);
  visitor(value.context);
  visitor(value.data);
  visitor(value.challenge_before);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthRecoveryClosure &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.action_id);
  visitor(value.tick);
  visitor(value.provided_half_days);
  visitor(value.reason);
  visitor(value.cause_fact_id);
  visitor(value.commit_result_id);
  visitor(value.commit_actor_id);
  visitor(value.commit_kind);
  visitor(value.commit_request_seq);
  visitor(value.commit_revision);
  visitor(value.commit_advance);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthRecoveryProgress &value,
                  Visitor &&visitor) {
  visitor(value.injury);
  visitor(value.points);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthRecoveryService &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.provider_actor_id);
  visitor(value.role_id);
  visitor(value.place_id);
  visitor(value.method);
  visitor(value.start_tick);
  visitor(value.duration);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthRecoveryState &value,
                  Visitor &&visitor) {
  visitor(value.services);
  visitor(value.credits);
  visitor(value.closures);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthRecoveryStep &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.action_id);
  visitor(value.tick);
  visitor(value.ordinal);
  visitor(value.before);
  visitor(value.after);
  visitor(value.offered_points);
  visitor(value.used_points);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthState &value,
                  Visitor &&visitor) {
  visitor(value.actions);
  visitor(value.lesson_capabilities);
  visitor(value.facts);
  visitor(value.life_activities);
  visitor(value.cards);
  visitor(value.fixed_service_boundaries);
  visitor(value.terminal_publications);
  visitor(value.trade_grants);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthStudyTeachingAgreement &value,
                  Visitor &&visitor) {
  visitor(value.request_id);
  visitor(value.teacher_actor_id);
  visitor(value.student_actor_id);
  visitor(value.skill_id);
  visitor(value.course_role_id);
  visitor(value.dialogue_id);
  visitor(value.node_id);
  visitor(value.node_kind);
  visitor(value.session_id);
  visitor(value.payment_policy);
  visitor(value.tariff);
  visitor(value.request_context);
  visitor(value.duration);
  visitor(value.accepted_tick);
  visitor(value.accepted_revision);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthTacticLessonAgreement &value,
                  Visitor &&visitor) {
  visitor(value.definition_id);
  visitor(value.tactic_id);
  visitor(value.student_id);
  visitor(value.teacher_id);
  visitor(value.place_id);
  visitor(value.payer_wallet_id);
  visitor(value.recipient_wallet_id);
  visitor(value.dialogue_id);
  visitor(value.node_id);
  visitor(value.session_id);
  visitor(value.scenario_id);
  visitor(value.world_id);
  visitor(value.context);
  visitor(value.tick);
  visitor(value.revision);
  visitor(value.duration);
  visitor(value.total_fee);
  visitor(value.teacher_source);
  visitor(value.prerequisite);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthTacticLessonClosure &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.started_fact_id);
  visitor(value.action_id);
  visitor(value.reason);
  visitor(value.cause_fact_id);
  visitor(value.tick);
  visitor(value.provided_half_days);
  visitor(value.fee_paid);
  visitor(value.fee_released);
  visitor(value.commit);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthTacticLessonCommit &value,
                  Visitor &&visitor) {
  visitor(value.result_id);
  visitor(value.actor_id);
  visitor(value.kind);
  visitor(value.request_seq);
  visitor(value.revision);
  visitor(value.from_tick);
  visitor(value.advance);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthTacticLessonCredit &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.started_fact_id);
  visitor(value.action_id);
  visitor(value.tick);
  visitor(value.ordinal);
  visitor(value.fee_paid);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthTacticLessonStart &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.action_id);
  visitor(value.agreement);
  visitor(value.commit);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthTacticLessonState &value,
                  Visitor &&visitor) {
  visitor(value.starts);
  visitor(value.credits);
  visitor(value.closures);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthTacticPrerequisite &value,
                  Visitor &&visitor) {
  visitor(value.skill_id);
  visitor(value.origin_kind);
  visitor(value.initial_prototype_id);
  visitor(value.required_level);
  visitor(value.observed_tick);
  visitor(value.initial_created_tick);
  visitor(value.initial_xp);
  visitor(value.minimum_xp);
  visitor(value.minimum_level);
  visitor(value.initial_source_ids);
  visitor(value.study_source_ids);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthTacticTeacherSource &value,
                  Visitor &&visitor) {
  visitor(value.source_kind);
  visitor(value.acquisition_fact_id);
  visitor(value.source_fact_id);
  visitor(value.initial_grant_id);
  visitor(value.acquired_tick);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthTeachingAgreement &value,
                  Visitor &&visitor) {
  visitor(value.request_id);
  visitor(value.teacher_actor_id);
  visitor(value.student_actor_id);
  visitor(value.skill_id);
  visitor(value.dialogue_id);
  visitor(value.node_id);
  visitor(value.node_kind);
  visitor(value.session_id);
  visitor(value.context);
  visitor(value.accepted_tick);
  visitor(value.accepted_revision);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthTerminalPublication &value,
                  Visitor &&visitor) {
  visitor(value.publication_id);
  visitor(value.action_id);
  visitor(value.original_result_ref);
  visitor(value.terminal_source_ref);
  visitor(value.terminal_source_schema);
  visitor(value.occurred_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthTrainingCardCompletion &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.actor_id);
  visitor(value.card_id);
  visitor(value.personal_skill_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LegacyCardSourceMapping &value,
                  Visitor &&visitor) {
  visitor(value.mapping_id);
  visitor(value.actor_id);
  visitor(value.card_id);
  visitor(value.legacy_source);
  visitor(value.canonical_acquisition_identity);
  visitor(value.profile_delivery_source_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LegacyCardSourceMappingRefV1 &value,
                  Visitor &&visitor) {
  visitor(value.mapping_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LessonCapabilitySource &value,
                  Visitor &&visitor) {
  visitor(value.action_id);
  visitor(value.lesson_ordinal);
  visitor(value.work_period_ref);
  visitor(value.provided_from);
  visitor(value.provided_to);
  visitor(value.student_capability);
  visitor(value.teacher_capability);
  visitor(value.xp_rule_ref);
  visitor(value.innate_source_ref);
  visitor(value.fatigue_effect_use_refs);
  visitor(value.student_condition_receipt_ref);
  visitor(value.teacher_condition_receipt_ref);
  visitor(value.original_lesson_result_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeAcceptedExecution &value,
                  Visitor &&visitor) {
  visitor(value.accepted_source_ref);
  visitor(value.source_schema);
  visitor(value.execution_rule_ref);
  visitor(value.actual_initiator_actor_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeAcceptedPermissionRef &value,
                  Visitor &&visitor) {
  visitor(value.permission_kind);
  visitor(value.original_ref);
  visitor(value.source_schema);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeAcceptedTermsInputChange &value,
                  Visitor &&visitor) {
  visitor(value.original_source_ref);
  visitor(value.source_schema);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeActivityFixedInputs &value,
                  Visitor &&visitor) {
  visitor(value.activity_id);
  visitor(value.terms_kind);
  visitor(value.service_role_ref);
  visitor(value.activity_ref);
  visitor(value.recovery_service_ref);
  visitor(value.original_quiet_or_rest_basis);
  visitor(value.trade_settlement_ref);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeActivityRecord &value,
                  Visitor &&visitor) {
  visitor(value.view);
  visitor(value.context);
  visitor(value.terms);
  visitor(value.cost_paid);
  visitor(value.wage_paid);
  visitor(value.field_order);
  visitor(value.view_field_order);
  visitor(value.selected_trade_settlement);
  visitor(value.trade_terminal_time);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeActualCommand &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.command_binding);
  visitor(value.route_basis);
  visitor(value.accepted_business_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeActualCommandBinding &value,
                  Visitor &&visitor) {
  visitor(value.command_kind);
  visitor(value.request_seq);
  visitor(value.result_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeActualInputChange &value,
                  Visitor &&visitor) {
  visitor(value.change);
  visitor(value.transition_rule_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeActualPositionInputChange &value,
                  Visitor &&visitor) {
  visitor(value.original_source_ref);
  visitor(value.source_schema);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeActualStopTrigger &value,
                  Visitor &&visitor) {
  visitor(value.original_stop_rule_ref);
  visitor(value.source);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeCatalogFieldRef &value,
                  Visitor &&visitor) {
  visitor(value.catalog_identity);
  visitor(value.scenario_id);
  visitor(value.entity_id);
  visitor(value.field_selector);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeConditionInputChange &value,
                  Visitor &&visitor) {
  visitor(value.condition_receipt_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeDeathInputChange &value,
                  Visitor &&visitor) {
  visitor(value.death_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeFacilityInputChange &value,
                  Visitor &&visitor) {
  visitor(value.original_source_ref);
  visitor(value.source_schema);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeFacilityUseRef &value,
                  Visitor &&visitor) {
  visitor(value.original_facility_use_ref);
  visitor(value.source_schema);
  visitor(value.actual_claim_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeFixedActorInputs &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.role);
  visitor(value.capability_frames);
  visitor(value.origin_actor_key);
  visitor(value.origin_schema);
  visitor(value.skill_qualification_views);
  visitor(value.effective_attribute_inputs);
  visitor(value.age_view);
  visitor(value.location_snapshot);
  visitor(value.actual_location_source_refs);
  visitor(value.participation_source_refs);
  visitor(value.occupation_source_refs);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeFixedCompletionGate &value,
                  Visitor &&visitor) {
  visitor(value.action_ref);
  visitor(value.original_required_rule_ref);
  visitor(value.last_actual_period_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeFixedOutcomeRef &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.id);
  visitor(value.period_ordinal);
  visitor(value.secondary_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeFixedOwnerRef &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.life_activity_id);
  visitor(value.terms_kind);
  visitor(value.action_id);
  visitor(value.fact_kind);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeFixedSpecificInputs &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.life_activity);
  visitor(value.growth_fact);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeGrowthInputChange &value,
                  Visitor &&visitor) {
  visitor(value.original_result_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeKnowledgeInputChange &value,
                  Visitor &&visitor) {
  visitor(value.original_source_ref);
  visitor(value.source_schema);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeNpcActorRoute &value,
                  Visitor &&visitor) {
  visitor(value.subject_actor_id);
  visitor(value.registration_epoch);
  visitor(value.decision_seq);
  visitor(value.policy_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeOccupationInputChange &value,
                  Visitor &&visitor) {
  visitor(value.original_source_ref);
  visitor(value.source_schema);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeOriginalResultTrigger &value,
                  Visitor &&visitor) {
  visitor(value.outcome);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::LifeOriginalTerminalApplication &value,
      Visitor &&visitor) {
  visitor(value.disposition);
  visitor(value.trigger);
  visitor(value.actual_result_refs);
  visitor(value.remaining_responsibility_refs);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeOwnedFieldRef &value,
                  Visitor &&visitor) {
  visitor(value.owner_ref);
  visitor(value.field_selector);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifePlayerActorRoute &value,
                  Visitor &&visitor) {
  visitor(value.original_control_source_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeQuietServiceInputs &value,
                  Visitor &&visitor) {
  visitor(value.place_ref);
  visitor(value.quiet_rule_ref);
  visitor(value.site_access_source_refs);
  visitor(value.actual_conflict_source_refs);
  visitor(value.observed_at);
  visitor(value.is_quiet);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeResourceBasisRef &value,
                  Visitor &&visitor) {
  visitor(value.purpose);
  visitor(value.original_source_ref);
  visitor(value.source_schema);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeResourceInputChange &value,
                  Visitor &&visitor) {
  visitor(value.original_source_ref);
  visitor(value.source_schema);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeScheduledInputChange &value,
                  Visitor &&visitor) {
  visitor(value.schedule_ref);
  visitor(value.period_ordinal);
  visitor(value.boundary);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LongRestActualPeriod &value,
                  Visitor &&visitor) {
  visitor(value.ordinal);
  visitor(value.provided_from);
  visitor(value.applied_at);
  visitor(value.input_boundary_refs);
  visitor(value.estate_basis);
  visitor(value.mouth_actor_ids);
  visitor(value.inventory_id);
  visitor(value.food_item_id);
  visitor(value.consumed_quantity);
  visitor(value.condition_receipt_refs);
  visitor(value.mouth_inputs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LongRestMouthInputs &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.original_adoption);
  visitor(value.condition_before);
  visitor(value.actual_location_sources);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MarketCaptureStep &value,
                  Visitor &&visitor) {
  visitor(value.stage);
  visitor(value.next_instance_before);
  visitor(value.next_instance_after);
  visitor(value.inventories);
  visitor(value.field_order);
  visitor(value.source_binding);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MarketInventoryChange &value,
                  Visitor &&visitor) {
  visitor(value.inventory_id);
  visitor(value.revision_before);
  visitor(value.revision_after);
  visitor(value.lots);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MarketLotChange &value,
                  Visitor &&visitor) {
  visitor(value.lot_id);
  visitor(value.before);
  visitor(value.after);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MarketMovementRecord &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.business_fact_id);
  visitor(value.business_kind);
  visitor(value.source_tick);
  visitor(value.observed_tick);
  visitor(value.steps);
  visitor(value.field_order);
  visitor(value.business_source_schema);
  visitor(value.original_business_execution);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PracticeAwardReceipt &value,
                  Visitor &&visitor) {
  visitor(value.award_id);
  visitor(value.actor_id);
  visitor(value.practice_rule_ref);
  visitor(value.original_source);
  visitor(value.contribution_role);
  visitor(value.skill_rows);
  visitor(value.applied_at);
  visitor(value.original_fact_ref);
  visitor(value.capability_application);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PracticeAwardRef &value,
                  Visitor &&visitor) {
  visitor(value.award_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PracticeAwardRule &value,
                  Visitor &&visitor) {
  visitor(value.rule_ref);
  visitor(value.rule_family);
  visitor(value.criterion);
  visitor(value.contribution_role);
  visitor(value.skill_awards);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PracticeSkillAward &value,
                  Visitor &&visitor) {
  visitor(value.skill_id);
  visitor(value.requested_xp);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PracticeSkillReceipt &value,
                  Visitor &&visitor) {
  visitor(value.skill_id);
  visitor(value.xp_before);
  visitor(value.requested_xp);
  visitor(value.actual_xp_gain);
  visitor(value.xp_after);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RestCancellation &value,
                  Visitor &&visitor) {
  visitor(value.action_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RestFact &value, Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.estate_id);
  visitor(value.phase);
  visitor(value.food_item_id);
  visitor(value.stop_reason);
  visitor(value.start_tick);
  visitor(value.duration);
  visitor(value.elapsed);
  visitor(value.completed_tick);
  visitor(value.ration_per_person);
  visitor(value.consumed);
  visitor(value.last_mouths);
  visitor(value.field_order);
  visitor(value.actual_periods);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RestTerms &value,
                  Visitor &&visitor) {
  visitor(value.estate_id);
  visitor(value.food_item_id);
  visitor(value.duration);
  visitor(value.ration_per_person);
  visitor(value.mouths);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ScenarioInitialCardGrant &value,
                  Visitor &&visitor) {
  visitor(value.scenario_id);
  visitor(value.actor_id);
  visitor(value.card_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ServiceTerms &value,
                  Visitor &&visitor) {
  visitor(value.duration);
  visitor(value.cost);
  visitor(value.wage);
  visitor(value.hp_gain);
  visitor(value.fatigue_delta);
  visitor(value.wallet_id);
  visitor(value.field_order);
}

// 作者版本地址与运行服务条款各自持有原字段，不把派生尾静默转为基类。
template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthLifeDefinitionRef1 &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.version);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthActivityDefinition &value,
                  Visitor &&visitor) {
  visitor(static_cast<const ::hanmo::resident::ServiceTerms &>(value));
  visitor(value.definition_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StudyAdjustmentExactBinding &value,
                  Visitor &&visitor) {
  visitor(value.settled_at);
  visitor(value.result_id);
  visitor(value.actor_id);
  visitor(value.command_kind);
  visitor(value.request_seq);
  visitor(value.revision);
  visitor(value.application_ordinal);
  visitor(value.original_fact_prefix);
  visitor(value.capability_application);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StudyAdjustmentFactPrefix &value,
                  Visitor &&visitor) {
  visitor(value.original_fact_prefix_size);
  visitor(value.last_fact_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StudyCancellation &value,
                  Visitor &&visitor) {
  visitor(value.action_id);
  visitor(value.completed_lessons);
  visitor(value.fee_paid);
  visitor(value.unprovided_released);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StudyCapabilityState &value,
                  Visitor &&visitor) {
  visitor(value.study_rule_bindings);
  visitor(value.legacy_study_bindings);
  visitor(value.study_time_bindings);
  visitor(value.lesson_capability_sources);
  visitor(value.study_condition_boundaries);
  visitor(value.group_teacher_capability_sources);
  visitor(value.group_lesson_capability_sources);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StudyChallengeContext &value,
                  Visitor &&visitor) {
  visitor(value.budget);
  visitor(value.quantity);
  visitor(value.relation_discount_bp);
  visitor(value.field_order);
  visitor(value.quote);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StudyChallengeRef &value,
                  Visitor &&visitor) {
  visitor(value.action_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StudyConditionBoundary &value,
                  Visitor &&visitor) {
  visitor(value.action_id);
  visitor(value.boundary_ordinal);
  visitor(value.boundary_kind);
  visitor(value.at);
  visitor(value.student_condition_ref);
  visitor(value.teacher_condition_ref);
  visitor(value.original_boundary_result_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StudyFact &value,
                  Visitor &&visitor) {
  visitor(value.phase);
  visitor(value.actor_id);
  visitor(value.code);
  visitor(value.start_tick);
  visitor(value.completed_tick);
  visitor(value.success);
  visitor(value.terms);
  visitor(value.lessons);
  visitor(value.pauses);
  visitor(value.xp_settlement_before);
  visitor(value.challenge);
  visitor(value.field_order);
  visitor(value.study_service_binding);
  visitor(value.study_service_terminal);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StudyFactTermsRef &value,
                  Visitor &&visitor) {
  visitor(value.action_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StudyFixedLessonRef &value,
                  Visitor &&visitor) {
  visitor(value.action_id);
  visitor(value.lesson_ordinal);
  visitor(value.original_fsl_source);
  visitor(value.source_schema);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StudyLesson &value,
                  Visitor &&visitor) {
  visitor(value.fatigue_before);
  visitor(value.injury);
  visitor(value.xp);
  visitor(value.field_order);
  visitor(value.segment);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StudyLessonCharge &value,
                  Visitor &&visitor) {
  visitor(value.action_id);
  visitor(value.lesson_ordinal);
  visitor(value.accepted_terms_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StudyLessonResultRef &value,
                  Visitor &&visitor) {
  visitor(value.action_id);
  visitor(value.lesson_ordinal);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::StudyOriginalParticipantUnavailable &value,
      Visitor &&visitor) {
  visitor(value.action_id);
  visitor(value.terms_ref);
  visitor(value.actor_id);
  visitor(value.role);
  visitor(value.cause_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StudyPause &value,
                  Visitor &&visitor) {
  visitor(value.start_tick);
  visitor(value.end_tick);
  visitor(value.after_lessons);
  visitor(value.student_fatigue);
  visitor(value.teacher_fatigue);
  visitor(value.student_xp);
  visitor(value.field_order);
  visitor(value.credited_xp);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StudyPauseChange &value,
                  Visitor &&visitor) {
  visitor(value.action_id);
  visitor(value.paused);
  visitor(value.paused_ticks);
  visitor(value.completed_lessons);
  visitor(value.reserved);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StudyPauseInterval &value,
                  Visitor &&visitor) {
  visitor(value.pause_ordinal);
  visitor(value.original_pause_source_ref);
  visitor(value.opened_at);
  visitor(value.closed_at);
  visitor(value.closure_source_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StudyPerformanceAdjustment &value,
                  Visitor &&visitor) {
  visitor(value.settled_tick);
  visitor(value.xp_before);
  visitor(value.performance_bp);
  visitor(value.xp_gained);
  visitor(value.field_order);
  visitor(value.exact_binding);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StudyPerformanceAdjustmentRef &value,
                  Visitor &&visitor) {
  visitor(value.action_id);
  visitor(value.lesson_ordinal);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StudyProvidedPeriodRef &value,
                  Visitor &&visitor) {
  visitor(value.lesson_ordinal);
  visitor(value.original_work_period_ref);
  visitor(value.original_lesson_result_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StudyReleaseCommitRef &value,
                  Visitor &&visitor) {
  visitor(value.result_id);
  visitor(value.actor_id);
  visitor(value.kind);
  visitor(value.request_seq);
  visitor(value.revision);
  visitor(value.from);
  visitor(value.to);
  visitor(value.advance);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StudyReservationReleaseResult &value,
                  Visitor &&visitor) {
  visitor(value.wallet_id);
  visitor(value.reservation_ref);
  visitor(value.amount_before);
  visitor(value.amount_after);
  visitor(value.original_release_commit_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StudyRuleBinding &value,
                  Visitor &&visitor) {
  visitor(value.action_id);
  visitor(value.study_rule_ref);
  visitor(value.condition_credit_rule_ref);
  visitor(value.origin_reader_policy);
  visitor(value.accepted_execution_ref);
  visitor(value.execution_binding);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StudyScheduledGateRef &value,
                  Visitor &&visitor) {
  visitor(value.lesson_ordinal);
  visitor(value.original_registered_gate_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StudySegment &value,
                  Visitor &&visitor) {
  visitor(value.ordinal);
  visitor(value.settled_tick);
  visitor(value.xp_before);
  visitor(value.performance_bp);
  visitor(value.xp_gained);
  visitor(value.performance_pending);
  visitor(value.adjustment);
  visitor(value.field_order);
  visitor(value.capability_application);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StudyServiceBindingV2 &value,
                  Visitor &&visitor) {
  visitor(value.accepted_execution_ref);
  visitor(value.terms_ref);
  visitor(value.accepted_at);
  visitor(value.provider_relation_rule_ref);
  visitor(value.study_time_binding_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StudyServiceTerminalV2 &value,
                  Visitor &&visitor) {
  visitor(value.action_id);
  visitor(value.accepted_execution_ref);
  visitor(value.terms_ref);
  visitor(value.terminal_ref);
  visitor(value.stop_cause);
  visitor(value.terminated_at);
  visitor(value.provided_lesson_refs);
  visitor(value.continuity_check);
  visitor(value.growth_continuation_refs);
  visitor(value.fee_source_refs);
  visitor(value.reservation_release_results);
  visitor(value.responsibility_refs);
  visitor(value.study_time_binding_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StudySkillStart &value,
                  Visitor &&visitor) {
  visitor(value.action_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StudyTerminal &value,
                  Visitor &&visitor) {
  visitor(value.action_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StudyTerminalTime &value,
                  Visitor &&visitor) {
  visitor(value.terminal_kind);
  visitor(value.at);
  visitor(value.original_terminal_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StudyTerms &value,
                  Visitor &&visitor) {
  visitor(value.skill_id);
  visitor(value.mode);
  visitor(value.student_id);
  visitor(value.teacher_id);
  visitor(value.payer_wallet_id);
  visitor(value.recipient_wallet_id);
  visitor(value.context);
  visitor(value.duration);
  visitor(value.xp_before);
  visitor(value.teacher_xp);
  visitor(value.student_fatigue);
  visitor(value.teacher_fatigue);
  visitor(value.xp_limit);
  visitor(value.fee);
  visitor(value.challenge);
  visitor(value.settlement_policy);
  visitor(value.field_order);
  visitor(value.study_teaching_agreement);
  visitor(value.innate_source);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StudyTimeBinding &value,
                  Visitor &&visitor) {
  visitor(value.action_id);
  visitor(value.execution_binding);
  visitor(value.start_at);
  visitor(value.scheduled_gate_refs);
  visitor(value.provided_period_refs);
  visitor(value.pause_intervals);
  visitor(value.terminal_time);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StudyTimeBindingRef &value,
                  Visitor &&visitor) {
  visitor(value.action_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StudyVerifiedParticipantDeath &value,
                  Visitor &&visitor) {
  visitor(value.death_ref);
  visitor(value.actor_id);
  visitor(value.role);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TeachStudyRequestStart &value,
                  Visitor &&visitor) {
  visitor(value.action_id);
  visitor(value.request_id);
  visitor(value.session_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TradeTerms &value,
                  Visitor &&visitor) {
  visitor(value.shop_id);
  visitor(value.item_id);
  visitor(value.side);
  visitor(value.payer_wallet_id);
  visitor(value.quantity);
  visitor(value.total);
  visitor(value.market_revision);
  visitor(value.valid_tick);
  visitor(value.owner);
  visitor(value.context);
  visitor(value.field_order);
  visitor(value.bargain_quote_id);
  visitor(value.bargain_basis_sha256);
  visitor(value.discount_bp);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TrainingFact &value,
                  Visitor &&visitor) {
  visitor(value.phase);
  visitor(value.action_kind);
  visitor(value.actor_id);
  visitor(value.start_tick);
  visitor(value.completed_tick);
  visitor(value.success);
  visitor(value.training);
  visitor(value.context);
  visitor(value.field_order);
  visitor(value.capability_application);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TrainingResultRef &value,
                  Visitor &&visitor) {
  visitor(value.action_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TrainingTerms &value,
                  Visitor &&visitor) {
  visitor(value.skill_id);
  visitor(value.student_id);
  visitor(value.teacher_id);
  visitor(value.place_id);
  visitor(value.payer_wallet_id);
  visitor(value.recipient_wallet_id);
  visitor(value.duration);
  visitor(value.mastery_before);
  visitor(value.mastery_after);
  visitor(value.student_fatigue);
  visitor(value.teacher_fatigue);
  visitor(value.fee);
  visitor(value.field_order);
  visitor(value.teaching_agreement);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::UnimplementedLifeFixedValue &value,
                  Visitor &&visitor) {
  (void)value;
  (void)visitor;
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::UnsupportedGroupLessonCapabilitySource &value,
      Visitor &&visitor) {
  (void)value;
  (void)visitor;
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::UnsupportedGroupTeacherCapabilitySource &value,
      Visitor &&visitor) {
  (void)value;
  (void)visitor;
}

template <class Visitor>
inline void visit(const ::hanmo::resident::UnsupportedLegacyStudyBinding &value,
                  Visitor &&visitor) {
  (void)value;
  (void)visitor;
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::UnsupportedStudyOriginalResponsibilityRef &value,
      Visitor &&visitor) {
  (void)value;
  (void)visitor;
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::UnsupportedStudyPermanentCapacitySource &value,
      Visitor &&visitor) {
  (void)value;
  (void)visitor;
}

template <class Visitor>
inline void visit(const ::hanmo::resident::WorkPracticeContribution &value,
                  Visitor &&visitor) {
  visitor(value.result);
  visitor(value.participation_refs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::WorkPracticeCriterion &value,
                  Visitor &&visitor) {
  visitor(value.work_kind);
  visitor(value.minimum_credited_half_days);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::SkillQualificationBoundaryRefV2 &value,
      Visitor &&visitor) {
  visitor(value.original_source_ref);
  visitor(value.source_schema);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LegacyCompletedStudyRef &value,
                  Visitor &&visitor) {
  visitor(value.action_id);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::GroupLessonSkillContributionRef &value,
      Visitor &&visitor) {
  visitor(value.course_id);
  visitor(value.period_ordinal);
  visitor(value.enrollment_id);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PracticeAwardSkillContributionRef &value,
      Visitor &&visitor) {
  visitor(value.award_id);
  visitor(value.skill_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VerifiedLegacyTradeGrantRef &value,
                  Visitor &&visitor) {
  visitor(value.original_trade_result_id);
}

// Raw payloads of self-charging owners; lease handling stays in each caller.
template <class Visitor>
inline void
visit_payload(const ::hanmo::resident::GrowthSkillQualificationV2 &value,
              Visitor &&visitor) {
  visitor(value.source_identity);
  visitor(value.actor_id);
  visitor(value.skill_id);
  visitor(value.source_kind);
  visitor(value.origin_ref);
  visitor(value.actual_boundary_ref);
  visitor(value.observed_at);
  visitor(value.minimum_xp);
  visitor(value.minimum_level);
  visitor(value.coverage_kind);
  visitor(value.contribution_refs);
}

template <class Visitor>
inline void
visit_payload(const ::hanmo::resident::LifeFixedServiceInputs &value,
              Visitor &&visitor) {
  visitor(value.actors);
  visitor(value.locked_rule_refs);
  visitor(value.accepted_permission_refs);
  visitor(value.facility_use_refs);
  visitor(value.resource_basis_refs);
  visitor(value.specific);
  visitor(value.field_order);
}

template <class Visitor>
inline void
visit_payload(const ::hanmo::resident::LifeFixedServiceBoundary &value,
              Visitor &&visitor) {
  visitor(value.boundary_id);
  visitor(value.source_schema);
  visitor(value.action_ref);
  visitor(value.action_kind);
  visitor(value.owner_ref);
  visitor(value.boundary_ordinal);
  visitor(value.previous_boundary_ref);
  visitor(value.kind);
  visitor(value.occurred_at);
  visitor(value.cause);
  visitor(value.input_snapshot);
  visitor(value.original_outcome_ref);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthEffect &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.code);
  visitor(value.skill_id);
  visitor(value.action_id);
  visitor(value.quote_id);
  visitor(value.success);
  visitor(value.already_completed);
  visitor(value.reopened);
  visitor(value.paused);
  visitor(value.mastery);
  visitor(value.xp_gained);
  visitor(value.fee_paid);
  visitor(value.unprovided_released);
  visitor(value.completed_lessons);
  visitor(value.elapsed);
  visitor(value.paused_ticks);
  visitor(value.reserved);
  visitor(value.challenge);
  visitor(value.public_challenge);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthPublicChallenge &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.phase);
  visitor(value.step_id);
  visitor(value.topic);
  visitor(value.stance);
  visitor(value.prompt);
  visitor(value.round);
  visitor(value.budget);
  visitor(value.quantity);
  visitor(value.options);
  visitor(value.conditions);
  visitor(value.result);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthPublicCondition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.kind);
  visitor(value.text);
  visitor(value.matched);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthPublicOption &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.text);
  visitor(value.kind);
  visitor(value.method);
  visitor(value.supply_condition_id);
  visitor(value.terms_condition_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StudyChallengeSource &value,
                  Visitor &&visitor) {
  visitor(value.field);
  visitor(value.kind);
  visitor(value.id);
  visitor(value.context);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::GrowthTeachingRequestDefinition &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.teacher_actor_id);
  visitor(value.student_actor_id);
  visitor(value.skill_id);
  visitor(value.dialogue_id);
  visitor(value.node_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthTeachingRequestCatalog &value,
                  Visitor &&visitor) {
  visitor(value.enabled);
  visitor(value.definitions);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthTacticLessonDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.tactic_id);
  visitor(value.teacher_actor_id);
  visitor(value.dialogue_id);
  visitor(value.offer_node_id);
  visitor(value.recipient_wallet_id);
  visitor(value.duration);
  visitor(value.total_fee);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthTacticLessonCatalog &value,
                  Visitor &&visitor) {
  visitor(value.definitions);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::GrowthStudyTeachingRequestDefinition &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.teacher_actor_id);
  visitor(value.student_actor_id);
  visitor(value.skill_id);
  visitor(value.course_role_id);
  visitor(value.dialogue_id);
  visitor(value.node_id);
  visitor(value.duration);
  visitor(value.payment_policy);
  visitor(value.tariff);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::GrowthStudyTeachingRequestCatalog &value,
      Visitor &&visitor) {
  visitor(value.enabled);
  visitor(value.definitions);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthShop &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.place_id);
  visitor(value.inventory_id);
  visitor(value.wallet_id);
  visitor(value.representative_roles);
  visitor(value.bargaining);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthRole &value,
                  Visitor &&visitor) {
  visitor(value.lessons);
  visitor(value.courses);
  visitor(value.activities);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthRecoveryCatalog &value,
                  Visitor &&visitor) {
  visitor(value.service_roles);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::GrowthPersonalMasteryInitialCatalog &value,
      Visitor &&visitor) {
  visitor(value.scenario_id);
  visitor(value.actors);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthMarketHistoryCatalog &value,
                  Visitor &&visitor) {
  visitor(value.shop_ids);
  visitor(value.initial_inventories);
  visitor(value.initial_next_instance_id);
  visitor(value.selected_version);
  visitor(value.resource_codec.item_instance_refs_enabled);
  visitor(value.resource_codec.lot_property_refs_enabled);
  visitor(value.resource_codec.domain_activity_reservation_refs_enabled);
  visitor(value.resource_codec.carrier_location_enabled);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthLesson &value,
                  Visitor &&visitor) {
  visitor(value.skill_id);
  visitor(value.wallet_id);
  visitor(value.required_trust);
  visitor(value.study_ticks);
  visitor(value.fee);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthInsightCardDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.name_key);
  visitor(value.description_key);
  visitor(value.hint_key);
  visitor(value.icon_id);
  visitor(value.condition);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthInsightCardCatalog &value,
                  Visitor &&visitor) {
  visitor(value.enabled);
  visitor(value.definitions);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthCourse &value,
                  Visitor &&visitor) {
  visitor(value.skill_id);
  visitor(value.mode);
  visitor(value.wallet_id);
  visitor(value.challenge);
  visitor(value.settlement_policy);
  visitor(value.definition_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthCatalog &value,
                  Visitor &&visitor) {
  visitor(value.cultivation);
  visitor(value.economy);
  visitor(value.family);
  visitor(value.regional_life);
  visitor(value.study_segments);
  visitor(value.roles);
  visitor(value.shops);
  visitor(value.etiquette);
  visitor(value.bargain);
  visitor(value.tradeable_items);
  visitor(value.place_names);
  visitor(value.organization_names);
  visitor(value.estate_ids);
  visitor(value.cards);
  visitor(value.actor_start);
  visitor(value.teaching_requests);
  visitor(value.insights);
  visitor(value.study_teaching_requests);
  visitor(value.innate_learning);
  visitor(value.complete_study_sources);
  visitor(value.complete_exact_period_sources);
  visitor(value.complete_exact_command_cuts);
  visitor(value.complete_exact_rest_mouth_sources);
  visitor(value.complete_capability_application_chain);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthCardInitialGrant &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.card_id);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthCardDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.name_key);
  visitor(value.description_key);
  visitor(value.hint_key);
  visitor(value.icon_id);
  visitor(value.personal_skill_id);
  visitor(value.category);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthCardCatalog &value,
                  Visitor &&visitor) {
  visitor(value.enabled);
  visitor(value.definitions);
  visitor(value.initial_grants);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthActorStartPolicy &value,
                  Visitor &&visitor) {
  visitor(value.cards);
  visitor(value.playable_actor_ids);
  visitor(value.initially_available_actor_ids);
  visitor(value.default_actor_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthActorCardDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.target_actor_id);
  visitor(value.name_key);
  visitor(value.description_key);
  visitor(value.hint_key);
  visitor(value.condition);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthActorCardCatalog &value,
                  Visitor &&visitor) {
  visitor(value.enabled);
  visitor(value.definitions);
  visitor(value.version);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BargainPolicy &value,
                  Visitor &&visitor) {
  visitor(value.challenge_id);
  visitor(value.source_sha256);
  visitor(value.condition_sources);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BargainConditionSource &value,
                  Visitor &&visitor) {
  visitor(value.condition_id);
  visitor(value.source);
  visitor(value.value);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ActionTimeRegistration &value,
                  Visitor &&visitor) {
  visitor(value.action_kind);
  visitor(value.owner_domain);
  visitor(value.profile_id);
  visitor(value.profile_version);
  visitor(value.source_schema);
  visitor(value.mode);
  visitor(value.remaining_meaning);
  visitor(value.shared_gate_kind);
  visitor(value.retained_wait_phases);
}

// History2 与真实交易终点的完整拥有字段；不按 wire 截短原 Common 库存或钱袋。
template <class Visitor>
inline void visit(const ::hanmo::resident::MarketLegacyOriginalStep &, Visitor &&) {
}
template <class Visitor>
inline void visit(const ::hanmo::resident::MarketSelectedTradeRows &value, Visitor &&visitor) {
  visitor(value.life_activity_id);
  visitor(value.source_schema);
  visitor(value.row_ordinals);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::MarketOriginalBusinessFifo &value, Visitor &&visitor) {
  visitor(value.business_fact_id);
  visitor(value.execution_stage_ordinal);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::MarketOriginalTick &, Visitor &&) {
}
template <class Visitor>
inline void visit(const ::hanmo::resident::MarketScalarActualStart &value, Visitor &&visitor) {
  visitor(value.started_at);
  visitor(value.action_ref);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::MarketProtectedHistoryBefore &, Visitor &&) {
}
template <class Visitor>
inline void visit(const ::hanmo::resident::MarketFullCapturedBefore &value, Visitor &&visitor) {
  visitor(value.inventory);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::MarketPriorPinnedStageAfter &value, Visitor &&visitor) {
  visitor(value.movement_id);
  visitor(value.execution_stage_ordinal);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::OriginalBusinessInventoryFrame &value, Visitor &&visitor) {
  visitor(value.inventory_id);
  visitor(value.before_basis);
  visitor(value.after_delta);
  visitor(value.field_order);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::MarketScalarTradeSlot &, Visitor &&) {
}
template <class Visitor>
inline void visit(const ::hanmo::resident::MarketProductionInputSlot &value, Visitor &&visitor) {
  visitor(value.receipt_input_ordinal);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::MarketProductionOutputSlot &value, Visitor &&visitor) {
  visitor(value.receipt_output_ordinal);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::MarketRationConsumptionSlot &, Visitor &&) {
}
template <class Visitor>
inline void visit(const ::hanmo::resident::MarketNotConsumption &, Visitor &&) {
}
template <class Visitor>
inline void visit(const ::hanmo::resident::MarketOriginalMaterialNeed &value, Visitor &&visitor) {
  visitor(value.rule_ref);
  visitor(value.need_slot_ordinal);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::MarketNotOutput &, Visitor &&) {
}
template <class Visitor>
inline void visit(const ::hanmo::resident::MarketOriginalPlainAdd &, Visitor &&) {
}
template <class Visitor>
inline void visit(const ::hanmo::resident::MarketRegisteredProductionOutput &value, Visitor &&visitor) {
  visitor(value.output_ordinal);
  visitor(value.properties_ref);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::MarketOriginalLotFlow &value, Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.source_inventory_id);
  visitor(value.source_lot_id);
  visitor(value.destination_inventory_id);
  visitor(value.destination_lot_id);
  visitor(value.item_id);
  visitor(value.quantity);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::OriginalBusinessItemOperation &value, Visitor &&visitor) {
  visitor(value.operation_index);
  visitor(value.source_slot);
  visitor(value.selections);
  visitor(value.material_need);
  visitor(value.output_basis);
  visitor(value.lot_flows);
  visitor(value.field_order);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::TradeWalletPosting &value, Visitor &&visitor) {
  visitor(value.wallet_id);
  visitor(value.owner);
  visitor(value.delta_qian);
  visitor(value.balance_before);
  visitor(value.balance_after);
  visitor(value.reservations_before);
  visitor(value.reservations_after);
  visitor(value.spendable_before);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::OriginalBusinessStage &value, Visitor &&visitor) {
  visitor(value.stage);
  visitor(value.next_instance_before);
  visitor(value.next_instance_after);
  visitor(value.inventory_frames);
  visitor(value.item_operations);
  visitor(value.wallet_postings);
  visitor(value.field_order);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::OriginalBusinessFifoExecution &value, Visitor &&visitor) {
  visitor(value.schema_version);
  visitor(value.adapter_rule_ref);
  visitor(value.time_basis);
  visitor(value.stages);
  visitor(value.field_order);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::TradeTerminalTime &value, Visitor &&visitor) {
  visitor(value.source_schema);
  visitor(value.ended_at);
}

// True original trade grants and their typed stable subkey.
template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthTradeGrantRef &value, Visitor &&visitor) {
  visitor(value.action_id);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::GrowthTradeGrant &value, Visitor &&visitor) {
  visitor(value.action_id);
  visitor(value.actor_id);
  visitor(value.skill_id);
  visitor(value.original_trade_result_ref);
  visitor(value.original_trade_source_schema);
  visitor(value.xp_before);
  visitor(value.actual_xp_gain);
  visitor(value.xp_after);
  visitor(value.capability_application);
}
