// 交互世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto appointments = decode_personal_appointment_profile_version(
    scenario, required_capabilities());
if (!appointments.ok)
  return {false, {}, appointments.error};
if (appointments.value) {
  auto decoded = decode_personal_appointment_catalog(
      field(scenario, "personal_appointments", "scenario"),
      *result->interaction);
  if (!decoded.ok)
    return {false, {}, decoded.error};
  result->personal_appointments =
      checkpoint_make_shared<const PersonalAppointmentCatalog>(
          std::move(decoded.value));
}
auto inquiries =
    decode_contact_inquiry_profile_version(scenario, required_capabilities());
if (!inquiries.ok)
  return {false, {}, inquiries.error};
if (inquiries.value) {
  auto decoded = decode_contact_inquiry_catalog(
      field(scenario, "contact_inquiries", "scenario"), *result->interaction);
  if (!decoded.ok)
    return {false, {}, decoded.error};
  result->contact_inquiries =
      checkpoint_make_shared<const ContactInquiryCatalog>(
          std::move(decoded.value));
}
auto visit_replies =
    decode_visit_reply_profile_version(scenario, required_capabilities());
if (!visit_replies.ok)
  return {false, {}, visit_replies.error};
if (visit_replies.value) {
  if (!result->interaction->visit_messages)
    return {
        false, {}, common_failure("VisitReplyOriginalProfileRequired").error};
  auto decoded = decode_visit_reply_catalog(
      field(scenario, "visit_message_replies", "scenario"),
      *result->interaction->visit_messages, *result->interaction);
  if (!decoded.ok)
    return {false, {}, decoded.error};
  if (decoded.value.version != visit_replies.value)
    return {false, {}, common_failure("VisitReplyProfileMismatch").error};
  result->visit_message_replies =
      checkpoint_make_shared<const VisitReplyCatalog>(std::move(decoded.value));
}
auto visit_appointments =
    decode_visit_appointment_profile_version(scenario, required_capabilities());
if (!visit_appointments.ok)
  return {false, {}, visit_appointments.error};
if (visit_appointments.value) {
  if (!result->interaction->visit_messages || !result->personal_appointments)
    return {false,
            {},
            common_failure("VisitAppointmentOriginalProfileRequired").error};
  auto decoded = decode_visit_appointment_catalog(
      field(scenario, "visit_appointment_invitations", "scenario"),
      *result->interaction->visit_messages, *result->personal_appointments,
      *result->interaction, result->visit_message_replies.get());
  if (!decoded.ok)
    return {false, {}, decoded.error};
  result->visit_appointment_invitations =
      checkpoint_make_shared<const VisitAppointmentCatalog>(
          std::move(decoded.value));
}
auto attendance = decode_personal_appointment_attendance_profile(
    scenario, required_capabilities());
if (!attendance.ok)
  return {false, {}, attendance.error};
auto written_attendance = decode_visit_appointment_attendance_profile(
    scenario, required_capabilities());
if (!written_attendance.ok)
  return {false, {}, written_attendance.error};
