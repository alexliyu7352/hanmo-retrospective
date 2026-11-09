// 真实供货告示来源的同一拥有字段图；不复制 Report 或知识账。
template <class Visitor>
inline void visit(const ::hanmo::resident::PublicSupplyInspectionCatalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.notice_sources);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::PublicSupplyInspectionState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.results);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::PublicSupplyInspection &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.actor_id);
  visitor(value.prop_id);
  visitor(value.report_id);
  visitor(value.original_observation);
  visitor(value.source_key);
  visitor(value.settlement_id);
  visitor(value.place_id);
  visitor(value.inspected_at);
}

// 职责：集中交互与消息中的有限拥有字段访问；仅借用原字段，不改变顺序或失败行为。
// 由中央登记头在 source_owned_fields
// 命名空间内包含；依赖与具名助手已先行可见。

template <class Visitor>
inline void visit(const ::hanmo::resident::ActorAddress &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ActorKnowledge &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.offers);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ActorSocialOccupancy &value,
                  Visitor &&visitor) {
  visitor(value.interaction_id);
  visitor(value.party_id);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::ActorSocialOccupancyInitialCatalog &value,
      Visitor &&visitor) {
  visitor(value.identity);
  visitor(value.scenario_id);
  visitor(value.authored_initial);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ActorSocialOccupancySources &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.transitions);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::ActorSocialOccupancyTransition &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.actor_id);
  visitor(value.previous_source);
  visitor(value.before);
  visitor(value.after);
  visitor(value.occurred_at);
  visitor(value.cause);
  visitor(value.original_context_id);
  visitor(value.initiator_actor_id);
  visitor(value.actual_cause_source);
  visitor(value.party_acceptance);
  visitor(value.session_opening);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::AuthoredNodeSourceProof &value,
                  Visitor &&visitor) {
  visitor(value.source_identity);
  visitor(value.actor_id);
  visitor(value.provider_actor_id);
  visitor(value.session_id);
  visitor(value.dialogue_id);
  visitor(value.node_id);
  visitor(value.node_kind);
  visitor(value.context);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::AuthoredProviderSourceProof &value,
                  Visitor &&visitor) {
  visitor(value.meeting);
  visitor(value.speaker_actor_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BornPersonalDialogueOpening &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.template_id);
  visitor(value.session_id);
  visitor(value.listener_actor_id);
  visitor(value.provider_actor_id);
  visitor(value.dialogue_id);
  visitor(value.place_id);
  visitor(value.birth_fact_id);
  visitor(value.creation_fact_id);
  visitor(value.prototype_id);
  visitor(value.tick);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::BornPersonalDialoguePlayerReceipt &value,
      Visitor &&visitor) {
  visitor(value.result_id);
  visitor(value.request_actor_id);
  visitor(value.request_sequence);
  visitor(value.result_revision);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BornPersonalDialogueProof &value,
                  Visitor &&visitor) {
  visitor(value.opening_fact_id);
  visitor(value.template_id);
  visitor(value.declaration);
  visitor(value.born);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BornPersonalDialogueState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.openings);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CommunicationDeliverySource &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.business_link_id);
  visitor(value.arrival_ref);
  visitor(value.arrival_schema);
  visitor(value.actual_receiver);
  visitor(value.authority_source);
  visitor(value.office_epoch);
  visitor(value.delivered_at);
  visitor(value.contents_disclosed);
  visitor(value.custody_receiver);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CommunicationDossierAccess &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.business_link_id);
  visitor(value.original_access_ref);
  visitor(value.source_schema);
  visitor(value.reader);
  visitor(value.authority_source);
  visitor(value.accessed_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CommunicationLinksState &value,
                  Visitor &&visitor) {
  visitor(value.links);
  visitor(value.deliveries);
  visitor(value.dossier_accesses);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CompanionTerms &value,
                  Visitor &&visitor) {
  visitor(value.role_id);
  visitor(value.cost_per_tick);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ContactInquiryContact &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.provider_actor_id);
  visitor(value.session_id);
  visitor(value.place_id);
  visitor(value.role_id);
  visitor(value.dialogue_id);
  visitor(value.node_id);
  visitor(value.tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ContactInquiryFinished &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.start_fact_id);
  visitor(value.action_id);
  visitor(value.tick);
  visitor(value.success);
  visitor(value.reason);
  visitor(value.reply_kind);
  visitor(value.answer);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ContactInquiryLifeSource &value,
                  Visitor &&visitor) {
  visitor(value.death_ref);
  visitor(value.start_fact_id);
  visitor(value.role);
  visitor(value.effect);
}

template <class Visitor>
inline void
visit_payload(const ::hanmo::resident::S3PlayerReceiptBuildView &value,
              Visitor &&visitor) {
  visit_s3_player_receipt_payload(value, visitor);
}

