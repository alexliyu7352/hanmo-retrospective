// 政治世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
// 独立法律段保存载荷；共同JSON只允许kind/domain，额外载荷、孤头或缺实体均拒。
{
  CommonStore legal_common(result.common, catalog.common);
  std::set<StableId> registry_ids;
  for (const auto &row : object(field(source, "facts", "state"), "facts")) {
    const auto kind = get_string(row.second, "kind", row.first);
    const auto *named = row.second.find("domain");
    if (kind != political_shop_registry_accept_kind &&
        (!named || string(*named, row.first) !=
                       political_shop_registry_succession_module))
      continue;
    if (!catalog.political_shop_registry_successions ||
        !result.political_shop_registry_successions)
      fail("PoliticalShopRegistrySuccessionProfileRequired", row.first);
    fields(row.second, {"kind", "domain"}, {}, row.first);
    if (!registry_ids.insert(row.first).second)
      fail("PoliticalShopRegistryPayloadRequired", row.first);
    const auto *h = legal_common.state().facts.find(row.first);
    if (!h)
      fail("PoliticalShopRegistryPayloadRequired", row.first);
    auto checked = validate_political_shop_registry_fact_header(
        legal_common, *result.political_shop_registry_successions, *h);
    if (!checked.ok)
      return {false, {}, checked.error};
  }
  if (result.political_shop_registry_successions &&
      registry_ids.size() !=
          result.political_shop_registry_successions->acceptances.size())
    fail("PoliticalShopRegistryPayloadRequired",
         "political_shop_registry_successions");
  std::set<StableId> continued_ids;
  for (const auto &row : object(field(source, "facts", "state"), "facts")) {
    const auto kind = get_string(row.second, "kind", row.first);
    const auto *named = row.second.find("domain");
    if (kind != political_governance_continue_kind &&
        (!named ||
         string(*named, row.first) != political_governance_continuation_module))
      continue;
    if (!catalog.political_governance_continuations ||
        !result.political_governance_continuations)
      fail("GovernanceContinuationProfileRequired", row.first);
    fields(row.second, {"kind", "domain"}, {}, row.first);
    if (!continued_ids.insert(row.first).second)
      fail("GovernanceContinuationFactMismatch", row.first);
    const auto *h = legal_common.state().facts.find(row.first);
    if (!h)
      fail("GovernanceContinuationFactMismatch", row.first);
    auto checked = validate_political_governance_continuation_fact_header(
        legal_common, *result.political_governance_continuations, *h);
    if (!checked.ok)
      return {false, {}, checked.error};
  }
  if (result.political_governance_continuations &&
      continued_ids.size() !=
          result.political_governance_continuations->continuations.size())
    fail("GovernanceContinuationFactMismatch",
         "political_governance_continuations");
  std::set<StableId> invitation_ids;
  for (const auto &row : object(field(source, "facts", "state"), "facts")) {
    const auto kind = get_string(row.second, "kind", row.first);
    const auto *named = row.second.find("domain");
    if (kind != governance_invitation_reply_fact_kind &&
        (!named ||
         string(*named, row.first) != governance_invitation_reply_module))
      continue;
    if (!catalog.political_governance_invitation_replies ||
        !result.governance_invitation_replies)
      fail("GovernanceInvitationProfileRequired", row.first);
    fields(row.second, {"kind", "domain"}, {}, row.first);
    if (!invitation_ids.insert(row.first).second)
      fail("GovernanceInvitationFactMismatch", row.first);
    const auto *h = legal_common.state().facts.find(row.first);
    if (!h)
      fail("GovernanceInvitationFactMismatch", row.first);
    auto checked = validate_governance_invitation_reply_fact_header(
        legal_common, *result.governance_invitation_replies, *h);
    if (!checked.ok)
      return {false, {}, checked.error};
  }
  if (result.governance_invitation_replies &&
      invitation_ids.size() !=
          result.governance_invitation_replies->replies.size())
    fail("GovernanceInvitationFactMismatch",
         "political_governance_invitation_replies");
  std::set<StableId> handover_ids;
  for (const auto &row : object(field(source, "facts", "state"), "facts")) {
    const auto kind = get_string(row.second, "kind", row.first);
    const auto *named = row.second.find("domain");
    if (!is_political_head_handover_fact_kind(kind) &&
        (!named || string(*named, row.first) != political_head_handover_domain))
      continue;
    if (!catalog.political_head_handovers || !result.political_head_handovers)
      fail("PoliticalHeadHandoverProfileRequired", row.first);
    fields(row.second, {"kind", "domain"}, {}, row.first);
    if (!handover_ids.insert(row.first).second)
      fail("PoliticalHeadHandoverFactMismatch", row.first);
    const auto *h = legal_common.state().facts.find(row.first);
    if (!h)
      fail("PoliticalHeadHandoverFactMismatch", row.first);
    auto checked = validate_political_head_handover_fact_header(
        legal_common, *result.political_head_handovers, *h);
    if (!checked.ok)
      return {false, {}, checked.error};
  }
  if (result.political_head_handovers &&
      handover_ids.size() !=
          result.political_head_handovers->offers.size() +
              result.political_head_handovers->answers.size() +
              result.political_head_handovers->withdrawals.size())
    fail("PoliticalHeadHandoverFactMismatch", political_head_handover_domain);
  std::set<StableId> round_ids;
  for (const auto &row : object(field(source, "facts", "state"), "facts")) {
    const auto kind = get_string(row.second, "kind", row.first);
    const auto *named = row.second.find("domain");
    if (!is_political_successor_round_fact_kind(kind) &&
        (!named ||
         string(*named, row.first) != political_successor_rounds_domain))
      continue;
    if (!catalog.political_successor_rounds ||
        !result.political_successor_rounds)
      fail("PoliticalSuccessorRoundProfileRequired", row.first);
    fields(row.second, {"kind", "domain"}, {}, row.first);
    if (!round_ids.insert(row.first).second)
      fail("PoliticalSuccessorRoundFactMismatch", row.first);
    const auto *h = legal_common.state().facts.find(row.first);
    if (!h)
      fail("PoliticalSuccessorRoundFactMismatch", row.first);
    auto checked = validate_political_successor_round_fact_header(
        legal_common, *result.political_successor_rounds, *h);
    if (!checked.ok)
      return {false, {}, checked.error};
  }
  if (result.political_successor_rounds &&
      round_ids.size() !=
          result.political_successor_rounds->nominations.size() +
              result.political_successor_rounds->answers.size() +
              result.political_successor_rounds->withdrawals.size() +
              result.political_successor_rounds->accessions.size())
    fail("PoliticalSuccessorRoundFactMismatch",
         political_successor_rounds_domain);
  std::set<StableId> term_ids;
  for (const auto &row : object(field(source, "facts", "state"), "facts")) {
    const auto kind = get_string(row.second, "kind", row.first);
    const auto *named = row.second.find("domain");
    if (!is_political_term_handover_fact_kind(kind) &&
        (!named || string(*named, row.first) != political_term_handover_domain))
      continue;
    if (!catalog.political_term_handovers || !result.political_term_handovers)
      fail("PoliticalTermHandoverProfileRequired", row.first);
    fields(row.second, {"kind", "domain"}, {}, row.first);
    if (!term_ids.insert(row.first).second)
      fail("PoliticalTermHandoverFactMismatch", row.first);
    const auto *h = legal_common.state().facts.find(row.first);
    if (!h)
      fail("PoliticalTermHandoverFactMismatch", row.first);
    auto checked = validate_political_term_handover_fact_header(
        legal_common, *result.political_term_handovers, *h);
    if (!checked.ok)
      return {false, {}, checked.error};
  }
  if (result.political_term_handovers &&
      term_ids.size() !=
          result.political_term_handovers->offers.size() +
              result.political_term_handovers->answers.size() +
              result.political_term_handovers->withdrawals.size())
    fail("PoliticalTermHandoverFactMismatch", political_term_handover_domain);
  std::set<StableId> reception_ids;
  for (const auto &row : object(field(source, "facts", "state"), "facts")) {
    const auto kind = get_string(row.second, "kind", row.first);
    const auto *named = row.second.find("domain");
    if (!is_political_office_reception_fact_kind(kind) &&
        (!named ||
         string(*named, row.first) != political_office_reception_domain))
      continue;
    if (!catalog.political_office_receptions ||
        !result.political_office_receptions)
      fail("PoliticalOfficeReceptionProfileRequired", row.first);
    fields(row.second, {"kind", "domain"}, {}, row.first);
    if (!reception_ids.insert(row.first).second)
      fail("PoliticalOfficeReceptionFactMismatch", row.first);
    const auto *h = legal_common.state().facts.find(row.first);
    if (!h)
      fail("PoliticalOfficeReceptionFactMismatch", row.first);
    auto checked = validate_political_office_reception_fact_header(
        legal_common, *result.political_office_receptions, *h);
    if (!checked.ok)
      return {false, {}, checked.error};
  }
  if (result.political_office_receptions &&
      reception_ids.size() !=
          result.political_office_receptions->assignments.size())
    fail("PoliticalOfficeReceptionFactMismatch",
         political_office_reception_domain);
  std::set<StableId> resignation_ids;
  for (const auto &row : object(field(source, "facts", "state"), "facts")) {
    const auto kind = get_string(row.second, "kind", row.first);
    const auto *named = row.second.find("domain");
    if (!is_political_office_resignation_fact_kind(kind) &&
        (!named ||
         string(*named, row.first) != political_office_resignation_module))
      continue;
    if (!catalog.political_office_resignations ||
        !result.political_office_resignations)
      fail("PoliticalOfficeResignationProfileRequired", row.first);
    fields(row.second, {"kind", "domain"}, {}, row.first);
    if (!resignation_ids.insert(row.first).second)
      fail("PoliticalOfficeResignationFactMismatch", row.first);
    const auto *h = legal_common.state().facts.find(row.first);
    if (!h)
      fail("PoliticalOfficeResignationFactMismatch", row.first);
    auto checked = validate_political_office_resignation_fact_header(
        legal_common, *result.political_office_resignations, *h);
    if (!checked.ok)
      return {false, {}, checked.error};
  }
  if (result.political_office_resignations &&
      resignation_ids.size() !=
          (result.political_office_resignations->requests.size() +
           result.political_office_resignations->answers.size() +
           result.political_office_resignations->withdrawals.size()))
    fail("PoliticalOfficeResignationFactMismatch",
         political_office_resignation_module);
  std::set<StableId> subordinate_ids;
  for (const auto &row : object(field(source, "facts", "state"), "facts")) {
    const auto kind = get_string(row.second, "kind", row.first);
    const auto *named = row.second.find("domain");
    if (!is_political_subordinate_appointment_fact_kind(kind) &&
        (!named ||
         string(*named, row.first) != political_subordinate_appointment_module))
      continue;
    if (!catalog.political_subordinate_appointments ||
        !result.political_subordinate_appointments)
      fail("PoliticalSubordinateAppointmentProfileRequired", row.first);
    fields(row.second, {"kind", "domain"}, {}, row.first);
    if (!subordinate_ids.insert(row.first).second)
      fail("PoliticalSubordinateAppointmentFactMismatch", row.first);
    const auto *h = legal_common.state().facts.find(row.first);
    if (!h)
      fail("PoliticalSubordinateAppointmentFactMismatch", row.first);
    auto checked = validate_political_subordinate_appointment_fact_header(
        legal_common, *result.political_subordinate_appointments, *h);
    if (!checked.ok)
      return {false, {}, checked.error};
  }
  if (result.political_subordinate_appointments &&
      subordinate_ids.size() !=
          (result.political_subordinate_appointments->offers.size() +
           result.political_subordinate_appointments->answers.size() +
           result.political_subordinate_appointments->withdrawals.size() +
           result.political_subordinate_appointments->arrivals.size()))
    fail("PoliticalSubordinateAppointmentFactMismatch",
         political_subordinate_appointment_module);
  std::set<StableId> agency_ids;
  for (const auto &row : object(field(source, "facts", "state"), "facts")) {
    const auto kind = get_string(row.second, "kind", row.first);
    const auto *named = row.second.find("domain");
    if (!is_political_governance_agency_fact_kind(kind) &&
        (!named ||
         string(*named, row.first) != political_governance_agency_domain))
      continue;
    if (!catalog.political_governance_agencies ||
        !result.political_governance_agencies)
      fail("PoliticalGovernanceAgencyProfileRequired", row.first);
    fields(row.second, {"kind", "domain"}, {}, row.first);
    if (!agency_ids.insert(row.first).second)
      fail("PoliticalGovernanceAgencyFactMismatch", row.first);
    const auto *h = legal_common.state().facts.find(row.first);
    if (!h)
      fail("PoliticalGovernanceAgencyFactMismatch", row.first);
    auto checked = validate_political_governance_agency_fact_header(
        legal_common, *result.political_governance_agencies, *h);
    if (!checked.ok)
      return {false, {}, checked.error};
  }
  if (result.political_governance_agencies &&
      agency_ids.size() !=
          (result.political_governance_agencies->offers.size() +
           result.political_governance_agencies->answers.size() +
           result.political_governance_agencies->revocations.size() +
           result.political_governance_agencies->issues.size()))
    fail("PoliticalGovernanceAgencyFactMismatch",
         political_governance_agency_domain);
  std::set<StableId> succession_ids;
  for (const auto &row : object(field(source, "facts", "state"), "facts")) {
    const auto kind = get_string(row.second, "kind", row.first);
    const auto *named = row.second.find("domain");
    if (!is_political_succession_fact_kind(kind) &&
        (!named || string(*named, row.first) != political_succession_domain))
      continue;
    if (!catalog.political_successions || !result.political_successions)
      fail("PoliticalSuccessionProfileRequired", row.first);
    fields(row.second, {"kind", "domain"}, {}, row.first);
    if (!succession_ids.insert(row.first).second)
      fail("PoliticalSuccessionFactMismatch", row.first);
    const auto *h = legal_common.state().facts.find(row.first);
    if (!h)
      fail("PoliticalSuccessionFactMismatch", row.first);
    auto checked = validate_political_succession_fact_header(
        legal_common, *result.political_successions, *h);
    if (!checked.ok)
      return {false, {}, checked.error};
  }
  if (result.political_successions &&
      succession_ids.size() !=
          result.political_successions->nominations.nominations.size() +
              result.political_successions->nominations.answers.size() +
              result.political_successions->nominations.withdrawals.size() +
              result.political_successions->accessions.accessions.size())
    fail("PoliticalSuccessionFactMismatch", political_succession_domain);
  std::set<StableId> legal_ids;
  for (const auto &row : object(field(source, "facts", "state"), "facts")) {
    const auto kind = get_string(row.second, "kind", row.first);
    const auto *named = row.second.find("domain");
    const bool legal_kind = kind == "political_shop_registry_initialized" ||
                            kind == "political_wanted_registered" ||
                            kind == "political_wanted_released" ||
                            kind == "political_shop_clearance_recorded" ||
                            kind == "political_shop_permit_issued" ||
                            kind == "political_shop_permit_revoked";
    if (!legal_kind && (!named || string(*named, row.first) !=
                                      political_shop_licensing_module))
      continue;
    if (!catalog.political_shop_licensing || !result.political_shop_licensing)
      fail("PoliticalShopLicensingProfileRequired", row.first);
    fields(row.second, {"kind", "domain"}, {}, "facts/" + row.first);
    if (object(row.second, row.first).size() != 2 ||
        !legal_ids.insert(row.first).second)
      fail("PoliticalShopPayloadMismatch", row.first);
    const auto *header = legal_common.state().facts.find(row.first);
    if (!header)
      fail("PoliticalShopFactMismatch", row.first);
    auto checked = validate_political_shop_licensing_fact_header(
        legal_common, *result.political_shop_licensing, *header);
    if (!checked.ok)
      return {false, {}, checked.error};
  }
  if (result.political_shop_licensing) {
    const auto &state = *result.political_shop_licensing;
    if (legal_ids.size() != state.registries.size() + state.warrants.size() +
                                state.releases.size() +
                                state.clearances.size() + state.permits.size() +
                                state.revocations.size())
      fail("PoliticalShopPayloadMismatch", "political_shop_licensing");
  }
}
