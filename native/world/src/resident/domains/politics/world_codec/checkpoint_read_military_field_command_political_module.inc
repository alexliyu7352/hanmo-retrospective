// 政治检查点恢复分派：沿原 else-if 链读取
// military_field_command_political_module,
// political_takeover；不改变段顺序、所有权或拒绝条件。
else if (section.domain == military_field_command_political_module) {
  auto state = make_state<Seed, PoliticalFieldAppointmentState>();
  r = visit_political_field_appointments_checkpoint(reader, *state);
  seed.political_field_appointments = std::move(state);
}
else if (section.domain == "political_takeover") {
  auto selected = read_political_locked_body_version(catalog.identity,
                                                     "political_takeover");
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (!selected.value || section.codec_version != selected.value ||
      !takeover_enabled(catalog) ||
      selected.value != catalog.political_takeover->version)
    return {
        false,
        {},
        common_failure("PoliticalTakeoverCheckpointSelectionMismatch").error};
  auto state = make_state<Seed, PoliticalTakeoverState>();
  r = selected.value == 2 ? visit_political_takeover_checkpoint2(reader, *state)
                          : visit_political_takeover_checkpoint(reader, *state);
  seed.political_takeover = std::move(state);
}