template <class Visitor>
inline void
visit_payload(const ::hanmo::resident::ContactInquiryPlayerReceipt &value,
              Visitor &&visitor) {
  visit_s3_player_receipt_payload(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ContactInquiryStarted &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.policy_id);
  visitor(value.action_id);
  visitor(value.contact);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ContactInquiryState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.started);
  visitor(value.finished);
  visitor(value.life_sources);
  visitor(value.temporal_events);
  visitor(value.by_listener);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CustodianActor &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CustodyAddress &value,
                  Visitor &&visitor) {
  visitor(value.receiver_actor_id);
  visitor(value.place_id);
  visitor(value.custodian);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CustodyReceiver &value,
                  Visitor &&visitor) {
  visitor(value.organization_id);
  visitor(value.office_id);
  visitor(value.place_id);
  visitor(value.custodian);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DeliverableProp &value,
                  Visitor &&visitor) {
  visitor(value.prop_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DialogueCommand &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.offer_id);
  visitor(value.shop_id);
  visitor(value.service_id);
  visitor(value.target_actor_id);
  visitor(value.activity);
  visitor(value.skill_id);
  visitor(value.context);
  visitor(value.field_order);
  visitor(value.place_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DialogueOutcome &value,
                  Visitor &&visitor) {
  visitor(value.present);
  visitor(value.ok);
  visitor(value.success);
  visitor(value.code);
  visitor(value.field_order);
  visitor(value.completion);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EncounterRecord &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.leaders);
  visitor(value.participants);
  visitor(value.anchors);
  visitor(value.anchor_field_orders);
  visitor(value.anchors_order);
  visitor(value.action_ids_order);
  visitor(value.resume_paused_order);
  visitor(value.action_ids);
  visitor(value.resume_paused);
  visitor(value.created_tick);
  visitor(value.resolved_tick);
  visitor(value.exact_origin);
  visitor(value.resolved_at);
  visitor(value.status);
  visitor(value.result_id);
  visitor(value.talked);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EventRecord &value,
                  Visitor &&visitor) {
  visitor(value.key);
  visitor(value.event_id);
  visitor(value.actor_id);
  visitor(value.place_id);
  visitor(value.status);
  visitor(value.last_tick);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ExactEncounterOrigin &value,
                  Visitor &&visitor) {
  visitor(value.contact_source);
  visitor(value.created_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FaceToFaceContactResult &value,
                  Visitor &&visitor) {
  visitor(value.session_id);
  visitor(value.session_actor_id);
  visitor(value.provider_actor_id);
  visitor(value.dialogue_id);
  visitor(value.first_contact_node_id);
  visitor(value.first_contact_phase);
  visitor(value.context);
  visitor(value.occurred_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FaceToFaceContactSources &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.results);
  visitor(value.report_disclosures);
  visitor(value.purchase_plan_disclosures);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FaceToFaceReportDisclosure &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.contact_source_id);
  visitor(value.sender_actor_id);
  visitor(value.recipient_actor_id);
  visitor(value.report_id);
  visitor(value.dialogue_node_id);
  visitor(value.dialogue_phase);
  visitor(value.sender_knowledge_source);
  visitor(value.permission);
  visitor(value.disclosed_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FlagRecord &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.value);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::InteractionContext &value,
                  Visitor &&visitor) {
  visitor(value.place_id);
  visitor(value.provider_actor_id);
  visitor(value.role_id);
  visitor(value.kind);
  visitor(value.encounter_id);
  visitor(value.input_error);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::InteractionResponseNode &value,
                  Visitor &&visitor) {
  visitor(value.node_id);
}

template <class Visitor>
inline void
visit_payload(const ::hanmo::resident::InteractionResponseSource &value,
              Visitor &&visitor) {
  visitor(value.response_fact_id);
  visitor(value.session_id);
  visitor(value.session_generation);
  visitor(value.selected_node_id);
  visitor(value.selected_choice_id);
  visitor(value.committed_result_ref);
  visitor(value.next_node_or_terminal);
  visitor(value.tail_consumed_result_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::InteractionResponseSources &value,
                  Visitor &&visitor) {
  visitor(value.physical_version);
  visitor(value.session_boundaries);
  visitor(value.response_sources);
  visitor(value.selected_response_tails);
  visitor(value.life_sources);
  visitor(value.action_confirmation_sources);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::InteractionResponseTerminal &value,
                  Visitor &&visitor) {
  (void)value;
  (void)visitor;
}

template <class Visitor>
inline void visit(const ::hanmo::resident::InteractionSession &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.id);
  visitor(value.dialogue_id);
  visitor(value.node_id);
  visitor(value.phase);
  visitor(value.pending_action_id);
  visitor(value.context);
  visitor(value.pending_command);
  visitor(value.last_outcome);
  visitor(value.trade_completed);
  visitor(value.field_order);
  visitor(value.regional_binding);
  visitor(value.regional_action_committed);
}

