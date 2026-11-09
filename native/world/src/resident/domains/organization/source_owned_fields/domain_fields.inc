// 职责：集中组织与授权中的有限拥有字段访问；仅借用原字段，不改变顺序或失败行为。
// 由中央登记头在 source_owned_fields
// 命名空间内包含；依赖与具名助手已先行可见。

template <class Visitor>
inline void visit(const ::hanmo::resident::ActingAuthority &value,
                  Visitor &&visitor) {
  visitor(value.organization_id);
  visitor(value.office_id);
  visitor(value.authority_epoch);
  visitor(value.actor_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::AuthorityBasisRef &value,
                  Visitor &&visitor) {
  visitor(value.issued_source);
  visitor(value.source_schema);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::AuthorityProof &value,
                  Visitor &&visitor) {
  visitor(value.organization_id);
  visitor(value.office_id);
  visitor(value.actor_id);
  visitor(value.command_kind);
  visitor(value.scope);
  visitor(value.candidate_revision);
  visitor(value.authority_epoch);
  visitor(value.amount);
  visitor(value.resource_versions);
  visitor(value.schema_version);
  visitor(value.source_identity);
  visitor(value.grant_id);
  visitor(value.grant_revision);
  visitor(value.office_source);
  visitor(value.grant_source);
  visitor(value.observed_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ByOfficeTransition &value,
                  Visitor &&visitor) {
  visitor(value.transition_source_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ExecutionBinding &value,
                  Visitor &&visitor) {
  visitor(value.activity_id);
  visitor(value.phase);
  visitor(value.mode);
  visitor(value.execution_epoch);
  visitor(value.local_session_id);
  visitor(value.last_receipt_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::Grant &value, Visitor &&visitor) {
  visitor(value.id);
  visitor(value.office_id);
  visitor(value.allowed_command_kinds);
  visitor(value.scopes);
  visitor(value.wallet_ids);
  visitor(value.inventory_ids);
  visitor(value.amount_limit);
  visitor(value.valid_from);
  visitor(value.valid_until);
  visitor(value.may_delegate);
  visitor(value.continuity_policy_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrantChangePayload &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.grant_id);
  visitor(value.policy_id);
  visitor(value.authority_grant_id);
  visitor(value.authority_grant_revision);
  visitor(value.before_revision);
  visitor(value.after_revision);
  visitor(value.issuer);
  visitor(value.authority_scope);
  visitor(value.target_office_epoch);
  visitor(value.before);
  visitor(value.after);
  visitor(value.effective_moment);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrantRuntimeRecord &value,
                  Visitor &&visitor) {
  visitor(value.terms);
  visitor(value.revision);
  visitor(value.revoked);
  visitor(value.parent_grant_id);
  visitor(value.parent_revision);
  visitor(value.policy_id);
  visitor(value.last_change_fact_id);
  visitor(value.issuer_office_id);
  visitor(value.issuer_office_epoch);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MemberEpisodeSource &value,
                  Visitor &&visitor) {
  visitor(value.offer_id);
  visitor(value.decision_id);
  visitor(value.terms_id);
  visitor(value.joined_fact_id);
  visitor(value.wage_binding_id);
  visitor(value.joined_tick);
  visitor(value.departed_tick);
  visitor(value.departure_fact_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MembershipDecision &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.offer_id);
  visitor(value.target_actor_id);
  visitor(value.offer_revision);
  visitor(value.kind);
  visitor(value.reason);
  visitor(value.source_fact_ids);
  visitor(value.tick);
  visitor(value.decision_seq);
  visitor(value.next_review_tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MembershipOffer &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.organization_id);
  visitor(value.issuer_actor_id);
  visitor(value.office_id);
  visitor(value.grant_id);
  visitor(value.target_actor_id);
  visitor(value.terms_id);
  visitor(value.authority_epoch);
  visitor(value.created_tick);
  visitor(value.expires_tick);
  visitor(value.revision);
  visitor(value.status);
  visitor(value.source_fact_id);
  visitor(value.decision_fact_id);
  visitor(value.member_episode_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OfficeState &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.organization_id);
  visitor(value.holder_actor_id);
  visitor(value.deputy_actor_id);
  visitor(value.epoch);
  visitor(value.status);
  visitor(value.grant_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OrgActivity &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.kind);
  visitor(value.principal);
  visitor(value.responsible_office_id);
  visitor(value.executors);
  visitor(value.commitment_ids);
  visitor(value.started_tick);
  visitor(value.phase_started_tick);
  visitor(value.due_tick);
  visitor(value.outcome_sequence);
  visitor(value.phase);
  visitor(value.terms_id);
  visitor(value.status);
  visitor(value.result_fact_ids);
  visitor(value.participation);
  visitor(value.execution);
  visitor(value.result_code);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OrgDuty &value, Visitor &&visitor) {
  visitor(value.id);
  visitor(value.organization_id);
  visitor(value.actor_id);
  visitor(value.role_id);
  visitor(value.office_id);
  visitor(value.policy_id);
  visitor(value.obligation_ids);
  visitor(value.commitment_ids);
  visitor(value.active);
  visitor(value.activity_terms_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OrgMemberState &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.organization_id);
  visitor(value.actor_id);
  visitor(value.policy_id);
  visitor(value.response);
  visitor(value.response_since_tick);
  visitor(value.next_review_tick);
  visitor(value.last_fact_id);
  visitor(value.duty_ids);
  visitor(value.episode_source);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OrganizationFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.kind);
  visitor(value.organization_id);
  visitor(value.actor_id);
  visitor(value.office_id);
  visitor(value.activity_id);
  visitor(value.tick);
  visitor(value.amount);
  visitor(value.cause_fact_ids);
  visitor(value.response);
  visitor(value.code);
  visitor(value.office_change_payload);
  visitor(value.grant_change_payload);
  visitor(value.occurred_at);
  visitor(value.service_effects);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::OrganizationOfficeChangedPayload &value,
      Visitor &&visitor) {
  visitor(value.schema_version);
  visitor(value.before_epoch);
  visitor(value.after_epoch);
  visitor(value.old_holder);
  visitor(value.new_holder);
  visitor(value.effective_moment);
  visitor(value.cause);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OrganizationRecord &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.kind);
  visitor(value.status);
  visitor(value.administrative_parent);
  visitor(value.branch_parent);
  visitor(value.headquarters_site);
  visitor(value.policy_profile_id);
  visitor(value.office_ids);
  visitor(value.succession_policy_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OrganizationState &value,
                  Visitor &&visitor) {
  visitor(value.current_grants);
  visitor(value.organizations);
  visitor(value.offices);
  visitor(value.duties);
  visitor(value.members);
  visitor(value.activities);
  visitor(value.facts);
  visitor(value.membership_offers);
  visitor(value.membership_decisions);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::Participation &value,
                  Visitor &&visitor) {
  visitor(value.subject);
  visitor(value.role);
  visitor(value.binding_kind);
  visitor(value.binding_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RoleRecord &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.enabled);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VerifiedHolderDeath &value,
                  Visitor &&visitor) {
  visitor(value.death_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MembershipOfferEffects &value,
                  Visitor &&visitor) {
  visitor(value.offer_id);
  visitor(value.decision_id);
  visitor(value.member_episode_id);
  visitor(value.fact_ids);
  visitor(value.receipt_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OrganizationEffects &value,
                  Visitor &&visitor) {
  visitor(value.receipt_ids);
  visitor(value.fact_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RoleDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.initial_actor);
  visitor(value.place_id);
  visitor(value.organization_id);
  visitor(value.dialogue_id);
  visitor(value.services);
  visitor(value.companion_cost_per_tick);
  visitor(value.initial_enabled);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OrganizationCatalog &value,
                  Visitor &&visitor) {
  visitor(value.enabled);
  visitor(value.authority_lifecycle_enabled);
  visitor(value.exact_fact_clock_enabled);
  visitor(value.authority_catalog_identity);
  visitor(value.grant_policies);
  visitor(value.organizations);
  visitor(value.offices);
  visitor(value.grants);
  visitor(value.institutional_issuers);
  visitor(value.member_policies);
  visitor(value.duties);
  visitor(value.members);
  visitor(value.activity_terms);
  visitor(value.initial_activities);
  visitor(value.named_membership_enabled);
  visitor(value.membership_terms);
  visitor(value.named_membership_policies);
  visitor(value.service_effects_enabled);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OrgActivityTerms &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.kind);
  visitor(value.duration_ticks);
  visitor(value.training_capacity);
  visitor(value.training_site_id);
  visitor(value.continuity_policy_id);
  visitor(value.consenting_cohort_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NamedMembershipPolicy &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.allowed_duty_kinds);
  visitor(value.minimum_wage_numerator);
  visitor(value.minimum_wage_denominator);
  visitor(value.minimum_wage_period_ticks);
  visitor(value.review_interval_ticks);
  visitor(value.decline_known_arrears);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MembershipTerms &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.organization_id);
  visitor(value.duty_kind);
  visitor(value.site_id);
  visitor(value.member_policy_id);
  visitor(value.payer_wallet_id);
  visitor(value.wage_rate_profile_id);
  visitor(value.wage_period_ticks);
  visitor(value.offer_duration_ticks);
  visitor(value.continuity_policy_id);
  visitor(value.terminal_fraction_policy_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MemberPolicy &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.request_after_ticks);
  visitor(value.refuse_after_ticks);
  visitor(value.leave_after_ticks);
  visitor(value.minimum_arrears);
  visitor(value.may_leave);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::InstitutionalIssuer &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.organization_id);
  visitor(value.continuity_policy_id);
  visitor(value.obligation_ids);
  visitor(value.source_container_ids);
  visitor(value.amount_limit);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::GrantPolicy &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.issuing_office_id);
  visitor(value.target_office_ids);
  visitor(value.maximum);
  visitor(value.maximum_window_ticks);
}

// native/world/src/resident/domains/organization/office_authority.hpp:15;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::AuthoredGrantAuthoritySource &value,
                  Visitor &&visitor) {
  visitor(value.catalog_identity);
  visitor(value.grant_id);
}

