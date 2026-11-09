# 交互与对白：显式保存本领域生产文件与身份清单，顺序由登记入口组合。
# 连续段不合并重排，确保原目标和摘要的路径顺序保持不变。

# 生产输入连续登记段：src/resident/domains/interaction/interaction_study_life.cpp。
set(HANMO_RESIDENT_SOURCES_interaction_interaction_study_life_cpp
    src/resident/domains/interaction/interaction_study_life.cpp
)

# 生产输入连续登记段：src/resident/domains/interaction/appointments/personal_appointment_catalog.cpp。
set(HANMO_RESIDENT_SOURCES_interaction_personal_appointment_catalog_cpp
    src/resident/domains/interaction/appointments/personal_appointment_catalog.cpp
    src/resident/domains/interaction/appointments/personal_appointment_validation.cpp
    src/resident/domains/interaction/appointments/personal_appointment_rules.cpp
    src/resident/domains/interaction/appointments/personal_appointment_codec.cpp
    src/resident/domains/interaction/appointments/personal_appointment_profile.cpp
    src/resident/domains/interaction/appointments/personal_appointment_view.cpp
    src/resident/domains/interaction/appointments/personal_appointment_boundary.cpp
    src/resident/domains/interaction/appointments/personal_appointment_receipts.cpp
    src/resident/domains/interaction/s3_player_receipt_memory.cpp
    src/resident/domains/interaction/appointments/personal_appointment_progress.cpp
    src/resident/domains/interaction/appointments/personal_appointment_presence.cpp
    src/resident/domains/interaction/known_contacts/known_contact_location.cpp
    src/resident/domains/interaction/known_contacts/known_contact_location_view.cpp
    src/resident/domains/interaction/known_contacts/known_contact_location_world.cpp
    src/resident/domains/interaction/appointments/personal_appointment_attendance.cpp
    src/resident/domains/interaction/appointments/personal_appointment_attendance_catalog.cpp
    src/resident/domains/interaction/appointments/personal_appointment_attendance_world.cpp
    src/resident/domains/interaction/appointments/personal_appointment_payloads.cpp
    src/resident/domains/interaction/appointments/personal_appointment_world.cpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry_catalog.cpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry_codec.cpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry_rules.cpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry_validation.cpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry_receipts.cpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry_progress.cpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry_boundary.cpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry_view.cpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry_payloads.cpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry_world.cpp
    src/resident/domains/interaction/visit_replies/visit_reply_catalog.cpp
    src/resident/domains/interaction/visit_replies/visit_reply_codec.cpp
    src/resident/domains/interaction/visit_replies/visit_reply_rules.cpp
    src/resident/domains/interaction/visit_replies/visit_reply_validation.cpp
    src/resident/domains/interaction/visit_replies/visit_reply_receipts.cpp
    src/resident/domains/interaction/visit_replies/visit_reply_read.cpp
    src/resident/domains/interaction/visit_replies/visit_reply_view.cpp
    src/resident/domains/interaction/visit_replies/visit_reply_boundary.cpp
    src/resident/domains/interaction/visit_replies/visit_reply_world.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_access.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_authored.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_boundary.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_catalog.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_codec.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_commitments.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_control.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_director.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_intents.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_meeting_codec.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_meeting_progress.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_meeting_receipts.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_meeting_rules.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_meeting_sources.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_meeting_validation.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_receipts.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_rules.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_validation.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_view.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_world.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_attendance_world.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_attendance_catalog.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_attendance.cpp
    src/resident/domains/interaction/born_dialogues/born_personal_dialogue_catalog.cpp
    src/resident/domains/interaction/born_dialogues/born_personal_dialogue_codec.cpp
    src/resident/domains/interaction/born_dialogues/born_personal_dialogue_proof_codec.cpp
    src/resident/domains/interaction/born_dialogues/born_personal_dialogue_rules.cpp
    src/resident/domains/interaction/born_dialogues/born_personal_dialogue_validation.cpp
    src/resident/domains/interaction/born_dialogues/born_personal_dialogue_access.cpp
    src/resident/domains/interaction/born_dialogues/born_personal_dialogue_control.cpp
    src/resident/domains/interaction/born_dialogues/born_personal_dialogue_receipts.cpp
    src/resident/domains/interaction/born_dialogues/born_personal_dialogue_boundary.cpp
    src/resident/domains/interaction/born_dialogues/born_personal_dialogue_view.cpp
    src/resident/domains/interaction/born_dialogues/born_personal_dialogue_world.cpp
    src/resident/domains/interaction/personal_dialogues/known_personal_actor_source.cpp
    src/resident/domains/interaction/personal_dialogues/native_personal_dialogue_sources4.cpp
    src/resident/domains/interaction/family_communication/family_communication_read.cpp
    src/resident/domains/interaction/personal_dialogues/known_personal_dialogue_catalog.cpp
    src/resident/domains/interaction/personal_dialogues/known_personal_dialogue_codec.cpp
    src/resident/domains/interaction/personal_dialogues/known_personal_dialogue_proof_codec.cpp
    src/resident/domains/interaction/personal_dialogues/known_personal_dialogue_rules.cpp
    src/resident/domains/interaction/personal_dialogues/known_personal_dialogue_validation.cpp
    src/resident/domains/interaction/personal_dialogues/known_personal_dialogue_access.cpp
    src/resident/domains/interaction/personal_dialogues/known_personal_dialogue_control.cpp
    src/resident/domains/interaction/personal_dialogues/known_personal_dialogue_receipts.cpp
    src/resident/domains/interaction/personal_dialogues/known_personal_dialogue_boundary.cpp
    src/resident/domains/interaction/personal_dialogues/known_personal_dialogue_view.cpp
    src/resident/domains/interaction/personal_dialogues/known_personal_dialogue_world.cpp
)

