// 共同编解码世界编解码片段：保持原调用位置、字段与首错顺序；仅由
// world_codec.cpp 登记包含。
if (result->envoy_tasks_enabled) {
  id.module_versions.emplace_back("diplomacy",
                                  std::to_string(result->diplomacy->version));
  id.module_versions.emplace_back("envoy_tasks",
                                  std::to_string(result->diplomacy->version));
}
auto political_body = read_political_author_body_version(scenario);
if (!political_body.ok)
  return {false, {}, std::move(political_body.error)};
if (politics_enabled(*result))
  id.module_versions.emplace_back("politics",
                                  std::to_string(political_body.value));
if (political_fiscal_enabled(*result))
  id.module_versions.emplace_back(
      "political_fiscal", std::to_string(result->political_fiscal->version));
{
  std::function<void(const Condition &, const StableId &)> verify =
      [&](const Condition &condition, const StableId &place) {
        if (condition.kind == Condition::Kind::treaty_personal_passage) {
          const auto *provision =
              treaty_passage_enabled(*result)
                  ? result->treaty_passage->provisions.find(condition.offer_id)
                  : nullptr;
          if (!provision || condition.place_id != place ||
              std::none_of(provision->places.begin(), provision->places.end(),
                           [&](const auto &p) { return p.place_id == place; }))
            fail("InvalidTreatyPassageAdmission", place);
        }
        for (const auto &child : condition.terms)
          verify(child, place);
      };
  for (const auto &admission : result->interaction->admissions)
    for (const auto &condition : admission.conditions)
      verify(condition, admission.id);
}
if (treaty_passage_enabled(*result))
  id.module_versions.emplace_back("treaty_passage", "1");
if (political_military_appointments_enabled(*result))
  id.module_versions.emplace_back(military_officer_political_module, "1");
if (political_field_appointments_enabled(*result))
  id.module_versions.emplace_back(military_field_command_political_module, "1");
if (occupation_fiscal_enabled(*result)) {
  id.module_versions.emplace_back("political_occupation_fiscal", "1");
  id.module_versions.emplace_back("occupation_fiscal_settlement", "1");
}
if (takeover_enabled(*result)) {
  id.module_versions.emplace_back(
      "political_takeover",
      std::to_string(result->political_takeover->version));
  id.module_versions.emplace_back("economic_takeover_execution", "1");
  id.module_versions.emplace_back(
      "task_takeover_execution",
      std::to_string(result->political_takeover->task_execution_version));
}
if (management_enabled(*result))
  id.module_versions.emplace_back("production_management", "1");
if (household_enabled(*result))
  id.module_versions.emplace_back("household_provisioning", "1");
if (settlement_enabled(*result))
  id.module_versions.emplace_back("fiscal_settlement", "1");
if (result->fiscal_taxpayer_continuity_enabled)
  id.module_versions.emplace_back(fiscal_taxpayer_continuity_module, "1");
if (result->governance_tasks_enabled)
  for (const auto *name :
       {"politics_governance", "governance_tasks", "public_relief"})
    id.module_versions.emplace_back(name, "1");
if (registry_enabled(*result))
  id.module_versions.emplace_back("director_registry", "1");
if (result->director && result->director->subjects2)
  id.module_versions.emplace_back("director_subjects", "2");
if (result->director && result->director->diplomatic_response)
  id.module_versions.emplace_back("diplomatic_response", "1");
if (result->director && result->director->authored_task_continuation)
  id.module_versions.emplace_back(
      "authored_task_continuation",
      std::to_string(result->director->authored_task_continuation->version));
if (obligation_needs_enabled(*result))
  id.module_versions.emplace_back("obligation_needs", "1");
if (fiscal_needs_enabled(*result))
  id.module_versions.emplace_back("fiscal_need_policies", "1");
if (result->occupation_fiscal_needs_enabled)
  id.module_versions.emplace_back(occupation_fiscal_need_module, "1");
if (cohort_wage_needs_enabled(*result))
  id.module_versions.emplace_back("cohort_wage_need_policies", "1");
if (outcome_enabled(*result))
  id.module_versions.emplace_back("regional_outcome", "1");
if (result->growth->study_segments)
  id.module_versions.emplace_back("study_segments", "1");