// native/world/src/resident/domains/organization/office_authority.hpp:11;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::AuthoredOfficeAuthoritySource &value,
                  Visitor &&visitor) {
  visitor(value.catalog_identity);
  visitor(value.office_id);
  visitor(value.initial_epoch);
}

// native/world/src/resident/domains/organization/authority_basis.hpp:10; actual
// owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::AuthorityBasis &value,
                  Visitor &&visitor) {
  visitor(value.source_identity);
  visitor(value.actor_id);
  visitor(value.organization_id);
  visitor(value.office_id);
  visitor(value.office_epoch);
  visitor(value.grant_id);
  visitor(value.grant_revision);
  visitor(value.delegation_id);
  visitor(value.parent_grant_ref);
  visitor(value.command_kind);
  visitor(value.scope);
  visitor(value.resources);
  visitor(value.amount);
  visitor(value.amount_limit);
  visitor(value.amount_components);
  visitor(value.budget_ref);
  visitor(value.valid_from);
  visitor(value.valid_until);
  visitor(value.continuity_policy_id);
  visitor(value.issued_tick);
  visitor(value.issued_fact_sequence);
  visitor(value.issued_at);
  visitor(value.issued_source);
  visitor(value.office_source);
  visitor(value.grant_source);
}

// native/world/src/resident/domains/organization/authority_basis.hpp:8; actual
// owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::BudgetAuthorizationRef &value,
                  Visitor &&visitor) {
  visitor(value.authorization_id);
  visitor(value.authorization_revision);
  visitor(value.approval_source);
}

