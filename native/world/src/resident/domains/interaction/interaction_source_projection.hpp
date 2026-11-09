// Original JSON and bounded projection share these exact borrowed field writers.
#pragma once
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
class CommonStore;struct ActionCoreView;struct FactRecord;
struct BornPersonalDialogueState;
struct ContactInquiryStarted;
struct ContactInquiryState;
struct KnownPersonalDialogueState;
struct PersonalAppointmentStarted;
struct PersonalAppointmentState;
struct VisitAppointmentMeetingStarted;
struct VisitAppointmentMeetingState;
struct VisitAppointmentState;
struct VisitReplyState;
struct VisitReceptionState;
Status emit_visit_reception_claims_fields(ProjectionWriter&,const VisitReceptionState&);

Outcome<bool> emit_personal_appointment_action_payload_fields(ProjectionWriter&,const PersonalAppointmentState*,const ActionCoreView&);
Outcome<bool> emit_contact_inquiry_action_payload_fields(ProjectionWriter&,const ContactInquiryState*,const ActionCoreView&);
Outcome<bool> emit_visit_appointment_action_payload_fields(ProjectionWriter&,const VisitAppointmentMeetingState*,const ActionCoreView&);
Outcome<bool> emit_visit_appointment_fact_payload_fields(ProjectionWriter&,const VisitAppointmentState*,const VisitAppointmentMeetingState*,const FactRecord&);
// Per-Common-row fields. Successful false leaves the sink untouched.
Outcome<bool> emit_visit_reply_fact_payload_fields(ProjectionWriter&,const VisitReplyState*,const FactRecord&);
Outcome<bool> emit_personal_appointment_fact_payload_fields(ProjectionWriter&,const PersonalAppointmentState*,const FactRecord&);
Outcome<bool> emit_born_personal_dialogue_fact_payload_fields(ProjectionWriter&,const BornPersonalDialogueState*,const FactRecord&);
Outcome<bool> emit_known_personal_dialogue_fact_payload_fields(ProjectionWriter&,const KnownPersonalDialogueState*,const FactRecord&);
Outcome<bool> emit_contact_inquiry_fact_payload_fields(ProjectionWriter&,const ContactInquiryState*,const FactRecord&);

Status emit_personal_appointments_fields(ProjectionWriter&,const PersonalAppointmentState&);
Status emit_personal_appointment_fact_fields(ProjectionWriter&,const PersonalAppointmentState&,const StableId&,const FactRecord* expected=nullptr);
Status emit_personal_appointment_action_fields(ProjectionWriter&,const PersonalAppointmentStarted&);
Status emit_contact_inquiries_fields(ProjectionWriter&,const ContactInquiryState&);
Status emit_contact_inquiry_fact_fields(ProjectionWriter&,const ContactInquiryState&,const StableId&,const FactRecord* expected=nullptr);
Status emit_contact_inquiry_action_fields(ProjectionWriter&,const ContactInquiryStarted&);
Status emit_visit_appointments_fields(ProjectionWriter&,const VisitAppointmentState&);
Status emit_visit_appointment_fact_fields(ProjectionWriter&,const VisitAppointmentState&,const StableId&,const FactRecord* expected=nullptr);
Status emit_visit_appointment_meetings_fields(ProjectionWriter&,const VisitAppointmentMeetingState&);
Status emit_visit_appointment_meeting_fact_fields(ProjectionWriter&,const VisitAppointmentMeetingState&,const StableId&,const FactRecord* expected=nullptr);
Status emit_visit_appointment_meeting_action_fields(ProjectionWriter&,const VisitAppointmentMeetingStarted&);
Status emit_visit_replies_fields(ProjectionWriter&,const VisitReplyState&);
Status emit_visit_reply_fact_fields(ProjectionWriter&,const VisitReplyState&,const StableId&,const FactRecord* expected=nullptr);
Status emit_known_personal_dialogues_fields(ProjectionWriter&,const KnownPersonalDialogueState&);
Status emit_known_personal_dialogue_fact_fields(ProjectionWriter&,const KnownPersonalDialogueState&,const StableId&,const FactRecord* expected=nullptr);
Status emit_born_personal_dialogues_fields(ProjectionWriter&,const BornPersonalDialogueState&);
Status emit_born_personal_dialogue_fact_fields(ProjectionWriter&,const BornPersonalDialogueState&,const StableId&,const FactRecord* expected=nullptr);
Status emit_personal_appointment_fact_payloads_fields(ProjectionWriter& w,const CommonStore& common,const PersonalAppointmentState* state);

Status emit_personal_appointment_action_payloads_fields(ProjectionWriter& w,const CommonStore& common,const PersonalAppointmentState* state);

Status emit_contact_inquiry_fact_payloads_fields(ProjectionWriter& w,const CommonStore& common,const ContactInquiryState* state);

Status emit_contact_inquiry_action_payloads_fields(ProjectionWriter& w,const CommonStore& common,const ContactInquiryState* state);

Status emit_visit_reply_fact_payloads_fields(ProjectionWriter& w,const CommonStore& common,const VisitReplyState* state);

Status emit_known_personal_dialogue_fact_payloads_fields(ProjectionWriter& w,const CommonStore& common,const KnownPersonalDialogueState* state);

Status emit_born_personal_dialogue_fact_payloads_fields(ProjectionWriter& w,const CommonStore& common,const BornPersonalDialogueState* state);

Status emit_visit_appointment_fact_payloads_fields(ProjectionWriter& w,const CommonStore& common,const VisitAppointmentState* state,const VisitAppointmentMeetingState* meetings);

Status emit_visit_appointment_action_payloads_fields(ProjectionWriter& w,const CommonStore& common,const VisitAppointmentMeetingState* meetings);

}









