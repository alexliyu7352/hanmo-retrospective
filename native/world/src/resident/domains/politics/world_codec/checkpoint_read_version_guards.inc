// 政治世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto handover_version =
    political_head_handover_selected_version(catalog.identity.module_versions);
if (!handover_version.ok)
  return {false, {}, handover_version.error};
if ((handover_version.value == 1) != bool(catalog.political_head_handovers))
  return {
      false, {}, common_failure("PoliticalHeadHandoverProfileMismatch").error};
auto round_version = political_successor_round_selected_version(
    catalog.identity.module_versions);
if (!round_version.ok)
  return {false, {}, round_version.error};
if ((round_version.value == 1) != bool(catalog.political_successor_rounds))
  return {false,
          {},
          common_failure("PoliticalSuccessorRoundProfileMismatch").error};
auto term_version =
    political_term_handover_selected_version(catalog.identity.module_versions);
if (!term_version.ok)
  return {false, {}, term_version.error};
if ((term_version.value == 1) != bool(catalog.political_term_handovers))
  return {
      false, {}, common_failure("PoliticalTermHandoverProfileMismatch").error};
auto reception_version = political_office_reception_selected_version(
    catalog.identity.module_versions);
if (!reception_version.ok)
  return {false, {}, reception_version.error};
if ((reception_version.value == 1) != bool(catalog.political_office_receptions))
  return {false,
          {},
          common_failure("PoliticalOfficeReceptionProfileMismatch").error};
auto resignation_version = political_office_resignation_selected_version(
    catalog.identity.module_versions);
if (!resignation_version.ok)
  return {false, {}, resignation_version.error};
if ((resignation_version.value != 0) !=
        bool(catalog.political_office_resignations) ||
    (catalog.political_office_resignations &&
     resignation_version.value !=
         catalog.political_office_resignations->version))
  return {false,
          {},
          common_failure("PoliticalOfficeResignationProfileMismatch").error};
auto subordinate_version = political_subordinate_appointment_selected_version(
    catalog.identity.module_versions);
if (!subordinate_version.ok)
  return {false, {}, subordinate_version.error};
if ((subordinate_version.value != 0) !=
        bool(catalog.political_subordinate_appointments) ||
    (catalog.political_subordinate_appointments &&
     subordinate_version.value !=
         catalog.political_subordinate_appointments->version))
  return {
      false,
      {},
      common_failure("PoliticalSubordinateAppointmentProfileMismatch").error};
auto dismissal_version = political_dismissal_source_selected_version(
    catalog.identity.module_versions);
if (!dismissal_version.ok)
  return {false, {}, dismissal_version.error};
if ((dismissal_version.value != 0) !=
        bool(catalog.political_dismissal_sources) ||
    (catalog.political_dismissal_sources &&
     dismissal_version.value != catalog.political_dismissal_sources->version))
  return {false,
          {},
          common_failure("PoliticalDismissalSourceProfileMismatch").error};
auto agency_version = political_governance_agency_selected_version(
    catalog.identity.module_versions);
if (!agency_version.ok)
  return {false, {}, agency_version.error};
if ((agency_version.value != 0) !=
        bool(catalog.political_governance_agencies) ||
    (catalog.political_governance_agencies &&
     agency_version.value != catalog.political_governance_agencies->version))
  return {false,
          {},
          common_failure("PoliticalGovernanceAgencyProfileMismatch").error};
auto succession_version =
    political_succession_selected_version(catalog.identity.module_versions);
if (!succession_version.ok)
  return {false, {}, succession_version.error};
if ((succession_version.value == 1) != bool(catalog.political_successions))
  return {
      false, {}, common_failure("PoliticalSuccessionProfileMismatch").error};