// native/world/src/resident/domains/organization/office_authority.hpp:16;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::GrantRevisionAuthoritySource &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.grant_id);
  visitor(value.revision);
}

// native/world/src/resident/domains/organization/office_authority.hpp:20;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::ItemAuthorityAmount &value,
                  Visitor &&visitor) {
  visitor(value.item_id);
  visitor(value.inventory_ids);
  visitor(value.amount);
}

// native/world/src/resident/domains/organization/organization_lifecycle_contract.hpp:11;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::LifecycleSeedPolicy &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.version);
  visitor(value.organization_kind);
  visitor(value.initial_lifecycle);
  visitor(value.terminal_reason);
  visitor(value.required_office_ids);
  visitor(value.base_right_ref);
  visitor(value.mobile_carrier_refs);
  visitor(value.initial_responsibility_refs);
  // 可选声明本体无动态存储；两个真实拥有字符串各计一次。
  if (value.initial_facility_seed_ref) {
    visitor(value.initial_facility_seed_ref->seed_row_key);
    visitor(value.initial_facility_seed_ref->original_asset_ref);
  }
}

// native/world/src/resident/domains/organization/office_authority.hpp:19;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::MoneyAuthorityAmount &value,
                  Visitor &&visitor) {
  visitor(value.currency_id);
  visitor(value.wallet_ids);
  visitor(value.amount);
}