if (result->player_plans) {
  if (result->dynamic_profile != DynamicProfile::local_society_v1)
    fail("PlayerPlanNativeProfileRequired", "scenario/player_plan_profile");
  id.module_versions.emplace_back("player_plan",
                                  std::to_string(selected_plan_version.value));
}
if (result->director && result->director->exclude_selected_player)
  id.module_versions.emplace_back(
      "selected_existing_actor",
      get_string(field(scenario, "dynamic_world", "scenario"), "founder_policy",
                 "dynamic_world") == "selected_existing_actor_roster_v1"
          ? "2"
          : "1");
if (result->common->extended_skills)
  id.module_versions.emplace_back("skills_extended", "1");
if (result->growth->cards.enabled)
  id.module_versions.emplace_back("growth_cards", "1");
if (result->task_offer_responses)
  id.module_versions.emplace_back(
      "task_offer_responses",
      std::to_string(result->task_offer_response_version));
if (result->task_personal_advances)
  id.module_versions.emplace_back("task_personal_advances", "1");
if (result->task_message_handoffs_enabled)
  id.module_versions.emplace_back(
      "task_message_handoffs",
      std::to_string(result->task_message_handoff_version));
if (result->party_invitations_enabled)
  id.module_versions.emplace_back("party_invitations", "1");
if (result->interaction->party_recruitment_consents)
  id.module_versions.emplace_back("party_recruitment_consents", "1");
if (result->growth->actor_start.cards.enabled)
  id.module_versions.emplace_back(
      "actor_card_unlocks",
      std::to_string(result->growth->actor_start.cards.version));
if (result->growth->teaching_requests.enabled)
  id.module_versions.emplace_back("teaching_requests", "1");
if (result->growth->study_teaching_requests.enabled)
  id.module_versions.emplace_back("study_teaching_requests", "1");
if (requests_enabled(*result))
  id.module_versions.emplace_back("military_requests", "1");
if (officers_enabled(*result))
  id.module_versions.emplace_back(military_officer_module, "1");
if (field_commands_enabled(*result))
  id.module_versions.emplace_back(military_field_command_module, "1");
if (military_recovery_enabled(*result))
  id.module_versions.emplace_back(military_recovery_module, "1");
if (military_supply_attrition_enabled(*result))
  id.module_versions.emplace_back(military_supply_attrition_module, "1");
if (military_rout_enabled(*result))
  id.module_versions.emplace_back(military_rout_module, "1");
if (military_command_shock_enabled(*result))
  id.module_versions.emplace_back(military_command_shock_module, "1");
if (military_garrison_rest_enabled(*result))
  id.module_versions.emplace_back(military_garrison_rest_module, "1");
if (military_combat_conditions_enabled(*result))
  id.module_versions.emplace_back(military_combat_conditions_module, "1");
if (military_commanded_damage_enabled(*result))
  id.module_versions.emplace_back(military_commanded_damage_module, "1");
if (military_sequential_enabled(*result))
  id.module_versions.emplace_back(military_sequential_module, "1");
if (result->military && result->military->field_terrain_policy)
  id.module_versions.emplace_back(military_field_terrain_module, "1");
if (military_command_resources_enabled(*result))
  id.module_versions.emplace_back(military_command_resources_module, "1");
if (military_tactic_learning_enabled(*result))
  id.module_versions.emplace_back(military_tactic_learning_module, "1");
if (military_siege_engineering_enabled(*result))
  id.module_versions.emplace_back(military_siege_engineering_module, "1");
if (result->growth->insights.enabled)
  id.module_versions.emplace_back("growth_insights", "1");
if (result->naval && result->naval->repair_policy)
  id.module_versions.emplace_back("naval_repairs", "1");
if (result->naval && result->naval->repair_charge_policy)
  id.module_versions.emplace_back("naval_repair_charges", "1");
if (result->naval && result->naval->navigation_policy)
  id.module_versions.emplace_back("naval_navigation_wear", "1");
if (result->naval && result->naval->combat_policy &&
    result->naval->combat_policy->combat.tactical_navigation)
  id.module_versions.emplace_back("naval_tactical_navigation", "1");
if (result->naval && result->naval->combat_policy &&
    result->naval->combat_policy->combat.gunnery)
  id.module_versions.emplace_back("naval_gunnery", "1");
if (result->naval)
  id.module_versions.emplace_back(
      "military_naval", std::to_string(naval_codec::version(*result->naval)));
if (household_control_enabled(*result))
  id.module_versions.emplace_back("household_control", "1");
if (household_management_enabled(*result))
  id.module_versions.emplace_back("household_provision_management", "1");
if (character_creation_enabled(*result))
  id.module_versions.emplace_back("character_creation", "1");
if (disruption_enabled(*result))
  id.module_versions.emplace_back("production_disruptions", "1");
