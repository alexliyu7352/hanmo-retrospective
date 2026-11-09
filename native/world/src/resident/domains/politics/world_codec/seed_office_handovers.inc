// 政治世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (catalog.political_head_handovers) {
  auto parsed = decode_political_head_handover_state(
      field(source, political_head_handover_domain, "state"));
  if (!parsed.ok)
    return {false, {}, parsed.error};
  result.political_head_handovers =
      std::make_shared<const PoliticalHeadHandoverState>(
          std::move(parsed.value));
} else if (source.find(political_head_handover_domain))
  fail("UnexpectedPoliticalHeadHandoverDomain", political_head_handover_domain);
if (catalog.political_successor_rounds) {
  auto parsed = decode_political_successor_round_state(
      field(source, political_successor_rounds_domain, "state"));
  if (!parsed.ok)
    return {false, {}, parsed.error};
  result.political_successor_rounds =
      std::make_shared<const PoliticalSuccessorRoundState>(
          std::move(parsed.value));
} else if (source.find(political_successor_rounds_domain))
  fail("UnexpectedPoliticalSuccessorRoundDomain",
       political_successor_rounds_domain);
if (catalog.political_term_handovers) {
  auto parsed = decode_political_term_handover_state(
      field(source, political_term_handover_domain, "state"));
  if (!parsed.ok)
    return {false, {}, parsed.error};
  result.political_term_handovers =
      std::make_shared<const PoliticalTermHandoverState>(
          std::move(parsed.value));
} else if (source.find(political_term_handover_domain))
  fail("UnexpectedPoliticalTermHandoverDomain", political_term_handover_domain);
if (catalog.political_office_receptions) {
  auto parsed = decode_political_office_reception_state(
      field(source, political_office_reception_domain, "state"));
  if (!parsed.ok)
    return {false, {}, parsed.error};
  result.political_office_receptions =
      std::make_shared<const PoliticalOfficeReceptionState>(
          std::move(parsed.value));
} else if (source.find(political_office_reception_domain))
  fail("UnexpectedPoliticalOfficeReceptionDomain",
       political_office_reception_domain);
if (catalog.political_office_resignations) {
  auto parsed = decode_political_office_resignation_state(
      field(source, political_office_resignation_module, "state"));
  if (!parsed.ok)
    return {false, {}, parsed.error};
  result.political_office_resignations =
      std::make_shared<const PoliticalOfficeResignationState>(
          std::move(parsed.value));
} else if (source.find(political_office_resignation_module))
  fail("UnexpectedPoliticalOfficeResignationDomain",
       political_office_resignation_module);
if (catalog.political_subordinate_appointments) {
  auto parsed = decode_political_subordinate_appointment_state(
      field(source, political_subordinate_appointment_module, "state"));
  if (!parsed.ok)
    return {false, {}, parsed.error};
  result.political_subordinate_appointments =
      std::make_shared<const PoliticalSubordinateAppointmentState>(
          std::move(parsed.value));
} else if (source.find(political_subordinate_appointment_module))
  fail("UnexpectedPoliticalSubordinateAppointmentDomain",
       political_subordinate_appointment_module);
if (catalog.political_dismissal_sources) {
  auto parsed = decode_political_dismissal_sources(
      field(source, political_dismissal_sources_module, "state"));
  if (!parsed.ok)
    return {false, {}, parsed.error};
  result.political_dismissal_sources =
      std::make_shared<const PoliticalDismissalSourceState>(
          std::move(parsed.value));
} else if (source.find(political_dismissal_sources_module))
  fail("UnexpectedPoliticalDismissalSourceDomain",
       political_dismissal_sources_module);
if (catalog.political_governance_agencies) {
  auto parsed = decode_political_governance_agency_state(
      field(source, political_governance_agency_domain, "state"));
  if (!parsed.ok)
    return {false, {}, parsed.error};
  result.political_governance_agencies =
      std::make_shared<const PoliticalGovernanceAgencyState>(
          std::move(parsed.value));
} else if (source.find(political_governance_agency_domain))
  fail("UnexpectedPoliticalGovernanceAgencyDomain",
       political_governance_agency_domain);