# 生产输入连续登记段：src/resident/domains/interaction/visit_replies/visit_reply_intents.cpp。
set(HANMO_RESIDENT_SOURCES_interaction_visit_reply_intents_cpp
    src/resident/domains/interaction/visit_replies/visit_reply_intents.cpp
    src/resident/domains/interaction/visit_replies/visit_reply_authored.cpp
    src/resident/domains/interaction/visit_replies/visit_reply_director.cpp
    src/resident/domains/interaction/visit_replies/visit_reply_control.cpp
)

# 生产输入连续登记段：src/resident/domains/interaction/dialogue_provider_source.cpp。
set(HANMO_RESIDENT_SOURCES_interaction_dialogue_provider_source_cpp
    src/resident/domains/interaction/dialogue_provider_source.cpp
    src/resident/domains/interaction/party_consent/party_recruitment_consent.cpp
    src/resident/domains/interaction/party_consent/party_recruitment_consent_codec.cpp
    src/resident/domains/interaction/party_consent/party_recruitment_consent_profile.cpp
    src/resident/domains/interaction/party_consent/party_recruitment_consent_view.cpp
)

# 生产输入连续登记段：src/resident/domains/interaction/dialogue_node_source.cpp。
set(HANMO_RESIDENT_SOURCES_interaction_dialogue_node_source_cpp
    src/resident/domains/interaction/dialogue_node_source.cpp
    src/resident/domains/interaction/dialogue_node_source.hpp
    src/resident/domains/interaction/face_to_face_contact.cpp
    src/resident/domains/interaction/face_to_face_contact.hpp
)

# 生产输入连续登记段：src/resident/domains/interaction/interaction_actions.cpp。
set(HANMO_RESIDENT_SOURCES_interaction_interaction_actions_cpp
    src/resident/domains/interaction/interaction_actions.cpp
    src/resident/domains/interaction/party_invitation.cpp
    src/resident/domains/interaction/party_invitation_codec.cpp
    src/resident/domains/interaction/party_invitation_commands.cpp
    src/resident/domains/interaction/party_invitation_profile.cpp
    src/resident/domains/interaction/party_invitation_recruitment.cpp
    src/resident/domains/interaction/party_invitation_view.cpp
    src/resident/domains/interaction/interaction_codec.cpp
    src/resident/domains/interaction/interaction_response_sources.cpp
    src/resident/domains/interaction/interaction_response_memory.cpp
    src/resident/domains/interaction/interaction_action_confirmation_memory.cpp
    src/resident/domains/interaction/interaction_growth_life.cpp
)

# 生产输入连续登记段：src/resident/domains/interaction/interaction_response_codec.cpp。
set(HANMO_RESIDENT_SOURCES_interaction_interaction_response_codec_cpp
    src/resident/domains/interaction/interaction_response_codec.cpp
    src/resident/domains/interaction/interaction_session_life.cpp
    src/resident/domains/interaction/interaction_session_life_memory.cpp
    src/resident/domains/interaction/interaction_phases.cpp
    src/resident/domains/interaction/interaction_rules.cpp
    src/resident/domains/interaction/interaction_role_history.cpp
    src/resident/domains/interaction/visit_messages/visit_message_catalog.cpp
    src/resident/domains/interaction/visit_messages/visit_message_codec.cpp
    src/resident/domains/interaction/visit_messages/visit_message_rules.cpp
    src/resident/domains/interaction/visit_messages/visit_message_validation.cpp
    src/resident/domains/interaction/visit_messages/visit_message_view.cpp
    src/resident/domains/interaction/visit_messages/visit_message_world.cpp
    src/resident/domains/interaction/visit_messages/visit_message_receipt_codec.cpp
    src/resident/domains/interaction/visit_messages/visit_message_receipt_control.cpp
    src/resident/domains/interaction/visit_messages/visit_message_receipt_profile.cpp
    src/resident/domains/interaction/visit_messages/visit_message_receipt_world.cpp
    src/resident/domains/interaction/visit_messages/visit_message_receipts.cpp
    src/resident/domains/interaction/visit_messages/visit_reception_catalog.cpp
    src/resident/domains/interaction/visit_messages/visit_reception_director.cpp
    src/resident/domains/interaction/visit_messages/visit_reception_world.cpp
    src/resident/domains/interaction/visit_messages/visit_reception_claims.cpp
    src/resident/domains/interaction/visit_messages/visit_reception_codec.cpp
    src/resident/domains/interaction/regional_life_dialogue.cpp
    src/resident/domains/interaction/regional_outcome_dialogue.cpp
    src/resident/domains/interaction/outcome_catalog.cpp
    src/resident/domains/interaction/regional_life_codec.cpp
    src/resident/domains/interaction/interaction_validation.cpp
)

