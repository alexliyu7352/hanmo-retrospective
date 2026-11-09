// 职责：集中政治与条约中的外交及条约通行依据访问；仅借用原字段，不改变顺序或失败行为。
// 由中央登记头在 source_owned_fields
// 命名空间内包含；依赖与具名助手已先行可见。

template <class Visitor>
inline void visit(const ::hanmo::resident::DiplomacyState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.offers);
  visitor(value.facts);
  visitor(value.presentations);
  visitor(value.replies);
  visitor(value.missions);
  visitor(value.v2);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DiplomacyV2State &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.mission_knowledge);
  visitor(value.reports);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DiplomaticEnvoyReportSource &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.mission_authority_fact_id);
  visitor(value.proposal_id);
  visitor(value.proposal_fact_id);
  visitor(value.presentation_fact_id);
  visitor(value.envoy_actor_id);
  visitor(value.terms_revision);
  visitor(value.completed_tick);
  visitor(value.task_id);
  visitor(value.contract_id);
  visitor(value.acceptance_fact_id);
  visitor(value.report_action_id);
  visitor(value.receiver);
  visitor(value.receiver_grant_id);
  visitor(value.report_site_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DiplomaticMissionApproval &value,
                  Visitor &&visitor) {
  visitor(value.source_identity);
  visitor(value.mission_authority_fact_id);
  visitor(value.proposal_id);
  visitor(value.proposal_fact_id);
  visitor(value.policy_id);
  visitor(value.rule_name_key);
  visitor(value.terms_revision);
  visitor(value.issuer_organization_id);
  visitor(value.target_organization_id);
  visitor(value.envoy_actor_id);
  visitor(value.issuer_office_id);
  visitor(value.issuer_actor_id);
  visitor(value.issuer_grant_id);
  visitor(value.issuer_authority_epoch);
  visitor(value.target_office_id);
  visitor(value.delivery_site_id);
  visitor(value.report_office_id);
  visitor(value.report_site_id);
  visitor(value.continuity_policy_id);
  visitor(value.issued_tick);
  visitor(value.acceptance_expires_tick);
  visitor(value.delivery_expires_tick);
  visitor(value.delivery_work_ticks);
  visitor(value.report_work_ticks);
  visitor(value.return_report_required);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DiplomaticMissionKnowledge &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.delegation_policy_id);
  visitor(value.mission_authority_fact_id);
  visitor(value.envoy_actor_id);
  visitor(value.issuer_actor_id);
  visitor(value.session_actor_id);
  visitor(value.session_id);
  visitor(value.site_id);
  visitor(value.role_id);
  visitor(value.provider_actor_id);
  visitor(value.context_kind);
  visitor(value.tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DiplomaticOffer &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.policy_id);
  visitor(value.terms_id);
  visitor(value.issuer_grant_id);
  visitor(value.envoy_actor_id);
  visitor(value.issuer);
  visitor(value.issued_tick);
  visitor(value.expires_tick);
  visitor(value.terms_revision);
  visitor(value.revision);
  visitor(value.status);
  visitor(value.last_fact_id);
  visitor(value.presentation_fact_id);
  visitor(value.mission_authority_fact_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DiplomaticOfferFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.offer_id);
  visitor(value.kind);
  visitor(value.actor_id);
  visitor(value.terms_id);
  visitor(value.tick);
  visitor(value.terms_revision);
  visitor(value.offer_revision);
  visitor(value.cause_fact_ids);
  visitor(value.witness_actor_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DiplomaticPresentation &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.offer_id);
  visitor(value.proposal_fact_id);
  visitor(value.terms_id);
  visitor(value.envoy_actor_id);
  visitor(value.representative_grant_id);
  visitor(value.representative);
  visitor(value.tick);
  visitor(value.terms_revision);
  visitor(value.offer_revision);
  visitor(value.session_actor_id);
  visitor(value.session_id);
  visitor(value.place_id);
  visitor(value.provider_actor_id);
  visitor(value.role_id);
  visitor(value.context_kind);
  visitor(value.task_binding);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::DiplomaticPresentationTaskBinding &value,
      Visitor &&visitor) {
  visitor(value.mission_authority_fact_id);
  visitor(value.task_id);
  visitor(value.contract_id);
  visitor(value.action_id);
  visitor(value.acceptance_fact_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DiplomaticReply &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.offer_id);
  visitor(value.presentation_fact_id);
  visitor(value.terms_id);
  visitor(value.grant_id);
  visitor(value.authority);
  visitor(value.decision);
  visitor(value.tick);
  visitor(value.terms_revision);
  visitor(value.offer_revision);
  visitor(value.session_actor_id);
  visitor(value.session_id);
  visitor(value.place_id);
  visitor(value.provider_actor_id);
  visitor(value.role_id);
  visitor(value.context_kind);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DiplomaticReportRecord &value,
                  Visitor &&visitor) {
  visitor(value.source);
  visitor(value.delivery_fact_id);
  visitor(value.session_actor_id);
  visitor(value.provider_actor_id);
  visitor(value.role_id);
  visitor(value.context_kind);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyPassageContact &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.target_actor_id);
  visitor(value.session_actor_id);
  visitor(value.session_id);
  visitor(value.place_id);
  visitor(value.provider_actor_id);
  visitor(value.role_id);
  visitor(value.context_kind);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyPassageControlSnapshot &value,
                  Visitor &&visitor) {
  visitor(value.jurisdiction_id);
  visitor(value.organization_id);
  visitor(value.occupation_fact_id);
  visitor(value.revision);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyPassageDelivery &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.offer_id);
  visitor(value.accepted_reply_id);
  visitor(value.source_fact_id);
  visitor(value.tick);
  visitor(value.terms_revision);
  visitor(value.contact);
  visitor(value.recipient_authority);
  visitor(value.recipient_grant_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyPassageSignature &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.offer_id);
  visitor(value.accepted_reply_id);
  visitor(value.terms_id);
  visitor(value.knowledge_source_fact_id);
  visitor(value.prior_signature_id);
  visitor(value.side);
  visitor(value.authority);
  visitor(value.witness_authority);
  visitor(value.grant_id);
  visitor(value.witness_grant_id);
  visitor(value.tick);
  visitor(value.terms_revision);
  visitor(value.contact);
  visitor(value.controls);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyPassageState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.deliveries);
  visitor(value.signatures);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyPassageProvision &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.terms_id);
  visitor(value.places);
  visitor(value.npc_ratifier_office_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyPassagePlaceBinding &value,
                  Visitor &&visitor) {
  visitor(value.place_id);
  visitor(value.jurisdiction_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyPassageCatalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.enabled);
  visitor(value.provisions);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DiplomaticResponsePolicy &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.office_id);
  visitor(value.directives);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DiplomaticResponseDirective &value,
                  Visitor &&visitor) {
  visitor(value.mission_policy_id);
  visitor(value.terms_id);
  visitor(value.choice);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DiplomaticPassageTerms &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.name_key);
  visitor(value.grantor_organization_id);
  visitor(value.beneficiary_organization_id);
  visitor(value.place_ids);
  visitor(value.duration_ticks);
  visitor(value.expiry_notice_ticks);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DiplomaticMissionTaskTerms &value,
                  Visitor &&visitor) {
  visitor(value.acceptance_valid_ticks);
  visitor(value.delivery_work_ticks);
  visitor(value.report_work_ticks);
  visitor(value.return_report_required);
  visitor(value.continuity_policy_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DiplomaticMissionPolicy &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.name_key);
  visitor(value.issuer_organization_id);
  visitor(value.target_organization_id);
  visitor(value.issuer_site_id);
  visitor(value.target_site_id);
  visitor(value.target_office_id);
  visitor(value.passage_terms_ids);
  visitor(value.offer_valid_ticks);
  visitor(value.task_terms);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DiplomaticDelegationPolicy &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.mission_policy_id);
  visitor(value.terms_id);
  visitor(value.issuer_office_id);
  visitor(value.envoy_actor_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DiplomacyV2Catalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.delegation_policies);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DiplomacyCatalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.enabled);
  visitor(value.passage_terms);
  visitor(value.mission_policies);
  visitor(value.v2);
}