if (adult_adoption_enabled(*result))
  id.module_versions.emplace_back("adult_adoption", "1");
if (family_life_enabled(*result))
  id.module_versions.emplace_back(
      "family_life", std::to_string(result->family_life->selected_version));
if (family_legacy_enabled(*result))
  id.module_versions.emplace_back(
      "family_legacy", std::to_string(result->family_legacy->version));
if (family_bequest_enabled(*result))
  id.module_versions.emplace_back("family_bequest", "1");
if (result->task_message_notices)
  id.module_versions.emplace_back("task_message_notices", "1");
if (family_inheritance_enabled(*result))
  id.module_versions.emplace_back("family_inheritance", "1");
if (family_marriage_enabled(*result))
  id.module_versions.emplace_back("family_marriage", "1");
if (result->task_static_assets_enabled)
  id.module_versions.emplace_back("task_static_assets", "1");
if (result->estate_employment)
  id.module_versions.emplace_back("estate_employment", "1");
if (result->economic_supply_policies_enabled) {
  id.module_versions.emplace_back("economic_supply_policies", "1");
  id.module_versions.emplace_back("political_supply_adoptions", "1");
}
auto supply_layout = validate_economic_supply_layout(
    result->economic_supply_policies_enabled, id.module_versions);
if (!supply_layout.ok)
  return {false, {}, supply_layout.error};
if (result->director && result->director->supply_renewals)
  id.module_versions.emplace_back("political_supply_renewals", "1");
if (voluntary_succession_enabled(*result))
  id.module_versions.emplace_back("voluntary_succession",
                                  result->family_voluntary_paths ? "2" : "1");
if (family_birth_enabled(*result)) {
  id.module_versions.emplace_back("family_lineage", "1");
  id.module_versions.emplace_back(
      "family_birth", std::to_string(result->family_birth->selected_version));
}
if (result->interaction->visit_messages)
  id.module_versions.emplace_back(
      "visit_messages",
      std::to_string(result->interaction->visit_messages->version));
if (result->growth->innate_learning)
  id.module_versions.emplace_back("family_innate_learning", "1");
if (result->director && result->director->visit_reception)
  id.module_versions.emplace_back("visit_reception_intents", "1");
if (result->director && result->director->visit_reception)
  id.module_versions.emplace_back("visit_reception_claims", "1");
if (family_care_relief_enabled(*result))
  id.module_versions.emplace_back("family_care_relief", "1");
if (result->growth_market_history)
  id.module_versions.emplace_back(
      "growth_market_history",
      std::to_string(result->growth_market_history->selected_version));
if (result->political_shop_licensing)
  id.module_versions.emplace_back(
      "political_shop_licensing",
      std::to_string(selected_shop_licensing_source_version));
if (result->private_shops)
  id.module_versions.emplace_back(
      "private_shops", std::to_string(result->private_shops->version));
if (result->task_shop_orders_enabled)
  id.module_versions.emplace_back("task_shop_orders", "1");
if (result->task_extensions)
  id.module_versions.emplace_back("task_extensions", "1");
if (result->political_shop_registry_successions)
  id.module_versions.emplace_back("political_shop_registry_successions", "1");
if (result->director && result->director->shop_registry_intents)
  id.module_versions.emplace_back("political_shop_registry_intents", "1");
if (result->personal_appointments)
  id.module_versions.emplace_back(
      "personal_appointments",
      std::to_string(result->personal_appointments->version));
if (result->personal_recovery)
  id.module_versions.emplace_back("personal_injury_recovery", "1");
if (result->director && result->director->personal_appointment_attendance)
  id.module_versions.emplace_back("personal_appointment_attendance", "1");
if (result->political_shop_licensing_receipts)
  id.module_versions.emplace_back("political_shop_licensing_receipts", "1");
if (result->private_shop_liquidations) {
  id.module_versions.emplace_back("private_shop_liquidations", "1");
  id.module_versions.emplace_back("task_shop_order_estate_cancellations", "1");
}
if (result->family_apprenticeship)
  id.module_versions.emplace_back("family_apprenticeship", "1");
auto liquidation_pair = validate_private_shop_liquidation_pair(
    bool(result->private_shop_liquidations), id.module_versions);
if (!liquidation_pair.ok)
  return {false, {}, liquidation_pair.error};
if (result->political_governance_continuations)
  id.module_versions.emplace_back("political_governance_continuations", "1");
