// 政治世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (const auto *config = dynamic->find("political_military_appointments")) {
  if (!politics_enabled(*result) || !result->military ||
      !result->military->officer_assignment_policy)
    fail("PoliticalMilitaryAppointmentDependenciesRequired",
         "dynamic_world/political_military_appointments");
  const auto &caps = required_capabilities();
  if (std::find(caps.begin(), caps.end(),
                "political_military_appointments_v1") == caps.end())
    unsupported("PoliticalMilitaryAppointmentCapabilityRequired",
                "dynamic_world/political_military_appointments");
  auto parsed = decode_political_military_appointment_catalog(*config);
  if (!parsed.ok)
    return {false, {}, parsed.error};
  if (!parsed.value.enabled)
    fail("PoliticalMilitaryAppointmentProfileRequired",
         "dynamic_world/political_military_appointments");
  PoliticalMilitaryAppointmentUnitRead units;
  units.garrison_site = [&](const StableId &id) -> std::optional<StableId> {
    for (const auto &g : result->military->garrisons)
      if (g.id == id)
        return g.place_id;
    return {};
  };
  auto valid = validate_political_military_appointment_catalog(
      *result->common, *result->organization, parsed.value, units);
  if (!valid.ok)
    return {false, {}, valid.error};
  result->political_military_appointments =
      checkpoint_make_shared<const PoliticalMilitaryAppointmentCatalog>(
          std::move(parsed.value));
}
if (const auto *config = dynamic->find("political_field_appointments")) {
  if (!political_military_appointments_enabled(*result) || !result->military ||
      !result->military->field_command_policy)
    fail("PoliticalFieldAppointmentDependenciesRequired",
         "dynamic_world/political_field_appointments");
  const auto &caps = required_capabilities();
  if (std::find(caps.begin(), caps.end(), "political_field_appointments_v1") ==
      caps.end())
    unsupported("PoliticalFieldAppointmentCapabilityRequired",
                "dynamic_world/political_field_appointments");
  auto parsed = decode_political_field_catalog(*config);
  if (!parsed.ok)
    return {false, {}, parsed.error};
  if (!parsed.value.enabled)
    fail("PoliticalFieldAppointmentProfileRequired",
         "dynamic_world/political_field_appointments");
  PoliticalFieldMilitaryRead units;
  units.garrison_site = [&](const StableId &id) -> std::optional<StableId> {
    for (const auto &g : result->military->garrisons)
      if (g.id == id)
        return g.place_id;
    return {};
  };
  auto valid = validate_political_field_catalog(
      *result->common, *result->organization, parsed.value, units);
  if (!valid.ok)
    return {false, {}, valid.error};
  result->political_field_appointments =
      checkpoint_make_shared<const PoliticalFieldAppointmentCatalog>(
          std::move(parsed.value));
}
if (const auto *config = dynamic->find("political_takeover")) {
  if (!politics_enabled(*result))
    fail("PoliticalTakeoverRequiresPolitics",
         "dynamic_world/political_takeover");
  auto body = read_political_author_body_version(scenario);
  if (!body.ok)
    return {false, {}, std::move(body.error)};
  const auto version = get_int(*config, "version", "political_takeover");
  if ((version != 1 && version != 2) || version != body.value)
    fail("PoliticalTakeoverAuthorVersionMismatch",
         "dynamic_world/political_takeover");
  auto parsed = version == 2 ? decode_political_takeover_catalog2(*config)
                             : decode_political_takeover_catalog(*config);
  if (!parsed.ok)
    return {false, {}, parsed.error};
  if (parsed.value.task_execution_version == 2) {
    const auto &caps = required_capabilities();
    if (std::find(caps.begin(), caps.end(),
                  "task_static_public_execution_v2") == caps.end())
      unsupported("TaskStaticPublicExecutionCapabilityRequired",
                  "dynamic_world/political_takeover/task_execution_version");
  }
  auto valid = validate_political_takeover_catalog(
      *result->common, *result->organization, *result->politics,
      *result->military, parsed.value);
  if (!valid.ok)
    return {false, {}, valid.error};
  result->political_takeover =
      checkpoint_make_shared<const PoliticalTakeoverCatalog>(
          std::move(parsed.value));
  result->task_takeover_execution_enabled = true;
}
