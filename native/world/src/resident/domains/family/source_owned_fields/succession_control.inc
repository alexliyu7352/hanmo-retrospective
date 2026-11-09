// 职责：集中家庭与接续中的死亡、继承与控制接续访问；仅借用原字段，不改变顺序或失败行为。
// 由中央登记头在 source_owned_fields
// 命名空间内包含；依赖与具名助手已先行可见。

template <class Visitor>
inline void visit(const ::hanmo::resident::AgeCheckFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.actor_id);
  visitor(value.world_id);
  visitor(value.scenario_id);
  visitor(value.age_basis);
  visitor(value.calendar_profile_ref);
  visitor(value.game_year_ordinal);
  visitor(value.age_years);
  visitor(value.life_policy_ref);
  visitor(value.settings_ref);
  visitor(value.was_controlled_at_check);
  visitor(value.threshold_bp);
  visitor(value.draw);
  visitor(value.outcome);
  visitor(value.from_tick);
  visitor(value.to_tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BequestDisclosureSource &value,
                  Visitor &&visitor) {
  visitor(value.declaration_id);
  visitor(value.donor_actor_id);
  visitor(value.beneficiary_actor_id);
  visitor(value.dialogue_id);
  visitor(value.node_id);
  visitor(value.node_kind);
  visitor(value.session_id);
  visitor(value.scenario_id);
  visitor(value.world_id);
  visitor(value.context);
  visitor(value.tick);
  visitor(value.revision);
  visitor(value.occurred_at);
  visitor(value.field_order);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::BiologicalControlCommitBinding &value,
      Visitor &&visitor) {
  visitor(value.result_id);
  visitor(value.actor_id);
  visitor(value.kind);
  visitor(value.request_seq);
  visitor(value.revision);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BiologicalControlOrigin &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.parent_actor_id);
  visitor(value.child_actor_id);
  visitor(value.original_catalog_actor_id);
  visitor(value.birth_fact_id);
  visitor(value.creation_fact_id);
  visitor(value.source_available_tick);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BiologicalControlState &value,
                  Visitor &&visitor) {
  visitor(value.commits);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CollateralControlState &value,
                  Visitor &&visitor) {
  visitor(value.recognitions);
  visitor(value.recognition_commits);
  visitor(value.legacy_commits);
  visitor(value.proposals);
  visitor(value.replies);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DeathRef &value, Visitor &&visitor) {
  visitor(value.domain);
  visitor(value.kind);
  visitor(value.id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DiscipleControlCommitBinding &value,
                  Visitor &&visitor) {
  visitor(value.result_id);
  visitor(value.actor_id);
  visitor(value.kind);
  visitor(value.request_seq);
  visitor(value.revision);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DiscipleControlState &value,
                  Visitor &&visitor) {
  visitor(value.commits);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EnvironmentalSeaDeathCause4 &value,
                  Visitor &&visitor) {
  visitor(value.sea_loss_result_id);
  visitor(value.actor_id);
  visitor(value.vessel_passenger_binding_ref);
  visitor(value.occurrence_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyBequestDeclaration &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.source);
  visitor(value.field_order);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::FamilyBequestDeclarationSource &value,
      Visitor &&visitor) {
  visitor(value.scope);
  visitor(value.scenario_id);
  visitor(value.world_id);
  visitor(value.tick);
  visitor(value.revision);
  visitor(value.donor_birth_tick);
  visitor(value.beneficiary_birth_tick);
  visitor(value.terms);
  visitor(value.field_order);
  visitor(value.age_sources);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyBequestDisclosure &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.source);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyBequestRevocation &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.declaration_id);
  visitor(value.donor_actor_id);
  visitor(value.tick);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyBequestState &value,
                  Visitor &&visitor) {
  visitor(value.declarations);
  visitor(value.revocations);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyBequestTerms &value,
                  Visitor &&visitor) {
  visitor(value.donor_actor_id);
  visitor(value.beneficiary_actor_id);
  visitor(value.relation_fact_id);
  visitor(value.source_wallet_id);
  visitor(value.source_inventory_id);
  visitor(value.money);
  visitor(value.lots);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyDeathPayload4 &value,
                  Visitor &&visitor) {
  visitor(value.death_id);
  visitor(value.effective_at);
  visitor(value.recorded_at);
  visitor(value.cause);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyDeathRecord &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.actor_id);
  visitor(value.scenario_id);
  visitor(value.cause_kind);
  visitor(value.cause_fact_id);
  visitor(value.battle_id);
  visitor(value.world_id);
  visitor(value.death_tick);
  visitor(value.recorded_tick);
  visitor(value.field_order);
  visitor(value.precise);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyDeathState &value,
                  Visitor &&visitor) {
  visitor(value.deaths);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyInheritanceAcceptance &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.declaration_id);
  visitor(value.disclosure_fact_id);
  visitor(value.beneficiary_actor_id);
  visitor(value.death_fact_id);
  visitor(value.notice_binding_fact_id);
  visitor(value.notice_task_id);
  visitor(value.notice_source_lot_id);
  visitor(value.notice_handoff_fact_id);
  visitor(value.notice_received_tick);
  visitor(value.tick);
  visitor(value.known_death);
  visitor(value.occurred_at);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyInheritanceKnowledge2 &value,
                  Visitor &&visitor) {
  visitor(value.receipt);
  visitor(value.learned_at);
  visitor(value.beneficiary_age);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyInheritanceSettlement &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.acceptance_fact_id);
  visitor(value.place_id);
  visitor(value.settlement_id);
  visitor(value.transfer);
  visitor(value.destination_lot_ids);
  visitor(value.tick);
  visitor(value.source_balance_before);
  visitor(value.destination_balance_before);
  visitor(value.occurred_at);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyInheritanceState &value,
                  Visitor &&visitor) {
  visitor(value.disclosures);
  visitor(value.acceptances);
  visitor(value.settlements);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyLegacyActivationFact &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.consent_fact_id);
  visitor(value.death_fact_id);
  visitor(value.tick);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyLegacyConsentFact &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.source);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyLegacyRevocationFact &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.consent_fact_id);
  visitor(value.source);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyLegacyState &value,
                  Visitor &&visitor) {
  visitor(value.consents);
  visitor(value.revocations);
  visitor(value.activations);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::HouseholdControlFact &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.definition_id);
  visitor(value.actor_id);
  visitor(value.household_group_id);
  visitor(value.source_role_id);
  visitor(value.operation);
  visitor(value.scenario_id);
  visitor(value.recorded_tick);
  visitor(value.effective_from_tick);
  visitor(value.through_tick);
  visitor(value.actor_accepted);
  visitor(value.field_order);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::HouseholdProvisionControlProof &value,
      Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.household_group_id);
  visitor(value.fact_id);
  visitor(value.source_role_id);
  visitor(value.operation);
  visitor(value.source_revision);
  visitor(value.effective_from_tick);
  visitor(value.through_tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LegacyControlDecisionSource &value,
                  Visitor &&visitor) {
  visitor(value.request_id);
  visitor(value.decision);
  visitor(value.predecessor_actor_id);
  visitor(value.successor_actor_id);
  visitor(value.relation_fact_id);
  visitor(value.dialogue_id);
  visitor(value.node_id);
  visitor(value.node_kind);
  visitor(value.session_id);
  visitor(value.scenario_id);
  visitor(value.world_id);
  visitor(value.context);
  visitor(value.terms_version);
  visitor(value.tick);
  visitor(value.revision);
  visitor(value.predecessor_birth_tick);
  visitor(value.successor_birth_tick);
  visitor(value.field_order);
  visitor(value.biological_origin);
  visitor(value.born_dialogue);
  visitor(value.collateral_dialogue);
  visitor(value.age_sources);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeAgeLastCheck &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.calendar_profile_ref);
  visitor(value.game_year_ordinal);
  visitor(value.check_fact_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeAgeSettingsChange &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.actor_id);
  visitor(value.before);
  visitor(value.after);
  visitor(value.effective_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeAgeState &value,
                  Visitor &&visitor) {
  visitor(value.checks);
  visitor(value.last_age_check_year);
  visitor(value.settings_changes);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MilitaryDeathCause4 &value,
                  Visitor &&visitor) {
  visitor(value.casualty_fact_id);
  visitor(value.battle_id);
  visitor(value.actor_id);
  visitor(value.occurrence_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NaturalAgeDeathCause4 &value,
                  Visitor &&visitor) {
  visitor(value.age_check_fact_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SuccessionAssets &value,
                  Visitor &&visitor) {
  visitor(value.wallets);
  visitor(value.goods);
  visitor(value.estates);
  visitor(value.disabled_roles);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SuccessionConsentFact &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.agreement);
  visitor(value.conclusion);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SuccessionConsentSource &value,
                  Visitor &&visitor) {
  visitor(value.request_id);
  visitor(value.relation_fact_id);
  visitor(value.dialogue_id);
  visitor(value.node_id);
  visitor(value.node_kind);
  visitor(value.session_id);
  visitor(value.scenario_id);
  visitor(value.world_id);
  visitor(value.context);
  visitor(value.terms_version);
  visitor(value.accepted_tick);
  visitor(value.accepted_revision);
  visitor(value.predecessor_birth_tick);
  visitor(value.successor_birth_tick);
  visitor(value.personal);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SuccessionEstate &value,
                  Visitor &&visitor) {
  visitor(value.estate_id);
  visitor(value.inventory_id);
  visitor(value.wallet_id);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SuccessionGoods &value,
                  Visitor &&visitor) {
  visitor(value.source_inventory_id);
  visitor(value.destination_inventory_id);
  visitor(value.lot_id);
  visitor(value.item_id);
  visitor(value.quantity);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SuccessionPersonalLot &value,
                  Visitor &&visitor) {
  visitor(value.lot_id);
  visitor(value.item_id);
  visitor(value.quantity);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SuccessionPersonalTerms &value,
                  Visitor &&visitor) {
  visitor(value.predecessor_actor_id);
  visitor(value.successor_actor_id);
  visitor(value.source_wallet_id);
  visitor(value.destination_wallet_id);
  visitor(value.source_inventory_id);
  visitor(value.destination_inventory_id);
  visitor(value.money);
  visitor(value.lots);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SuccessionPlanConclusion &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.tick);
  visitor(value.succession_fact_id);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SuccessionWallet &value,
                  Visitor &&visitor) {
  visitor(value.source_id);
  visitor(value.destination_id);
  visitor(value.amount);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VoluntarySuccessionFact &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.consent_fact_id);
  visitor(value.tick);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::WitnessedDeathNoticeSource &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.scenario_id);
  visitor(value.death_fact_id);
  visitor(value.decedent_actor_id);
  visitor(value.sender_actor_id);
  visitor(value.recipient_actor_id);
  visitor(value.casualty_fact_id);
  visitor(value.battle_id);
  visitor(value.unit_id);
  visitor(value.world_id);
  visitor(value.observed_tick);
  visitor(value.issued_tick);
  visitor(value.field_order);
}

