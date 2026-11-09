// 职责：集中政治与条约中的任职、继任和职务交接访问；仅借用原字段，不改变顺序或失败行为。
// 由中央登记头在 source_owned_fields
// 命名空间内包含；依赖与具名助手已先行可见。

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalAppointment &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.jurisdiction_id);
  visitor(value.policy_id);
  visitor(value.actor_id);
  visitor(value.issuer_actor_id);
  visitor(value.issuer_office_id);
  visitor(value.organization_id);
  visitor(value.issuer_epoch);
  visitor(value.target_office_epoch);
  visitor(value.jurisdiction_revision);
  visitor(value.created_tick);
  visitor(value.expires_tick);
  visitor(value.revision);
  visitor(value.status);
  visitor(value.offered_fact_id);
  visitor(value.last_fact_id);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalFieldAppointmentState &value,
      Visitor &&visitor) {
  visitor(value.version);
  visitor(value.authorizations);
  visitor(value.answers);
  visitor(value.withdrawals);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalHeadHandoverAnswer &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.offer_fact_id);
  visitor(value.actor_id);
  visitor(value.office_change_fact_id);
  visitor(value.tick);
  visitor(value.expected_office_epoch);
  visitor(value.installed_office_epoch);
  visitor(value.accepted);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalHeadHandoverOffer &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.policy_id);
  visitor(value.organization_id);
  visitor(value.office_id);
  visitor(value.issuer_actor_id);
  visitor(value.candidate_actor_id);
  visitor(value.grant_id);
  visitor(value.office_epoch);
  visitor(value.issued_tick);
  visitor(value.response_until);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalHeadHandoverState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.offers);
  visitor(value.answers);
  visitor(value.withdrawals);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalHeadHandoverWithdrawal &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.offer_fact_id);
  visitor(value.actor_id);
  visitor(value.grant_id);
  visitor(value.office_epoch);
  visitor(value.tick);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalMilitaryAppointmentState &value,
      Visitor &&visitor) {
  visitor(value.version);
  visitor(value.authorizations);
  visitor(value.answers);
  visitor(value.withdrawals);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalOfficeReceptionAssignment &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.policy_id);
  visitor(value.organization_id);
  visitor(value.office_id);
  visitor(value.role_id);
  visitor(value.actor_id);
  visitor(value.previous_actor_id);
  visitor(value.previous_assignment_fact_id);
  visitor(value.office_change_fact_id);
  visitor(value.grant_id);
  visitor(value.office_epoch);
  visitor(value.tick);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalOfficeReceptionState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.assignments);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalOfficeResignationAnswer &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.request_fact_id);
  visitor(value.actor_id);
  visitor(value.grant_id);
  visitor(value.office_change_fact_id);
  visitor(value.approver_epoch);
  visitor(value.vacant_office_epoch);
  visitor(value.tick);
  visitor(value.accepted);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalOfficeResignationAuthoredOrigin &value,
      Visitor &&visitor) {
  visitor(value.intent_id);
  visitor(value.director_policy_id);
  visitor(value.actor_id);
  visitor(value.request_fact_id);
  visitor(value.reason);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalOfficeResignationRequest &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.policy_id);
  visitor(value.organization_id);
  visitor(value.office_id);
  visitor(value.actor_id);
  visitor(value.approver_office_id);
  visitor(value.approver_actor_id);
  visitor(value.office_epoch);
  visitor(value.approver_epoch);
  visitor(value.jurisdiction_revision);
  visitor(value.tick);
  visitor(value.response_until);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalOfficeResignationState &value,
      Visitor &&visitor) {
  visitor(value.version);
  visitor(value.requests);
  visitor(value.answers);
  visitor(value.withdrawals);
  visitor(value.authored_origins);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalOfficeResignationWithdrawal &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.request_fact_id);
  visitor(value.actor_id);
  visitor(value.tick);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalShopRegistrySuccessionState &value,
      Visitor &&visitor) {
  visitor(value.version);
  visitor(value.acceptances);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalSubordinateAppointmentAnswer &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.offer_fact_id);
  visitor(value.actor_id);
  visitor(value.tick);
  visitor(value.arrival_until);
  visitor(value.accepted);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalSubordinateAppointmentArrival &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.acceptance_fact_id);
  visitor(value.actor_id);
  visitor(value.office_change_fact_id);
  visitor(value.tick);
  visitor(value.installed_epoch);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalSubordinateAppointmentAuthoredOrigin
          &value,
      Visitor &&visitor) {
  visitor(value.intent_id);
  visitor(value.director_policy_id);
  visitor(value.actor_id);
  visitor(value.offer_fact_id);
  visitor(value.stage);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalSubordinateAppointmentOffer &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.policy_id);
  visitor(value.organization_id);
  visitor(value.office_id);
  visitor(value.head_office_id);
  visitor(value.issuer_actor_id);
  visitor(value.candidate_actor_id);
  visitor(value.grant_id);
  visitor(value.head_epoch);
  visitor(value.target_epoch);
  visitor(value.jurisdiction_revision);
  visitor(value.tick);
  visitor(value.response_until);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalSubordinateAppointmentState &value,
      Visitor &&visitor) {
  visitor(value.version);
  visitor(value.offers);
  visitor(value.answers);
  visitor(value.withdrawals);
  visitor(value.arrivals);
  visitor(value.authored_origins);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalSubordinateAppointmentWithdrawal &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.offer_fact_id);
  visitor(value.actor_id);
  visitor(value.grant_id);
  visitor(value.head_epoch);
  visitor(value.tick);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalSuccessionAccession &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.acceptance_fact_id);
  visitor(value.actor_id);
  visitor(value.death_fact_id);
  visitor(value.notice_binding_fact_id);
  visitor(value.office_change_fact_id);
  visitor(value.tick);
  visitor(value.installed_office_epoch);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalSuccessionAccessionState &value,
      Visitor &&visitor) {
  visitor(value.version);
  visitor(value.accessions);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalSuccessionAnswer &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.nomination_fact_id);
  visitor(value.actor_id);
  visitor(value.previous_acceptance_fact_id);
  visitor(value.tick);
  visitor(value.accepted);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalSuccessionNomination &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.policy_id);
  visitor(value.organization_id);
  visitor(value.office_id);
  visitor(value.issuer_actor_id);
  visitor(value.candidate_actor_id);
  visitor(value.grant_id);
  visitor(value.office_epoch);
  visitor(value.issued_tick);
  visitor(value.response_until);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalSuccessionNominationState &value,
      Visitor &&visitor) {
  visitor(value.version);
  visitor(value.nominations);
  visitor(value.answers);
  visitor(value.withdrawals);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalSuccessionPlayerReceipt &value,
      Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.result_id);
  visitor(value.request_sequence);
  visitor(value.result_revision);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalSuccessionState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.nominations);
  visitor(value.accessions);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalSuccessionWithdrawal &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.nomination_fact_id);
  visitor(value.actor_id);
  visitor(value.grant_id);
  visitor(value.office_epoch);
  visitor(value.tick);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalSuccessorRoundAccession &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.acceptance_fact_id);
  visitor(value.actor_id);
  visitor(value.death_fact_id);
  visitor(value.notice_binding_fact_id);
  visitor(value.office_change_fact_id);
  visitor(value.tick);
  visitor(value.installed_office_epoch);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalSuccessorRoundAnswer &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.nomination_fact_id);
  visitor(value.actor_id);
  visitor(value.previous_acceptance_fact_id);
  visitor(value.tick);
  visitor(value.accepted);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalSuccessorRoundNomination &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.policy_id);
  visitor(value.organization_id);
  visitor(value.office_id);
  visitor(value.issuer_actor_id);
  visitor(value.candidate_actor_id);
  visitor(value.grant_id);
  visitor(value.head_source_fact_id);
  visitor(value.head_office_change_fact_id);
  visitor(value.office_epoch);
  visitor(value.issued_tick);
  visitor(value.response_until);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalSuccessorRoundState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.nominations);
  visitor(value.answers);
  visitor(value.withdrawals);
  visitor(value.accessions);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalSuccessorRoundWithdrawal &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.nomination_fact_id);
  visitor(value.actor_id);
  visitor(value.grant_id);
  visitor(value.office_epoch);
  visitor(value.tick);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalTermHandoverAnswer &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.offer_fact_id);
  visitor(value.actor_id);
  visitor(value.office_change_fact_id);
  visitor(value.tick);
  visitor(value.expected_office_epoch);
  visitor(value.installed_office_epoch);
  visitor(value.accepted);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalTermHandoverOffer &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.policy_id);
  visitor(value.organization_id);
  visitor(value.office_id);
  visitor(value.issuer_actor_id);
  visitor(value.candidate_actor_id);
  visitor(value.grant_id);
  visitor(value.head_source_fact_id);
  visitor(value.head_office_change_fact_id);
  visitor(value.office_epoch);
  visitor(value.issued_tick);
  visitor(value.response_until);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalTermHandoverState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.offers);
  visitor(value.answers);
  visitor(value.withdrawals);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalTermHandoverWithdrawal &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.offer_fact_id);
  visitor(value.actor_id);
  visitor(value.grant_id);
  visitor(value.office_epoch);
  visitor(value.tick);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalTermHandoverPolicy &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.successor_round_policy_id);
  visitor(value.handover_grant_id);
  visitor(value.response_ticks);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalTermHandoverCatalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.policies);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalSuccessorRoundPolicy &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.succession_policy_id);
  visitor(value.nomination_grant_id);
  visitor(value.response_ticks);
  visitor(value.accession_ticks);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalSuccessorRoundCatalog &value,
      Visitor &&visitor) {
  visitor(value.version);
  visitor(value.policies);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalSuccessionPolicy &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.organization_id);
  visitor(value.leader_office_id);
  visitor(value.seat_place_id);
  visitor(value.nomination_grant_id);
  visitor(value.response_ticks);
  visitor(value.accession_ticks);
  visitor(value.jurisdiction_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalSuccessionCatalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.policies);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalSubordinateAppointmentPolicy &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.jurisdiction_id);
  visitor(value.head_succession_policy_id);
  visitor(value.appointment_grant_id);
  visitor(value.appointment_policy_id);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalSubordinateAppointmentIntentCatalog
          &value,
      Visitor &&visitor) {
  visitor(value.version);
  visitor(value.intents);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalSubordinateAppointmentIntent &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.appointment_policy_id);
  visitor(value.issuer_actor_id);
  visitor(value.candidate_actor_id);
  visitor(value.issuer_director_policy_id);
  visitor(value.candidate_director_policy_id);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalSubordinateAppointmentCatalog &value,
      Visitor &&visitor) {
  visitor(value.version);
  visitor(value.policies);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalStaffingPost &value,
                  Visitor &&visitor) {
  visitor(value.issuing_office_id);
  visitor(value.jurisdiction_id);
  visitor(value.appointment_policy_id);
  visitor(value.candidate_actor_ids);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalShopRegistrySuccessorPolicy &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.court_id);
  visitor(value.organization_id);
  visitor(value.office_id);
  visitor(value.role_id);
  visitor(value.dialogue_id);
  visitor(value.request_node_id);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalShopRegistrySuccessionCatalog &value,
      Visitor &&visitor) {
  visitor(value.enabled);
  visitor(value.version);
  visitor(value.policies);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalOfficeResignationResponseIntent &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.resignation_policy_id);
  visitor(value.approver_actor_id);
  visitor(value.director_policy_id);
  visitor(value.minimum_service_ticks);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalOfficeResignationResponseCatalog &value,
      Visitor &&visitor) {
  visitor(value.version);
  visitor(value.intents);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalOfficeResignationPolicy &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.jurisdiction_id);
  visitor(value.approval_succession_policy_id);
  visitor(value.approval_grant_id);
  visitor(value.response_ticks);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalOfficeResignationCatalog &value,
      Visitor &&visitor) {
  visitor(value.version);
  visitor(value.policies);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalOfficeReceptionPolicy &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.succession_policy_id);
  visitor(value.role_id);
  visitor(value.reception_grant_id);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalOfficeReceptionCatalog &value,
      Visitor &&visitor) {
  visitor(value.version);
  visitor(value.policies);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalMilitaryAppointmentPolicy &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.organization_id);
  visitor(value.garrison_id);
  visitor(value.site_id);
  visitor(value.response_ticks);
  visitor(value.arrival_ticks);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalMilitaryAppointmentCatalog &value,
      Visitor &&visitor) {
  visitor(value.version);
  visitor(value.enabled);
  visitor(value.policies);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalHeadHandoverPolicy &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.succession_policy_id);
  visitor(value.handover_grant_id);
  visitor(value.response_ticks);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalHeadHandoverCatalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.policies);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalFieldAppointmentPolicy &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.organization_id);
  visitor(value.origin_garrison_id);
  visitor(value.origin_site_id);
  visitor(value.response_ticks);
  visitor(value.arrival_ticks);
  visitor(value.allow_unit);
  visitor(value.allow_army);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PoliticalFieldAppointmentCatalog &value,
      Visitor &&visitor) {
  visitor(value.version);
  visitor(value.enabled);
  visitor(value.policies);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalCareerPolicy &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.response);
  visitor(value.maximum_travel_quote_cost);
  visitor(value.acceptable_organization_ids);
  visitor(value.acceptable_jurisdiction_ids);
  visitor(value.staffing_posts);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::AppointmentPolicy &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.name_key);
  visitor(value.response_ticks);
  visitor(value.arrival_ticks);
  visitor(value.minimum_administration);
  visitor(value.minimum_merit);
}