# 生产输入连续登记段：src/resident/domains/interaction/communication_delivery_read.cpp。
set(HANMO_RESIDENT_SOURCES_interaction_communication_delivery_read_cpp
    src/resident/domains/interaction/communication_delivery_read.cpp
    src/resident/domains/interaction/communication_links.cpp
    src/resident/domains/interaction/communication_links_codec.cpp
    src/resident/domains/interaction/face_to_face_contact_source.cpp
    src/resident/domains/interaction/face_to_face_contact_source_codec.cpp
    src/resident/domains/interaction/face_to_face_propagation.cpp
    src/resident/domains/interaction/face_to_face_report.cpp
    src/resident/domains/interaction/interaction_exact_encounter.cpp
    src/resident/domains/interaction/interaction_exact_encounter_codec.cpp
    src/resident/domains/interaction/report_disclosure.cpp
    src/resident/domains/interaction/runtime_offer_knowledge.cpp
    src/resident/domains/interaction/s3_commitment_read.cpp
)

# 生产输入连续登记段：src/resident/domains/interaction/interaction_task_life.cpp。
set(HANMO_RESIDENT_SOURCES_interaction_interaction_task_life_cpp
    src/resident/domains/interaction/interaction_task_life.cpp
)

# 生产输入连续登记段：src/resident/domains/interaction/visit_messages/visit_report_custody.cpp。
set(HANMO_RESIDENT_SOURCES_interaction_visit_report_custody_cpp
    src/resident/domains/interaction/visit_messages/visit_report_custody.cpp
)

# 生产输入连续登记段：src/resident/domains/interaction/occupancy/party_acceptance_history.cpp。
set(HANMO_RESIDENT_SOURCES_interaction_party_acceptance_history_cpp
    src/resident/domains/interaction/occupancy/party_acceptance_history.cpp
)

# 生产输入连续登记段：src/resident/domains/interaction/occupancy/actor_social_occupancy.cpp。
set(HANMO_RESIDENT_SOURCES_interaction_actor_social_occupancy_cpp
    src/resident/domains/interaction/occupancy/actor_social_occupancy.cpp
    src/resident/domains/interaction/occupancy/actor_social_occupancy_initial.cpp
    src/resident/domains/interaction/occupancy/actor_social_occupancy_codec.cpp
)

# 生产输入连续登记段：src/resident/domains/interaction/local_communication_sources.cpp。
set(HANMO_RESIDENT_SOURCES_interaction_local_communication_sources_cpp
    src/resident/domains/interaction/local_communication_sources.cpp
    src/resident/domains/interaction/local_propagation_channels.cpp
    src/resident/domains/interaction/official_content_knowledge.cpp
    src/resident/domains/interaction/report_official_content.cpp
    src/resident/domains/interaction/visit_messages/visit_report_propagation.cpp
)

# 生产输入连续登记段：src/resident/domains/interaction/appointments/personal_appointment_exact_phase.cpp。
set(HANMO_RESIDENT_SOURCES_interaction_personal_appointment_exact_phase_cpp
    src/resident/domains/interaction/appointments/personal_appointment_exact_phase.cpp
    src/resident/domains/interaction/appointments/personal_appointment_life.cpp
    src/resident/domains/interaction/appointments/personal_appointment_native.cpp
    src/resident/domains/interaction/appointments/personal_appointment_time2.cpp
    src/resident/domains/interaction/appointments/personal_appointment_time2_validation.cpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry_exact_phase.cpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry_life.cpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry_native.cpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry_time2.cpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry_time2_validation.cpp
    src/resident/domains/interaction/interaction_exact_phase.cpp
    src/resident/domains/interaction/s3_event_time.cpp
    src/resident/domains/interaction/s3_leaf_life.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_exact_phase.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_life.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_meeting_life.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_meeting_time2.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_meeting_time2_validation.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_native.cpp
    src/resident/domains/interaction/visit_messages/visit_message_life.cpp
)

# 生产输入连续登记段：src/resident/domains/interaction/visit_messages/visit_message_receipt_native.cpp。
set(HANMO_RESIDENT_SOURCES_interaction_visit_message_receipt_native_cpp
    src/resident/domains/interaction/visit_messages/visit_message_receipt_native.cpp
    src/resident/domains/interaction/visit_replies/visit_reply_life.cpp
    src/resident/domains/interaction/visit_replies/visit_reply_native.cpp
    src/resident/domains/interaction/visit_messages/visit_authored_command.cpp
    src/resident/domains/interaction/visit_messages/visit_message_native.cpp
    src/resident/domains/interaction/visit_messages/visit_original_result_read.cpp
)

# 生产身份输入连续登记段：src/resident/domains/interaction/interaction_study_life.hpp。
set(HANMO_RESIDENT_IDENTITY_interaction_interaction_study_life_hpp
    src/resident/domains/interaction/interaction_study_life.hpp
    src/resident/domains/interaction/interaction_study_life.cpp
)

# 生产身份输入连续登记段：src/resident/domains/interaction/visit_appointments/visit_appointment_attendance_layout.hpp。
set(HANMO_RESIDENT_IDENTITY_interaction_visit_appointment_attendance_layout_hpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_attendance_layout.hpp
)

