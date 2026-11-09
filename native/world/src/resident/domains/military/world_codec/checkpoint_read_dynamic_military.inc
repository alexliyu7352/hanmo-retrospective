// 军事检查点恢复分派：沿原 else-if 链读取 dynamic_military,
// military_action_time, military_training_completion_time,
// military_action_conditions, military_formation_reservations,
// military_action_resources, military_return_detachments,
// military_authority_time；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "dynamic_military") {
  auto selected = selected_native_military_state_versions(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (!selected.value.dynamic_military ||
      section.codec_version != selected.value.dynamic_military ||
      seed.military_exact)
    return {false,
            {},
            common_failure("DynamicMilitaryCheckpointSelectionMismatch").error};
  auto state = make_state<Seed, MilitaryExactState>();
  r = visit_military_exact_state(reader, *state,
                                 selected.value.dynamic_military);
  seed.military_exact = std::move(state);
}
else if (section.domain == "military_action_time") {
  auto selected = selected_native_military_state_versions(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (!selected.value.military_action_time ||
      section.codec_version != selected.value.military_action_time ||
      military_action_time)
    return {
        false,
        {},
        common_failure("MilitaryActionTimeCheckpointSelectionMismatch").error};
  military_action_time = make_state<Seed, MilitaryActionTimeState>();
  r = visit_military_action_time_state(reader, *military_action_time,
                                       selected.value.military_action_time);
}
else if (section.domain == "military_training_completion_time") {
  auto selected = selected_native_military_state_versions(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (selected.value.military_training_completion_time != 1 ||
      section.codec_version != 1 || !military_action_time)
    return {
        false,
        {},
        common_failure("MilitaryTrainingCompletionCheckpointSelectionMismatch")
            .error};
  r = visit_military_training_completion_time(reader, *military_action_time, 1);
}
else if (section.domain == "military_action_conditions") {
  auto selected = selected_native_military_state_versions(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if ((selected.value.military_action_conditions != 1 &&
       selected.value.military_action_conditions != 2) ||
      section.codec_version != selected.value.military_action_conditions ||
      !military_action_time)
    return {
        false,
        {},
        common_failure("MilitaryConditionCheckpointSelectionMismatch").error};
  r = visit_military_action_conditions(
      reader, *military_action_time, selected.value.military_action_conditions);
}
else if (section.domain == "military_formation_reservations") {
  auto selected = selected_native_military_state_versions(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if ((selected.value.military_formation_reservations != 1 &&
       selected.value.military_formation_reservations != 2) ||
      section.codec_version != selected.value.military_formation_reservations ||
      !military_action_time)
    return {false,
            {},
            common_failure(
                "MilitaryFormationReservationCheckpointSelectionMismatch")
                .error};
  r = visit_military_formation_reservations(
      reader, *military_action_time,
      selected.value.military_formation_reservations);
}
else if (section.domain == "military_action_resources") {
  auto selected = selected_native_military_state_versions(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (selected.value.military_action_resources != 1 ||
      section.codec_version != 1 || !military_action_time)
    return {
        false,
        {},
        common_failure("MilitaryResourceCheckpointSelectionMismatch").error};
  r = visit_military_action_resources(reader, *military_action_time, 1);
}
else if (section.domain == "military_return_detachments") {
  auto selected = selected_native_military_state_versions(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (selected.value.military_return_detachments != 1 ||
      section.codec_version != 1 || !military_action_time)
    return {
        false,
        {},
        common_failure("MilitaryReturnDetachmentCheckpointSelectionMismatch")
            .error};
  r = visit_military_return_detachments(reader, *military_action_time, 1);
}
else if (section.domain == "military_authority_time") {
  auto selected = selected_native_military_state_versions(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (selected.value.military_authority_time != 1 || section.codec_version != 1)
    return {
        false,
        {},
        common_failure("MilitaryAuthorityCheckpointSelectionMismatch").error};
  r = visit_military_authority_time(reader, *m, 1);
}