// 真实已定义的依据值仅遍历拥有字段；不授予条约执行能力。
template <class Visitor>
inline void visit(const ::hanmo::resident::ActivatedTreatyPassageBasis &value,
                  Visitor &&visitor) {
  visitor(value.activation_ref);
  visitor(value.term_id);
}

// native/world/src/resident/domains/politics/treaty/political_passage_decision.hpp:15;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::ExistingOriginalPassageBasis &value,
                  Visitor &&visitor) {
  visitor(value.policy_id);
  visitor(value.policy_version);
  visitor(value.catalog_digest);
}

// native/world/src/resident/domains/politics/treaty/treaty_mandate_sources.hpp:5;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::NegotiationMandate &value,
                  Visitor &&visitor) {
  visitor(value.allowed_term_kinds);
  visitor(value.scope);
  visitor(value.quantity_caps);
  visitor(value.money_cap);
  visitor(value.window);
  visitor(value.may_sign);
}

// native/world/src/resident/domains/politics/treaty/political_passage_decision.hpp:8;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::PassageTariff &value,
                  Visitor &&visitor) {
  visitor(value.source);
  visitor(value.kind);
  visitor(value.currency_id);
  visitor(value.payee_wallet_id);
  visitor(value.amount_per_unit);
  visitor(value.weight_unit);
}