# 生产身份输入连续登记段：src/resident/domains/interaction/appointments/personal_appointment_catalog.cpp。
set(HANMO_RESIDENT_IDENTITY_interaction_personal_appointment_catalog_cpp
    src/resident/domains/interaction/appointments/personal_appointment_catalog.cpp
    src/resident/domains/interaction/appointments/personal_appointment_validation.cpp
    src/resident/domains/interaction/appointments/personal_appointment_rules.cpp
    src/resident/domains/interaction/appointments/personal_appointment_codec.cpp
    src/resident/domains/interaction/appointments/personal_appointment_profile.cpp
    src/resident/domains/interaction/appointments/personal_appointment_view.cpp
    src/resident/domains/interaction/appointments/personal_appointment_boundary.cpp
    src/resident/domains/interaction/appointments/personal_appointment_receipts.cpp
    src/resident/domains/interaction/s3_player_receipt_memory.cpp
    src/resident/domains/interaction/appointments/personal_appointment_progress.cpp
    src/resident/domains/interaction/appointments/personal_appointment_presence.cpp
    src/resident/domains/interaction/known_contacts/known_contact_location.cpp
    src/resident/domains/interaction/known_contacts/known_contact_location_view.cpp
    src/resident/domains/interaction/known_contacts/known_contact_location_world.cpp
    src/resident/domains/interaction/appointments/personal_appointment_attendance.cpp
    src/resident/domains/interaction/appointments/personal_appointment_attendance_catalog.cpp
    src/resident/domains/interaction/appointments/personal_appointment_attendance_world.cpp
    src/resident/domains/interaction/appointments/personal_appointment_payloads.cpp
    src/resident/domains/interaction/appointments/personal_appointment_world.cpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry_catalog.cpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry_codec.cpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry_rules.cpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry_validation.cpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry_receipts.cpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry_progress.cpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry_boundary.cpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry_view.cpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry_payloads.cpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry_world.cpp
    src/resident/domains/interaction/visit_replies/visit_reply_catalog.cpp
    src/resident/domains/interaction/visit_replies/visit_reply_codec.cpp
    src/resident/domains/interaction/visit_replies/visit_reply_rules.cpp
    src/resident/domains/interaction/visit_replies/visit_reply_validation.cpp
    src/resident/domains/interaction/visit_replies/visit_reply_receipts.cpp
    src/resident/domains/interaction/visit_replies/visit_reply_read.cpp
    src/resident/domains/interaction/visit_replies/visit_reply_view.cpp
    src/resident/domains/interaction/visit_replies/visit_reply_boundary.cpp
    src/resident/domains/interaction/visit_replies/visit_reply_world.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_access.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_authored.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_boundary.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_catalog.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_codec.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_commitments.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_control.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_director.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_intents.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_meeting_codec.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_meeting_progress.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_meeting_receipts.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_meeting_rules.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_meeting_sources.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_meeting_validation.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_receipts.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_rules.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_validation.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_view.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_world.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_attendance_world.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_attendance_catalog.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_attendance.cpp
    src/resident/domains/interaction/born_dialogues/born_personal_dialogue_catalog.cpp
    src/resident/domains/interaction/born_dialogues/born_personal_dialogue_codec.cpp
    src/resident/domains/interaction/born_dialogues/born_personal_dialogue_proof_codec.cpp
    src/resident/domains/interaction/born_dialogues/born_personal_dialogue_rules.cpp
    src/resident/domains/interaction/born_dialogues/born_personal_dialogue_validation.cpp
    src/resident/domains/interaction/born_dialogues/born_personal_dialogue_access.cpp
    src/resident/domains/interaction/born_dialogues/born_personal_dialogue_control.cpp
    src/resident/domains/interaction/born_dialogues/born_personal_dialogue_receipts.cpp
    src/resident/domains/interaction/born_dialogues/born_personal_dialogue_boundary.cpp
    src/resident/domains/interaction/born_dialogues/born_personal_dialogue_view.cpp
    src/resident/domains/interaction/born_dialogues/born_personal_dialogue_world.cpp
    src/resident/domains/interaction/personal_dialogues/known_personal_actor_source.cpp
    src/resident/domains/interaction/personal_dialogues/native_personal_dialogue_sources4.cpp
    src/resident/domains/interaction/personal_dialogues/native_personal_dialogue_sources4.hpp
    src/resident/domains/interaction/family_communication/family_communication_read.cpp
    src/resident/domains/interaction/family_communication/family_communication_read.hpp
    src/resident/domains/interaction/family_communication/family_communication_model.hpp
    src/resident/domains/interaction/personal_dialogues/known_personal_dialogue_catalog.cpp
    src/resident/domains/interaction/personal_dialogues/known_personal_dialogue_codec.cpp
    src/resident/domains/interaction/personal_dialogues/known_personal_dialogue_proof_codec.cpp
    src/resident/domains/interaction/personal_dialogues/known_personal_dialogue_rules.cpp
    src/resident/domains/interaction/personal_dialogues/known_personal_dialogue_validation.cpp
    src/resident/domains/interaction/personal_dialogues/known_personal_dialogue_access.cpp
    src/resident/domains/interaction/personal_dialogues/known_personal_dialogue_control.cpp
    src/resident/domains/interaction/personal_dialogues/known_personal_dialogue_receipts.cpp
    src/resident/domains/interaction/personal_dialogues/known_personal_dialogue_boundary.cpp
    src/resident/domains/interaction/personal_dialogues/known_personal_dialogue_view.cpp
    src/resident/domains/interaction/personal_dialogues/known_personal_dialogue_world.cpp
    src/resident/domains/interaction/known_contacts/family_known_contact.hpp
)

