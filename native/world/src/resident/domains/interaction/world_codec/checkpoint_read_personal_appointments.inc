// 交互检查点恢复分派：沿原 else-if 链读取 personal_appointments,
// contact_inquiries, visit_message_replies, born_personal_dialogues,
// visit_appointment_invitations,
// visit_appointment_meetings；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "personal_appointments") {
  i->personal_appointments.emplace();
  r = visit_personal_appointments_checkpoint(reader, *i->personal_appointments);
  if (r.ok && i->personal_appointments->version != section.codec_version)
    r = common_failure("InteractionSectionPayloadVersionMismatch",
                       section.domain);
}
else if (section.domain == "contact_inquiries") {
  i->contact_inquiries.emplace();
  r = visit_contact_inquiries_checkpoint(reader, *i->contact_inquiries);
  if (r.ok && i->contact_inquiries->version != section.codec_version)
    r = common_failure("InteractionSectionPayloadVersionMismatch",
                       section.domain);
}
else if (section.domain == "visit_message_replies") {
  i->visit_message_replies.emplace();
  r = visit_visit_replies_checkpoint(reader, *i->visit_message_replies);
}
else if (section.domain == "born_personal_dialogues") {
  i->born_personal_dialogues.emplace();
  r = visit_born_personal_dialogues_checkpoint(reader,
                                               *i->born_personal_dialogues);
}
else if (section.domain == "visit_appointment_invitations") {
  i->visit_appointment_invitations.emplace();
  r = visit_visit_appointments_checkpoint(reader,
                                          *i->visit_appointment_invitations);
  if (r.ok &&
      i->visit_appointment_invitations->version != section.codec_version)
    r = common_failure("InteractionSectionPayloadVersionMismatch",
                       section.domain);
}
else if (section.domain == "visit_appointment_meetings") {
  i->visit_appointment_meetings.emplace();
  r = visit_visit_appointment_meetings_checkpoint(
      reader, *i->visit_appointment_meetings);
  if (r.ok && i->visit_appointment_meetings->version != section.codec_version)
    r = common_failure("InteractionSectionPayloadVersionMismatch",
                       section.domain);
}