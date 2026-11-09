// 共同编解码世界编解码片段：保持原调用位置、字段与首错顺序；仅由
// world_codec.cpp 登记包含。
CheckpointDomainList domains_for(const PreparedCatalog &catalog) {
  auto relations = validate_native_political_relations_catalog(catalog);
  if (!relations.ok)
    throw boundary::DecodeFailure{std::move(relations.error)};
  auto injury_version =
      selected_native_character_injury_source_version(catalog.identity);
  if (!injury_version.ok)
    throw boundary::DecodeFailure{std::move(injury_version.error)};
  if (injury_version.value &&
      (!catalog.field_battle || !catalog.exact_time ||
       !exact_complete_selected(*catalog.exact_time) || !catalog.growth ||
       !catalog.growth->complete_capability_application_chain))
    throw boundary::DecodeFailure{
        common_failure("CharacterInjuryCompleteGrowth4Required").error};

  auto social_version = selected_native_dynamic_social_version(catalog);
  if (!social_version.ok)
    throw boundary::DecodeFailure{social_version.error};
  auto original_social_version =
      selected_native_original_social_version(catalog.identity);
  if (!original_social_version.ok)
    throw boundary::DecodeFailure{original_social_version.error};
  auto inspection_selection =
      validate_native_public_supply_inspection_catalog(catalog);
  if (!inspection_selection.ok)
    throw boundary::DecodeFailure{inspection_selection.error};
  auto field_selection = validate_field_battle_selection(
      catalog.identity, catalog.field_battle.get());
  if (!field_selection.ok)
    throw boundary::DecodeFailure{field_selection.error};
  auto political_body =
      read_political_locked_body_version(catalog.identity, "politics");
  auto fiscal_body =
      read_political_locked_body_version(catalog.identity, "political_fiscal");
  auto takeover_body = read_political_locked_body_version(catalog.identity,
                                                          "political_takeover");
  if (!political_body.ok)
    throw boundary::DecodeFailure{political_body.error};
  if (!fiscal_body.ok)
    throw boundary::DecodeFailure{fiscal_body.error};
  if (!takeover_body.ok)
    throw boundary::DecodeFailure{takeover_body.error};
  if (bool(political_body.value) != politics_enabled(catalog) ||
      bool(fiscal_body.value) != political_fiscal_enabled(catalog) ||
      bool(takeover_body.value) != takeover_enabled(catalog) ||
      (fiscal_body.value &&
       (fiscal_body.value != political_body.value ||
        fiscal_body.value != catalog.political_fiscal->version)) ||
      (takeover_body.value &&
       (takeover_body.value != political_body.value ||
        takeover_body.value != catalog.political_takeover->version)))
    throw boundary::DecodeFailure{
        common_failure("PoliticalLockedBodySelectionMismatch").error};
  auto field_passage = validate_native_field_passage_catalog(catalog);
  if (!field_passage.ok) throw boundary::DecodeFailure{field_passage.error};
  CheckpointDomainList domains(base_domains_for(catalog));
  if (catalog.field_group_passage) domains.push_back({"political_passage", 3});
  if (catalog.political_relations)
    domains.push_back({"political_relations", 1});

  for (auto &domain : domains)
    if (domain.name == "social")
      domain.codec_version =
          static_cast<std::uint32_t>(original_social_version.value);
  if (population_enabled(catalog))
    for (auto &domain : domains)
      if (domain.name == "population_military")
        domain.codec_version =
            static_cast<std::uint32_t>(population_version(catalog));
  auto deliveries = profile_delivery_selected_version(catalog.identity);
  if (!deliveries.ok)
    boundary::fail("ProfileDeliveryVersionUnsupported",
                   "checkpoint/profile_deliveries");
  if (deliveries.value == 1)
    domains.push_back({"profile_deliveries", 1});
  if (outcome_enabled(catalog))
    for (auto &d : domains)
      if (d.name == "interaction")
        d.codec_version = 4;
  if (catalog.growth && catalog.growth->study_segments)
    domains = study_domains(std::move(domains));
  if (catalog.player_plans)
    domains.push_back({"player_plan", static_cast<std::uint32_t>(
                                          player_plan_version_for(catalog))});
  if (registry_enabled(catalog))
    domains = registry_domains(std::move(domains));
  if (catalog.director && catalog.director->subjects2) {
    auto selected = selected_native_director_version(catalog.identity);
    if (!selected.ok)
      boundary::fail("DirectorSelectedCatalogMismatch", "checkpoint/director");
    for (auto &domain : domains)
      if (domain.name == "director")
        domain.codec_version = static_cast<std::uint32_t>(selected.value);
  }
  if (obligation_needs_enabled(catalog))
    domains = obligation_domains(std::move(domains));
  if (catalog.common && catalog.common->extended_skills)
    domains = extended_skill_domains(std::move(domains));
  if (catalog.growth && catalog.growth->cards.enabled)
    domains = card_domains(std::move(domains));
  if (politics_enabled(catalog))
    domains.push_back(
        {"politics", static_cast<std::uint32_t>(political_body.value)});
  if (political_fiscal_enabled(catalog))
    domains.push_back(
        {"political_fiscal", static_cast<std::uint32_t>(fiscal_body.value)});
  if (household_enabled(catalog) || management_enabled(catalog))
    domains = economic_domains(std::move(domains),
                               household_enabled(catalog) ? 5 : 4);
  if (settlement_enabled(catalog))
    domains.push_back({"fiscal_settlement", 1});
  if (catalog.governance_tasks_enabled)
    domains = governance_domains(std::move(domains));
  if (catalog.task_offer_responses)
    domains.push_back(
        {"task_offer_responses", catalog.task_offer_response_version});
  if (catalog.task_personal_advances)
    domains.push_back({"task_personal_advances", 1});
  if (catalog.task_message_handoffs_enabled)
    domains.push_back(
        {"task_message_handoffs",
         static_cast<std::uint32_t>(catalog.task_message_handoff_version)});
  if (fiscal_needs_enabled(catalog))
    domains = fiscal_need_domains(std::move(domains));
  if (cohort_wage_needs_enabled(catalog))
    domains = cohort_wage_need_domains(std::move(domains));
  if (catalog.growth && catalog.growth->teaching_requests.enabled)
    domains = teaching_domains(std::move(domains));
  if (catalog.growth && catalog.growth->study_teaching_requests.enabled)
    domains = study_teaching_domains(std::move(domains));
  if (catalog.envoy_tasks_enabled)
    domains = diplomacy_domains(std::move(domains), catalog.diplomacy->version);
  if (requests_enabled(catalog))
    domains = request_domains(std::move(domains));
  if (officers_enabled(catalog))
    domains = officer_domains(std::move(domains));
  if (field_commands_enabled(catalog))
    domains = field_command_domains(std::move(domains));
  if (military_recovery_enabled(catalog))
    domains = military_recovery_domains(std::move(domains));
  if (military_supply_attrition_enabled(catalog))
    domains = military_supply_attrition_domains(std::move(domains));
  if (military_rout_enabled(catalog))
    domains = military_rout_domains(std::move(domains));
  if (military_command_shock_enabled(catalog))
    domains = military_command_shock_domains(std::move(domains));
  if (military_garrison_rest_enabled(catalog))
    domains = military_garrison_rest_domains(std::move(domains));
  if (military_combat_conditions_enabled(catalog))
    domains = military_combat_conditions_domains(std::move(domains));
  if (military_commanded_damage_enabled(catalog))
    domains = military_commanded_damage_domains(std::move(domains));
  if (military_command_resources_enabled(catalog))
    domains = military_command_resources_domains(std::move(domains));
  if (military_tactic_learning_enabled(catalog))
    domains = military_tactic_learning_domains(std::move(domains));
  if (military_siege_engineering_enabled(catalog))
    domains = military_siege_engineering_domains(std::move(domains));
  if (catalog.naval)
    domains.push_back(
        {"military_naval",
         static_cast<std::uint32_t>(naval_codec::version(*catalog.naval))});
  if (household_control_enabled(catalog))
    domains = household_control_domains(std::move(domains));
  if (household_management_enabled(catalog))
    domains.push_back({"household_provision_management", 1});
  if (character_creation_enabled(catalog))
    domains = character_creation_domains(std::move(domains));
  if (disruption_enabled(catalog))
    domains.push_back({"production_disruptions", 1});
  if (adult_adoption_enabled(catalog))
    domains = adult_adoption_domains(std::move(domains));
  if (voluntary_succession_enabled(catalog))
    domains = voluntary_succession_domains(std::move(domains));
  if (takeover_enabled(catalog)) {
    domains = takeover_domains(
        std::move(domains), catalog.political_takeover->task_execution_version);
    for (auto &domain : domains)
      if (domain.name == "political_takeover")
        domain.codec_version = static_cast<std::uint32_t>(takeover_body.value);
  }
  if (family_life_enabled(catalog))
    domains.push_back(
        {"family_life",
         static_cast<std::uint32_t>(catalog.family_life->selected_version)});
  if (family_legacy_enabled(catalog))
    domains.push_back({"family_legacy", static_cast<std::uint32_t>(
                                            catalog.family_legacy->version)});
  if (occupation_fiscal_enabled(catalog))
    domains = occupation_fiscal_domains(std::move(domains));
  if (family_bequest_enabled(catalog))
    domains.push_back({"family_bequest", 1});
  if (catalog.task_message_notices)
    domains.push_back({"task_message_notices", 1});
  if (treaty_passage_enabled(catalog))
    domains.push_back({"treaty_passage", 1});
  if (family_inheritance_enabled(catalog))
    domains.push_back({"family_inheritance", 1});
  if (catalog.fiscal_taxpayer_continuity_enabled)
    domains.push_back({"fiscal_taxpayer_continuity", 1});
  if (political_military_appointments_enabled(catalog))
    domains.push_back({military_officer_political_module, 1});
  if (family_marriage_enabled(catalog))
    domains.push_back({"family_marriage", 1});
  if (catalog.task_static_assets_enabled)
    domains.push_back({"task_static_assets", 1});
  if (catalog.estate_employment)
    domains.push_back({"estate_employment", 1});
  if (political_field_appointments_enabled(catalog))
    domains.push_back({military_field_command_political_module, 1});
  if (catalog.economic_supply_policies_enabled) {
    domains.push_back({"economic_supply_policies", 1});
    domains.push_back({"political_supply_adoptions", 1});
  }
  if (family_birth_enabled(catalog))
    domains.push_back(
        {"family_birth",
         static_cast<std::uint32_t>(catalog.family_birth->selected_version)});
  if (catalog.interaction && catalog.interaction->visit_messages)
    domains.push_back(
        {"visit_messages", static_cast<std::uint32_t>(
                               catalog.interaction->visit_messages->version)});
  if (catalog.director && catalog.director->visit_reception)
    domains.push_back({"visit_reception_claims", 1});
  if (family_care_relief_enabled(catalog))
    domains.push_back({"family_care_relief", 1});
  if (catalog.growth_market_history)
    domains.push_back({"growth_market_history",
                       static_cast<std::uint32_t>(
                           catalog.growth_market_history->selected_version)});
  if (catalog.political_shop_licensing) {
    auto selected =
        selected_native_political_shop_licensing_version(catalog.identity);
    if (!selected.ok)
      throw boundary::DecodeFailure{std::move(selected.error)};
    if (!selected.value)
      throw boundary::DecodeFailure{
          common_failure("PoliticalShopLicensingPairRequired").error};
    domains.push_back({"political_shop_licensing",
                       static_cast<std::uint32_t>(selected.value)});
  }
  if (catalog.private_shops)
    domains.push_back({"private_shops", static_cast<std::uint32_t>(
                                            catalog.private_shops->version)});
  if (catalog.task_shop_orders_enabled)
    domains.push_back({"task_shop_orders", 1});
  if (catalog.task_extensions)
    domains.push_back({"task_extensions", 1});
  if (catalog.political_shop_registry_successions)
    domains.push_back({"political_shop_registry_successions", 1});
  if (catalog.personal_appointments)
    domains.push_back(
        {"personal_appointments",
         static_cast<std::uint32_t>(catalog.personal_appointments->version)});
  if (catalog.personal_recovery)
    domains.push_back({"personal_injury_recovery", 1});
  if (catalog.political_shop_licensing_receipts)
    domains.push_back({"political_shop_licensing_receipts", 1});
  if (catalog.private_shop_liquidations) {
    domains.push_back({"private_shop_liquidations", 1});
    domains.push_back({"task_shop_order_estate_cancellations", 1});
  }
  if (catalog.family_apprenticeship)
    domains.push_back({"family_apprenticeship", 1});
  if (catalog.political_governance_continuations)
    domains.push_back({"political_governance_continuations", 1});
  if (catalog.contact_inquiries)
    domains.push_back(
        {"contact_inquiries",
         static_cast<std::uint32_t>(catalog.contact_inquiries->version)});
  if (catalog.visit_message_replies)
    domains.push_back(
        {"visit_message_replies",
         static_cast<std::uint32_t>(catalog.visit_message_replies->version)});
  if (catalog.family_disciple_legacy)
    domains.push_back({"family_disciple_legacy", 1});
  if (catalog.political_governance_invitation_replies)
    domains.push_back({governance_invitation_reply_module, 1});
  if (catalog.private_shop_daily_rent)
    domains.push_back({private_shop_daily_rent_module, 1});
  if (catalog.born_personal_dialogues)
    domains.push_back({"born_personal_dialogues", 1});
  if (catalog.family_biological_legacy)
    domains.push_back({"family_biological_legacy", 1});
  if (catalog.political_successions)
    domains.push_back({political_succession_domain, 1});
  if (catalog.visit_appointment_invitations) {
    domains.push_back({"visit_appointment_invitations",
                       static_cast<std::uint32_t>(
                           catalog.visit_appointment_invitations->version)});
    domains.push_back({"visit_appointment_meetings",
                       static_cast<std::uint32_t>(
                           catalog.visit_appointment_invitations->version)});
  }
  if (catalog.political_head_handovers)
    domains.push_back({political_head_handover_domain, 1});
  if (military_tactic_learning_enabled(catalog))
    domains.push_back({military_tactic_learning_module, 1});
  if (catalog.growth_tactic_lessons)
    domains.push_back({"growth_tactic_lessons", 1});
  if (catalog.political_successor_rounds)
    domains.push_back({political_successor_rounds_domain, 1});
  if (catalog.private_shop_command_results)
    domains.push_back({private_shop_command_results_module, 1});
  if (catalog.political_term_handovers)
    domains.push_back({political_term_handover_domain, 1});
  if (catalog.visit_message_receipts_enabled) {
    auto version = visit_message_receipt_selected_version(
        catalog.identity.module_versions);
    if (!version.ok || !version.value)
      boundary::fail("VisitMessageReceiptProfileMismatch",
                     "checkpoint/visit_message_receipts");
    domains.push_back({visit_message_receipt_module,
                       static_cast<std::uint32_t>(version.value)});
  }
  if (catalog.known_personal_dialogues)
    domains.push_back({"known_personal_dialogues", 1});
  if (catalog.political_office_receptions)
    domains.push_back({political_office_reception_domain, 1});
  if (catalog.task_shop_order_contacts_enabled)
    domains.push_back({task_shop_order_contact_module, 1});
  if (catalog.family_collateral_legacy)
    domains.push_back({"family_collateral_legacy", 1});
  if (catalog.political_governance_agencies)
    domains.push_back({political_governance_agency_domain,
                       static_cast<std::uint32_t>(
                           catalog.political_governance_agencies->version)});
  if (catalog.task_message_returns)
    domains.push_back({task_message_return_module, 1});
  for (auto &d : domains)
    if (d.name == "family")
      d.codec_version = static_cast<std::uint32_t>(family_version_for(catalog));
  if (catalog.family_voluntary_paths)
    domains.push_back({"family_voluntary_paths", 1});
  if (catalog.task_personal_messages)
    domains.push_back({task_personal_message_module, 1});
  if (catalog.political_office_resignations)
    domains.push_back({political_office_resignation_module,
                       static_cast<std::uint32_t>(
                           catalog.political_office_resignations->version)});
  if (catalog.task_personal_reports)
    domains.push_back({task_personal_report_module, 1});
  if (catalog.task_personal_cancellations)
    domains.push_back({task_personal_cancellation_module, 1});
  if (catalog.political_subordinate_appointments)
    domains.push_back({political_subordinate_appointment_module, 1});
  if (catalog.task_personal_acceptances)
    domains.push_back({task_personal_acceptance_module, 1});
  if (catalog.political_dismissal_sources)
    domains.push_back({political_dismissal_sources_module, 1});
  if (catalog.exact_time) {
    for (auto &d : domains)
      if (d.name == "growth")
        d.codec_version +=
            exact_complete_selected(*catalog.exact_time)
                ? (catalog.growth->complete_capability_application_chain ? 192
                   : catalog.growth->complete_exact_rest_mouth_sources   ? 128
                   : catalog.growth->complete_exact_command_cuts         ? 96
                                                                         : 64)
                : 32;
    for (const auto &d : std::initializer_list<CheckpointDomainView>{
             {"world_timeline", 2},
             {"action_elapsed", 2},
             {"actor_start_context",
              exact_complete_selected(*catalog.exact_time) ? 3u : 2u},
             {"character_origin_records", 1}})
      domains.push_back(d);
    if (catalog.growth->complete_capability_application_chain) {
      domains.push_back({"growth_permanent_contributions", 3});
      domains.push_back({"trade-grant", 1});
    }
    domains.push_back({"character_condition_extensions",
                       injury_version.value == 1 ? 8u
                       : catalog.growth->complete_capability_application_chain
                           ? 7u
                           : 1u});
    if (injury_version.value == 1)
      domains.push_back({"character-injury-sources", 1});
    if (exact_complete_selected(*catalog.exact_time)) {
      domains.push_back({"growth_terminal_publications", 1});
      domains.push_back({"facility_claims", 2});
      if (catalog.household_purchases) {
        domains.push_back(
            {"household_purchases",
             static_cast<std::uint32_t>(catalog.selected_household_version)});
        domains.push_back({"household_retail_settlements", 1});
      }
    }
    if (exact_social_selected(*catalog.exact_time) ||
        exact_complete_selected(*catalog.exact_time)) {
      for (auto &d : domains)
        if (d.name == "dynamic_social")
          d.codec_version = static_cast<std::uint32_t>(social_version.value);
      domains.push_back({"social_period_sources", 1});
    }
  }
  auto face_selected = selected_native_face_to_face_version(catalog.identity);
  if (!face_selected.ok)
    throw boundary::DecodeFailure{face_selected.error};
  auto face_catalog = validate_native_face_to_face_catalog(catalog);
  if (!face_catalog.ok)
    throw boundary::DecodeFailure{face_catalog.error};
  if (face_selected.value)
    domains.push_back({"face_to_face_contacts", 3});
  if (catalog.actor_social_occupancy_selected) {
    auto version =
        selected_native_actor_social_occupancy_version(catalog.identity);
    if (!version.ok || !version.value)
      boundary::fail("ActorSocialOccupancyModuleMismatch",
                     "checkpoint/actor_social_occupancy");
    domains.push_back(
        {"actor_social_occupancy", static_cast<std::uint32_t>(version.value)});
  }
  auto life_age = validate_native_life_age_catalog(catalog);
  if (!life_age.ok)
    throw boundary::DecodeFailure{life_age.error};
  if (catalog.life_age_checks)
    domains.push_back({"life_age_checks", 1});
  auto control = validate_native_control_continuation_catalog(catalog);
  if (!control.ok)
    throw boundary::DecodeFailure{control.error};
  if (catalog.control_continuation)
    domains.push_back({"control_continuation",
                       static_cast<std::uint32_t>(
                           catalog.control_continuation->selected_version)});
  auto shop_catalog = validate_native_private_shop_operation_catalog(catalog);
  if (!shop_catalog.ok)
    throw boundary::DecodeFailure{shop_catalog.error};
  auto shop_versions =
      selected_native_private_shop_operation_versions(catalog.identity);
  if (!shop_versions.ok)
    throw boundary::DecodeFailure{shop_versions.error};
  if (shop_versions.value.exact_operations()) {
    domains.push_back({"private_shop_operation_sources", 1});
    domains.push_back({"private_shop_operation_capabilities", 1});
  }
  auto custody_catalog =
      validate_native_private_shop_household_custody_catalog(catalog);
  if (!custody_catalog.ok)
    throw boundary::DecodeFailure{custody_catalog.error};
  if (catalog.private_shop_household_custody)
    domains.push_back({"private_shop_household_custody_sources", 1});
  auto item_craft_catalog = validate_native_item_craft_catalogs(catalog);
  if (!item_craft_catalog.ok)
    throw boundary::DecodeFailure{item_craft_catalog.error};
  auto item_craft = selected_native_item_craft_versions(catalog.identity);
  if (!item_craft.ok)
    throw boundary::DecodeFailure{item_craft.error};
  if (item_craft.value.item_instances) {
    domains.push_back({"item_instances", static_cast<std::uint32_t>(
                                             item_craft.value.item_instances)});
    domains.push_back({"lot_properties", 1});
  }
  if (item_craft.value.craft_jobs)
    domains.push_back({"craft_jobs", 1});
  auto facility_catalogs = validate_native_economic_facility_catalogs(catalog);
  if (!facility_catalogs.ok)
    throw boundary::DecodeFailure{facility_catalogs.error};
  auto facilities =
      selected_native_economic_facility_versions(catalog.identity);
  if (!facilities.ok)
    throw boundary::DecodeFailure{facilities.error};
  if (facilities.value.facilities) {
    domains.push_back({"economic_facility_capacity_sources", 1});
    domains.push_back({"production_facility_conditions", 1});
  }
  if (facilities.value.work_expense)
    domains.push_back(
        {"governance_work_expense",
         static_cast<std::uint32_t>(facilities.value.work_expense)});
  auto organization_runtime =
      selected_native_organization_runtime(catalog.identity);
  if (!organization_runtime.ok)
    throw boundary::DecodeFailure{organization_runtime.error};
  auto org_catalog = validate_native_organization_runtime_catalog(catalog);
  if (!org_catalog.ok)
    throw boundary::DecodeFailure{org_catalog.error};
  if (organization_runtime.value) {
    auto body = select_organization_runtime_codec(catalog.identity);
    if (!body.ok)
      throw boundary::DecodeFailure{body.error};
    for (auto &domain : domains)
      if (domain.name == "organization")
        domain.codec_version =
            static_cast<std::uint32_t>(body.value.organization_version);
    domains.push_back({"organization_runtime", 1});
    if (body.value.service_effects_schema)
      domains.push_back({"organization_service_effects", 1});
  }
  auto obligation_versions =
      selected_native_economic_obligation_versions(catalog.identity);
  if (!obligation_versions.ok)
    throw boundary::DecodeFailure{obligation_versions.error};
  auto obligation_catalog =
      validate_native_economic_obligation_catalog(catalog);
  if (!obligation_catalog.ok)
    throw boundary::DecodeFailure{obligation_catalog.error};
  if (obligation_versions.value.sources ||
      obligation_versions.value.named_birth_population)
    for (auto &domain : domains)
      if (domain.name == "local_economy")
        domain.codec_version =
            static_cast<std::uint32_t>(obligation_versions.value.local_economy);
  if (obligation_versions.value.sources)
    domains.push_back({"economic_obligation_sources", 1});
  auto governance_catalog = validate_native_governance_program_catalog(catalog);
  if (!governance_catalog.ok)
    throw boundary::DecodeFailure{governance_catalog.error};
  auto governance =
      selected_native_governance_program_versions(catalog.identity);
  if (!governance.ok)
    throw boundary::DecodeFailure{governance.error};
  if (governance.value.state)
    domains.push_back({"governance_programs",
                       static_cast<std::uint32_t>(governance.value.state)});
  auto battle_lifecycle_catalog =
      validate_native_military_battle_lifecycle_catalog(catalog);
  if (!battle_lifecycle_catalog.ok)
    throw boundary::DecodeFailure{battle_lifecycle_catalog.error};
  auto battle_lifecycle =
      selected_native_military_battle_lifecycle(catalog.identity);
  if (!battle_lifecycle.ok)
    throw boundary::DecodeFailure{battle_lifecycle.error};
  if (battle_lifecycle.value)
    domains.push_back({"military_battle_lifecycle", 1});
  auto personal_lifecycle_catalog =
      validate_native_personal_battle_lifecycle_catalog(catalog);
  if (!personal_lifecycle_catalog.ok)
    throw boundary::DecodeFailure{personal_lifecycle_catalog.error};
  auto personal_lifecycle =
      selected_native_personal_battle_lifecycle(catalog.identity);
  if (!personal_lifecycle.ok)
    throw boundary::DecodeFailure{personal_lifecycle.error};
  if (personal_lifecycle.value)
    domains.push_back({"personal_battle_lifecycle", 1});
  auto military_catalog = validate_native_military_state_catalog(catalog);
  if (!military_catalog.ok)
    throw boundary::DecodeFailure{military_catalog.error};
  auto military = selected_native_military_state_versions(catalog.identity);
  if (!military.ok)
    throw boundary::DecodeFailure{military.error};
  if (military.value.dynamic_military)
    domains.push_back(
        {"dynamic_military",
         static_cast<std::uint32_t>(military.value.dynamic_military)});
  if (catalog.field_battle)
    domains.push_back({"field_battle", static_cast<std::uint32_t>(field_battle_checkpoint_codec_revision)});
  if (catalog.public_supply_inspection)
    domains.push_back({"public_supply_inspection", 1});
  if (military.value.military_action_time)
    domains.push_back({"military_action_time", 1});
  if (military.value.military_authority_time)
    domains.push_back({"military_authority_time", 1});
  if (military.value.military_action_resources)
    domains.push_back({"military_action_resources", 1});
  if (military.value.military_return_detachments)
    domains.push_back({"military_return_detachments", 1});
  if (military.value.military_action_conditions)
    domains.push_back({"military_action_conditions",
                       static_cast<std::uint32_t>(
                           military.value.military_action_conditions)});
  if (military.value.military_training_completion_time)
    domains.push_back({"military_training_completion_time", 1});
  if (military.value.military_formation_reservations)
    domains.push_back({"military_formation_reservations",
                       static_cast<std::uint32_t>(
                           military.value.military_formation_reservations)});
  auto production = validate_native_production_cycles_catalog(catalog);
  if (!production.ok)
    throw boundary::DecodeFailure{production.error};
  if (catalog.production_cycles) {
    domains.push_back({"production_cycles", 3});
    domains.push_back({"civilian_transport_population", 3});
  }
  auto funding = validate_native_private_transport_funding_catalog(catalog);
  if (!funding.ok)
    throw boundary::DecodeFailure{funding.error};
  if (catalog.private_transport_funding_selected)
    domains.push_back({"private_transport_funding", 1});
  auto occupation = validate_native_military_occupation_catalog(catalog);
  if (!occupation.ok)
    throw boundary::DecodeFailure{occupation.error};
  if (catalog.selected_military_occupation_schema == 2)
    for (auto &d : domains)
      if (d.name == "military") {
        if (d.codec_version < 3 || d.codec_version > 17)
          boundary::fail("MilitaryOccupationCheckpointProfileMismatch",
                         "military");
        d.codec_version += 32;
      }
  for(auto& d:domains)if(d.name=="military")d.codec_version+=64;
  auto work = selected_native_task_work_version(catalog.identity);
  if (!work.ok)
    throw boundary::DecodeFailure{work.error};
  if (work.value)
    domains.push_back(
        {"task_work_execution", static_cast<std::uint32_t>(work.value)});
  auto task_execution =
      selected_native_task_execution_version(catalog.identity);
  if (!task_execution.ok)
    throw boundary::DecodeFailure{task_execution.error};
  if (task_execution.value)
    domains.push_back({"task_execution_time",
                       static_cast<std::uint32_t>(task_execution.value)});
  auto transport = apply_native_transport_domains(domains, catalog.identity);
  if (!transport.ok)
    throw boundary::DecodeFailure{transport.error};
  auto interaction_physical = native_interaction_physical_version(catalog);
  if (!interaction_physical.ok)
    throw boundary::DecodeFailure{interaction_physical.error};
  for (auto &domain : domains)
    if (domain.name == "interaction")
      domain.codec_version =
          static_cast<std::uint32_t>(interaction_physical.value);
  return domains;
}