// native/world/src/resident/domains/politics/treaty/political_passage_decision.hpp:12;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::PassageToll &value,
                  Visitor &&visitor) {
  visitor(value.currency_id);
  visitor(value.amount);
  visitor(value.payee_wallet_id);
  visitor(value.tariff_source_ref);
}

// native/world/src/resident/domains/politics/treaty/political_passage_decision.hpp:32;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalPassageDecision &value,
                  Visitor &&visitor) {
  visitor(value.identity);
  visitor(value.boundary);
  visitor(value.subject);
  visitor(value.mode);
  visitor(value.entry_tick);
  visitor(value.admission);
  visitor(value.toll);
  visitor(value.authority_basis_refs);
  visitor(value.treaty_term_refs);
  visitor(value.local_delivery_source_refs);
  visitor(value.admission_source);
  visitor(value.requester);
  visitor(value.source_schema_version);
  visitor(value.visible_reason_code);
  visitor(value.read_stamp);
  visitor(value.purpose_context);
  visitor(value.authority_basis);
  visitor(value.requested_window);
}

// native/world/src/resident/domains/politics/treaty/political_passage_source.hpp:10;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalPassageDecisionSource &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.execution);
  visitor(value.carrier);
  visitor(value.entry_ordinal);
  visitor(value.composition_revision);
  visitor(value.manifest_revision);
  visitor(value.occurred_at);
  visitor(value.decision);
}

// native/world/src/resident/domains/politics/treaty/safe_exit_entry.hpp:20;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::SafeExitBinding &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.treaty_id);
  visitor(value.terms_revision);
  visitor(value.term_id);
  visitor(value.entry_receipt_ref);
  visitor(value.entry_carrier);
  visitor(value.entered_manifest_basis);
  visitor(value.entered_subject_refs);
  visitor(value.exit_policy_ref);
  visitor(value.state);
  visitor(value.termination_source);
  visitor(value.termination_at);
  visitor(value.exit_from);
  visitor(value.exit_until);
  visitor(value.exit_line_refs);
  visitor(value.manifest_successor_refs);
  visitor(value.last_result_ref);
}

// native/world/src/resident/domains/politics/treaty/safe_exit_entry.hpp:11;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::SafeExitCargoScopeRef &value,
                  Visitor &&visitor) {
  visitor(value.entry_manifest_ref);
  visitor(value.inventory_id);
}

// native/world/src/resident/domains/politics/treaty/safe_exit_entry.hpp:9;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void
visit(const ::hanmo::resident::SafeExitCivilianSelectionScope &value,
      Visitor &&visitor) {
  visitor(value.population_selection_or_entry_ref);
  visitor(value.current_successor_refs);
}

// native/world/src/resident/domains/politics/treaty/safe_exit_entry.hpp:13;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::SafeExitLine &value,
                  Visitor &&visitor) {
  visitor(value.line_id);
  visitor(value.binding_id);
  visitor(value.entitlement_source_ref);
  visitor(value.subject_scope);
  visitor(value.cargo_scope_refs);
  visitor(value.chosen_path_id);
  visitor(value.last_exit_boundary_ordinal);
  visitor(value.state);
  visitor(value.actual_crossing_refs);
}