template <class Visitor>
inline void
visit_payload(const ::hanmo::resident::InteractionSessionBoundary &value,
              Visitor &&visitor) {
  visit_interaction_session_boundary_payload(value, visitor);
}

template <class Visitor>
inline void visit_payload(
    const ::hanmo::resident::InteractionSessionBoundaryBuildView &value,
    Visitor &&visitor) {
  visit_interaction_session_boundary_payload(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::InteractionSessionOpening &value,
                  Visitor &&visitor) {
  visitor(value.session_id);
  visitor(value.actor_id);
  visitor(value.dialogue_id);
  visitor(value.context);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::InteractionState &value,
                  Visitor &&visitor) {
  visitor(value.response_sources);
  visitor(value.social_occupancy);
  visitor(value.communication_links);
  visitor(value.runtime_offer_knowledge);
  visitor(value.flags);
  visitor(value.knowledge);
  visitor(value.events);
  visitor(value.interactions);
  visitor(value.encounters);
  visitor(value.face_to_face_contacts);
  visitor(value.regional);
  visitor(value.visit_messages);
  visitor(value.visit_message_receipts);
  visitor(value.visit_reception_claims);
  visitor(value.personal_appointments);
  visitor(value.contact_inquiries);
  visitor(value.visit_message_replies);
  visitor(value.visit_appointment_invitations);
  visitor(value.visit_appointment_meetings);
  visitor(value.born_personal_dialogues);
  visitor(value.known_personal_dialogues);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::KnowledgeRecord &value,
                  Visitor &&visitor) {
  visitor(value.offer_id);
  visitor(value.discovered_tick);
  visitor(value.source);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::KnowledgeSource &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.id);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::KnownContactLocation &value,
                  Visitor &&visitor) {
  visitor(value.subject_actor_id);
  visitor(value.place_id);
  visitor(value.source_fact_id);
  visitor(value.source_kind);
  visitor(value.observed_tick);
  visitor(value.learned_tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::KnownPersonalActorOrigin &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.actor_id);
  visitor(value.birth_fact_id);
  visitor(value.creation_fact_id);
  visitor(value.prototype_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::KnownPersonalDialogueOpening &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.template_id);
  visitor(value.session_id);
  visitor(value.dialogue_id);
  visitor(value.place_id);
  visitor(value.listener_origin);
  visitor(value.provider_origin);
  visitor(value.tick);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::KnownPersonalDialoguePlayerReceipt &value,
      Visitor &&visitor) {
  visitor(value.result_id);
  visitor(value.request_actor_id);
  visitor(value.request_sequence);
  visitor(value.result_revision);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::KnownPersonalDialogueProof &value,
                  Visitor &&visitor) {
  visitor(value.opening_fact_id);
  visitor(value.template_id);
  visitor(value.declaration);
  visitor(value.listener_origin);
  visitor(value.provider_origin);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::KnownPersonalDialogueState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.openings);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MessageBusinessLink &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.original_public_message_ref);
  visitor(value.original_business_fact_ref);
  visitor(value.addressed_to);
  visitor(value.route_source_refs);
  visitor(value.dossier_source_refs);
  visitor(value.original_business_source_schema);
  visitor(value.cancellation_source);
  visitor(value.official_content_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OfficialContentRef &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.id);
  visitor(value.terms_revision);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PartyAcceptanceNode &value,
                  Visitor &&visitor) {
  visitor(value.session_actor_id);
  visitor(value.session_id);
  visitor(value.dialogue_id);
  visitor(value.node_id);
  visitor(value.node_kind);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PartyMembershipAcceptance &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.leader_actor_id);
  visitor(value.role_id);
  visitor(value.cost_per_tick);
  visitor(value.context);
  visitor(value.authored_node);
  visitor(value.authored_declaration_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PartyRecord &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.leader_id);
  visitor(value.members);
  visitor(value.terms);
  visitor(value.created_tick);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PersonalAppointmentAccepted &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.policy_id);
  visitor(value.contact);
  visitor(value.earliest_tick);
  visitor(value.latest_tick);
  visitor(value.duration_ticks);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PersonalAppointmentCancelled &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.appointment_id);
  visitor(value.contact);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PersonalAppointmentContact &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.provider_actor_id);
  visitor(value.session_id);
  visitor(value.place_id);
  visitor(value.role_id);
  visitor(value.dialogue_id);
  visitor(value.node_id);
  visitor(value.tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PersonalAppointmentFinished &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.appointment_id);
  visitor(value.start_fact_id);
  visitor(value.action_id);
  visitor(value.tick);
  visitor(value.success);
  visitor(value.reason);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PersonalAppointmentLifeSource &value,
                  Visitor &&visitor) {
  visitor(value.death_ref);
  visitor(value.appointment_id);
  visitor(value.role);
  visitor(value.effect);
}