# 生产身份输入连续登记段：src/resident/domains/interaction/visit_replies/visit_reply_intents.cpp。
set(HANMO_RESIDENT_IDENTITY_interaction_visit_reply_intents_cpp
    src/resident/domains/interaction/visit_replies/visit_reply_intents.cpp
    src/resident/domains/interaction/visit_replies/visit_reply_authored.cpp
    src/resident/domains/interaction/visit_replies/visit_reply_director.cpp
    src/resident/domains/interaction/visit_replies/visit_reply_control.cpp
)

# 生产身份输入连续登记段：src/resident/domains/interaction/appointments/personal_appointment.hpp。
set(HANMO_RESIDENT_IDENTITY_interaction_personal_appointment_hpp
    src/resident/domains/interaction/appointments/personal_appointment.hpp
    src/resident/domains/interaction/appointments/personal_appointment_state.hpp
    src/resident/domains/interaction/appointments/personal_appointment_detail.hpp
    src/resident/domains/interaction/appointments/personal_appointment_view.hpp
    src/resident/domains/interaction/appointments/personal_appointment_boundary.hpp
    src/resident/domains/interaction/appointments/personal_appointment_progress.hpp
    src/resident/domains/interaction/appointments/personal_appointment_presence.hpp
    src/resident/domains/interaction/known_contacts/known_contact_location.hpp
    src/resident/domains/interaction/known_contacts/known_contact_location_view.hpp
    src/resident/domains/interaction/known_contacts/known_contact_location_world.hpp
    src/resident/domains/interaction/appointments/personal_appointment_attendance.hpp
    src/resident/domains/interaction/appointments/personal_appointment_attendance_world.hpp
    src/resident/domains/interaction/appointments/personal_appointment_world.hpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry.hpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry_state.hpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry_detail.hpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry_receipts.hpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry_progress.hpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry_boundary.hpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry_view.hpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry_world.hpp
    src/resident/domains/interaction/visit_replies/visit_reply.hpp
    src/resident/domains/interaction/visit_replies/visit_reply_state.hpp
    src/resident/domains/interaction/visit_replies/visit_reply_detail.hpp
    src/resident/domains/interaction/visit_replies/visit_reply_receipts.hpp
    src/resident/domains/interaction/visit_replies/visit_reply_read.hpp
    src/resident/domains/interaction/visit_replies/visit_reply_view.hpp
    src/resident/domains/interaction/visit_replies/visit_reply_boundary.hpp
    src/resident/domains/interaction/visit_replies/visit_reply_world.hpp
    src/resident/domains/interaction/visit_appointments/visit_appointment.hpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_access.hpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_authored.hpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_boundary.hpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_catalog.hpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_commitments.hpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_contract.hpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_control.hpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_detail.hpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_director.hpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_intents.hpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_meeting.hpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_meeting_progress.hpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_meeting_receipts.hpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_meeting_sources.hpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_meeting_state.hpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_receipts.hpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_state.hpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_view.hpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_world.hpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_attendance.hpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_attendance_world.hpp
    src/resident/domains/interaction/born_dialogues/born_personal_dialogue.hpp
    src/resident/domains/interaction/born_dialogues/born_personal_dialogue_state.hpp
    src/resident/domains/interaction/born_dialogues/born_personal_dialogue_catalog.hpp
    src/resident/domains/interaction/born_dialogues/born_personal_dialogue_contract.hpp
    src/resident/domains/interaction/born_dialogues/born_personal_dialogue_detail.hpp
    src/resident/domains/interaction/born_dialogues/born_personal_dialogue_control.hpp
    src/resident/domains/interaction/born_dialogues/born_personal_dialogue_receipts.hpp
    src/resident/domains/interaction/born_dialogues/born_personal_dialogue_boundary.hpp
    src/resident/domains/interaction/born_dialogues/born_personal_dialogue_view.hpp
    src/resident/domains/interaction/born_dialogues/born_personal_dialogue_world.hpp
    src/resident/domains/interaction/personal_dialogues/known_personal_actor_source.hpp
    src/resident/domains/interaction/personal_dialogues/known_personal_dialogue.hpp
    src/resident/domains/interaction/personal_dialogues/known_personal_dialogue_catalog.hpp
    src/resident/domains/interaction/personal_dialogues/known_personal_dialogue_state.hpp
    src/resident/domains/interaction/personal_dialogues/known_personal_dialogue_contract.hpp
    src/resident/domains/interaction/personal_dialogues/known_personal_dialogue_codec_detail.hpp
    src/resident/domains/interaction/personal_dialogues/known_personal_dialogue_detail.hpp
    src/resident/domains/interaction/personal_dialogues/known_personal_dialogue_control.hpp
    src/resident/domains/interaction/personal_dialogues/known_personal_dialogue_receipts.hpp
    src/resident/domains/interaction/personal_dialogues/known_personal_dialogue_boundary.hpp
    src/resident/domains/interaction/personal_dialogues/known_personal_dialogue_view.hpp
    src/resident/domains/interaction/personal_dialogues/known_personal_dialogue_world.hpp
)

# 生产身份输入连续登记段：src/resident/domains/interaction/visit_replies/visit_reply_intents.hpp。
set(HANMO_RESIDENT_IDENTITY_interaction_visit_reply_intents_hpp
    src/resident/domains/interaction/visit_replies/visit_reply_intents.hpp
    src/resident/domains/interaction/visit_replies/visit_reply_authored.hpp
    src/resident/domains/interaction/visit_replies/visit_reply_director.hpp
    src/resident/domains/interaction/visit_replies/visit_reply_control.hpp
    src/resident/domains/interaction/known_contacts/known_contact_location_contract.hpp
)