// native/world/src/resident/domains/politics/treaty/safe_exit_entry.hpp:8;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::SafeExitNamedSubjects &value,
                  Visitor &&visitor) {
  visitor(value.actor_or_member_refs);
}

// native/world/src/resident/domains/politics/treaty/treaty_terms_contract.hpp:9;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::SafeExitPath &value,
                  Visitor &&visitor) {
  visitor(value.path_id);
  visitor(value.inside_start_scope);
  visitor(value.directed_boundary_refs);
  visitor(value.outside_end_site);
}

// native/world/src/resident/domains/politics/treaty/treaty_terms_contract.hpp:13;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::SafeExitTerms &value,
                  Visitor &&visitor) {
  visitor(value.grace_ticks);
  visitor(value.max_exit_half_days);
  visitor(value.paths);
  visitor(value.eligible_manifest_policy);
  visitor(value.include_birth_dependents);
  visitor(value.allowed_actions);
  visitor(value.tariff_rule_ref);
  visitor(value.blocked_fallback);
}

// native/world/src/resident/domains/politics/treaty/treaty_activation_sources.hpp:6;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyActivationSource &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.treaty_id);
  visitor(value.terms_revision);
  visitor(value.terms_source);
  visitor(value.signature_refs);
  visitor(value.activated_at);
}

// native/world/src/resident/domains/politics/treaty/treaty_terms_contract.hpp:46;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyAidItemAmount &value,
                  Visitor &&visitor) {
  visitor(value.item_id);
  visitor(value.quantity);
}

// native/world/src/resident/domains/politics/treaty/treaty_terms_contract.hpp:47;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyAidMoney &value,
                  Visitor &&visitor) {
  visitor(value.currency_id);
  visitor(value.amount);
}

// native/world/src/resident/domains/politics/treaty/treaty_aid_occurrence.hpp:8;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyAidOccurrenceBinding &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.schedule_ref);
  visitor(value.occurrence_ordinal);
  visitor(value.establishment_result_ref);
  visitor(value.original_obligation_ref);
  visitor(value.shipment_ref);
  visitor(value.receipt_result_refs);
  visitor(value.deadline_result_ref);
  visitor(value.termination_disposition_ref);
}

// native/world/src/resident/domains/politics/treaty/treaty_aid_schedule.hpp:6;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyAidSchedule &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.treaty_id);
  visitor(value.terms_revision);
  visitor(value.term_id);
  visitor(value.activation_ref);
  visitor(value.schedule_terms_ref);
  visitor(value.next_occurrence_ordinal);
  visitor(value.termination_ref);
}

// native/world/src/resident/domains/politics/treaty/treaty_terms_contract.hpp:51;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyBoundedCompensation &value,
                  Visitor &&visitor) {
  visitor(value.currency_id);
  visitor(value.maximum_amount);
  visitor(value.obligation_owner);
}

// native/world/src/resident/domains/politics/treaty/treaty_termination_sources.hpp:9;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void
visit(const ::hanmo::resident::TreatyBreachAfterNoticeTermination &value,
      Visitor &&visitor) {
  visitor(value.incident_ref);
  visitor(value.notice_refs);
  visitor(value.actual_decision_ref);
}

// native/world/src/resident/domains/politics/treaty/treaty_revision_sources.hpp:10;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyCounterofferSource &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.treaty_id);
  visitor(value.terms_revision);
  visitor(value.previous_terms_source);
  visitor(value.terms_definition_id);
  visitor(value.offering_party);
  visitor(value.issuer);
  visitor(value.issued_at);
}

// native/world/src/resident/domains/politics/treaty/treaty_signature_sources.hpp:8;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyDirectOfficeSigner &value,
                  Visitor &&visitor) {
  visitor(value.authority);
}

// native/world/src/resident/domains/politics/treaty/treaty_termination_sources.hpp:7;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyExpiryTermination &value,
                  Visitor &&visitor) {
}

// native/world/src/resident/domains/politics/treaty/treaty_incident.hpp:11;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyIncident &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.treaty_id);
  visitor(value.terms_revision);
  visitor(value.term_id);
  visitor(value.source_fact_ref);
  visitor(value.occurrence_tick);
  visitor(value.incident_kind);
  visitor(value.liable_party);
  visitor(value.remaining_obligation_refs);
}