// native/world/src/resident/domains/organization/office_authority.hpp:12;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::OfficeChangedAuthoritySource &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
}

// native/world/src/resident/domains/organization/organization_activity_policy.hpp:9;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::OrganizationActivityPolicy &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.version);
  visitor(value.organization_kind);
  visitor(value.allowed_template_keys);
  visitor(value.allowed_activity_kinds);
  visitor(value.authority_policy_ref);
  visitor(value.allowed_scope_keys);
  visitor(value.reserve_policy_ref);
  visitor(value.report_policy_ref);
  visitor(value.max_candidates_per_review);
  visitor(value.max_optional_adoptions_per_boundary);
  visitor(value.retry_delay_ticks);
  visitor(value.known_report_freshness_ticks);
}

// native/world/src/resident/domains/organization/organization_activity_policy.hpp:8;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void
visit(const ::hanmo::resident::OrganizationActivityPolicyRuleRef &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.version);
  visitor(value.catalog_digest);
}

// native/world/src/resident/domains/organization/organization_lifecycle_contract.hpp:12;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::OrganizationAuthoredOrigin &value,
                  Visitor &&visitor) {
  visitor(value.catalog_identity);
  visitor(value.organization_row_id);
  visitor(value.lifecycle_seed_policy_ref);
}

// native/world/src/resident/domains/organization/organization_lifecycle_contract.hpp:13;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::OrganizationFoundedOrigin &value,
                  Visitor &&visitor) {
  visitor(value.founding_decision_ref);
  visitor(value.registration_result_ref);
}

// native/world/src/resident/domains/organization/organization_runtime.hpp:6;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::OrganizationGrantVersionRef &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.grant_id);
  visitor(value.revision);
  visitor(value.change_source);
}

// native/world/src/resident/domains/organization/organization_lifecycle_contract.hpp:17;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void
visit(const ::hanmo::resident::OrganizationLifecycleMigrationDecision &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.source_checkpoint_identity);
  visitor(value.original_profile);
  visitor(value.original_organization_id);
  visitor(value.original_status);
  visitor(value.selected_lifecycle);
  visitor(value.selected_terminal_reason);
  visitor(value.evidence_refs);
}

// native/world/src/resident/domains/organization/organization_lifecycle_contract.hpp:16;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::OrganizationLifecycleOrigin &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.organization_id);
  visitor(value.origin);
}

// native/world/src/resident/domains/organization/organization_lifecycle_contract.hpp:21;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::OrganizationLifecycleOutcome &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.old_org_ids);
  visitor(value.new_org_id);
  visitor(value.member_transition_refs);
  visitor(value.control_transition_refs);
  visitor(value.asset_receipts);
  visitor(value.responsibility_dispositions);
  visitor(value.status);
  visitor(value.cause_ref);
}

// native/world/src/resident/domains/organization/organization_lifecycle_contract.hpp:19;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void
visit(const ::hanmo::resident::OrganizationLifecycleTransition &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.organization_id);
  visitor(value.before_revision);
  visitor(value.after_revision);
  visitor(value.before_status);
  visitor(value.after_status);
  visitor(value.cause_kind);
  visitor(value.cause_ref);
  visitor(value.outcome_ref);
  visitor(value.effective_moment);
  visitor(value.terminal_reason);
}

