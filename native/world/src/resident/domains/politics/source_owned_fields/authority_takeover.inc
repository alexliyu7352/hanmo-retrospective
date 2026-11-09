// 职责：集中政治与条约中的权责、接管和公共政策访问；仅借用原字段，不改变顺序或失败行为。
// 由中央登记头在 source_owned_fields
// 命名空间内包含；依赖与具名助手已先行可见。

template <class Visitor>
inline void visit(const ::hanmo::resident::AppeasementRule &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.version);
  visitor(value.name_key);
  visitor(value.grain_item_id);
  visitor(value.duration_ticks);
  visitor(value.cash_amount);
  visitor(value.grain_amount);
  visitor(value.base_support_gain);
  visitor(value.support_cap);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CivilConsequence &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.jurisdiction_id);
  visitor(value.source_domain);
  visitor(value.source_fact_id);
  visitor(value.plan_id);
  visitor(value.previous_fact_id);
  visitor(value.tick);
  visitor(value.before);
  visitor(value.after);
  visitor(value.requested_delta);
  visitor(value.revision);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FiscalPolicyRecord &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.issuer_actor_id);
  visitor(value.issuer_office_id);
  visitor(value.grant_id);
  visitor(value.authority_epoch);
  visitor(value.snapshot);
  visitor(value.issued_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FiscalPolicySnapshot &value,
                  Visitor &&visitor) {
  visitor(value.jurisdiction_id);
  visitor(value.site_id);
  visitor(value.organization_id);
  visitor(value.public_wallet_id);
  visitor(value.public_inventory_id);
  visitor(value.source_fact_id);
  visitor(value.policy_revision);
  visitor(value.issued_tick);
  visitor(value.effective_from_tick);
  visitor(value.mode);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::Jurisdiction &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.name_key);
  visitor(value.office_name_key);
  visitor(value.seat_place_id);
  visitor(value.political_organization_id);
  visitor(value.controlling_organization_id);
  visitor(value.office_id);
  visitor(value.public_wallet_id);
  visitor(value.public_inventory_ids);
  visitor(value.revision);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::JurisdictionCivilState &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.registration_fact_id);
  visitor(value.last_fact_id);
  visitor(value.support);
  visitor(value.revision);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::LegacyPatrolSecuritySkillSnapshot &value,
      Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.skill_id);
  visitor(value.experience);
  visitor(value.level);
  visitor(value.original_qualification_sources);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OccupationFiscalPolicyRecord &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.registration_fact_id);
  visitor(value.grant_id);
  visitor(value.authority);
  visitor(value.policy);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OccupationFiscalState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.policies);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::OccupationFiscalTaxpayerContainer &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.owner);
  visitor(value.occupation_fact_ids);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalAuthoredResponsibilitySource &value,
      Visitor &&visitor) {
  visitor(value.table);
  visitor(value.id);
  visitor(value.parent_id);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalDismissalResultBinding &value,
      Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.result_id);
  visitor(value.request_sequence);
  visitor(value.result_revision);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalDismissalSource &value,
                  Visitor &&visitor) {
  visitor(value.policy_id);
  visitor(value.dismissal_fact_id);
  visitor(value.office_change_fact_id);
  visitor(value.grant_id);
  visitor(value.removed_actor_id);
  visitor(value.authority);
  visitor(value.tick);
  visitor(value.jurisdiction_revision);
  visitor(value.previous_target_epoch);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalDismissalSourceRecord &value,
      Visitor &&visitor) {
  visitor(value.source);
  visitor(value.actual_result);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalDismissalSourceState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.dismissals);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalExecutionAccount &value,
                  Visitor &&visitor) {
  visitor(value.role);
  visitor(value.original_container_id);
  visitor(value.execution_container_id);
  visitor(value.inventory);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalFieldAnswer &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.authorization_fact_id);
  visitor(value.actor_id);
  visitor(value.accepted);
  visitor(value.witness_actor_ids);
  visitor(value.tick);
  visitor(value.apply_before_tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalFieldAuthorization &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.policy_id);
  visitor(value.grant_id);
  visitor(value.authority);
  visitor(value.target);
  visitor(value.tick);
  visitor(value.answer_before_tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalFieldWithdrawal &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.authorization_fact_id);
  visitor(value.grant_id);
  visitor(value.authority);
  visitor(value.witness_actor_ids);
  visitor(value.tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalFiscalState &value,
                  Visitor &&visitor) {
  visitor(value.policies);
  visitor(value.version);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalMilitaryAnswer &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.authorization_fact_id);
  visitor(value.actor_id);
  visitor(value.accepted);
  visitor(value.witness_actor_ids);
  visitor(value.tick);
  visitor(value.apply_before_tick);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalMilitaryAuthorization &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.policy_id);
  visitor(value.grant_id);
  visitor(value.meeting_place_id);
  visitor(value.authority);
  visitor(value.target);
  visitor(value.tick);
  visitor(value.answer_before_tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalMilitaryWithdrawal &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.authorization_fact_id);
  visitor(value.grant_id);
  visitor(value.witness_actor_ids);
  visitor(value.authority);
  visitor(value.tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalOccupationRuleSource &value,
                  Visitor &&visitor) {
  visitor(value.rule_id);
  visitor(value.rule_version);
  visitor(value.integration_ticks);
  visitor(value.income_basis_points);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalOriginalResponsibility &value,
      Visitor &&visitor) {
  visitor(value.responsibility);
  visitor(value.principal);
  visitor(value.assignee);
  visitor(value.beneficiaries);
  visitor(value.delivery_beneficiaries);
  visitor(value.assets);
  visitor(value.containers);
  visitor(value.protected_custody);
  visitor(value.source_fact_ids);
  visitor(value.authored_sources);
  visitor(value.record_sources);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalProtectedCustody &value,
                  Visitor &&visitor) {
  visitor(value.inventory_id);
  visitor(value.lot_id);
  visitor(value.task_id);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalResponsibilityAssetSource &value,
      Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.id);
  visitor(value.frozen_owner);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalResponsibilityContainer &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.role);
  visitor(value.inventory);
  visitor(value.selected);
  visitor(value.item_id);
  visitor(value.original_owner);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalResponsibilityDecision &value,
      Visitor &&visitor) {
  visitor(value.decision_fact_id);
  visitor(value.takeover_fact_id);
  visitor(value.responsibility);
  visitor(value.disposition);
  visitor(value.original_principal);
  visitor(value.original_beneficiaries);
  visitor(value.original_assignee);
  visitor(value.original_delivery_beneficiaries);
  visitor(value.execution_principal);
  visitor(value.collection_beneficiaries);
  visitor(value.original_assets);
  visitor(value.payment_wallet_id);
  visitor(value.receipt_wallet_id);
  visitor(value.payment_inventory_id);
  visitor(value.receipt_inventory_id);
  visitor(value.production_asset_ids);
  visitor(value.execution_accounts);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalResponsibilityRecordSource &value,
      Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalResponsibilityRef &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.responsibility_id);
  visitor(value.unit_id);
  visitor(value.asset_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalShopClearance &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.court_id);
  visitor(value.registry_fact_id);
  visitor(value.grant_id);
  visitor(value.request);
  visitor(value.authority);
  visitor(value.session_id);
  visitor(value.role_id);
  visitor(value.dialogue_id);
  visitor(value.node_id);
  visitor(value.tick);
  visitor(value.valid_through_tick);
  visitor(value.occurred_at);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalShopLicensingPlayerResult &value,
      Visitor &&visitor) {
  visitor(value.source_fact_id);
  visitor(value.actor_id);
  visitor(value.command_kind);
  visitor(value.result_id);
  visitor(value.tick);
  visitor(value.request_sequence);
  visitor(value.result_revision);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalShopLicensingReceiptState &value,
      Visitor &&visitor) {
  visitor(value.version);
  visitor(value.bindings);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalShopLicensingState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.registries);
  visitor(value.warrants);
  visitor(value.releases);
  visitor(value.clearances);
  visitor(value.permits);
  visitor(value.revocations);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalShopPermit &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.clearance_fact_id);
  visitor(value.tick);
  visitor(value.occurred_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalShopRegistry &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.court_id);
  visitor(value.tick);
  visitor(value.occurred_at);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalShopRegistryAcceptance &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.policy_id);
  visitor(value.court_id);
  visitor(value.initial_registry_fact_id);
  visitor(value.previous_acceptance_fact_id);
  visitor(value.registration_fact_id);
  visitor(value.approval_fact_id);
  visitor(value.occupation_fact_id);
  visitor(value.authority);
  visitor(value.grant_id);
  visitor(value.tick);
  visitor(value.retained_legal_fact_ids);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalShopRegistryPlayerReceipt &value,
      Visitor &&visitor) {
  visitor(value.result_id);
  visitor(value.request_sequence);
  visitor(value.result_revision);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalShopRevocation &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.permit_fact_id);
  visitor(value.grant_id);
  visitor(value.authority);
  visitor(value.tick);
  visitor(value.occurred_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalSupplyAdoptionRecord &value,
                  Visitor &&visitor) {
  visitor(value.decision_fact_id);
  visitor(value.policy_issue_fact_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalSupplyAdoptionState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.bindings);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalTakeoverApprovalRecord &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.registration_fact_id);
  visitor(value.previous_approval_fact_id);
  visitor(value.authority);
  visitor(value.grant_id);
  visitor(value.tick);
  visitor(value.decision_fact_ids);
  visitor(value.approved_at);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalTakeoverDecisionRecord &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.approval_fact_id);
  visitor(value.decision);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalTakeoverRegistration &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.rule_id);
  visitor(value.occupation_fact_id);
  visitor(value.source_identity);
  visitor(value.tick);
  visitor(value.captured_next_instance_id);
  visitor(value.former_organization_id);
  visitor(value.successor_organization_id);
  visitor(value.public_wallet_ids);
  visitor(value.public_inventory_ids);
  visitor(value.responsibilities);
  visitor(value.registered_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalTakeoverState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.registrations);
  visitor(value.approvals);
  visitor(value.decisions);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalWantedRecord &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.court_id);
  visitor(value.actor_id);
  visitor(value.reason_code);
  visitor(value.registry_fact_id);
  visitor(value.grant_id);
  visitor(value.authority);
  visitor(value.tick);
  visitor(value.occurred_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalWantedRelease &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.warrant_fact_id);
  visitor(value.grant_id);
  visitor(value.authority);
  visitor(value.tick);
  visitor(value.occurred_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticsFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.kind);
  visitor(value.jurisdiction_id);
  visitor(value.appointment_id);
  visitor(value.actor_id);
  visitor(value.issuer_actor_id);
  visitor(value.organization_id);
  visitor(value.office_id);
  visitor(value.decision_actor_id);
  visitor(value.witness_actor_ids);
  visitor(value.tick);
  visitor(value.office_epoch);
  visitor(value.status);
  visitor(value.occurred_at);
  visitor(value.cause_fact_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticsState &value,
                  Visitor &&visitor) {
  visitor(value.jurisdictions);
  visitor(value.appointments);
  visitor(value.facts);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PublicReliefState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.commitments);
  visitor(value.receipts);
  visitor(value.releases);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SecurityConsequence &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.jurisdiction_id);
  visitor(value.program_ref);
  visitor(value.actual_work_result);
  visitor(value.target_line_id);
  visitor(value.previous_fact_id);
  visitor(value.program_spec_ref);
  visitor(value.skill_at_completion);
  visitor(value.before);
  visitor(value.after);
  visitor(value.requested_delta);
  visitor(value.revision);
  visitor(value.applied_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VacantPrunedFacilityPrefix &value,
                  Visitor &&visitor) {
  visitor(value.last_removed_revision);
  visitor(value.cut_at);
  visitor(value.last_close_result_ref);
  visitor(value.last_close_source_schema);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalFiscalEffects &value,
                  Visitor &&visitor) {
  visitor(value.policy_fact_id);
  visitor(value.policy_revision);
  visitor(value.effective_from_tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticsEffects &value,
                  Visitor &&visitor) {
  visitor(value.appointment_id);
  visitor(value.fact_ids);
  visitor(value.receipt_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticsCatalog &value,
                  Visitor &&visitor) {
  visitor(value.enabled);
  visitor(value.jurisdictions);
  visitor(value.appointment_policies);
  visitor(value.command_name_keys);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalWantedSeed &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.reason_code);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalTakeoverRule &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.jurisdiction_id);
  visitor(value.site_id);
  visitor(value.stronghold_id);
  visitor(value.garrison_id);
  visitor(value.successor_office_id);
  visitor(value.occupation_rule);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalTakeoverCatalog &value,
                  Visitor &&visitor) {
  visitor(value.enabled);
  visitor(value.version);
  visitor(value.rules);
  visitor(value.task_execution_version);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalSupplyRenewalIntent &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.takeover_rule_id);
  visitor(value.author_policy_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalSupplyRenewalCatalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.intents);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalShopRegistryIntentCatalog &value,
      Visitor &&visitor) {
  visitor(value.version);
  visitor(value.intents);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalShopRegistryIntent &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.actor_id);
  visitor(value.registry_policy_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalShopLicensingCatalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.courts);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalShopCourt &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.jurisdiction_id);
  visitor(value.office_id);
  visitor(value.role_id);
  visitor(value.dialogue_id);
  visitor(value.request_node_id);
  visitor(value.premises_ids);
  visitor(value.warrant_reason_codes);
  visitor(value.initial_wanted);
  visitor(value.complete_registry);
  visitor(value.permit_ticks);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalFiscalCatalog &value,
                  Visitor &&visitor) {
  visitor(value.enabled);
  visitor(value.version);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalDismissalSourcePolicy &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.jurisdiction_id);
  visitor(value.issuer_office_id);
  visitor(value.grant_id);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalDismissalSourceCatalog &value,
      Visitor &&visitor) {
  visitor(value.version);
  visitor(value.policies);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OccupationFiscalCatalog &value,
                  Visitor &&visitor) {
  visitor(value.enabled);
  visitor(value.version);
}

