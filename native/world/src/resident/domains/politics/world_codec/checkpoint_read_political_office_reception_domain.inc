// 政治检查点恢复分派：沿原 else-if 链读取 political_office_reception_domain,
// political_office_resignation_module,
// political_subordinate_appointment_module, political_dismissal_sources_module,
// political_governance_agency_domain；不改变段顺序、所有权或拒绝条件。
else if (section.domain == political_office_reception_domain) {
  auto state = make_state<Seed, PoliticalOfficeReceptionState>();
  r = visit_political_office_reception_checkpoint(reader, *state);
  seed.political_office_receptions = std::move(state);
}
else if (section.domain == political_office_resignation_module) {
  auto state = make_state<Seed, PoliticalOfficeResignationState>();
  r = visit_political_office_resignation_checkpoint(reader, *state);
  seed.political_office_resignations = std::move(state);
}
else if (section.domain == political_subordinate_appointment_module) {
  auto state = make_state<Seed, PoliticalSubordinateAppointmentState>();
  r = visit_political_subordinate_appointment_checkpoint(reader, *state);
  seed.political_subordinate_appointments = std::move(state);
}
else if (section.domain == political_dismissal_sources_module) {
  auto state = make_state<Seed, PoliticalDismissalSourceState>();
  r = visit_political_dismissal_sources(reader, *state);
  seed.political_dismissal_sources = std::move(state);
}
else if (section.domain == political_governance_agency_domain) {
  auto state = make_state<Seed, PoliticalGovernanceAgencyState>();
  r = visit_political_governance_agency_checkpoint(reader, *state);
  seed.political_governance_agencies = std::move(state);
}