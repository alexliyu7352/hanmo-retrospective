// 决策世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
std::shared_ptr<const VisitReceptionCatalog> reception_catalog;
if (visit_reception.value) {
  auto parsed = decode_visit_reception_catalog(
      field(scenario, "visit_reception_intents", "scenario"),
      *result->interaction->visit_messages, *result->interaction);
  if (!parsed.ok)
    return {false, {}, parsed.error};
  reception_catalog = checkpoint_make_shared<const VisitReceptionCatalog>(
      std::move(parsed.value));
}
auto agency_intents_profile = decode_political_governance_agency_intent_profile(
    scenario, required_capabilities());
if (!agency_intents_profile.ok)
  return {false, {}, agency_intents_profile.error};
auto resignation_responses_profile =
    decode_political_office_resignation_response_profile(
        scenario, required_capabilities());
if (!resignation_responses_profile.ok)
  return {false, {}, resignation_responses_profile.error};
auto director_subjects = decode_native_director_subject_selection(scenario);
if (!director_subjects.ok)
  return {false, {}, std::move(director_subjects.error)};
auto director = decode_director_catalog(
    field(*dynamic, "director", "dynamic_world"), result->regional_life,
    result->named_membership, politics_enabled(*result),
    result->envoy_tasks_enabled,
    result->diplomacy ? result->diplomacy->version : 1,
    treaty_passage_enabled(*result),
    DirectorMilitaryAppointmentProfile{
        political_military_appointments_enabled(*result),
        political_field_appointments_enabled(*result)},
    DirectorSupplyRenewalProfile{result->economic_supply_policies_enabled},
    reception_catalog,
    DirectorShopRegistryProfile{
        dynamic->find("political_shop_registry_successions") != nullptr},
    attendance.value,
    DirectorGovernanceIntentProfile{result->politics_governance &&
                                    result->politics_governance->enabled &&
                                    result->public_relief != nullptr},
    DirectorGovernanceAgencyProfile{agency_intents_profile.value.has_value()},
    DirectorOfficeResignationResponseProfile{
        resignation_responses_profile.value.has_value()},
    director_subjects.value.subjects);
if (!director.ok)
  return {false, {}, director.error};
if (director.value.personal_appointment_attendance) {
  if (!result->personal_appointments)
    fail("AppointmentAttendanceProfileMismatch", "director");
  auto checked = validate_personal_appointment_attendance_catalog(
      *director.value.personal_appointment_attendance,
      *result->personal_appointments);
  if (!checked.ok)
    return {false, {}, checked.error};
}
director.value.command_acceptance_bindings =
    director_subjects.value.physical_version == 6;
director.value.exclude_selected_player = native_existing_actor_policy(
    get_string(*dynamic, "founder_policy", "dynamic_world"));
result->director =
    checkpoint_make_shared<const DirectorCatalog>(std::move(director.value));
if (const auto *config = scenario.find("visit_appointment_intents")) {
  if (!result->visit_appointment_invitations ||
      !result->director->visit_reception)
    return {
        false, {}, common_failure("VisitAppointmentAuthorshipRequired").error};
  auto decoded = decode_visit_appointment_intents(
      *config, *result->visit_appointment_invitations,
      *result->director->visit_reception, *result->interaction->visit_messages,
      *result->personal_appointments, *result->interaction,
      result->visit_message_replies.get());
  if (!decoded.ok)
    return {false, {}, decoded.error};
  result->visit_appointment_intents =
      checkpoint_make_shared<const VisitAppointmentIntentCatalog>(
          std::move(decoded.value));
}
if (written_attendance.value) {
  if (!result->director->personal_appointment_attendance ||
      !result->visit_appointment_invitations || !result->personal_appointments)
    return {false,
            {},
            common_failure("VisitAppointmentAttendanceProfileRequired").error};
  auto parsed = decode_visit_appointment_attendance_catalog(
      field(scenario, "visit_appointment_attendance", "scenario"));
  if (!parsed.ok)
    return {false, {}, parsed.error};
  auto valid = validate_visit_appointment_attendance_catalog(
      parsed.value, *result->visit_appointment_invitations,
      *result->director->personal_appointment_attendance,
      *result->personal_appointments);
  if (!valid.ok)
    return {false, {}, valid.error};
  result->visit_appointment_attendance =
      checkpoint_make_shared<const VisitAppointmentAttendanceCatalog>(
          std::move(parsed.value));
}
if (visit_reply_authored_version(visit_replies.value)) {
  if (!result->director->visit_reception || !result->visit_message_replies)
    return {false, {}, common_failure("VisitReplyAuthorshipRequired").error};
  auto parsed = decode_visit_reply_intents(
      field(scenario, "visit_reply_intents", "scenario"),
      *result->visit_message_replies, *result->director->visit_reception,
      *result->interaction->visit_messages, *result->interaction);
  if (!parsed.ok)
    return {false, {}, parsed.error};
  result->visit_reply_intents =
      checkpoint_make_shared<const VisitReplyIntentCatalog>(
          std::move(parsed.value));
}
if (result->director->governance_intents) {
  const auto &caps = required_capabilities();
  if (!result->politics || !result->organization ||
      !result->politics_governance ||
      std::find(caps.begin(), caps.end(), "political_governance_intents_v1") ==
          caps.end())
    fail("GovernanceIntentProfileRequired", "director");
  auto checked = validate_political_governance_intents(
      *result->director->governance_intents, *result->common, *result->politics,
      *result->politics_governance, *result->organization);
  if (!checked.ok)
    return {false, {}, checked.error};
}
if (result->director->authored_task_continuation) {
  const auto &caps = required_capabilities();
  if (std::find(caps.begin(), caps.end(),
                result->director->authored_task_continuation->version == 2
                    ? "task_authored_continuation_v2"
                    : "task_authored_continuation_v1") == caps.end())
    unsupported("TaskContinuationCapabilityRequired", "dynamic_world/director");
  auto valid = validate_task_authored_continuation_catalog(
      *result->director->authored_task_continuation, *result->tasks,
      *result->common);
  if (!valid.ok)
    return {false, {}, valid.error};
}
if (result->director->supply_renewals) {
  const auto &caps = required_capabilities();
  if (std::find(caps.begin(), caps.end(), "political_supply_renewals_v1") ==
      caps.end())
    unsupported("PoliticalSupplyRenewalCapabilityRequired", "director");
  if (!result->political_takeover || !result->organization ||
      !result->economic_supply_policies)
    fail("PoliticalSupplyRenewalPairRequired", "director");
  auto valid = validate_political_supply_renewals(
      *result->director->supply_renewals, *result->political_takeover,
      *result->organization, *result->economic_supply_policies);
  if (!valid.ok)
    return {false, {}, valid.error};
}