if (result->contact_inquiries)
  id.module_versions.emplace_back(
      "contact_inquiries", std::to_string(result->contact_inquiries->version));
if (result->director && result->director->governance_intents)
  id.module_versions.emplace_back("political_governance_intents", "1");
if (result->visit_message_replies)
  id.module_versions.emplace_back(
      "visit_message_replies",
      std::to_string(result->visit_message_replies->version));
if (result->family_disciple_legacy)
  id.module_versions.emplace_back("family_disciple_legacy", "1");
if (result->visit_reply_intents)
  id.module_versions.emplace_back("visit_reply_intents", "1");
if (result->political_governance_invitation_replies)
  id.module_versions.emplace_back(governance_invitation_reply_module, "1");
auto invitation_modules = validate_governance_invitation_reply_modules(
    id.module_versions, result->political_governance_invitation_replies);
if (!invitation_modules.ok)
  return {false, {}, invitation_modules.error};
if (result->private_shop_daily_rent)
  id.module_versions.emplace_back(private_shop_daily_rent_module, "1");
auto daily_pair = validate_private_shop_daily_rent_pair(
    bool(result->private_shop_daily_rent), id.module_versions);
if (!daily_pair.ok)
  return {false, {}, daily_pair.error};
if (result->born_personal_dialogues)
  id.module_versions.emplace_back("born_personal_dialogues", "1");
if (result->family_biological_legacy)
  id.module_versions.emplace_back("family_biological_legacy", "1");
if (result->political_successions)
  id.module_versions.emplace_back(political_succession_domain, "1");
auto succession_modules =
    political_succession_selected_version(id.module_versions);
if (!succession_modules.ok)
  return {false, {}, succession_modules.error};
if (result->visit_appointment_invitations) {
  id.module_versions.emplace_back(
      "visit_appointment_invitations",
      std::to_string(result->visit_appointment_invitations->version));
  id.module_versions.emplace_back(
      "visit_appointment_meetings",
      std::to_string(result->visit_appointment_invitations->version));
  if (result->visit_appointment_intents)
    id.module_versions.emplace_back("visit_appointment_intents", "1");
}
if (result->political_head_handovers)
  id.module_versions.emplace_back(political_head_handover_domain, "1");
auto handover_modules =
    political_head_handover_selected_version(id.module_versions);
if (!handover_modules.ok)
  return {false, {}, handover_modules.error};
if (result->private_shop_command_results)
  id.module_versions.emplace_back(private_shop_command_results_module, "1");
auto shop_results_pair = validate_private_shop_command_results_pair(
    bool(result->private_shop_command_results), bool(result->private_shops),
    id.module_versions);
if (!shop_results_pair.ok)
  return {false, {}, shop_results_pair.error};
if (result->visit_appointment_attendance)
  id.module_versions.emplace_back("visit_appointment_attendance", "1");
if (result->growth_tactic_lessons)
  id.module_versions.emplace_back("growth_tactic_lessons", "1");
if (result->political_successor_rounds)
  id.module_versions.emplace_back(political_successor_rounds_domain, "1");
auto round_modules =
    political_successor_round_selected_version(id.module_versions);
if (!round_modules.ok)
  return {false, {}, round_modules.error};
if (result->political_term_handovers)
  id.module_versions.emplace_back(political_term_handover_domain, "1");
auto term_modules =
    political_term_handover_selected_version(id.module_versions);
if (!term_modules.ok)
  return {false, {}, term_modules.error};
if (result->visit_message_receipts_enabled)
  id.module_versions.emplace_back(visit_message_receipt_module,
                                  std::to_string(message_receipts.value));
auto message_receipt_modules =
    visit_message_receipt_selected_version(id.module_versions);
if (!message_receipt_modules.ok)
  return {false, {}, message_receipt_modules.error};
if (result->known_personal_dialogues)
  id.module_versions.emplace_back("known_personal_dialogues", "1");
if (result->political_office_receptions)
  id.module_versions.emplace_back(political_office_reception_domain, "1");
auto reception_modules =
    political_office_reception_selected_version(id.module_versions);
if (!reception_modules.ok)
  return {false, {}, reception_modules.error};
if (result->task_shop_order_contacts_enabled)
  id.module_versions.emplace_back(task_shop_order_contact_module, "1");
auto shop_contact_modules =
    task_shop_order_contact_selected_version(id.module_versions);
if (!shop_contact_modules.ok)
  return {false, {}, shop_contact_modules.error};
if (result->family_collateral_legacy)
  id.module_versions.emplace_back("family_collateral_legacy", "1");
