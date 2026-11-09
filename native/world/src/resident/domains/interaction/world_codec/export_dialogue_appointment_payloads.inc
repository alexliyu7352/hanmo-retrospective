// 交互世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
WORLD_FIELDS(P::fact_payloads, "personal_appointments",
             emit_personal_appointment_fact_payloads_fields(
                 out, w.common,
                 w.interaction.personal_appointments
                     ? &*w.interaction.personal_appointments
                     : nullptr));
WORLD_FIELDS(P::action_payloads, "personal_appointments",
             emit_personal_appointment_action_payloads_fields(
                 out, w.common,
                 w.interaction.personal_appointments
                     ? &*w.interaction.personal_appointments
                     : nullptr));
WORLD_FIELDS(P::fact_payloads, "visit_appointment_invitations",
             emit_visit_appointment_fact_payloads_fields(
                 out, w.common,
                 w.interaction.visit_appointment_invitations
                     ? &*w.interaction.visit_appointment_invitations
                     : nullptr,
                 w.interaction.visit_appointment_meetings
                     ? &*w.interaction.visit_appointment_meetings
                     : nullptr));
WORLD_FIELDS(P::action_payloads, "visit_appointment_meetings",
             emit_visit_appointment_action_payloads_fields(
                 out, w.common,
                 w.interaction.visit_appointment_meetings
                     ? &*w.interaction.visit_appointment_meetings
                     : nullptr));
WORLD_FIELDS(P::fact_payloads, "contact_inquiries",
             emit_contact_inquiry_fact_payloads_fields(
                 out, w.common,
                 w.interaction.contact_inquiries
                     ? &*w.interaction.contact_inquiries
                     : nullptr));
WORLD_FIELDS(P::action_payloads, "contact_inquiries",
             emit_contact_inquiry_action_payloads_fields(
                 out, w.common,
                 w.interaction.contact_inquiries
                     ? &*w.interaction.contact_inquiries
                     : nullptr));
WORLD_FIELDS(P::fact_payloads, "visit_message_replies",
             emit_visit_reply_fact_payloads_fields(
                 out, w.common,
                 w.interaction.visit_message_replies
                     ? &*w.interaction.visit_message_replies
                     : nullptr));
WORLD_FIELDS(P::fact_payloads, "born_personal_dialogues",
             emit_born_personal_dialogue_fact_payloads_fields(
                 out, w.common,
                 w.interaction.born_personal_dialogues
                     ? &*w.interaction.born_personal_dialogues
                     : nullptr));
WORLD_FIELDS(P::fact_payloads, "known_personal_dialogues",
             emit_known_personal_dialogue_fact_payloads_fields(
                 out, w.common,
                 w.interaction.known_personal_dialogues
                     ? &*w.interaction.known_personal_dialogues
                     : nullptr));