// native/world/src/resident/domains/politics/treaty/treaty_mandate_sources.hpp:11;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyMandateSource &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.treaty_id);
  visitor(value.original_terms_revision);
  visitor(value.original_terms_source);
  visitor(value.issuer);
  visitor(value.envoy_actor_id);
  visitor(value.mandate);
  visitor(value.issued_at);
}

// native/world/src/resident/domains/politics/treaty/treaty_signature_sources.hpp:9;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyMandatedSigner &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.actual_mandate_ref);
  visitor(value.actual_task_acceptance_ref);
}

// native/world/src/resident/domains/politics/treaty/treaty_terms_contract.hpp:53;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyMaterialAidTerm &value,
                  Visitor &&visitor) {
  visitor(value.source_party);
  visitor(value.beneficiary);
  visitor(value.delivery_site);
  visitor(value.items);
  visitor(value.money);
  visitor(value.schedule);
  visitor(value.period_ticks);
  visitor(value.occurrences);
  visitor(value.first_due_tick);
  visitor(value.delivery_deadline_ticks);
  visitor(value.transport_cost_payer);
  visitor(value.budget_ref);
  visitor(value.late_policy);
  visitor(value.establishment_gate);
}

// native/world/src/resident/domains/politics/treaty/treaty_terms_contract.hpp:38;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyMutualDefenseTerm &value,
                  Visitor &&visitor) {
  visitor(value.protected_parties);
  visitor(value.covered_jurisdictions);
  visitor(value.trigger_kind);
  visitor(value.eligible_force_pool_ids);
  visitor(value.max_dispatched_persons);
  visitor(value.max_concurrent_calls);
  visitor(value.dispatch_within_ticks);
  visitor(value.max_service_ticks);
  visitor(value.supply_contract_refs);
  visitor(value.budget_ref);
  visitor(value.return_policy);
}

// native/world/src/resident/domains/politics/treaty/treaty_termination_sources.hpp:8;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void
visit(const ::hanmo::resident::TreatyMutualReleaseTermination &value,
      Visitor &&visitor) {
  visitor(value.release_acceptance_refs);
}

// native/world/src/resident/domains/politics/treaty/treaty_incident.hpp:7;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyObligationRef &value,
                  Visitor &&visitor) {
  visitor(value.owner_domain);
  visitor(value.contract_id);
  visitor(value.term_id);
  visitor(value.terms_revision);
}

// native/world/src/resident/domains/politics/treaty/treaty_revision_sources.hpp:7;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyOfferSource &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.terms_definition_id);
  visitor(value.offering_party);
  visitor(value.issuer);
  visitor(value.issued_at);
}

// native/world/src/resident/domains/politics/treaty/treaty_terms_contract.hpp:22;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyOrganizationPair &value,
                  Visitor &&visitor) {
  visitor(value.first);
  visitor(value.second);
}

// native/world/src/resident/domains/politics/treaty/treaty_terms_contract.hpp:30;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyPassageTerm &value,
                  Visitor &&visitor) {
  visitor(value.eligible_parties);
  visitor(value.allowed_modes);
  visitor(value.boundary_refs);
  visitor(value.window);
  visitor(value.credential_policy);
  visitor(value.tariff);
  visitor(value.safe_exit);
}

// native/world/src/resident/domains/politics/treaty/treaty_recognition_sources.hpp:6;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyPresentationSource &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.treaty_id);
  visitor(value.terms_revision);
  visitor(value.terms_source);
  visitor(value.recipient);
  visitor(value.actual_content_receipt);
  visitor(value.presented_at);
}

// native/world/src/resident/domains/politics/treaty/treaty_terms_contract.hpp:50;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyRecordBreach &value,
                  Visitor &&visitor) {
}

// native/world/src/resident/domains/politics/treaty/treaty_recognition_sources.hpp:20;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyReleaseAnswerSource &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.treaty_id);
  visitor(value.terms_revision);
  visitor(value.activation_source);
  visitor(value.request_source);
  visitor(value.respondent);
  visitor(value.decision);
  visitor(value.actual_request_receipt);
  visitor(value.answered_at);
}

// native/world/src/resident/domains/politics/treaty/treaty_recognition_sources.hpp:16;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyReleaseRequestSource &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.treaty_id);
  visitor(value.terms_revision);
  visitor(value.activation_source);
  visitor(value.terms_source);
  visitor(value.requester);
  visitor(value.requested_at);
}