if (result->political_governance_agencies) {
  id.module_versions.emplace_back(
      political_governance_agency_domain,
      std::to_string(result->political_governance_agencies->version));
  if (result->director && result->director->governance_agency_intents)
    id.module_versions.emplace_back(political_governance_agency_intents_module,
                                    "1");
}
auto agency_modules =
    political_governance_agency_selected_version(id.module_versions);
if (!agency_modules.ok)
  return {false, {}, agency_modules.error};
if (!result->personal->simultaneous_outcome_objectives.empty())
  id.module_versions.emplace_back(personal_simultaneous_outcomes_module, "1");
if (result->task_message_returns)
  id.module_versions.emplace_back(task_message_return_module, "1");
auto return_modules = task_message_return_selected_version(id.module_versions);
if (!return_modules.ok)
  return {false, {}, return_modules.error};
if (result->family_voluntary_paths)
  id.module_versions.emplace_back("family_voluntary_paths", "1");
if (result->task_personal_messages)
  id.module_versions.emplace_back(task_personal_message_module, "1");
if (result->task_personal_reports)
  id.module_versions.emplace_back(task_personal_report_module, "1");
if (result->task_personal_cancellations)
  id.module_versions.emplace_back(task_personal_cancellation_module, "1");
auto cancellation_modules =
    task_personal_cancellation_selected_version(id.module_versions);
if (!cancellation_modules.ok)
  return {false, {}, cancellation_modules.error};
auto report_modules = task_personal_report_selected_version(id.module_versions);
if (!report_modules.ok)
  return {false, {}, report_modules.error};
auto personal_modules =
    task_personal_message_selected_version(id.module_versions);
if (!personal_modules.ok)
  return {false, {}, personal_modules.error};
if (result->political_office_resignations) {
  id.module_versions.emplace_back(
      political_office_resignation_module,
      std::to_string(result->political_office_resignations->version));
  if (result->director && result->director->office_resignation_responses)
    id.module_versions.emplace_back(
        political_office_resignation_responses_module, "1");
}
auto resignation_modules =
    political_office_resignation_selected_version(id.module_versions);
if (!resignation_modules.ok)
  return {false, {}, resignation_modules.error};
if (result->political_subordinate_appointments)
  id.module_versions.emplace_back(political_subordinate_appointment_module,
                                  "1");
if (result->political_subordinate_appointment_intents)
  id.module_versions.emplace_back(
      political_subordinate_appointment_intents_module, "1");
auto subordinate_modules =
    political_subordinate_appointment_selected_version(id.module_versions);
if (!subordinate_modules.ok)
  return {false, {}, subordinate_modules.error};
if (result->task_personal_acceptances)
  id.module_versions.emplace_back(task_personal_acceptance_module, "1");
auto acceptance_modules =
    task_personal_acceptance_selected_version(id.module_versions);
if (!acceptance_modules.ok)
  return {false, {}, acceptance_modules.error};
if (result->political_dismissal_sources)
  id.module_versions.emplace_back(political_dismissal_sources_module, "1");
auto dismissal_modules =
    political_dismissal_source_selected_version(id.module_versions);
if (!dismissal_modules.ok)
  return {false, {}, dismissal_modules.error};
// History2完整pair还需下面原native阶段实际加入sources/capabilities；旧History1首错不变。
if (!result->growth_market_history ||
    result->growth_market_history->selected_version != 2) {
  auto shop_pair = validate_private_shop_pair(bool(result->private_shops),
                                              id.module_versions);
  if (!shop_pair.ok)
    return {false, {}, shop_pair.error};
}
auto innate_layout = validate_innate_learning_layout(
    result->growth->innate_learning, id.module_versions);
if (!innate_layout.ok)
  return {false, {}, innate_layout.error};
// 只有本情景实际启用的路遇政策要求宏观profile；未激活的作者定义不扩张当前能力。
if (result->dynamic_profile != DynamicProfile::local_society_v1 &&
    std::any_of(result->personal->road_conflicts.rows().begin(),
                result->personal->road_conflicts.rows().end(),
                [](const auto &row) { return bool(row.second.cargo_policy); }))
  for (const auto &actor_id : strings(
           field(field(scenario, "enabled", "scenario"), "actors", "enabled"),
           "enabled/actors")) {
    const auto *terms = result->personal->road_conflicts.find(actor_id);
    if (terms && terms->cargo_policy)
      fail("TaskCargoProfileRequired",
           "actors/" + actor_id + "/road_conflict/cargo_policy");
  }