// native/world/src/resident/domains/politics/dismissal_sources/political_dismissal2.hpp:7;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalDismissalRecord2 &value,
                  Visitor &&visitor) {
  visitor(value.dismissal_source);
  visitor(value.office_change_source);
  visitor(value.authority_basis_ref);
  visitor(value.jurisdiction_revision);
}

// native/world/src/resident/domains/politics/public_market/public_market_authority.hpp:9;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::PublicMarketAuthorityPolicy &value,
                  Visitor &&visitor) {
  visitor(value.policy_ref);
  visitor(value.market_definition_ref);
  visitor(value.opening_rule_ref);
  visitor(value.usage_rule_ref);
  visitor(value.organization_id);
  visitor(value.asset_id);
  visitor(value.original_asset_initialization_ref);
  visitor(value.original_asset_right_ref);
  visitor(value.original_usage_right_ref);
  visitor(value.allowed_opening_intervals);
  visitor(value.allowed_usage_intervals);
}

// native/world/src/resident/domains/politics/public_market/public_market_authority.hpp:16;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::PublicMarketAuthorityRecord &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.policy_ref);
  visitor(value.market_definition_ref);
  visitor(value.revision);
  visitor(value.previous_source_ref);
  visitor(value.decision);
  visitor(value.issuer);
  visitor(value.original_asset_initialization_ref);
  visitor(value.original_asset_right_ref);
  visitor(value.original_usage_right_ref);
  visitor(value.opening_rule_ref);
  visitor(value.usage_rule_ref);
  visitor(value.committed_opening_intervals);
  visitor(value.valid_usage_intervals);
  visitor(value.authority_basis_ref);
  visitor(value.issued_at);
}