# 生产身份输入连续登记段：src/resident/domains/interaction/regional_life_dialogue.hpp。
set(HANMO_RESIDENT_IDENTITY_interaction_regional_life_dialogue_hpp
    src/resident/domains/interaction/regional_life_dialogue.hpp
)

# 生产身份输入连续登记段：src/resident/domains/interaction/dialogue_provider_source.hpp。
set(HANMO_RESIDENT_IDENTITY_interaction_dialogue_provider_source_hpp
    src/resident/domains/interaction/dialogue_provider_source.hpp
    src/resident/domains/interaction/party_consent/party_recruitment_consent.hpp
)

# 生产身份输入连续登记段：src/resident/domains/interaction/dialogue_provider_source.cpp。
set(HANMO_RESIDENT_IDENTITY_interaction_dialogue_provider_source_cpp
    src/resident/domains/interaction/dialogue_provider_source.cpp
    src/resident/domains/interaction/party_consent/party_recruitment_consent.cpp
    src/resident/domains/interaction/party_consent/party_recruitment_consent_codec.cpp
    src/resident/domains/interaction/party_consent/party_recruitment_consent_profile.cpp
    src/resident/domains/interaction/party_consent/party_recruitment_consent_view.cpp
)

# 生产身份输入连续登记段：src/resident/domains/interaction/dialogue_node_source.cpp。
set(HANMO_RESIDENT_IDENTITY_interaction_dialogue_node_source_cpp
    src/resident/domains/interaction/dialogue_node_source.cpp
    src/resident/domains/interaction/dialogue_node_source.hpp
    src/resident/domains/interaction/face_to_face_contact.cpp
    src/resident/domains/interaction/face_to_face_contact.hpp
)

# 生产身份输入连续登记段：src/resident/domains/interaction/interaction_access.hpp。
set(HANMO_RESIDENT_IDENTITY_interaction_interaction_access_hpp
    src/resident/domains/interaction/interaction_access.hpp
    src/resident/domains/interaction/interaction_actions.cpp
    src/resident/domains/interaction/party_invitation.cpp
    src/resident/domains/interaction/party_invitation_codec.cpp
    src/resident/domains/interaction/party_invitation_commands.cpp
    src/resident/domains/interaction/party_invitation_profile.cpp
    src/resident/domains/interaction/party_invitation_recruitment.cpp
    src/resident/domains/interaction/party_invitation_view.cpp
    src/resident/domains/interaction/interaction_actions.hpp
    src/resident/domains/interaction/interaction_codec.cpp
    src/resident/domains/interaction/interaction_response_sources.cpp
    src/resident/domains/interaction/interaction_response_memory.cpp
    src/resident/domains/interaction/interaction_action_confirmation_memory.cpp
    src/resident/domains/interaction/interaction_growth_life.cpp
)

# 生产身份输入连续登记段：src/resident/domains/interaction/interaction_response_codec.cpp。
set(HANMO_RESIDENT_IDENTITY_interaction_interaction_response_codec_cpp
    src/resident/domains/interaction/interaction_response_codec.cpp
    src/resident/domains/interaction/interaction_session_life.cpp
    src/resident/domains/interaction/interaction_session_life_memory.cpp
    src/resident/domains/interaction/interaction_codec.hpp
    src/resident/domains/interaction/interaction_model.hpp
    src/resident/domains/interaction/interaction_role_history.hpp
    src/resident/domains/interaction/interaction_response_sources.hpp
    src/resident/domains/interaction/interaction_action_confirmation.hpp
    src/resident/domains/interaction/interaction_growth_life.hpp
    src/resident/domains/interaction/dialogue_command.hpp
    src/resident/domains/interaction/interaction_response_codec_fields.hpp
    src/resident/domains/interaction/interaction_session_life.hpp
    src/resident/domains/interaction/visit_messages/visit_message.hpp
    src/resident/domains/interaction/visit_messages/visit_message_state.hpp
    src/resident/domains/interaction/visit_messages/visit_message_catalog.hpp
    src/resident/domains/interaction/visit_messages/visit_message_view.hpp
    src/resident/domains/interaction/visit_messages/visit_message_world.hpp
    src/resident/domains/interaction/visit_messages/visit_message_receipt_state.hpp
    src/resident/domains/interaction/visit_messages/visit_message_receipt_control.hpp
    src/resident/domains/interaction/visit_messages/visit_message_receipt_profile.hpp
    src/resident/domains/interaction/visit_messages/visit_message_receipt_world.hpp
    src/resident/domains/interaction/visit_messages/visit_message_receipts.hpp
    src/resident/domains/interaction/visit_messages/visit_reception_catalog.hpp
    src/resident/domains/interaction/visit_messages/visit_reception_director.hpp
    src/resident/domains/interaction/visit_messages/visit_reception_world.hpp
    src/resident/domains/interaction/visit_messages/visit_reception_state.hpp
    src/resident/domains/interaction/visit_messages/visit_reception_claims.hpp
    src/resident/domains/interaction/party_invitation.hpp
    src/resident/domains/interaction/party_invitation_codec.hpp
    src/resident/domains/interaction/party_invitation_commands.hpp
    src/resident/domains/interaction/party_invitation_profile.hpp
    src/resident/domains/interaction/party_invitation_recruitment.hpp
    src/resident/domains/interaction/party_invitation_view.hpp
    src/resident/domains/interaction/interaction_phases.cpp
    src/resident/domains/interaction/interaction_phases.hpp
    src/resident/domains/interaction/interaction_rules.cpp
    src/resident/domains/interaction/interaction_role_history.cpp
    src/resident/domains/interaction/visit_messages/visit_message_catalog.cpp
    src/resident/domains/interaction/visit_messages/visit_message_codec.cpp
    src/resident/domains/interaction/visit_messages/visit_message_rules.cpp
    src/resident/domains/interaction/visit_messages/visit_message_validation.cpp
    src/resident/domains/interaction/visit_messages/visit_message_view.cpp
    src/resident/domains/interaction/visit_messages/visit_message_world.cpp
    src/resident/domains/interaction/visit_messages/visit_message_receipt_codec.cpp
    src/resident/domains/interaction/visit_messages/visit_message_receipt_control.cpp
    src/resident/domains/interaction/visit_messages/visit_message_receipt_profile.cpp
    src/resident/domains/interaction/visit_messages/visit_message_receipt_world.cpp
    src/resident/domains/interaction/visit_messages/visit_message_receipts.cpp
    src/resident/domains/interaction/visit_messages/visit_reception_catalog.cpp
    src/resident/domains/interaction/visit_messages/visit_reception_director.cpp
    src/resident/domains/interaction/visit_messages/visit_reception_world.cpp
    src/resident/domains/interaction/visit_messages/visit_reception_claims.cpp
    src/resident/domains/interaction/visit_messages/visit_reception_codec.cpp
    src/resident/domains/interaction/regional_life_dialogue.cpp
    src/resident/domains/interaction/regional_outcome_dialogue.cpp
    src/resident/domains/interaction/outcome_catalog.cpp
    src/resident/domains/interaction/regional_life_codec.cpp
    src/resident/domains/interaction/interaction_validation.cpp
)

