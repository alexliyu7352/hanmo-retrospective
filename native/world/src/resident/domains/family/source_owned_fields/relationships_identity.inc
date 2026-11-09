// 职责：集中家庭与接续中的家庭关系与身份访问；仅借用原字段，不改变顺序或失败行为。
// 由中央登记头在 source_owned_fields
// 命名空间内包含；依赖与具名助手已先行可见。

template <class Visitor>
inline void visit(const ::hanmo::resident::AdoptionFact &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.guardian_actor_id);
  visitor(value.start_tick);
  visitor(value.terms);
  visitor(value.context);
  visitor(value.completed_tick);
  visitor(value.child_actor_id);
  visitor(value.success);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::AdoptionTerms &value,
                  Visitor &&visitor) {
  visitor(value.guardian_actor_id);
  visitor(value.provider_actor_id);
  visitor(value.role_id);
  visitor(value.place_id);
  visitor(value.prototype_id);
  visitor(value.display_name);
  visitor(value.birth_tick);
  visitor(value.duration);
  visitor(value.fee);
  visitor(value.payer_wallet_id);
  visitor(value.recipient_wallet_id);
  visitor(value.policy);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::AdultAdoptionAgreement &value,
                  Visitor &&visitor) {
  visitor(value.request_id);
  visitor(value.adopter_actor_id);
  visitor(value.adoptee_actor_id);
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
  visitor(value.adopter_birth_tick);
  visitor(value.adoptee_birth_tick);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::AdultAdoptionFact &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.agreement);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ApprenticeshipCommitBinding &value,
                  Visitor &&visitor) {
  visitor(value.result_id);
  visitor(value.actor_id);
  visitor(value.kind);
  visitor(value.request_seq);
  visitor(value.revision);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ApprenticeshipExit &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.bond_id);
  visitor(value.actor_id);
  visitor(value.tick);
  visitor(value.revision);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ApprenticeshipProposal &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.source);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ApprenticeshipReply &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.proposal_id);
  visitor(value.actor_id);
  visitor(value.decision);
  visitor(value.tick);
  visitor(value.revision);
  visitor(value.confirmation);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ApprenticeshipStatementSource &value,
                  Visitor &&visitor) {
  visitor(value.statement_id);
  visitor(value.speaker_actor_id);
  visitor(value.listener_actor_id);
  visitor(value.teacher_actor_id);
  visitor(value.student_actor_id);
  visitor(value.skill_id);
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
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CharacterCreationDraft &value,
                  Visitor &&visitor) {
  visitor(value.mode);
  visitor(value.display_name);
  visitor(value.age_years);
  visitor(value.attributes);
  visitor(value.skills);
  visitor(value.field_order);
  visitor(value.attributes_order);
  visitor(value.skills_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CharacterCreationFact &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.actor_id);
  visitor(value.prototype_actor_id);
  visitor(value.scenario_id);
  visitor(value.appearance_choice_id);
  visitor(value.created_tick);
  visitor(value.draft);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CollateralCommitBinding &value,
                  Visitor &&visitor) {
  visitor(value.result_id);
  visitor(value.actor_id);
  visitor(value.kind);
  visitor(value.request_seq);
  visitor(value.revision);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CollateralFamilyOrigin &value,
                  Visitor &&visitor) {
  visitor(value.first_actor_id);
  visitor(value.second_actor_id);
  visitor(value.common_ancestor_actor_id);
  visitor(value.first_descent);
  visitor(value.second_descent);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CollateralRecognitionFact &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.source);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CollateralRecognitionSource &value,
                  Visitor &&visitor) {
  visitor(value.request_id);
  visitor(value.first_actor_id);
  visitor(value.second_actor_id);
  visitor(value.scenario_id);
  visitor(value.world_id);
  visitor(value.terms_version);
  visitor(value.tick);
  visitor(value.revision);
  visitor(value.first_birth_tick);
  visitor(value.second_birth_tick);
  visitor(value.ancestry);
  visitor(value.statement);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DeclaredAgeBasisRef &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.actor_id);
  visitor(value.source_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyAction &value,
                  Visitor &&visitor) {
  visitor(value.training);
  visitor(value.context);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyApprenticeshipState &value,
                  Visitor &&visitor) {
  visitor(value.proposals);
  visitor(value.replies);
  visitor(value.exits);
  visitor(value.commits);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyChoice &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.guardian_actor_id);
  visitor(value.heir_actor_id);
  visitor(value.adoption_source);
  visitor(value.place_id);
  visitor(value.tick);
  visitor(value.designation_source);
  visitor(value.assets);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyDecisionAgeSources4 &value,
                  Visitor &&visitor) {
  visitor(value.predecessor);
  visitor(value.successor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyMarriageState &value,
                  Visitor &&visitor) {
  visitor(value.proposals);
  visitor(value.replies);
  visitor(value.dissolutions);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyPolicy &value,
                  Visitor &&visitor) {
  visitor(value.child_age_years);
  visitor(value.guardian_min_age);
  visitor(value.heir_min_age);
  visitor(value.retirement_min_age);
  visitor(value.enable_travel_plan);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyState &value,
                  Visitor &&visitor) {
  visitor(value.founder_actor_id);
  visitor(value.children);
  visitor(value.designations);
  visitor(value.successions);
  visitor(value.facts);
  visitor(value.actions);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::IdentityCreationFact &value,
                  Visitor &&visitor) {
  visitor(value.prototype_id);
  visitor(value.actor_id);
  visitor(value.created_tick);
  visitor(value.birth_tick);
  visitor(value.display_name);
  visitor(value.source);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::InnateParentSource &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.level);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::InnateStudySource &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.skill_id);
  visitor(value.origin_kind);
  visitor(value.origin_fact_id);
  visitor(value.observed_tick);
  visitor(value.bonus_bp);
  visitor(value.known_levels);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MarriageDissolution &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.marriage_id);
  visitor(value.source);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MarriageProposal &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.source);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MarriageReply &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.proposal_id);
  visitor(value.actor_id);
  visitor(value.decision);
  visitor(value.tick);
  visitor(value.revision);
  visitor(value.confirmation);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MarriageStatementSource &value,
                  Visitor &&visitor) {
  visitor(value.statement_id);
  visitor(value.speaker_actor_id);
  visitor(value.listener_actor_id);
  visitor(value.dialogue_id);
  visitor(value.node_id);
  visitor(value.node_kind);
  visitor(value.intent);
  visitor(value.session_id);
  visitor(value.scenario_id);
  visitor(value.world_id);
  visitor(value.context);
  visitor(value.terms_version);
  visitor(value.tick);
  visitor(value.revision);
  visitor(value.speaker_birth_tick);
  visitor(value.listener_birth_tick);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PopulationOriginRef &value,
                  Visitor &&visitor) {
  visitor(value.cohort_id);
  visitor(value.split_receipt_id);
  visitor(value.count);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VoluntaryPathCancellation &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.plan_id);
  visitor(value.actor_id);
  visitor(value.reason);
  visitor(value.tick);
  visitor(value.revision);
  visitor(value.withdrawal);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VoluntaryPathCommitBinding &value,
                  Visitor &&visitor) {
  visitor(value.result_id);
  visitor(value.actor_id);
  visitor(value.kind);
  visitor(value.request_seq);
  visitor(value.revision);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VoluntaryPathSource &value,
                  Visitor &&visitor) {
  visitor(value.relation_kind);
  visitor(value.relation_established_tick);
  visitor(value.disciple_skill_id);
  visitor(value.biological);
  visitor(value.statement);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VoluntaryPathState &value,
                  Visitor &&visitor) {
  visitor(value.consents);
  visitor(value.cancellations);
  visitor(value.completion_revisions);
  visitor(value.commits);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyEffect &value,
                  Visitor &&visitor) {
  visitor(value.success);
  visitor(value.already_completed);
  visitor(value.code);
  visitor(value.actor_id);
  visitor(value.fact_id);
  visitor(value.heir_actor_id);
  visitor(value.player_actor_id);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VoluntaryPathDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.template_id);
  visitor(value.accept_node_id);
  visitor(value.withdraw_node_id);
  visitor(value.relation_kind);
  visitor(value.disciple_skill_id);
  visitor(value.terms_version);
  visitor(value.personal);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VoluntaryPathCatalog &value,
                  Visitor &&visitor) {
  visitor(value.enabled);
  visitor(value.definitions);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MarriageStatementDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.speaker_actor_id);
  visitor(value.listener_actor_id);
  visitor(value.dialogue_id);
  visitor(value.node_id);
  visitor(value.intent);
  visitor(value.terms_version);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MarriageSourceCatalog &value,
                  Visitor &&visitor) {
  visitor(value.enabled);
  visitor(value.origins);
  visitor(value.statements);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MarriageOrigin &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.status);
  visitor(value.spouse_actor_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::InnateFamilyDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.learning_skill_ids);
  visitor(value.learning_bonus_bp);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::IdentityPrototype &value,
                  Visitor &&visitor) {
  visitor(value.attributes);
  visitor(value.skills);
  visitor(value.attributes_order);
  visitor(value.skills_order);
  visitor(value.appearance_kind);
  visitor(value.attire_style);
  visitor(value.fixed_features);
  visitor(value.candidates);
  visitor(value.historical);
  visitor(value.has_travel_plan);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyLineageCatalog &value,
                  Visitor &&visitor) {
  visitor(value.enabled);
  visitor(value.origins);
  visitor(value.original_innate_levels);
  visitor(value.endowment);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyConfig &value,
                  Visitor &&visitor) {
  visitor(value.prototype_id);
  visitor(value.child_name_key);
  visitor(value.wallet_id);
  visitor(value.fee);
  visitor(value.adoption_ticks);
  visitor(value.policy);
  visitor(value.valid);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyCatalog &value,
                  Visitor &&visitor) {
  visitor(value.enabled);
  visitor(value.founder_actor_id);
  visitor(value.roles);
  visitor(value.prototypes);
  visitor(value.allowed_founder_actor_ids);
  visitor(value.founder_selection);
  visitor(value.extended_skills);
  visitor(value.household_control);
  visitor(value.character_creation);
  visitor(value.adult_adoption);
  visitor(value.voluntary_succession);
  visitor(value.voluntary_source_mode);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CharacterCreationGoods &value,
                  Visitor &&visitor) {
  visitor(value.item_id);
  visitor(value.quantity);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CharacterCreationCatalog &value,
                  Visitor &&visitor) {
  visitor(value.prototype_actor_id);
  visitor(value.starting_place_id);
  visitor(value.allowed_modes);
  visitor(value.initial_money);
  visitor(value.initial_goods);
  visitor(value.appearance_choices);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CharacterCreationAppearance &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.features);
  visitor(value.attire_style);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BiologicalOrigin &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.parent_actor_ids);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::ApprenticeshipStatementDefinition &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.speaker_actor_id);
  visitor(value.speaker_side);
  visitor(value.skill_id);
  visitor(value.dialogue_id);
  visitor(value.node_id);
  visitor(value.listener_actor_id);
  visitor(value.terms_version);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ApprenticeshipCatalog &value,
                  Visitor &&visitor) {
  visitor(value.enabled);
  visitor(value.statements);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::AdultAdoptionRequestDefinition &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.adopter_actor_id);
  visitor(value.adoptee_actor_id);
  visitor(value.dialogue_id);
  visitor(value.node_id);
  visitor(value.terms_version);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::AdultAdoptionRequestCatalog &value,
                  Visitor &&visitor) {
  visitor(value.enabled);
  visitor(value.definitions);
}

// native/world/src/resident/domains/family/family_apprenticeship.hpp:38; actual
// owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::ApprenticeshipBond &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.proposal_id);
  visitor(value.teacher_actor_id);
  visitor(value.student_actor_id);
  visitor(value.skill_id);
  visitor(value.accepted_tick);
  visitor(value.exit_fact_id);
}

// native/world/src/resident/domains/family/family_marriage.hpp:21; actual
// owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::MarriageBond &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.first_actor_id);
  visitor(value.second_actor_id);
  visitor(value.authored_origin);
}