// native/world/src/resident/domains/organization/organization_lifecycle_contract.hpp:18;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::OrganizationMetadata &value,
                  Visitor &&visitor) {
  visitor(value.organization_id);
  visitor(value.revision);
  visitor(value.origin_ref);
  visitor(value.last_lifecycle_transition_ref);
}

// native/world/src/resident/domains/organization/organization_lifecycle_contract.hpp:14;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::OrganizationMigratedOrigin &value,
                  Visitor &&visitor) {
  visitor(value.source_checkpoint_identity);
  visitor(value.original_profile);
  visitor(value.original_organization_id);
  visitor(value.original_status);
  visitor(value.migration_decision_ref);
}

// native/world/src/resident/domains/organization/organization_runtime.hpp:9;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::OrganizationRelationBinding &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.organization_id);
  visitor(value.related_organization_id);
  visitor(value.kind);
  visitor(value.source_ref);
}

// native/world/src/resident/domains/organization/organization_lifecycle_contract.hpp:23;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void
visit(const ::hanmo::resident::OrganizationTerminalDisposition &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.organization_id);
  visitor(value.lifecycle_outcome_ref);
  visitor(value.responsibility_disposition_refs);
}

// native/world/src/resident/domains/organization/authority_basis.hpp:9; actual
// owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::ParentGrantRef &value,
                  Visitor &&visitor) {
  visitor(value.grant_id);
  visitor(value.grant_revision);
  visitor(value.authority_source);
}

// native/world/src/resident/domains/organization/office_lifecycle.hpp:7; actual
// owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::VerifiedOfficeTransition &value,
                  Visitor &&visitor) {
  visitor(value.source);
  visitor(value.organization_id);
  visitor(value.office_id);
  visitor(value.expected_epoch);
  visitor(value.old_holder);
  visitor(value.new_holder);
  visitor(value.kind);
  visitor(value.effective_moment);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OrganizationLifecycleState &value,
                  Visitor &&visitor) {
  visitor(value.organization_metadata);
  visitor(value.lifecycle_origins);
  visitor(value.lifecycle_migration_decisions);
  visitor(value.lifecycle_transitions);
  visitor(value.lifecycle_outcomes);
  visitor(value.terminal_dispositions);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OrganizationRuntime &value,
                  Visitor &&visitor) {
  visitor(static_cast<const ::hanmo::resident::OrganizationLifecycleState &>(
      value));
  visitor(value.office_transition_sources);
  visitor(value.grant_versions);
  visitor(value.authority_bases);
  visitor(value.relations);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OrganizationLifecycleCatalog &value,
                  Visitor &&visitor) {
  visitor(value.identity);
  visitor(value.seed_policies);
  visitor(value.authored_seeds);
}

template <class Visitor>
inline void visit(const OrganizationRoleEnabledChange &value,
                  Visitor &&visitor) {
  organization_service_effects_fields::role_change(
      value, [&](const char *, const auto &member) { visitor(member); });
}

template <class Visitor>
inline void visit(const OrganizationServiceEffect &value, Visitor &&visitor) {
  organization_service_effects_fields::effect(
      value, [&](const char *, const auto &member) { visitor(member); });
}

template <class Visitor>
inline void visit(const OrganizationServiceEffects &value, Visitor &&visitor) {
  organization_service_effects_fields::payload(
      value, [&](const char *, const auto &member) { visitor(member); });
}

template <class Visitor>
inline void visit_payload(const OrganizationServiceEffectBuildView &value,
                          Visitor &&visitor) {
  organization_service_effects_fields::effect(
      value, [&](const auto &, const auto &member) { visitor(member); });
}

template <class Visitor>
inline void visit_payload(const OrganizationServiceEffectsBuildView &value,
                          Visitor &&visitor) {
  organization_service_effects_fields::payload(
      value, [&](const auto &, const auto &member) { visitor(member); });
}