# 生产身份输入连续登记段：src/resident/domains/interaction/communication_delivery_read.cpp。
set(HANMO_RESIDENT_IDENTITY_interaction_communication_delivery_read_cpp
    src/resident/domains/interaction/communication_delivery_read.cpp
    src/resident/domains/interaction/communication_links.cpp
    src/resident/domains/interaction/communication_links_codec.cpp
    src/resident/domains/interaction/face_to_face_contact_source.cpp
    src/resident/domains/interaction/face_to_face_contact_source_codec.cpp
    src/resident/domains/interaction/face_to_face_propagation.cpp
    src/resident/domains/interaction/face_to_face_report.cpp
    src/resident/domains/interaction/interaction_exact_encounter.cpp
    src/resident/domains/interaction/interaction_exact_encounter_codec.cpp
    src/resident/domains/interaction/local_communication_sources.cpp
    src/resident/domains/interaction/local_propagation_channels.cpp
    src/resident/domains/interaction/occupancy/actor_social_occupancy.cpp
    src/resident/domains/interaction/occupancy/actor_social_occupancy_codec.cpp
    src/resident/domains/interaction/occupancy/actor_social_occupancy_initial.cpp
    src/resident/domains/interaction/official_content_knowledge.cpp
    src/resident/domains/interaction/report_disclosure.cpp
    src/resident/domains/interaction/report_official_content.cpp
    src/resident/domains/interaction/runtime_offer_knowledge.cpp
    src/resident/domains/interaction/s3_commitment_read.cpp
    src/resident/domains/interaction/visit_messages/visit_report_custody.cpp
    src/resident/domains/interaction/visit_messages/visit_report_propagation.cpp
)

# 生产身份输入连续登记段：src/resident/domains/interaction/interaction_task_life.cpp。
set(HANMO_RESIDENT_IDENTITY_interaction_interaction_task_life_cpp
    src/resident/domains/interaction/interaction_task_life.cpp
)

# 生产身份输入连续登记段：src/resident/domains/interaction/occupancy/party_acceptance_history.cpp。
set(HANMO_RESIDENT_IDENTITY_interaction_party_acceptance_history_cpp
    src/resident/domains/interaction/occupancy/party_acceptance_history.cpp
)

# 生产身份输入连续登记段：src/resident/domains/interaction/occupancy/party_acceptance_history.hpp。
set(HANMO_RESIDENT_IDENTITY_interaction_party_acceptance_history_hpp
    src/resident/domains/interaction/occupancy/party_acceptance_history.hpp
)