// Fixed Control3 and original Military /3 owned fields; one shared path.
template <class Visitor>
inline void visit(const ::hanmo::resident::CauseRef &value, Visitor &&visitor) {
  visitor(value.domain);
  visitor(value.kind);
  visitor(value.fact_id);
  visitor(value.ordinal);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ConsentRef &value,
                  Visitor &&visitor) {
  visitor(value.proposal_id);
  visitor(value.terms_version);
  visitor(value.decision_actor_id);
  visitor(value.accepted_fact_id);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::ContinuationBoundarySessionRef &value,
      Visitor &&visitor) {
  visitor(value.session_id);
  visitor(value.session_generation);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ContinuationBoundaryWork &value,
                  Visitor &&visitor) {
  visitor(value.candidate);
  visitor(value.path);
  visitor(value.basis_ref);
  visitor(value.acceptance_ref);
  visitor(value.owner);
  visitor(value.session_ref);
  visitor(value.frozen_ordinal);
  visitor(value.next_operation);
  visitor(value.completed_source_refs);
  visitor(value.status);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ControlBinding &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.control_epoch);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PendingContinuation &value,
                  Visitor &&visitor) {
  visitor(value.pending_id);
  visitor(value.mode);
  visitor(value.predecessor_actor_id);
  visitor(value.source_ref);
  visitor(value.created_tick);
  visitor(value.generation);
  visitor(value.status);
  visitor(value.control_epoch_at_open);
  visitor(value.boundary_work);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ControlRequestOrigin &value,
                  Visitor &&visitor) {
  visitor(value.world_id);
  visitor(value.execution_epoch_at_commit);
  visitor(value.predecessor_actor_id);
  visitor(value.request_seq);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ControlActivationRecord &value,
                  Visitor &&visitor) {
  visitor(value.activation_id);
  visitor(value.pending_id);
  visitor(value.predecessor_actor_id);
  visitor(value.successor_actor_id);
  visitor(value.path);
  visitor(value.basis_ref);
  visitor(value.acceptance_ref);
  visitor(value.mode);
  visitor(value.source_ref);
  visitor(value.effective_tick);
  visitor(value.previous_control_epoch);
  visitor(value.new_control_epoch);
  visitor(value.command_result_ref);
  visitor(value.request_origin);
  visitor(value.command_kind);
  visitor(value.committed_tick);
  visitor(value.committed_revision);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ContinuationConsentSource &value,
                  Visitor &&visitor) {
  visitor(value.decision);
  visitor(value.consent_ref);
  visitor(value.original_source);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ControlContinuationState &value,
                  Visitor &&visitor) {
  visitor(value.control_binding);
  visitor(value.pending_continuations);
  visitor(value.activation_records);
  visitor(value.continuation_consent_sources);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ControlContinuationCatalog &value,
                  Visitor &&visitor) {
  visitor(value.enabled);
  visitor(value.selected_version);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ContinuationObserver &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.actor_id);
  visitor(value.control_epoch);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ContinuationCandidateRef &value,
                  Visitor &&visitor) {
  visitor(value.pending_id);
  visitor(value.pending_generation);
  visitor(value.candidate_actor_id);
  visitor(value.path);
  visitor(value.basis_ref);
  visitor(value.acceptance_ref);
  visitor(value.source_identity);
  visitor(value.control_epoch);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ContinuationBoundaryRef &value,
                  Visitor &&visitor) {
  visitor(value.candidate);
  visitor(value.owner);
  visitor(value.session_ref);
  visitor(value.session_generation);
  visitor(value.frozen_segment_ordinal);
  visitor(value.expected_next_boundary);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::StabilizeContinuationBoundary &value,
                  Visitor &&visitor) {
  visitor(value.pending_id);
  visitor(value.expected_pending_generation);
  visitor(value.candidate_actor_id);
  visitor(value.path);
  visitor(value.basis_ref);
  visitor(value.acceptance_ref);
  visitor(value.boundary_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ContinuationBoundaryResult &value,
                  Visitor &&visitor) {
  visitor(value.boundary_work_ref);
  visitor(value.status);
  visitor(value.settled_source_refs);
  visitor(value.world_half_days);
  visitor(value.next_boundary_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ContinuationBoundaryReceipt &value,
                  Visitor &&visitor) {
  visitor(value.request);
  visitor(value.result);
  visitor(value.actual_at);
  visitor(value.candidate_prefix);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SelectContinuation &value,
                  Visitor &&visitor) {
  visitor(value.pending_id);
  visitor(value.candidate_ref);
  visitor(value.expected_control_epoch);
  visitor(value.voluntary_confirmation);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CancelPendingRetirement &value,
                  Visitor &&visitor) {
  visitor(value.pending_id);
  visitor(value.expected_control_epoch);
  visitor(value.expected_pending_generation);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::AcceptWorkReassignment &value,
                  Visitor &&visitor) {
  visitor(value.approval_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PendingRetirementCancelled &value,
                  Visitor &&visitor) {
  visit_pending_retirement_cancelled_payload(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PendingRetirementCancelledBuildView &value,
      Visitor &&visitor) {
  visit_pending_retirement_cancelled_payload(value, visitor);
}

template <class Visitor>
inline void visit_payload(const ::hanmo::resident::CommitResult &value,
                          Visitor &&visitor) {
  visitor(value.ok);
  visitor(value.kind);
  visitor(value.result_id);
  visitor(value.tick);
  visitor(value.revision);
  visitor(value.details);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CancelContinuationBoundary &value,
                  Visitor &&visitor) {
  visitor(value.pending_id);
  visitor(value.expected_control_epoch);
  visitor(value.expected_pending_generation);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ContinuationBoundaryCancelled &value,
                  Visitor &&visitor) {
  visitor(value.request);
  visitor(value.generation);
  visitor(value.stopped_slice_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeAgeSettingsDefinition &value,
                  Visitor &&visitor) {
  visitor(value.ref);
  visitor(value.controlled_natural_death_enabled);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LifeAgeCatalog &value,
                  Visitor &&visitor) {
  visitor(value.policy_ref);
  visitor(value.initial_settings_ref);
  visitor(value.settings);
  visitor(value.calendar_profile_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SuccessionPersonalSelection &value,
                  Visitor &&visitor) {
  visitor(value.money);
  visitor(value.goods);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SuccessionItemSelection &value,
                  Visitor &&visitor) {
  visitor(value.item_id);
  visitor(value.quantity);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SuccessionConsentDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.predecessor_actor_id);
  visitor(value.successor_actor_id);
  visitor(value.dialogue_id);
  visitor(value.node_id);
  visitor(value.terms_version);
  visitor(value.personal);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SuccessionConsentCatalog &value,
                  Visitor &&visitor) {
  visitor(value.enabled);
  visitor(value.definitions);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::LegacyControlConsentDefinition &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.predecessor_actor_id);
  visitor(value.successor_actor_id);
  visitor(value.dialogue_id);
  visitor(value.accept_node_id);
  visitor(value.revoke_node_id);
  visitor(value.terms_version);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LegacyControlConsentCatalog &value,
                  Visitor &&visitor) {
  visitor(value.enabled);
  visitor(value.definitions);
  visitor(value.version);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::InnateInheritanceWeights &value,
                  Visitor &&visitor) {
  visitor(value.absent);
  visitor(value.retained);
  visitor(value.strengthened);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::HouseholdControlDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.actor_id);
  visitor(value.household_group_id);
  visitor(value.source_role_id);
  visitor(value.operation);
  visitor(value.effective_from_tick);
  visitor(value.through_tick);
  visitor(value.actor_accepted);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::HouseholdControlCatalog &value,
                  Visitor &&visitor) {
  visitor(value.initial_mandates);
  visitor(value.purchase_control_enabled);
  visitor(value.market_staffing_control_enabled);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyInheritancePolicy &value,
                  Visitor &&visitor) {
  visitor(value.enabled);
  visitor(value.selected_version);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyDeathPolicy &value,
                  Visitor &&visitor) {
  visitor(value.enabled);
  visitor(value.selected_version);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyBequestPolicy &value,
                  Visitor &&visitor) {
  visitor(value.enabled);
  visitor(value.selected_version);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DiscipleControlRequestBasis &value,
                  Visitor &&visitor) {
  visitor(value.request_id);
  visitor(value.skill_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DiscipleControlCatalog &value,
                  Visitor &&visitor) {
  visitor(value.enabled);
  visitor(value.requests);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CollateralControlDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.template_id);
  visitor(value.recognize_node_id);
  visitor(value.accept_node_id);
  visitor(value.revoke_node_id);
  visitor(value.terms_version);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CollateralControlCatalog &value,
                  Visitor &&visitor) {
  visitor(value.enabled);
  visitor(value.definitions);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BiologicalControlDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.predecessor_actor_id);
  visitor(value.successor_actor_id);
  visitor(value.dialogue_id);
  visitor(value.template_id);
  visitor(value.accept_node_id);
  visitor(value.revoke_node_id);
  visitor(value.meeting_kind);
  visitor(value.terms_version);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BiologicalControlCatalog &value,
                  Visitor &&visitor) {
  visitor(value.enabled);
  visitor(value.definitions);
}
