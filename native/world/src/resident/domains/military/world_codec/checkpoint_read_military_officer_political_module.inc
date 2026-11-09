// 军事检查点恢复分派：沿原 else-if 链读取
// military_officer_political_module；不改变段顺序、所有权或拒绝条件。
else if (section.domain == military_officer_political_module) {
  auto state = make_state<Seed, PoliticalMilitaryAppointmentState>();
  r = visit_political_military_appointments_checkpoint(
      reader, *state, political_field_appointments_enabled(catalog));
  seed.political_military_appointments = std::move(state);
}