# 生产身份输入连续登记段：src/resident/domains/interaction/appointments/personal_appointment_exact_phase.cpp。
set(HANMO_RESIDENT_IDENTITY_interaction_personal_appointment_exact_phase_cpp
    src/resident/domains/interaction/appointments/personal_appointment_exact_phase.cpp
    src/resident/domains/interaction/appointments/personal_appointment_life.cpp
    src/resident/domains/interaction/appointments/personal_appointment_native.cpp
    src/resident/domains/interaction/appointments/personal_appointment_native.hpp
    src/resident/domains/interaction/appointments/personal_appointment_time2.cpp
    src/resident/domains/interaction/appointments/personal_appointment_time2.hpp
    src/resident/domains/interaction/appointments/personal_appointment_time2_validation.cpp
    src/resident/domains/interaction/born_dialogues/born_personal_dialogue_proof_codec.hpp
    src/resident/domains/interaction/communication_delivery_read.hpp
    src/resident/domains/interaction/communication_links.hpp
    src/resident/domains/interaction/communication_ref_codec.hpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry_exact_phase.cpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry_life.cpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry_native.cpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry_native.hpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry_time2.cpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry_time2.hpp
    src/resident/domains/interaction/contact_inquiries/contact_inquiry_time2_validation.cpp
    src/resident/domains/interaction/face_to_face_contact_source.hpp
    src/resident/domains/interaction/face_to_face_propagation.hpp
    src/resident/domains/interaction/face_to_face_report.hpp
    src/resident/domains/interaction/interaction_context.hpp
    src/resident/domains/interaction/interaction_exact_phase.cpp
    src/resident/domains/interaction/interaction_exact_phase.hpp
    src/resident/domains/interaction/interaction_projection.hpp
    src/resident/domains/interaction/interaction_source_projection.hpp
    src/resident/domains/interaction/local_communication_sources.hpp
    src/resident/domains/interaction/local_propagation_channels.hpp
    src/resident/domains/interaction/occupancy/actor_social_occupancy.hpp
    src/resident/domains/interaction/occupancy/actor_social_occupancy_initial.hpp
    src/resident/domains/interaction/official_content_knowledge.hpp
    src/resident/domains/interaction/report_disclosure.hpp
    src/resident/domains/interaction/report_disclosure_codec_fields.hpp
    src/resident/domains/interaction/report_disclosure_types.hpp
    src/resident/domains/interaction/report_official_content.hpp
    src/resident/domains/interaction/s3_event_time.cpp
    src/resident/domains/interaction/s3_event_time.hpp
    src/resident/domains/interaction/s3_event_time_codec.hpp
    src/resident/domains/interaction/s3_leaf_life.cpp
    src/resident/domains/interaction/s3_leaf_life.hpp
    src/resident/domains/interaction/s3_leaf_life_codec.hpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_exact_phase.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_life.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_life.hpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_meeting_life.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_meeting_time2.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_meeting_time2.hpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_meeting_time2_validation.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_native.cpp
    src/resident/domains/interaction/visit_appointments/visit_appointment_native.hpp
    src/resident/domains/interaction/visit_messages/visit_message_life.cpp
    src/resident/domains/interaction/visit_messages/visit_message_life.hpp
    src/resident/domains/interaction/visit_messages/visit_report_custody.hpp
    src/resident/domains/interaction/visit_messages/visit_report_propagation.hpp
)

# 生产身份输入连续登记段：src/resident/domains/interaction/visit_messages/visit_message_receipt_native.cpp。
set(HANMO_RESIDENT_IDENTITY_interaction_visit_message_receipt_native_cpp
    src/resident/domains/interaction/visit_messages/visit_message_receipt_native.cpp
    src/resident/domains/interaction/visit_messages/visit_message_receipt_native.hpp
    src/resident/domains/interaction/visit_replies/visit_reply_life.cpp
    src/resident/domains/interaction/visit_replies/visit_reply_life.hpp
    src/resident/domains/interaction/visit_replies/visit_reply_native.cpp
    src/resident/domains/interaction/visit_replies/visit_reply_native.hpp
)

# 生产身份输入连续登记段：src/resident/domains/interaction/visit_messages/visit_authored_command.cpp。
set(HANMO_RESIDENT_IDENTITY_interaction_visit_authored_command_cpp
    src/resident/domains/interaction/visit_messages/visit_authored_command.cpp
    src/resident/domains/interaction/visit_messages/visit_authored_command.hpp
    src/resident/domains/interaction/visit_messages/visit_message_native.cpp
    src/resident/domains/interaction/visit_messages/visit_original_result_read.cpp
    src/resident/domains/interaction/visit_messages/visit_message_native.hpp
    src/resident/domains/interaction/visit_messages/visit_original_result_read.hpp
)

# 本领域的拆分实现片段：只随原翻译单元包含，同时进入目标与生产身份清单。
list(APPEND HANMO_RESIDENT_SPLIT_IMPLEMENTATION_SOURCES
    src/resident/domains/interaction/source_owned_fields/domain_fields.inc
    src/resident/domains/interaction/world_codec/catalog_appointments_inquiries.inc
    src/resident/domains/interaction/world_codec/catalog_born_dialogues.inc
    src/resident/domains/interaction/world_codec/catalog_known_dialogues.inc
    src/resident/domains/interaction/world_codec/catalog_message_profiles.inc
    src/resident/domains/interaction/world_codec/catalog_original.inc
    src/resident/domains/interaction/world_codec/checkpoint_read_actor_social_occupancy.inc
    src/resident/domains/interaction/world_codec/checkpoint_read_interaction.inc
    src/resident/domains/interaction/world_codec/checkpoint_read_known_personal_dialogues.inc
    src/resident/domains/interaction/world_codec/checkpoint_read_personal_appointments.inc
    src/resident/domains/interaction/world_codec/checkpoint_read_visit_message_receipt_module.inc
    src/resident/domains/interaction/world_codec/checkpoint_read_visit_messages.inc
    src/resident/domains/interaction/world_codec/export_dialogue_appointment_payloads.inc
    src/resident/domains/interaction/world_codec/seed_dialogues_appointments.inc
    src/resident/domains/interaction/world_codec/seed_validate_known_dialogues.inc
    src/resident/domains/interaction/world_codec/seed_validate_original_dialogues.inc
    src/resident/domains/interaction/world_codec/seed_validate_visit_appointments.inc
)