template <class Visitor>
inline void
visit_payload(const ::hanmo::resident::PersonalAppointmentPlayerReceipt &value,
              Visitor &&visitor) {
  visit_s3_player_receipt_payload(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PersonalAppointmentStarted &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.appointment_id);
  visitor(value.action_id);
  visitor(value.contact);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PersonalAppointmentState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.accepted);
  visitor(value.cancelled);
  visitor(value.started);
  visitor(value.finished);
  visitor(value.life_sources);
  visitor(value.temporal_events);
  visitor(value.by_participant);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RegionalExposure &value,
                  Visitor &&visitor) {
  visitor(value.observer_actor_id);
  visitor(value.binding_id);
  visitor(value.episode);
  visitor(value.last_tick);
  visitor(value.source);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RegionalInteractionState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.exposures);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RegionalOpportunityBinding &value,
                  Visitor &&visitor) {
  visitor(value.binding_id);
  visitor(value.topic_id);
  visitor(value.dialogue_id);
  visitor(value.observer_actor_id);
  visitor(value.provider_actor_id);
  visitor(value.role_id);
  visitor(value.place_id);
  visitor(value.source);
  visitor(value.task_command);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RegionalSourceRef &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.fact_id);
  visitor(value.report_id);
  visitor(value.need_id);
  visitor(value.offer_ref);
  visitor(value.need_revision);
  visitor(value.task_outcome);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RegionalTaskOutcomeRef &value,
                  Visitor &&visitor) {
  visitor(value.task_id);
  visitor(value.contract_id);
  visitor(value.task_template_id);
  visitor(value.delivery_fact_id);
  visitor(value.transfer_fact_id);
  visitor(value.settlement_id);
  visitor(value.receipt_id);
  visitor(value.action_id);
  visitor(value.state_digest);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ReportDisclosurePermission &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.original_source);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RoleAddress &value,
                  Visitor &&visitor) {
  visitor(value.organization_id);
  visitor(value.office_id);
  visitor(value.scope);
  visitor(value.site_id);
  visitor(value.delivery_policy);
  visitor(value.required_service_kind);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RuntimeOfferDiscovery &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.observer);
  visitor(value.offer);
  visitor(value.original_disclosure_ref);
  visitor(value.disclosed_terms_revision);
  visitor(value.learned_at);
  visitor(value.local_place_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RuntimeOfferKnowledgeState &value,
                  Visitor &&visitor) {
  visitor(value.discoveries);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::S3ActionFinishedSource &value,
                  Visitor &&visitor) {
  visit_S3ActionFinishedSource_members(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::S3ActionFinishedSourceBuildView &value,
      Visitor &&visitor) {
  visit_S3ActionFinishedSource_members(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::S3ActionStartedSource &value,
                  Visitor &&visitor) {
  visit_S3ActionStartedSource_members(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::S3ActionStartedSourceBuildView &value,
      Visitor &&visitor) {
  visit_S3ActionStartedSource_members(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::S3ActualTerminationSource &value,
                  Visitor &&visitor) {
  visit_S3ActualTerminationSource_members(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::S3ActualTerminationSourceBuildView &value,
      Visitor &&visitor) {
  visit_S3ActualTerminationSource_members(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::S3AppointmentParticipationStopped &value,
      Visitor &&visitor) {
  visitor(value.start_fact_ref);
  visitor(value.original_action_terminal_ref);
  visitor(value.actual_meeting_end_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::S3CommittedZeroStepSource &value,
                  Visitor &&visitor) {
  visit_S3CommittedZeroStepSource_members(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::S3CommittedZeroStepSourceBuildView &value,
      Visitor &&visitor) {
  visit_S3CommittedZeroStepSource_members(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::S3ContactInquiryRecord &value,
                  Visitor &&visitor) {
  visit_S3ContactInquiryRecord_members(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::S3ContactInquiryRecordBuildView &value,
      Visitor &&visitor) {
  visit_S3ContactInquiryRecord_members(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::S3EventTime &value,
                  Visitor &&visitor) {
  visit_S3EventTime_members(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::S3EventTimeBuildView &value,
                  Visitor &&visitor) {
  visit_S3EventTime_members(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::S3ExactEventMoment &value,
                  Visitor &&visitor) {
  visit_S3ExactEventMoment_members(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::S3ExactEventMomentBuildView &value,
                  Visitor &&visitor) {
  visit_S3ExactEventMoment_members(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::S3InquiryInterrupted &value,
                  Visitor &&visitor) {
  visitor(value.original_action_terminal_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::S3InteractionStepRecord &value,
                  Visitor &&visitor) {
  visit_S3InteractionStepRecord_members(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::S3InteractionStepRecordBuildView &value,
      Visitor &&visitor) {
  visit_S3InteractionStepRecord_members(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::S3LeafLifeEffect &value,
                  Visitor &&visitor) {
  visitor(value.result_id);
  visitor(value.original_terms_ref);
  visitor(value.effect);
  visitor(value.preserved_result_refs);
  visitor(value.effective_game_time);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::S3LegacyEventTick &value,
                  Visitor &&visitor) {
  (void)value;
  (void)visitor;
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::S3MessageResponsibilityContinued &value,
      Visitor &&visitor) {
  visitor(value.disposition);
  visitor(value.actual_channel_result_ref);
  visitor(value.successor_authority_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::S3PersonalAppointmentRecord &value,
                  Visitor &&visitor) {
  visit_S3PersonalAppointmentRecord_members(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::S3PersonalAppointmentRecordBuildView &value,
      Visitor &&visitor) {
  visit_S3PersonalAppointmentRecord_members(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::S3ProfessionalCustodyReturnRecord &value,
      Visitor &&visitor) {
  visit_S3ProfessionalCustodyReturnRecord_members(value, visitor);
}

template <class Visitor>
inline void visit(
    const ::hanmo::resident::S3ProfessionalCustodyReturnRecordBuildView &value,
    Visitor &&visitor) {
  visit_S3ProfessionalCustodyReturnRecord_members(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::S3ProfessionalInstallmentRecord &value,
      Visitor &&visitor) {
  visit_S3ProfessionalInstallmentRecord_members(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::S3ProfessionalInstallmentRecordBuildView &value,
      Visitor &&visitor) {
  visit_S3ProfessionalInstallmentRecord_members(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::S3ProfessionalResultDeliveryRecord &value,
      Visitor &&visitor) {
  visit_S3ProfessionalResultDeliveryRecord_members(value, visitor);
}

template <class Visitor>
inline void visit(
    const ::hanmo::resident::S3ProfessionalResultDeliveryRecordBuildView &value,
    Visitor &&visitor) {
  visit_S3ProfessionalResultDeliveryRecord_members(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::S3ReceivedKnowledgeSource &value,
                  Visitor &&visitor) {
  visit_S3ReceivedKnowledgeSource_members(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::S3ReceivedKnowledgeSourceBuildView &value,
      Visitor &&visitor) {
  visit_S3ReceivedKnowledgeSource_members(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::S3ResidualCopyDeliveryRecord &value,
                  Visitor &&visitor) {
  visit_S3ResidualCopyDeliveryRecord_members(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::S3ResidualCopyDeliveryRecordBuildView &value,
      Visitor &&visitor) {
  visit_S3ResidualCopyDeliveryRecord_members(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::S3SessionStopped &value,
                  Visitor &&visitor) {
  visitor(value.original_action_terminal_ref);
  visitor(value.selected_response_tail_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::S3TaskResponsibilityContinued &value,
                  Visitor &&visitor) {
  visitor(value.disposition);
  visitor(value.original_action_terminal_ref);
  visitor(value.original_task_terminal_ref);
  visitor(value.actual_authority_ref);
  visitor(value.remaining_responsibility_refs);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::S3VisitAppointmentMeetingRecord &value,
      Visitor &&visitor) {
  visit_S3VisitAppointmentMeetingRecord_members(value, visitor);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::S3VisitAppointmentMeetingRecordBuildView &value,
      Visitor &&visitor) {
  visit_S3VisitAppointmentMeetingRecord_members(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SocialPeriodSources &value,
                  Visitor &&visitor) {
  visitor(value.openings);
  visitor(value.closures);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SocialState &value,
                  Visitor &&visitor) {
  visitor(value.last_tick);
  visitor(value.relations);
  visitor(value.memories);
  visitor(value.events);
  visitor(value.field_order);
  visitor(value.dynamic);
  visitor(value.original_relation_version);
  visitor(value.relation_results);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VisitAppointmentAccepted &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.invitation_fact_id);
  visitor(value.collection_fact_id);
  visitor(value.actor_id);
  visitor(value.tick);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::VisitAppointmentAuthoredOrigin &value,
      Visitor &&visitor) {
  visitor(value.intent_id);
  visitor(value.reception_claim_fact_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VisitAppointmentCollected &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.invitation_fact_id);
  visitor(value.contact);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VisitAppointmentLifeSource &value,
                  Visitor &&visitor) {
  visitor(value.death_ref);
  visitor(value.invitation_fact_id);
  visitor(value.role);
  visitor(value.effect);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::VisitAppointmentMeetingCancelled &value,
      Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.invitation_fact_id);
  visitor(value.acceptance_fact_id);
  visitor(value.contact);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::VisitAppointmentMeetingFinished &value,
      Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.invitation_fact_id);
  visitor(value.start_fact_id);
  visitor(value.action_id);
  visitor(value.tick);
  visitor(value.success);
  visitor(value.reason);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::VisitAppointmentMeetingLifeSource &value,
      Visitor &&visitor) {
  visitor(value.death_ref);
  visitor(value.invitation_fact_id);
  visitor(value.acceptance_fact_id);
  visitor(value.role);
  visitor(value.effect);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::VisitAppointmentMeetingStarted &value,
      Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.invitation_fact_id);
  visitor(value.acceptance_fact_id);
  visitor(value.action_id);
  visitor(value.contact);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VisitAppointmentMeetingState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.started);
  visitor(value.cancelled);
  visitor(value.finished);
  visitor(value.life_sources);
  visitor(value.temporal_events);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VisitAppointmentOffered &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.policy_id);
  visitor(value.message_fact_id);
  visitor(value.received_fact_id);
  visitor(value.contact);
  visitor(value.terms);
  visitor(value.player_receipt);
  visitor(value.authored_origin);
}

template <class Visitor>
inline void
visit_payload(const ::hanmo::resident::VisitAppointmentPlayerReceipt &value,
              Visitor &&visitor) {
  visit_s3_player_receipt_payload(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VisitAppointmentState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.offered);
  visitor(value.collected);
  visitor(value.accepted);
  visitor(value.life_sources);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VisitAppointmentTerms &value,
                  Visitor &&visitor) {
  visitor(value.appointment_policy_id);
  visitor(value.inviter_actor_id);
  visitor(value.invitee_actor_id);
  visitor(value.place_id);
  visitor(value.role_id);
  visitor(value.offered_tick);
  visitor(value.accept_before_tick);
  visitor(value.earliest_tick);
  visitor(value.latest_tick);
  visitor(value.duration_ticks);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VisitMessageContact &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.custodian_actor_id);
  visitor(value.place_id);
  visitor(value.role_id);
  visitor(value.session_id);
  visitor(value.dialogue_id);
  visitor(value.node_id);
  visitor(value.tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VisitMessageLifeSource &value,
                  Visitor &&visitor) {
  visitor(value.death_ref);
  visitor(value.message_fact_id);
  visitor(value.role);
  visitor(value.effect);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VisitMessagePosted &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.policy_id);
  visitor(value.sender_actor_id);
  visitor(value.recipient_actor_id);
  visitor(value.contact);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VisitMessageReceiptState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.bindings);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VisitMessageResultBinding &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.producer);
  visitor(value.request_actor_id);
  visitor(value.result_id);
  visitor(value.claim_fact_id);
  visitor(value.intent_id);
  visitor(value.result_kind);
  visitor(value.request_sequence);
  visitor(value.result_revision);
  visitor(value.from_tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VisitMessageState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.posted);
  visitor(value.received);
  visitor(value.withdrawn);
  visitor(value.posted_by_sender);
  visitor(value.received_by_recipient);
  visitor(value.life_sources);
  visitor(value.report_deposits);
  visitor(value.report_pickups);
  visitor(value.report_withdrawals);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VisitMessageTerminal &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.message_fact_id);
  visitor(value.contact);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VisitReceptionClaim &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.intent_id);
  visitor(value.policy_id);
  visitor(value.session_id);
  visitor(value.actor_id);
  visitor(value.provider_actor_id);
  visitor(value.role_id);
  visitor(value.dialogue_id);
  visitor(value.entry_node_id);
  visitor(value.place_id);
  visitor(value.tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VisitReceptionState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.claims);
  visitor(value.by_session);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VisitReplyAuthoredOrigin &value,
                  Visitor &&visitor) {
  visitor(value.intent_id);
  visitor(value.reception_claim_fact_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VisitReplyCollected &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.reply_fact_id);
  visitor(value.contact);
  visitor(value.player_receipt);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VisitReplyLifeSource &value,
                  Visitor &&visitor) {
  visitor(value.death_ref);
  visitor(value.reply_fact_id);
  visitor(value.role);
  visitor(value.effect);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VisitReplyPlayerReceipt &value,
                  Visitor &&visitor) {
  visitor(value.result_id);
  visitor(value.request_actor_id);
  visitor(value.request_sequence);
  visitor(value.result_revision);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VisitReplyPosted &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.policy_id);
  visitor(value.message_fact_id);
  visitor(value.received_fact_id);
  visitor(value.response);
  visitor(value.contact);
  visitor(value.player_receipt);
  visitor(value.authored_origin);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VisitReplyState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.posted);
  visitor(value.collected);
  visitor(value.life_sources);
  visitor(value.posted_by_author);
  visitor(value.collected_by_sender);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VisitReportCustodyDeposit &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.policy_id);
  visitor(value.report_id);
  visitor(value.contact);
  visitor(value.contact_source);
  visitor(value.sender_knowledge_source);
  visitor(value.permission);
  visitor(value.deposited_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VisitReportCustodyTerminal &value,
                  Visitor &&visitor) {
  visitor(value.fact_id);
  visitor(value.deposit_ref);
  visitor(value.contact);
  visitor(value.contact_source);
  visitor(value.occurred_at);
}

template <class Visitor>
inline void
visit_payload(const ::hanmo::resident::InteractionSessionLifeSource &value,
              Visitor &&visitor) {
  visitor(value.death_ref);
  visitor(value.session_id);
  visitor(value.role);
  visitor(value.effect);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ActionCompletion &value,
                  Visitor &&visitor) {
  visitor(value.ok);
  visitor(value.completed);
  visitor(value.success);
  visitor(value.already_completed);
  visitor(value.code);
  visitor(value.military);
  visitor(value.task);
  visitor(value.family);
  visitor(value.growth);
  visitor(value.military_field_order);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::InteractionEffect &value,
                  Visitor &&visitor) {
  visitor(value.encounter_id);
  visitor(value.settlement_result);
  visitor(value.party_id);
  visitor(value.person_id);
  visitor(value.news_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VisitReplyPolicy &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.message_policy_id);
  visitor(value.reply_node_id);
  visitor(value.collect_node_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VisitReplyIntentCatalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.intents);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VisitReplyIntent &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.reception_intent_id);
  visitor(value.reply_policy_id);
  visitor(value.response);
  visitor(value.route);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VisitReplyCatalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.policies);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VisitReceptionStep &value,
                  Visitor &&visitor) {
  visitor(value.node_id);
  visitor(value.option_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VisitReceptionIntent &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.actor_id);
  visitor(value.message_policy_id);
  visitor(value.purpose);
  visitor(value.cooldown_ticks);
  visitor(value.route);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VisitReceptionCatalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.intents);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VisitMessagePolicy &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.sender_actor_id);
  visitor(value.recipient_actor_id);
  visitor(value.custodian_actor_id);
  visitor(value.role_id);
  visitor(value.dialogue_id);
  visitor(value.deposit_node_id);
  visitor(value.receive_node_id);
  visitor(value.withdraw_node_id);
  visitor(value.report_custody_included_in_contact);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VisitMessageCatalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.policies);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VisitAppointmentPolicy &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.message_policy_id);
  visitor(value.appointment_policy_id);
  visitor(value.offer_node_id);
  visitor(value.collect_node_id);
  visitor(value.inviter_commits_fixed_window);
  visitor(value.accept_before_offset);
  visitor(value.earliest_offset);
  visitor(value.latest_offset);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VisitAppointmentIntentCatalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.intents);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VisitAppointmentIntent &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.reception_intent_id);
  visitor(value.invitation_policy_id);
  visitor(value.route);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VisitAppointmentCatalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.policies);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::VisitAppointmentAttendanceCatalog &value,
      Visitor &&visitor) {
  visitor(value.version);
  visitor(value.invitation_policy_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SceneSettlementDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.name_key);
  visitor(value.entry_place);
  visitor(value.point);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SceneRouteDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.duration_ticks);
  visitor(value.points);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ScenePoint &value,
                  Visitor &&visitor) {
  visitor(value.x);
  visitor(value.y);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ScenePlaceDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.settlement_id);
  visitor(value.name_key);
  visitor(value.visual_tag);
  visitor(value.parent_place);
  visitor(value.scene_template_id);
  visitor(value.scene_template_ids);
  visitor(value.template_mapping_valid);
  visitor(value.legacy_template_valid);
  visitor(value.invalid_template_views);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SceneMapDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.scene_template_id);
  visitor(value.template_mapping_valid);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SceneCatalog &value,
                  Visitor &&visitor) {
  visitor(value.places);
  visitor(value.settlements);
  visitor(value.routes);
  visitor(value.maps);
  visitor(value.scenario_maps);
  visitor(value.estate_name_keys);
  visitor(value.estates);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RegionalOutcomeTopic &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.role_id);
  visitor(value.task_template_id);
  visitor(value.dialogue_id);
  visitor(value.cooldown_ticks);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RegionalNeedTopic &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.role_id);
  visitor(value.dialogue_id);
  visitor(value.item_id);
  visitor(value.cooldown_ticks);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RegionalActorTopic &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.actor_id);
  visitor(value.dialogue_id);
  visitor(value.activity);
  visitor(value.cooldown_ticks);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PropDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.place_id);
  visitor(value.name_key);
  visitor(value.kind);
  visitor(value.offer_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PlaceAdmission &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.conditions);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PersonalAppointmentPolicy &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.provider_actor_id);
  visitor(value.role_id);
  visitor(value.dialogue_id);
  visitor(value.accept_node_id);
  visitor(value.cancel_node_id);
  visitor(value.meet_node_id);
  visitor(value.earliest_offset);
  visitor(value.latest_offset);
  visitor(value.duration_ticks);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PersonalAppointmentCatalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.policies);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PersonalAppointmentAttendancePolicy &value,
      Visitor &&visitor) {
  visitor(value.appointment_policy_id);
  visitor(value.depart_before_ticks);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PersonalAppointmentAttendanceCatalog &value,
      Visitor &&visitor) {
  visitor(value.version);
  visitor(value.policies);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PartyRecruitmentDeclaration &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.leader_actor_id);
  visitor(value.companion_actor_id);
  visitor(value.role_id);
  visitor(value.dialogue_id);
  visitor(value.node_id);
  visitor(value.decision);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PartyRecruitmentConsentCatalog &value,
      Visitor &&visitor) {
  visitor(value.version);
  visitor(value.declarations);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PartyInvitationDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.leader_actor_id);
  visitor(value.invitee_actor_id);
  visitor(value.dialogue_id);
  visitor(value.node_id);
  visitor(value.role_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PartyInvitationCatalog &value,
                  Visitor &&visitor) {
  visitor(value.invitations);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OfferDiscoveryDefinition &value,
                  Visitor &&visitor) {
  visitor(value.offer_id);
  visitor(value.sources);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::KnownPersonalDialogueTemplate &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.dialogue_id);
  visitor(value.listener);
  visitor(value.provider);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::KnownPersonalDialogueSelector &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.source_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::KnownPersonalDialogueCatalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.templates);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::InteractionCatalog &value,
                  Visitor &&visitor) {
  visitor(value.response_sources_version);
  visitor(value.admissions);
  visitor(value.road_encounters);
  visitor(value.npc_society);
  visitor(value.flags);
  visitor(value.place_ids);
  visitor(value.settlement_ids);
  visitor(value.map_ids);
  visitor(value.offer_ids);
  visitor(value.common);
  visitor(value.actor_dialogues);
  visitor(value.dialogues);
  visitor(value.events);
  visitor(value.routes);
  visitor(value.props);
  visitor(value.discoveries);
  visitor(value.companions);
  visitor(value.scene);
  visitor(value.regional_enabled);
  visitor(value.regional_actor_topics);
  visitor(value.regional_need_topics);
  visitor(value.regional_outcome_topics);
  visitor(value.party_invitations);
  visitor(value.party_recruitment_consents);
  visitor(value.visit_messages);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EventDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.place_id);
  visitor(value.trigger);
  visitor(value.on_unavailable);
  visitor(value.dialogue_id);
  visitor(value.once_per_actor);
  visitor(value.cooldown_ticks);
  visitor(value.conditions);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EncounterRouteDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.origin);
  visitor(value.destination);
  visitor(value.line);
  visitor(value.duration_ticks);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DialogueOption &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.next);
  visitor(value.text_key);
  visitor(value.conditions);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DialogueNode &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.kind);
  visitor(value.next);
  visitor(value.speaker);
  visitor(value.text_key);
  visitor(value.if_true);
  visitor(value.if_false);
  visitor(value.on_success);
  visitor(value.on_failure);
  visitor(value.condition);
  visitor(value.command);
  visitor(value.options);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DialogueDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.context_role);
  visitor(value.entry);
  visitor(value.nodes);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ContactInquiryPolicy &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.provider_actor_id);
  visitor(value.subject_actor_id);
  visitor(value.role_id);
  visitor(value.dialogue_id);
  visitor(value.node_id);
  visitor(value.disclosure);
  visitor(value.duration_ticks);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ContactInquiryCatalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.policies);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::CompanionRoleDefinition &value,
                  Visitor &&visitor) {
  visitor(value.role_id);
  visitor(value.required_trust);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BornPersonalDialogueTemplate &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.prototype_id);
  visitor(value.dialogue_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BornPersonalDialogueCatalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.templates);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ActorDialogueDefinition &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.dialogue_id);
  visitor(value.road_dialogue_id);
}

template <class Visitor>
inline void visit_payload(
    const ::hanmo::resident::InteractionActionConfirmationSource &value,
    Visitor &&visitor) {
  visit_interaction_action_confirmation_payload(value, visitor);
}

template <class Visitor>
inline void visit_payload(
    const ::hanmo::resident::InteractionActionConfirmationBuildView &value,
    Visitor &&visitor) {
  visit_interaction_action_confirmation_payload(value, visitor);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::HouseholdPurchaseReportDisclosure &v,
                  Visitor &&visitor) {
  visitor(v.id);
  visitor(v.controller_actor_id);
  visitor(v.cohort_id);
  visitor(v.report_id);
  visitor(v.purchase_plan_ref);
  visitor(v.accepted_policy_ref);
  visitor(v.population_selection_ref);
  visitor(v.accepted_policy_revision);
  visitor(v.controller_authority);
  visitor(v.controller_knowledge_source);
  visitor(v.original_observation);
  visitor(v.controller_learned_at);
  visitor(v.disclosed_at);
  visitor(v.permission);
  visitor(v.actual_population);
  visitor(v.population_location_source);
  visitor(v.actual_contact_prefix);
}