// native/world/src/resident/domains/politics/treaty/treaty_recognition_sources.hpp:11;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyReplySource &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.treaty_id);
  visitor(value.terms_revision);
  visitor(value.terms_source);
  visitor(value.presentation_source);
  visitor(value.respondent);
  visitor(value.decision);
  visitor(value.replied_at);
}

// 安全退出依据按原型完整拥有字段遍历，不因此打开 P2 执行入口。
template <class Visitor>
inline void visit(const ::hanmo::resident::TreatySafeExitPassageBasis &value,
                  Visitor &&visitor) {
  visitor(value.binding_id);
  visitor(value.policy_version);
  visitor(value.entry_receipt_ref);
  visitor(value.current_manifest_successor_refs);
  visitor(value.exit_until);
}

// native/world/src/resident/domains/politics/treaty/treaty_signature_sources.hpp:14;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::TreatySignatureSource &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.treaty_id);
  visitor(value.terms_revision);
  visitor(value.terms_source);
  visitor(value.party_id);
  visitor(value.signer);
  visitor(value.actual_content_receipt);
  visitor(value.signed_at);
}

// native/world/src/resident/domains/politics/treaty/treaty_terms_contract.hpp:63;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::TreatySubmissionTerm &value,
                  Visitor &&visitor) {
  visitor(value.subordinate_org);
  visitor(value.superior_org);
  visitor(value.jurisdiction_ids);
  visitor(value.shared_powers);
  visitor(value.retained_policy_id);
  visitor(value.command_bindings);
  visitor(value.tribute_term_ids);
  visitor(value.exit_policy);
  visitor(value.notice_ticks);
}

// native/world/src/resident/domains/politics/treaty/treaty_terms_contract.hpp:70;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyTerm &value,
                  Visitor &&visitor) {
  visitor(value.term_id);
  visitor(value.terms);
}

// native/world/src/resident/domains/politics/treaty/treaty_termination_sources.hpp:13;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyTerminationSource &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.treaty_id);
  visitor(value.terms_revision);
  visitor(value.activation_ref);
  visitor(value.cause);
  visitor(value.terminated_at);
}

// native/world/src/resident/domains/politics/treaty/treaty_terms_contract.hpp:78;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyTermsDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.terms);
}

// native/world/src/resident/domains/politics/treaty/treaty_terms_contract.hpp:73;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyTermsV1 &value,
                  Visitor &&visitor) {
  visitor(value.treaty_kind);
  visitor(value.parties);
  visitor(value.terms);
  visitor(value.effective_window);
  visitor(value.ratification_policy);
  visitor(value.termination_policy);
  visitor(value.rule_version);
}

// native/world/src/resident/domains/politics/treaty/treaty_terms_contract.hpp:23;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyTruceTerm &value,
                  Visitor &&visitor) {
  visitor(value.organization_pairs);
  visitor(value.jurisdiction_ids);
  visitor(value.boundary_refs);
  visitor(value.prohibited_actions);
  visitor(value.notice_ticks);
  visitor(value.withdrawal_route_policy);
}

// Field 守门只登记真实原拥有体；选择与来源仍由原 validator 裁决。
template<class Visitor> inline void visit(const ::hanmo::resident::FieldGroupPassagePolicy &value, Visitor &&visitor) {
  visitor(value.id);
  visitor(value.version);
  visitor(value.boundary);
  visitor(value.organization_id);
}

// Field 守门只登记真实原拥有体；选择与来源仍由原 validator 裁决。
template<class Visitor> inline void visit(const ::hanmo::resident::FieldGroupPassageCatalog &value, Visitor &&visitor) {
  visitor(value.identity);
  visitor(value.schema_version);
  visitor(value.provenance);
  visitor(value.policies);
}

// Field 守门只登记真实原拥有体；选择与来源仍由原 validator 裁决。
template<class Visitor> inline void visit(const ::hanmo::resident::PoliticalPassageDecisionState &value, Visitor &&visitor) {
  visitor(value.decisions);
}

// Field 守门只登记真实原拥有体；选择与来源仍由原 validator 裁决。
template<class Visitor> inline void visit(const ::hanmo::resident::SafeExitState &value, Visitor &&visitor) {
  visitor(value.bindings);
  visitor(value.lines);
}
