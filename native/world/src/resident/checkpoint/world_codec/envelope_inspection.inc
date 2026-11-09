// 共同编解码世界编解码片段：保持原调用位置、字段与首错顺序；仅由
// world_codec.cpp 登记包含。
// 唯一域顺序/版本严格匹配后继续原身份门；相同段数不能伪装另一能力。
bool checkpoint_v2_bytes(const Bytes &bytes) {
  const StableId magic = "HM-RESIDENT-CHECKPOINT/2\n";
  return bytes.size() >= magic.size() &&
         std::equal(magic.begin(), magic.end(), bytes.begin());
}
Outcome<CheckpointEnvelope> inspect_known_checkpoint(const Bytes &bytes) {
  std::vector<CheckpointDomain> domains;
  CheckpointDomainSelector selector = [&](const CheckpointIdentity &identity) {
    auto selected = selected_checkpoint_domains(identity);
    if (selected.ok)
      domains = selected.value;
    return selected;
  };
  auto decoded =
      checkpoint_v2_bytes(bytes)
          ? inspect_checkpoint_v2_selected(
                bytes, selector, engineering_checkpoint_capacity_profile())
          : inspect_checkpoint_selected(bytes, selector);
  if (!decoded.ok)
    return decoded;
  const bool exact =
      std::any_of(decoded.value.identity.catalog.module_versions.begin(),
                  decoded.value.identity.catalog.module_versions.end(),
                  [](const auto &row) {
                    return row.first == "world_timeline" && row.second == "2";
                  });
  if (exact != (decoded.value.container_version == 2))
    return {false,
            {},
            common_failure("ExactCheckpointContainerVersionMismatch").error};
  const auto &identity = decoded.value.identity.catalog;
  const auto &versions = identity.module_versions;
  auto continuity_layout = validate_fiscal_taxpayer_continuity_layout(
      std::any_of(versions.begin(), versions.end(),
                  [](const auto &v) {
                    return v.first == fiscal_taxpayer_continuity_module;
                  }),
      versions);
  if (!continuity_layout.ok)
    return {false, {}, continuity_layout.error};
  auto fiscal_needs_layout = validate_occupation_fiscal_need_layout(
      std::any_of(versions.begin(), versions.end(),
                  [](const auto &v) {
                    return v.first == occupation_fiscal_need_module;
                  }),
      versions);
  if (!fiscal_needs_layout.ok)
    return {false, {}, fiscal_needs_layout.error};
  const auto has = [&](const char *name, const char *version) {
    return std::find(versions.begin(), versions.end(),
                     std::make_pair(StableId{name}, StableId{version})) !=
           versions.end();
  };
  auto common_domain =
      std::find_if(domains.begin(), domains.end(),
                   [](const auto &d) { return d.name == "common"; });
  if (common_domain == domains.end())
    return {false, {}, common_failure("CommonCheckpointDomainRequired").error};
  auto common_physical =
      decode_common_codec_selection(common_domain->codec_version);
  if (!common_physical.ok)
    return {false, {}, std::move(common_physical.error)};
  auto common_pair =
      validate_common_codec_selection(common_physical.value.base_version,
                                      identity, common_domain->codec_version);
  if (!common_pair.ok)
    return {false, {}, std::move(common_pair.error)};
  const auto version = [&](const char *name) -> Integer {
    const auto found =
        std::find_if(domains.begin(), domains.end(),
                     [&](const auto &domain) { return domain.name == name; });
    if (found == domains.end())
      return 0;
    return found->name == "common" ? common_physical.value.base_version
                                   : Integer(found->codec_version);
  };
  // 冷封套沿原有限选型拆基形，不把已登记的后继物理版本误当旧格式。
  auto organization_runtime = selected_native_organization_runtime(identity);
  if (!organization_runtime.ok)
    return {false, {}, std::move(organization_runtime.error)};
  Integer organization_physical = version("organization");
  bool organization_named = organization_physical == 3;
  if (organization_runtime.value) {
    auto original = select_organization_runtime_codec(identity);
    if (!original.ok)
      return {false, {}, std::move(original.error)};
    if (organization_physical != original.value.organization_version)
      return {false, {}, common_failure("CheckpointProfileMismatch").error};
    organization_named = original.value.named_membership;
  }
  auto economy_selection =
      selected_native_economic_obligation_versions(identity);
  if (!economy_selection.ok)
    return {false, {}, std::move(economy_selection.error)};
  const Integer economy_physical = economy_selection.value.local_economy;
  if (version("local_economy") != economy_physical)
    return {false, {}, common_failure("CheckpointProfileMismatch").error};
  const Integer economy_base =
      economy_physical -
      (economy_selection.value.named_birth_population ? 10 : 0) -
      (economy_selection.value.sources ? 5 : 0);
  auto interaction_response =
      selected_native_interaction_response_version(identity);
  if (!interaction_response.ok)
    return {false, {}, std::move(interaction_response.error)};
  Integer interaction_base = version("interaction");
  if (interaction_response_version(interaction_base)) {
    if (interaction_response.value != interaction_base)
      return {false,
              {},
              common_failure("InteractionResponseSelectionMismatch").error};
    auto selected = interaction_response_base_version(interaction_base);
    if (!selected.ok)
      return {false, {}, std::move(selected.error)};
    interaction_base = selected.value;
  } else if (interaction_response.value)
    return {false,
            {},
            common_failure("InteractionResponseSelectionMismatch").error};
  auto selected_politics =
      read_political_locked_body_version(identity, "politics");
  auto selected_fiscal =
      read_political_locked_body_version(identity, "political_fiscal");
  auto selected_takeover =
      read_political_locked_body_version(identity, "political_takeover");
  if (!selected_politics.ok)
    return {false, {}, std::move(selected_politics.error)};
  if (!selected_fiscal.ok)
    return {false, {}, std::move(selected_fiscal.error)};
  if (!selected_takeover.ok)
    return {false, {}, std::move(selected_takeover.error)};
  if (version("politics") != selected_politics.value ||
      version("political_fiscal") != selected_fiscal.value ||
      version("political_takeover") != selected_takeover.value)
    return {false,
            {},
            common_failure("PoliticalLockedBodySelectionMismatch").error};
  const bool takeover = selected_takeover.value != 0;
  const auto task_execution_version = version("task_takeover_execution");
  if (takeover && task_execution_version != 1 && task_execution_version != 2)
    return {
        false, {}, common_failure("PoliticalTakeoverProfileMismatch").error};
  for (const auto *name : {"political_takeover", "economic_takeover_execution",
                           "task_takeover_execution"}) {
    const auto expected = std::string_view{name} == "task_takeover_execution"
                              ? task_execution_version
                          : std::string_view{name} == "political_takeover"
                              ? selected_takeover.value
                              : 1;
    const auto count =
        std::count_if(versions.begin(), versions.end(),
                      [&](const auto &v) { return v.first == name; });
    if (count != (takeover ? 1 : 0) ||
        version(name) != (takeover ? expected : 0) ||
        (takeover && !has(name, expected == 2 ? "2" : "1")))
      return {
          false, {}, common_failure("PoliticalTakeoverProfileMismatch").error};
  }
  if (takeover &&
      (version("common") < 5 || version("politics") != selected_takeover.value))
    return {
        false, {}, common_failure("PoliticalTakeoverProfileMismatch").error};
  const bool occupation_fiscal = version("political_occupation_fiscal") == 1;
  for (const auto *name :
       {"political_occupation_fiscal", "occupation_fiscal_settlement"}) {
    const auto count =
        std::count_if(versions.begin(), versions.end(),
                      [&](const auto &v) { return v.first == name; });
    if (count != (occupation_fiscal ? 1 : 0) ||
        version(name) != (occupation_fiscal ? 1 : 0) ||
        (occupation_fiscal && !has(name, "1")))
      return {
          false, {}, common_failure("OccupationFiscalProfileMismatch").error};
  }
  if (occupation_fiscal && (!takeover || version("fiscal_settlement") != 1 ||
                            version("political_fiscal") != 1))
    return {false, {}, common_failure("OccupationFiscalProfileMismatch").error};
  const auto naval_markers =
      std::count_if(versions.begin(), versions.end(),
                    [](const auto &v) { return v.first == "military_naval"; });
  const auto naval_version = version("military_naval");
  if (has("naval_gunnery", "1") != (naval_version == 6))
    return {false, {}, common_failure("CheckpointProfileMismatch").error};
  const bool naval = naval_version == 1 || naval_version == 2 ||
                     naval_version == 3 || naval_version == 4 ||
                     (naval_version == 5 || naval_version == 6);
  if (has("naval_tactical_navigation", "1") && naval_version != 2 &&
      naval_version != 3 && naval_version != 4 && naval_version != 5 &&
      naval_version != 6)
    return {false, {}, common_failure("CheckpointProfileMismatch").error};
  if (has("naval_navigation_wear", "1") !=
      ((naval_version == 5 || naval_version == 6)))
    return {false, {}, common_failure("CheckpointProfileMismatch").error};
  if (has("naval_repair_charges", "1") !=
      (naval_version == 4 || (naval_version == 5 || naval_version == 6)))
    return {false, {}, common_failure("CheckpointProfileMismatch").error};
  if (has("naval_repairs", "1") != (naval_version == 3 || naval_version == 4 ||
                                    (naval_version == 5 || naval_version == 6)))
    return {false, {}, common_failure("CheckpointProfileMismatch").error};
  if (naval_markers != (naval ? 1 : 0) ||
      (naval && (!has("military_naval",
                      naval_version == 6                           ? "6"
                      : (naval_version == 5 || naval_version == 6) ? "5"
                      : naval_version == 4                         ? "4"
                      : naval_version == 3                         ? "3"
                      : naval_version == 2                         ? "2"
                                                                   : "1") ||
                 version("common") < 8 || version("military") < 4)))
    return {false, {}, common_failure("CheckpointProfileMismatch").error};
  const auto request_markers =
      std::count_if(versions.begin(), versions.end(), [](const auto &value) {
        return value.first == "military_requests";
      });
  const bool requests = has("military_requests", "1");
  auto occupation_schema = selected_military_occupation_schema(identity);
  if (!occupation_schema.ok)
    return {false, {}, std::move(occupation_schema.error)};
  auto military_base = military_checkpoint_base_version(
      version("military"), occupation_schema.value);
  if (!military_base.ok)
    return {false, {}, std::move(military_base.error)};
  auto military_layout = validate_military_siege_engineering_checkpoint_layout(
      military_base.value, versions);
  if (!military_layout.ok)
    return {false, {}, military_layout.error};
  if (request_markers != (requests ? 1 : 0) ||
      (requests && (!has("military_requests", "1") || version("common") < 5)))
    return {false, {}, common_failure("CheckpointProfileMismatch").error};
  const auto base_growth_version =
      has("world_timeline", "2")
          ? version("growth") -
                (has("actor_start_context", "3")
                     ? (has("growth_fixed_service_time2", "4")   ? 192
                        : has("growth_fixed_service_time2", "3") ? 128
                        : has("growth_fixed_service_time2", "2") ? 96
                                                                 : 64)
                     : 32)
          : version("growth");
  const bool study_teaching =
      base_growth_version >= 19 && base_growth_version <= 34;
  const auto without_study_teaching =
      study_teaching ? base_growth_version - 16 : base_growth_version;
  const bool teaching =
      without_study_teaching >= 11 && without_study_teaching <= 18;
  const auto without_teaching =
      teaching ? without_study_teaching - 8 : without_study_teaching;
  const bool cards = without_teaching >= 7 && without_teaching <= 10;
  const auto growth_version = cards ? without_teaching - 4 : without_teaching;
  const bool dynamic = version("common") >= 5,
             population = version("population_military") != 0,
             regional = growth_version == 4 || growth_version == 6,
             siege = version("siege_integration") != 0,
             named = organization_named,
             segmented = growth_version == 5 || growth_version == 6,
             registry = version("director") == 4 || version("director") == 5 ||
                        version("director") == 6,
             cohort_wages = version("affairs") == 4,
             fiscal_needs = version("affairs") == 3 ||
                            (cohort_wages && has("fiscal_need_policies", "1")),
             obligations =
                 version("affairs") == 2 || ((fiscal_needs || cohort_wages) &&
                                             has("obligation_needs", "1"));
  if (has("task_message_handoffs", "1") !=
          (version("task_message_handoffs") == 1) ||
      has("task_message_handoffs", "2") !=
          (version("task_message_handoffs") == 2) ||
      has("task_message_handoffs", "3") !=
          (version("task_message_handoffs") == 3) ||
      has("task_message_handoffs", "4") !=
          (version("task_message_handoffs") == 4) ||
      (!dynamic && version("task_message_handoffs")))
    return {
        false, {}, common_failure("TaskMessageHandoffProfileMismatch").error};
  if (has("family_life", "1") != (version("family_life") == 1) ||
      has("family_life", "4") != (version("family_life") == 4) ||
      (!dynamic && version("family_life")))
    return {false, {}, common_failure("CheckpointProfileMismatch").error};
  if ((has("family_legacy", "1") || has("family_legacy", "2") ||
       has("family_legacy", "3")) !=
          (version("family_legacy") == 1 || version("family_legacy") == 2 ||
           version("family_legacy") == 3) ||
      (version("family_legacy") &&
       (!dynamic || !(has("family_life", "1") || has("family_life", "4")))))
    return {false, {}, common_failure("CheckpointProfileMismatch").error};
  if (has("family_collateral_legacy", "1") !=
          (version("family_collateral_legacy") == 1) ||
      (version("family_collateral_legacy") &&
       (!dynamic || !has("family_legacy", "3") ||
        !has("known_personal_dialogues", "1") || !has("family_lineage", "1"))))
    return {
        false, {}, common_failure("CollateralControlProfileMismatch").error};
  {
    auto selected = task_message_return_selected_version(versions);
    if (!selected.ok)
      return {false, {}, selected.error};
    if (selected.value != version(task_message_return_module) ||
        (selected.value && !dynamic))
      return {
          false, {}, common_failure("TaskMessageReturnProfileMismatch").error};
  }
  if (has("family_bequest", "1") != (version("family_bequest") == 1) ||
      (version("family_bequest") &&
       (!dynamic || !(has("family_life", "1") || has("family_life", "4")))))
    return {false, {}, common_failure("CheckpointProfileMismatch").error};
  if (has("family_inheritance", "1") != (version("family_inheritance") == 1) ||
      (version("family_inheritance") &&
       (!dynamic || !has("family_bequest", "1") ||
        !has("task_message_notices", "1"))))
    return {
        false, {}, common_failure("FamilyInheritanceProfileMismatch").error};
  if (has("family_marriage", "1") != (version("family_marriage") == 1) ||
      (version("family_marriage") &&
       (!dynamic || !(has("family_life", "1") || has("family_life", "4")))))
    return {false, {}, common_failure("FamilyMarriageProfileMismatch").error};
  auto supply_layout = validate_economic_supply_layout(
      has("economic_supply_policies", "1"), versions);
  if (!supply_layout.ok)
    return {false, {}, supply_layout.error};
  if (has("economic_supply_policies", "1") !=
          (version("economic_supply_policies") == 1) ||
      has("political_supply_adoptions", "1") !=
          (version("political_supply_adoptions") == 1) ||
      version("economic_supply_policies") > 1 ||
      version("political_supply_adoptions") > 1)
    return {
        false, {}, common_failure("EconomicSupplyPolicyProfileMismatch").error};
  if (has("family_care_relief", "1") != (version("family_care_relief") == 1) ||
      (version("family_care_relief") &&
       (!dynamic || !has("family_birth", "1") || !has("family_lineage", "1") ||
        !(has("family_life", "1") || has("family_life", "4")))))
    return {false, {}, common_failure("ChildCareReliefProfileMismatch").error};
  if (has("task_extensions", "1") != (version("task_extensions") == 1) ||
      (!dynamic && version("task_extensions")))
    return {false, {}, common_failure("TaskExtensionProfileMismatch").error};
  auto interaction_selected = selected_native_interaction_versions(identity);
  if (!interaction_selected.ok)
    return {false, {}, std::move(interaction_selected.error)};
  const auto &selected_interaction = interaction_selected.value;
  if (version("visit_messages") != selected_interaction.messages ||
      version("personal_appointments") != selected_interaction.personal ||
      version("contact_inquiries") != selected_interaction.inquiry ||
      version("visit_appointment_invitations") !=
          selected_interaction.invitations ||
      version("visit_appointment_meetings") != selected_interaction.meetings ||
      (!dynamic &&
       (selected_interaction.messages || selected_interaction.personal ||
        selected_interaction.inquiry || selected_interaction.invitations)))
    return {false,
            {},
            common_failure("InteractionCheckpointSelectionMismatch").error};
  if (selected_interaction.replies != version("visit_message_replies") ||
      (selected_interaction.replies &&
       (!dynamic || !selected_interaction.messages)) ||
      has("visit_reply_intents", "1") !=
          visit_reply_authored_version(selected_interaction.replies) ||
      (has("visit_reply_intents", "1") &&
       (!has("visit_reception_intents", "1") ||
        !has("visit_reception_claims", "1"))))
    return {false, {}, common_failure("VisitReplyProfileMismatch").error};
  if (has("born_personal_dialogues", "1") !=
          (version("born_personal_dialogues") == 1) ||
      (version("born_personal_dialogues") &&
       (!dynamic || !has("family_birth", "1") || !has("family_lineage", "1") ||
        !(has("family_life", "1") || has("family_life", "4")))))
    return {
        false, {}, common_failure("BornPersonalDialogueProfileMismatch").error};
  auto handover_version = political_head_handover_selected_version(versions);
  if (!handover_version.ok)
    return {false, {}, handover_version.error};
  if (handover_version.value != version(political_head_handover_domain))
    return {false,
            {},
            common_failure("PoliticalHeadHandoverProfileMismatch").error};
  auto round_version = political_successor_round_selected_version(versions);
  if (!round_version.ok)
    return {false, {}, round_version.error};
  if (round_version.value != version(political_successor_rounds_domain))
    return {false,
            {},
            common_failure("PoliticalSuccessorRoundProfileMismatch").error};
  auto term_version = political_term_handover_selected_version(versions);
  if (!term_version.ok)
    return {false, {}, term_version.error};
  if (term_version.value != version(political_term_handover_domain))
    return {false,
            {},
            common_failure("PoliticalTermHandoverProfileMismatch").error};
  auto message_receipts = visit_message_receipt_selected_version(versions);
  if (!message_receipts.ok)
    return {false, {}, message_receipts.error};
  if (message_receipts.value != version(visit_message_receipt_module) ||
      (message_receipts.value && !dynamic))
    return {
        false, {}, common_failure("VisitMessageReceiptProfileMismatch").error};
  auto reception_version =
      political_office_reception_selected_version(versions);
  if (!reception_version.ok)
    return {false, {}, reception_version.error};
  if (reception_version.value != version(political_office_reception_domain))
    return {false,
            {},
            common_failure("PoliticalOfficeReceptionProfileMismatch").error};
  auto resignation_version =
      political_office_resignation_selected_version(versions);
  if (!resignation_version.ok)
    return {false, {}, resignation_version.error};
  if (resignation_version.value != version(political_office_resignation_module))
    return {false,
            {},
            common_failure("PoliticalOfficeResignationProfileMismatch").error};
  auto subordinate_version =
      political_subordinate_appointment_selected_version(versions);
  if (!subordinate_version.ok)
    return {false, {}, subordinate_version.error};
  if (subordinate_version.value !=
      version(political_subordinate_appointment_module))
    return {
        false,
        {},
        common_failure("PoliticalSubordinateAppointmentProfileMismatch").error};
  auto dismissal_version =
      political_dismissal_source_selected_version(versions);
  if (!dismissal_version.ok)
    return {false, {}, dismissal_version.error};
  if (dismissal_version.value != version(political_dismissal_sources_module))
    return {false,
            {},
            common_failure("PoliticalDismissalSourceProfileMismatch").error};
  auto agency_version = political_governance_agency_selected_version(versions);
  if (!agency_version.ok)
    return {false, {}, agency_version.error};
  if (agency_version.value != version(political_governance_agency_domain))
    return {false,
            {},
            common_failure("PoliticalGovernanceAgencyProfileMismatch").error};
  auto succession_version = political_succession_selected_version(versions);
  if (!succession_version.ok)
    return {false, {}, succession_version.error};
  if (succession_version.value != version(political_succession_domain))
    return {
        false, {}, common_failure("PoliticalSuccessionProfileMismatch").error};
  if (has("visit_appointment_intents", "1") &&
      (!selected_interaction.invitations ||
       !has("visit_reception_intents", "1") ||
       !has("visit_reception_claims", "1")))
    return {false, {}, common_failure("VisitAppointmentProfileMismatch").error};
  if (has("personal_appointment_attendance", "1") &&
      (!dynamic || !selected_interaction.personal))
    return {false,
            {},
            common_failure("AppointmentAttendanceProfileMismatch").error};
  {
    auto attendance_layout = validate_visit_appointment_attendance_layout(
        versions, dynamic, version("visit_appointment_attendance") != 0);
    if (!attendance_layout.ok)
      return {false, {}, attendance_layout.error};
  }
  if (has("growth_tactic_lessons", "1") !=
          (version("growth_tactic_lessons") == 1) ||
      (version("growth_tactic_lessons") &&
       (!dynamic || !(has("family_life", "1") || has("family_life", "4")) ||
        !has("military_tactic_learning", "1"))))
    return {
        false, {}, common_failure("GrowthTacticLessonProfileMismatch").error};
  {
    auto selected = known_personal_dialogue_selected(versions);
    if (!selected.ok)
      return {false, {}, selected.error};
    if (selected.value != (version("known_personal_dialogues") == 1) ||
        (version("known_personal_dialogues") && !dynamic))
      return {false,
              {},
              common_failure("KnownPersonalDialogueProfileMismatch").error};
  }
  {
    auto selected = task_shop_order_contact_selected_version(versions);
    if (!selected.ok)
      return {false, {}, selected.error};
    if (selected.value != version(task_shop_order_contact_module) ||
        (selected.value && !dynamic))
      return {false,
              {},
              common_failure("TaskShopOrderContactProfileMismatch").error};
  }
  if (has("personal_injury_recovery", "1") !=
          (version("personal_injury_recovery") == 1) ||
      (version("personal_injury_recovery") &&
       (!dynamic || !regional ||
        !(has("family_life", "1") || has("family_life", "4")))))
    return {false,
            {},
            common_failure("PersonalInjuryRecoveryProfileMismatch").error};
  if ((has("family_birth", "1") || has("family_birth", "4")) !=
          has("family_lineage", "1") ||
      has("family_birth", "1") != (version("family_birth") == 1) ||
      has("family_birth", "4") != (version("family_birth") == 4) ||
      (version("family_birth") &&
       (!dynamic || !(has("family_life", "1") || has("family_life", "4")) ||
        !has("skills_extended", "1"))) ||
      (has("family_birth", "4") &&
       (!has("actor_start_context", "3") || !has("family_life", "4"))))
    return {false, {}, common_failure("FamilyBirthProfileMismatch").error};
  if (has("visit_reception_intents", "1") !=
          has("visit_reception_claims", "1") ||
      has("visit_reception_claims", "1") !=
          (version("visit_reception_claims") == 1) ||
      (has("visit_reception_intents", "1") &&
       (!dynamic || !selected_interaction.messages)))
    return {false, {}, common_failure("VisitReceptionProfileMismatch").error};
  auto employment_layout = validate_estate_employment_layout(
      has("estate_employment", "1"), versions);
  if (!employment_layout.ok)
    return {false, {}, employment_layout.error};
  if (has("estate_employment", "1") != (version("estate_employment") == 1) ||
      version("estate_employment") > 1)
    return {false, {}, common_failure("EstateEmploymentProfileMismatch").error};
  if (has("task_message_notices", "1") !=
          (version("task_message_notices") == 1) ||
      (version("task_message_notices") &&
       (!dynamic || !(has("family_life", "1") || has("family_life", "4")) ||
        !version("task_message_handoffs"))))
    return {
        false, {}, common_failure("TaskMessageNoticeProfileMismatch").error};
  if (has("task_static_assets", "1") != (version("task_static_assets") == 1) ||
      (!dynamic && version("task_static_assets")))
    return {false, {}, common_failure("TaskStaticAssetProfileMismatch").error};
  const auto disruption_markers =
      std::count_if(versions.begin(), versions.end(), [](const auto &marker) {
        return marker.first == "production_disruptions";
      });
  if (disruption_markers != (version("production_disruptions") == 1 ? 1 : 0) ||
      (disruption_markers && (!dynamic || !has("production_disruptions", "1"))))
    return {false, {}, common_failure("CheckpointProfileMismatch").error};
  const auto actual_family = version("family");
  auto family_shape = inspect_family_codec_version(actual_family);
  if (!family_shape.ok)
    return {false, {}, family_shape.error};
  auto family_pair = family_modules_for(actual_family, versions);
  if (!family_pair.ok)
    return {false, {}, family_pair.error};
  const bool voluntary = family_shape.value.voluntary_succession;
  const bool adult_adoption = family_shape.value.adult_adoption;
  const auto family_version = 1 +
                              (family_shape.value.household_control ? 1 : 0) +
                              (family_shape.value.character_creation ? 2 : 0);
  if (family_shape.value.voluntary_paths !=
          (version("family_voluntary_paths") == 1) ||
      (version("family_voluntary_paths") &&
       version("family_voluntary_paths") != 1))
    return {
        false, {}, common_failure("FamilyVoluntaryCodecModuleMismatch").error};
  {
    auto selected = task_personal_message_selected_version(versions);
    if (!selected.ok)
      return {false, {}, selected.error};
    if (selected.value != version(task_personal_message_module) ||
        (selected.value && !dynamic))
      return {false,
              {},
              common_failure("TaskPersonalMessageProfileMismatch").error};
  }
  {
    auto selected = task_personal_report_selected_version(versions);
    if (!selected.ok)
      return {false, {}, selected.error};
    if (selected.value != version(task_personal_report_module) ||
        (selected.value && !dynamic))
      return {
          false, {}, common_failure("TaskPersonalReportProfileMismatch").error};
  }
  {
    auto selected = task_personal_acceptance_selected_version(versions);
    if (!selected.ok)
      return {false, {}, selected.error};
    if (selected.value != version(task_personal_acceptance_module) ||
        (selected.value && !dynamic))
      return {false,
              {},
              common_failure("TaskPersonalAcceptanceProfileMismatch").error};
  }
  {
    auto selected = task_personal_cancellation_selected_version(versions);
    if (!selected.ok)
      return {false, {}, selected.error};
    if (selected.value != version(task_personal_cancellation_module) ||
        (selected.value && !dynamic))
      return {false,
              {},
              common_failure("TaskPersonalCancellationProfileMismatch").error};
  }
  const auto voluntary_markers =
      std::count_if(versions.begin(), versions.end(), [](const auto &value) {
        return value.first == "voluntary_succession";
      });
  if (voluntary_markers != (voluntary ? 1 : 0) ||
      (voluntary &&
       (!dynamic || !has("voluntary_succession",
                         family_shape.value.voluntary_paths ? "2" : "1"))))
    return {false, {}, common_failure("CheckpointProfileMismatch").error};
  const auto adult_markers =
      std::count_if(versions.begin(), versions.end(), [](const auto &value) {
        return value.first == "adult_adoption";
      });
  if (family_version < 1 || family_version > 4 ||
      adult_markers != (adult_adoption ? 1 : 0) ||
      (adult_adoption && (!dynamic || !has("adult_adoption", "1"))))
    return {false, {}, common_failure("CheckpointProfileMismatch").error};
  const bool household_management =
      version("household_provision_management") == 1;
  const auto management_markers =
      std::count_if(versions.begin(), versions.end(), [](const auto &value) {
        return value.first == "household_provision_management";
      });
  if (management_markers != (household_management ? 1 : 0) ||
      (household_management &&
       (!has("household_provision_management", "1") ||
        (family_version != 2 && family_version != 4) || economy_base != 5 ||
        !has("household_provisioning", "1"))))
    return {false, {}, common_failure("CheckpointProfileMismatch").error};
  const bool household_control = family_version == 2 || family_version == 4;
  const auto control_markers =
      std::count_if(versions.begin(), versions.end(), [](const auto &value) {
        return value.first == "household_control";
      });
  if (control_markers != (household_control ? 1 : 0) ||
      (household_control && (!dynamic || !has("household_control", "1"))))
    return {false, {}, common_failure("CheckpointProfileMismatch").error};
  const bool created = family_version == 3 || family_version == 4;
  const auto creation_markers =
      std::count_if(versions.begin(), versions.end(), [](const auto &value) {
        return value.first == "character_creation";
      });
  if (creation_markers != (created ? 1 : 0) ||
      (created && (!dynamic || !has("character_creation", "1"))))
    return {false, {}, common_failure("CheckpointProfileMismatch").error};
  const auto study_teaching_markers =
      std::count_if(versions.begin(), versions.end(), [](const auto &value) {
        return value.first == "study_teaching_requests";
      });
  if (study_teaching_markers != (study_teaching ? 1 : 0) ||
      (study_teaching && (!dynamic || !has("study_teaching_requests", "1"))))
    return {false, {}, common_failure("CheckpointProfileMismatch").error};
  const auto teaching_markers =
      std::count_if(versions.begin(), versions.end(), [](const auto &value) {
        return value.first == "teaching_requests";
      });
  if (teaching_markers != (teaching ? 1 : 0) ||
      (teaching && (!dynamic || !has("teaching_requests", "1"))))
    return {false, {}, common_failure("CheckpointProfileMismatch").error};
  const auto card_markers =
      std::count_if(versions.begin(), versions.end(), [](const auto &value) {
        return value.first == "growth_cards";
      });
  if (card_markers != (cards ? 1 : 0) ||
      (cards && (!dynamic || !has("growth_cards", "1"))))
    return {false, {}, common_failure("CheckpointProfileMismatch").error};
  const auto insight_markers =
      std::count_if(versions.begin(), versions.end(), [](const auto &value) {
        return value.first == "growth_insights";
      });
  if (insight_markers > 1 ||
      (insight_markers && (!dynamic || !has("growth_insights", "1"))))
    return {false, {}, common_failure("CheckpointProfileMismatch").error};
  const auto party_markers =
      std::count_if(versions.begin(), versions.end(), [](const auto &value) {
        return value.first == "party_invitations";
      });
  if (party_markers > 1 ||
      (party_markers && (!dynamic || !has("party_invitations", "1"))))
    return {false, {}, common_failure("CheckpointProfileMismatch").error};
  const auto recruitment_markers =
      std::count_if(versions.begin(), versions.end(), [](const auto &value) {
        return value.first == "party_recruitment_consents";
      });
  if (recruitment_markers > 1 ||
      (recruitment_markers &&
       (!dynamic || !has("party_recruitment_consents", "1"))))
    return {false, {}, common_failure("PartyRecruitmentProfileMismatch").error};
  const auto actor_markers =
      std::count_if(versions.begin(), versions.end(), [](const auto &value) {
        return value.first == "selected_existing_actor";
      });
  if ((created && actor_markers) || actor_markers > 1 ||
      (actor_markers == 1 &&
       (!dynamic || (!has("selected_existing_actor", "1") &&
                     !has("selected_existing_actor", "2")))))
    return {false, {}, common_failure("CheckpointProfileMismatch").error};
  const auto unlock_markers =
      std::count_if(versions.begin(), versions.end(), [](const auto &value) {
        return value.first == "actor_card_unlocks";
      });
  if (unlock_markers > 1 ||
      (unlock_markers &&
       (!dynamic || actor_markers != 1 ||
        (!has("actor_card_unlocks", "1") && !has("actor_card_unlocks", "2")))))
    return {false, {}, common_failure("CheckpointProfileMismatch").error};
  for (const auto *module : {"politics", "political_fiscal"}) {
    const auto count =
        std::count_if(versions.begin(), versions.end(),
                      [&](const auto &v) { return v.first == module; });
    const auto expected = std::string_view{module} == "politics"
                              ? selected_politics.value
                              : selected_fiscal.value;
    if (count != (expected ? 1 : 0) ||
        (count && (!dynamic || !has(module, expected == 2 ? "2" : "1"))))
      return {false, {}, common_failure("CheckpointProfileMismatch").error};
  }
  const auto task_response_version = version("task_offer_responses");
  const auto task_response_count =
      std::count_if(versions.begin(), versions.end(), [](const auto &v) {
        return v.first == "task_offer_responses";
      });
  if (task_response_count != (task_response_version ? 1 : 0) ||
      (task_response_version &&
       (!dynamic || task_response_version > 2 ||
        !has("task_offer_responses",
             std::to_string(task_response_version).c_str()))))
    return {false, {}, common_failure("CheckpointProfileMismatch").error};
  if (selected_fiscal.value && selected_fiscal.value != selected_politics.value)
    return {false, {}, common_failure("CheckpointProfileMismatch").error};
  const bool treaty = version("treaty_passage") != 0;
  TreatyPassageState treaty_state;
  auto treaty_layout = validate_treaty_passage_layout(
      treaty, versions, treaty ? &treaty_state : nullptr);
  if (!treaty_layout.ok || (treaty != has("treaty_passage", "1")))
    return {false, {}, common_failure("TreatyPassageProfileMismatch").error};
  PoliticalMilitaryAppointmentState officer_state;
  auto officer_layout = validate_political_military_appointment_layout(
      has(military_officer_political_module, "1"), versions,
      version(military_officer_political_module) == 1 ? &officer_state
                                                      : nullptr);
  if (!officer_layout.ok)
    return {false, {}, officer_layout.error};
  PoliticalFieldAppointmentState field_state;
  auto field_layout = validate_political_field_appointment_layout(
      has(military_field_command_political_module, "1"), versions,
      version(military_field_command_political_module) == 1 ? &field_state
                                                            : nullptr);
  if (!field_layout.ok)
    return {false, {}, field_layout.error};
  const bool diplomatic = version("diplomacy") != 0;
  auto diplomatic_layout = validate_diplomacy_layout(
      diplomatic, versions, version("diplomacy") != 0,
      version("envoy_tasks") != 0, diplomatic ? version("diplomacy") : 1);
  if (!diplomatic_layout.ok || (diplomatic && !dynamic))
    return {false, {}, common_failure("CheckpointProfileMismatch").error};
  const auto response_markers =
      std::count_if(versions.begin(), versions.end(), [](const auto &value) {
        return value.first == "diplomatic_response";
      });
  if (response_markers > 1 ||
      (response_markers && (!diplomatic || !has("diplomatic_response", "1") ||
                            !has("director_registry", "1"))))
    return {false, {}, common_failure("CheckpointProfileMismatch").error};
  const auto continuation_markers =
      std::count_if(versions.begin(), versions.end(), [](const auto &value) {
        return value.first == "authored_task_continuation";
      });
  if (continuation_markers > 1 ||
      (continuation_markers &&
       (!dynamic || !(has("authored_task_continuation", "1") ||
                      has("authored_task_continuation", "2")))))
    return {false, {}, common_failure("TaskContinuationProfileMismatch").error};
  auto renewal_layout = validate_political_supply_renewal_modules(
      has("political_supply_renewals", "1"), versions);
  if (!renewal_layout.ok)
    return {false, {}, renewal_layout.error};
  auto registry_intents_layout =
      validate_political_shop_registry_intent_modules(
          has("political_shop_registry_intents", "1"), versions);
  if (!registry_intents_layout.ok)
    return {false, {}, registry_intents_layout.error};
  auto governance_intents_layout = validate_political_governance_intent_modules(
      has("political_governance_intents", "1"), versions);
  if (!governance_intents_layout.ok)
    return {false, {}, governance_intents_layout.error};
  const bool governance = version("politics_governance") != 0;
  for (const auto *module :
       {"politics_governance", "governance_tasks", "public_relief"}) {
    const auto count =
        std::count_if(versions.begin(), versions.end(),
                      [&](const auto &v) { return v.first == module; });
    if (count != (governance ? 1 : 0) ||
        version(module) != (governance ? 1 : 0) ||
        (governance &&
         (!dynamic || version("politics") != 1 || !has(module, "1"))))
      return {false, {}, common_failure("CheckpointProfileMismatch").error};
  }
  const bool management = has("production_management", "1"),
             household = has("household_provisioning", "1");
  for (const auto *module : {"production_management", "household_provisioning",
                             "fiscal_settlement"}) {
    const auto count =
        std::count_if(versions.begin(), versions.end(),
                      [&](const auto &v) { return v.first == module; });
    if (count > 1 || (count && (!dynamic || !has(module, "1"))) ||
        (StableId(module) == "fiscal_settlement" &&
         count != (version("fiscal_settlement") == 1 ? 1 : 0)))
      return {false, {}, common_failure("CheckpointProfileMismatch").error};
  }
  const Integer economy_version = household    ? 5
                                  : management ? 4
                                  : named      ? 3
                                  : population ? 2
                                               : 1;
  if ((dynamic &&
       (economy_base != economy_version ||
        !has("local_economy", std::to_string(economy_physical).c_str()))) ||
      (version("fiscal_settlement") && !version("political_fiscal")))
    return {false, {}, common_failure("CheckpointProfileMismatch").error};

  const bool extended_skills = version("common") >= 8;
  const auto skill_markers =
      std::count_if(versions.begin(), versions.end(), [](const auto &value) {
        return value.first == "skills_extended";
      });
  if (skill_markers != (extended_skills ? 1 : 0) ||
      (extended_skills && !has("skills_extended", "1")))
    return {false, {}, common_failure("CheckpointProfileMismatch").error};
  const auto registry_markers =
      std::count_if(versions.begin(), versions.end(), [](const auto &value) {
        return value.first == "director_registry";
      });
  const auto obligation_markers =
      std::count_if(versions.begin(), versions.end(), [](const auto &value) {
        return value.first == "obligation_needs";
      });
  const auto cohort_wage_markers =
      std::count_if(versions.begin(), versions.end(), [](const auto &value) {
        return value.first == "cohort_wage_need_policies";
      });
  if (cohort_wage_markers != (cohort_wages ? 1 : 0) ||
      (cohort_wages &&
       (!has("cohort_wage_need_policies", "1") ||
        std::count_if(versions.begin(), versions.end(), [](const auto &value) {
          return value.first == "affairs";
        }) != 1)))
    return {false, {}, common_failure("CheckpointProfileMismatch").error};
  const auto fiscal_need_markers =
      std::count_if(versions.begin(), versions.end(), [](const auto &value) {
        return value.first == "fiscal_need_policies";
      });
  if (fiscal_need_markers != (fiscal_needs ? 1 : 0) ||
      (fiscal_needs &&
       (!has("fiscal_need_policies", "1") ||
        version("fiscal_settlement") != 1 ||
        std::count_if(versions.begin(), versions.end(), [](const auto &value) {
          return value.first == "affairs";
        }) != 1)))
    return {false, {}, common_failure("CheckpointProfileMismatch").error};
  if (obligation_markers != (obligations ? 1 : 0) ||
      (obligations && !has("obligation_needs", "1")) ||
      (dynamic && !has("affairs", cohort_wages   ? "4"
                                  : fiscal_needs ? "3"
                                  : obligations  ? "2"
                                                 : "1")))
    return {false, {}, common_failure("CheckpointProfileMismatch").error};
  if (registry_markers != (registry ? 1 : 0) ||
      (registry && !has("director_registry", "1")))
    return {false, {}, common_failure("CheckpointProfileMismatch").error};
  if ((identity.ruleset_id == dynamic_ruleset_id) != dynamic ||
      has("population_military", "4") != population ||
      (population && version("population_military") != 4) ||
      has("siege_integration", "1") != siege ||
      has("regional_life", "1") != regional ||
      has("named_membership", "1") != named ||
      has("study_segments", "1") != segmented ||
      (has("player_plan", "1") != (version("player_plan") == 1) ||
       has("player_plan", "2") != (version("player_plan") == 2)) ||
      has("regional_outcome", "1") != (interaction_base == 4) ||
      (dynamic && !has("director", version("director") == 6   ? "6"
                                   : version("director") == 5 ? "5"
                                   : registry                 ? "4"
                                   : named                    ? "3"
                                   : regional                 ? "2"
                                                              : "1")) ||
      (dynamic &&
       !has("organization", organization_runtime.value
                                ? std::to_string(organization_physical).c_str()
                            : named ? "3"
                                    : "2")))
    return {false, {}, common_failure("CheckpointProfileMismatch").error};
  return decoded;
}
