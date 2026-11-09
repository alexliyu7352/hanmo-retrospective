// 共同编解码世界编解码片段：保持原调用位置、字段与首错顺序；仅由
// world_codec.cpp 登记包含。
// 从已核封套的显式身份选择唯一原格式，内存仅与实际域数相关。
Outcome<std::vector<CheckpointDomain>>
selected_checkpoint_domains_impl(const CheckpointIdentity &identity,
                                 bool population_shape_only) {
  try {
    if (!population_shape_only) {
      for (const auto &row : identity.catalog.module_versions)
        if (row.first == "population_military" && row.second != "4") {
          auto error =
              common_failure("CheckpointFormatVersionUnsupported", row.first,
                             ErrorKind::unsupported_capability)
                  .error;
          error.domain = row.first;
          error.reason =
              "observed=" + row.second + ", expected=4; no migration";
          return {false, {}, std::move(error)};
        }
    }
    // 新精确首链完整选择后才复用旧基形目录；缺一段或未知组合不能降级到legacy。
    const bool complete = std::any_of(
        identity.catalog.module_versions.begin(),
        identity.catalog.module_versions.end(), [](const auto &row) {
          return row.first == "actor_start_context" && row.second == "3";
        });
    auto political_body =
        read_political_locked_body_version(identity.catalog, "politics");
    auto fiscal_body = read_political_locked_body_version(identity.catalog,
                                                          "political_fiscal");
    auto takeover_body = read_political_locked_body_version(
        identity.catalog, "political_takeover");
    if (!political_body.ok)
      return {false, {}, std::move(political_body.error)};
    if (!fiscal_body.ok)
      return {false, {}, std::move(fiscal_body.error)};
    if (!takeover_body.ok)
      return {false, {}, std::move(takeover_body.error)};
    if (((political_body.value == 2 || fiscal_body.value == 2 ||
          takeover_body.value == 2) &&
         !complete) ||
        (fiscal_body.value && fiscal_body.value != political_body.value) ||
        (takeover_body.value && takeover_body.value != political_body.value))
      fail("PoliticalLockedBodySelectionMismatch",
           "checkpoint/module_versions");
    auto director_physical = selected_native_director_version(identity.catalog);
    if (!director_physical.ok)
      return {false, {}, std::move(director_physical.error)};
    const bool director_subjects =
        director_physical.value == 5 || director_physical.value == 6;
    Integer growth_phase = 1;
    bool growth_marker = false;
    for (const auto &row : identity.catalog.module_versions)
      if (row.first == "growth_fixed_service_time2") {
        if (growth_marker ||
            (row.second != "2" && row.second != "3" && row.second != "4"))
          fail("CheckpointGrowthCommandCutProfileMismatch", row.first);
        growth_marker = true;
        growth_phase = row.second == "4" ? 4 : row.second == "3" ? 3 : 2;
      }
    const bool command_cuts = growth_phase >= 2,
               rest_mouths = growth_phase >= 3,
               capability_chain = growth_phase == 4;
    auto injury_version =
        selected_native_character_injury_source_version(identity.catalog);
    if (!injury_version.ok)
      return {false, {}, std::move(injury_version.error)};
    const bool injury_sources = injury_version.value == 1;
    // 业务 phase4 不变；源类别后继必须由真实格式身份明示，旧格式不迁移。
    if (capability_chain) {
      for (const auto &expected :
           std::initializer_list<std::pair<const char *, const char *>>{
               {"growth_permanent_contributions", "3"},
               {"character_condition_extensions",
                injury_sources ? "8" : "7"}}) {
        const StableId *observed = nullptr;
        std::size_t count = 0;
        for (const auto &row : identity.catalog.module_versions)
          if (row.first == expected.first) {
            observed = &row.second;
            ++count;
          }
        if (count != 1 || !observed || *observed != expected.second) {
          auto error =
              common_failure("CheckpointFormatVersionUnsupported",
                             expected.first, ErrorKind::unsupported_capability)
                  .error;
          error.domain = expected.first;
          error.reason =
              "observed=" + (observed ? *observed : StableId{"missing"}) +
              ", expected=" + expected.second;
          return {false, {}, std::move(error)};
        }
      }
    }
    auto interaction_sections =
        selected_native_interaction_versions(identity.catalog);
    if (!interaction_sections.ok)
      return {false, {}, std::move(interaction_sections.error)};
    auto interaction_response =
        selected_native_interaction_response_version(identity.catalog);
    if (!interaction_response.ok)
      return {false, {}, std::move(interaction_response.error)};
    if (growth_marker && !complete)
      fail("CheckpointGrowthCommandCutProfileMismatch",
           "checkpoint/module_versions");
    auto occupation_schema =
        selected_military_occupation_schema(identity.catalog);
    if (!occupation_schema.ok)
      return {false, {}, std::move(occupation_schema.error)};
    if (occupation_schema.value == 2 && !complete)
      fail("MilitaryOccupationCompleteProfileRequired",
           "checkpoint/module_versions");
    const bool exact_population = std::any_of(
        identity.catalog.module_versions.begin(),
        identity.catalog.module_versions.end(), [](const auto &row) {
          return row.first == "population_military" && row.second == "4";
        });
    if (exact_population && !complete)
      fail("PopulationMilitaryExactProfileRequired",
           "checkpoint/module_versions");
    auto control_version =
        selected_native_control_continuation_version(identity.catalog);
    if (!control_version.ok)
      return {false, {}, std::move(control_version.error)};
    if (control_version.value && !complete)
      fail("ControlContinuationCompleteProfileRequired",
           "checkpoint/module_versions");
    auto inspection_version =
        selected_native_public_supply_inspection(identity.catalog);
    if (!inspection_version.ok)
      return {false, {}, std::move(inspection_version.error)};
    auto field_passage_version = selected_native_field_passage_version(identity.catalog);
    if (!field_passage_version.ok) return {false, {}, std::move(field_passage_version.error)};
    auto relations_version =
        selected_native_political_relations_version(identity.catalog);
    if (!relations_version.ok)
      return {false, {}, std::move(relations_version.error)};
    auto field_version = selected_field_battle_version(identity.catalog);
    if (!field_version.ok)
      return {false, {}, field_version.error};
    auto military_versions =
        selected_native_military_state_versions(identity.catalog);
    if (!military_versions.ok)
      return {false, {}, std::move(military_versions.error)};
    if ((military_versions.value.dynamic_military ||
         military_versions.value.military_duty_catalog ||
         military_versions.value.military_action_time ||
         military_versions.value.military_authority_time ||
         military_versions.value.military_action_resources ||
         military_versions.value.military_return_detachments ||
         military_versions.value.military_action_conditions ||
         military_versions.value.military_training_completion_time ||
         military_versions.value.military_formation_reservations) &&
        !complete)
      fail("MilitaryExactCompleteProfileRequired",
           "checkpoint/module_versions");
    auto personal_lifecycle =
        selected_native_personal_battle_lifecycle(identity.catalog);
    if (!personal_lifecycle.ok)
      return {false, {}, std::move(personal_lifecycle.error)};
    auto battle_lifecycle =
        selected_native_military_battle_lifecycle(identity.catalog);
    if (!battle_lifecycle.ok)
      return {false, {}, std::move(battle_lifecycle.error)};
    auto task_execution =
        selected_native_task_execution_version(identity.catalog);
    if (!task_execution.ok)
      return {false, {}, std::move(task_execution.error)};
    if (task_execution.value && !complete)
      fail("TaskExecutionCompleteProfileRequired",
           "checkpoint/module_versions");
    auto organization_runtime =
        selected_native_organization_runtime(identity.catalog);
    if (!organization_runtime.ok)
      return {false, {}, std::move(organization_runtime.error)};
    auto organization_effects =
        selected_native_organization_service_effects(identity.catalog);
    if (!organization_effects.ok)
      return {false, {}, std::move(organization_effects.error)};
    Integer organization_body = 0;
    if (organization_runtime.value) {
      if (!complete)
        fail("OrganizationRuntimeCompleteProfileRequired",
             "checkpoint/module_versions");
      auto selected = select_organization_runtime_codec(identity.catalog);
      if (!selected.ok)
        return {false, {}, std::move(selected.error)};
      organization_body = selected.value.organization_version;
    }
    auto obligation_versions =
        selected_native_economic_obligation_versions(identity.catalog);
    if (!obligation_versions.ok)
      return {false, {}, std::move(obligation_versions.error)};
    if (obligation_versions.value.sources && !complete)
      fail("EconomicObligationCompleteProfileRequired",
           "checkpoint/module_versions");
    if (obligation_versions.value.named_birth_population && !complete)
      fail("NamedBirthPopulationCompleteProfileRequired",
           "checkpoint/module_versions");
    if (organization_runtime.value && obligation_versions.value.sources != 1)
      fail("OrganizationRuntimeActualObligationRootRequired",
           "checkpoint/module_versions");
    auto governance_versions =
        selected_native_governance_program_versions(identity.catalog);
    if (!governance_versions.ok)
      return {false, {}, std::move(governance_versions.error)};
    if (governance_versions.value.state &&
        (!complete || !organization_runtime.value))
      fail("GovernanceProgramCompleteOriginalRuntimeRequired",
           "checkpoint/module_versions");
    bool life4 = false, age_checks = false;
    for (const auto &row : identity.catalog.module_versions) {
      if (row.first == "family_life" && row.second == "4") {
        if (life4 || !complete)
          fail("Life4CompleteProfileRequired", row.first);
        life4 = true;
      }
      if (row.first == "life_age_checks") {
        if (age_checks || row.second != "1" || !complete)
          fail("LifeAgeCompleteProfileRequired", row.first);
        age_checks = true;
      }
    }
    if (age_checks && !life4)
      fail("LifeAgeLife4Required", "checkpoint/module_versions");
    if (task_execution.value == 2 && !life4)
      fail("TaskExecutionOriginalLife4ModuleRequired",
           "checkpoint/module_versions");
    if (control_version.value && !life4)
      fail("ControlContinuationLife4Required", "checkpoint/module_versions");
    auto birth_version = selected_native_family_birth_version(identity.catalog);
    if (!birth_version.ok)
      return {false, {}, std::move(birth_version.error)};
    if (birth_version.value == 4 && (!complete || !life4))
      fail("Birth4CompleteOriginalModulesRequired",
           "checkpoint/module_versions");
    std::map<StableId, StableId> exact_keys = {
        {"world_timeline", "2"},
        {"action_elapsed", "2"},
        {"actor_start_context", complete ? "3" : "2"},
        {"character_origin_records", "1"},
        {"character_condition_extensions", injury_sources     ? "8"
                                           : capability_chain ? "7"
                                                              : "1"}};
    if (injury_sources)
      exact_keys.emplace("character-injury-sources", "1");
    if (personal_lifecycle.value) {
      // 只随明选生命周期核原个人身份；物理个人段仍由原基形唯一提供。
      exact_keys.emplace("personal", "1");
      exact_keys.emplace("personal_battle_lifecycle", "1");
    }
    if (battle_lifecycle.value)
      exact_keys.emplace("military_battle_lifecycle", "1");
    if (organization_runtime.value)
      exact_keys.emplace("organization_runtime", "1");
    if (organization_effects.value)
      exact_keys.emplace("organization_service_effects", "1");
    if (obligation_versions.value.sources)
      exact_keys.emplace("economic_obligation_sources", "1");
    if (obligation_versions.value.named_birth_population)
      exact_keys.emplace("named_birth_population_sources", "1");
    if (governance_versions.value.state) {
      exact_keys.emplace("governance_programs",
                         std::to_string(governance_versions.value.state));
      exact_keys.emplace("governance_program_catalog",
                         std::to_string(governance_versions.value.catalog));
    }
    if (capability_chain)
      exact_keys.emplace("growth_permanent_contributions", "3");
    if (director_subjects)
      exact_keys.emplace("director_subjects", "2");
    if (age_checks)
      exact_keys.emplace("life_age_checks", "1");
    if (control_version.value)
      exact_keys.emplace("control_continuation",
                         std::to_string(control_version.value));
    if (military_versions.value.dynamic_military)
      exact_keys.emplace(
          "dynamic_military",
          std::to_string(military_versions.value.dynamic_military));
    if (military_versions.value.military_duty_catalog)
      exact_keys.emplace("military_duty_catalog", "1");
    if (military_versions.value.military_action_time)
      exact_keys.emplace("military_action_time", "1");
    if (military_versions.value.military_authority_time)
      exact_keys.emplace("military_authority_time", "1");
    if (military_versions.value.military_action_resources)
      exact_keys.emplace("military_action_resources", "1");
    if (military_versions.value.military_return_detachments)
      exact_keys.emplace("military_return_detachments", "1");
    if (military_versions.value.military_action_conditions)
      exact_keys.emplace(
          "military_action_conditions",
          std::to_string(military_versions.value.military_action_conditions));
    if (military_versions.value.military_training_completion_time)
      exact_keys.emplace("military_training_completion_time", "1");
    if (military_versions.value.military_formation_reservations)
      exact_keys.emplace(
          "military_formation_reservations",
          std::to_string(
              military_versions.value.military_formation_reservations));
    if (task_execution.value)
      exact_keys.emplace("task_execution_time",
                         std::to_string(task_execution.value));
    auto item_craft_versions =
        selected_native_item_craft_versions(identity.catalog);
    if (!item_craft_versions.ok)
      return {false, {}, std::move(item_craft_versions.error)};
    if (item_craft_versions.value.item_instances) {
      if (!complete)
        fail("ItemCraftCompleteProfileRequired", "checkpoint/module_versions");
      exact_keys.emplace(
          "item_instances",
          std::to_string(item_craft_versions.value.item_instances));
      exact_keys.emplace("lot_properties", "1");
    }
    if (item_craft_versions.value.craft_jobs)
      exact_keys.emplace("craft_jobs", "1");
    if (item_craft_versions.value.selected_resource_operations) {
      if (!complete)
        fail("SelectedResourceCompleteProfileRequired",
             "checkpoint/module_versions");
      exact_keys.emplace("selected_resource_operations", "1");
    }
    auto facility_versions =
        selected_native_economic_facility_versions(identity.catalog);
    if (!facility_versions.ok)
      return {false, {}, std::move(facility_versions.error)};
    if (facility_versions.value.facilities ||
        facility_versions.value.work_expense) {
      if (!complete)
        fail("EconomicFacilityCompleteProfileRequired",
             "checkpoint/module_versions");
    }
    if (facility_versions.value.facilities) {
      exact_keys.emplace("economic_facility_capacity_sources", "1");
      exact_keys.emplace("production_facility_conditions", "1");
    }
    if (facility_versions.value.work_expense)
      exact_keys.emplace("governance_work_expense",
                         std::to_string(facility_versions.value.work_expense));
    if (item_craft_versions.value.domain_activity) {
      if (!complete)
        fail("ItemCraftCompleteProfileRequired", "checkpoint/module_versions");
      exact_keys.emplace("domain_activity_reservations", "1");
    }
    auto shop_versions =
        selected_native_private_shop_operation_versions(identity.catalog);
    if (!shop_versions.ok)
      return {false, {}, std::move(shop_versions.error)};
    if (shop_versions.value.exact_operations()) {
      if (!complete)
        fail("ShopOperationCompleteProfileRequired",
             "checkpoint/module_versions");
      exact_keys.emplace("private_shop_operation_sources", "1");
      exact_keys.emplace("private_shop_operation_capabilities", "1");
    }
    auto custody_version =
        selected_native_private_shop_household_custody_version(
            identity.catalog);
    if (!custody_version.ok)
      return {false, {}, custody_version.error};
    if (custody_version.value) {
      if (!complete)
        fail("PrivateShopCustodyCompleteProfileRequired",
             "checkpoint/module_versions");
      exact_keys.emplace("private_shop_household_custody_sources", "1");
    }
    auto work_version = selected_native_task_work_version(identity.catalog);
    if (!work_version.ok)
      return {false, {}, std::move(work_version.error)};
    if (work_version.value) {
      if (!complete)
        fail("TaskWorkCompleteProfileRequired", "checkpoint/module_versions");
      exact_keys.emplace("task_work_execution",
                         std::to_string(work_version.value));
    }
    auto population_version =
        selected_native_population_transport_version(identity.catalog);
    if (!population_version.ok)
      return {false, {}, std::move(population_version.error)};
    auto production_version =
        selected_native_production_cycles_version(identity.catalog);
    if (!production_version.ok)
      return {false, {}, std::move(production_version.error)};
    if (production_version.value) {
      if (!complete)
        fail("ProductionCyclesCompleteProfileRequired",
             "checkpoint/module_versions");
      exact_keys.emplace("production_cycles", "3");
      exact_keys.emplace("civilian_transport_population", "3");
    }
    auto funding_version =
        selected_native_private_transport_funding_version(identity.catalog);
    if (!funding_version.ok)
      return {false, {}, std::move(funding_version.error)};
    if (funding_version.value) {
      if (!complete)
        fail("PrivateTransportFundingCompleteProfileRequired",
             "checkpoint/module_versions");
      exact_keys.emplace("private_transport_funding", "1");
    }
    auto transport_versions =
        selected_native_transport_versions(identity.catalog);
    if (!transport_versions.ok)
      return {false, {}, std::move(transport_versions.error)};
    if (transport_versions.value.travel) {
      if (!complete)
        fail("TransportCompleteProfileRequired", "checkpoint/module_versions");
      exact_keys.emplace("travel",
                         std::to_string(transport_versions.value.travel));
    }
    if (transport_versions.value.group)
      exact_keys.emplace("group_transport",
                         std::to_string(transport_versions.value.group));
    if (transport_versions.value.manifest)
      exact_keys.emplace("transport_manifest_bindings",
                         std::to_string(transport_versions.value.manifest));
    if (transport_versions.value.carrier_location)
      exact_keys.emplace("carrier_location", "1");
    auto boundary_extension =
        selected_native_movement_boundary_version(identity.catalog);
    if (!boundary_extension.ok)
      return {false, {}, std::move(boundary_extension.error)};
    if (boundary_extension.value)
      exact_keys.emplace("movement_boundary_contacts", "1");
    auto continuation_extension =
        selected_native_movement_boundary_continuation_version(identity.catalog);
    if (!continuation_extension.ok)
      return {false, {}, std::move(continuation_extension.error)};
    if (continuation_extension.value) {
      exact_keys.emplace(movement_boundary_continuation_module, "1");
      // 来源是既有identity字符串，不当作数字codec版本或另一个物理段。
      exact_keys.emplace(movement_boundary_continuation_derivation_key,
                         movement_boundary_continuation_derivation);
    }
    auto local_move_extension =
        selected_native_local_move_version(identity.catalog);
    if (!local_move_extension.ok)
      return {false, {}, std::move(local_move_extension.error)};
    if (local_move_extension.value) {
      exact_keys.emplace(local_move_module, "1");
      // 来源是既有identity字符串，不当作数字codec版本或另一个物理段。
      exact_keys.emplace(local_move_derivation_key,
                         "travel/" + std::to_string(transport_versions.value.travel));
    }
    auto handoff_extensions =
        selected_native_group_member_handoff_versions(identity.catalog);
    if (!handoff_extensions.ok)
      return {false, {}, std::move(handoff_extensions.error)};
    if (handoff_extensions.value.releases)
      exact_keys.emplace("movement_group_member_releases", "1");
    if (handoff_extensions.value.losses)
      exact_keys.emplace("movement_group_member_losses", "2");
    const bool social_exact = std::any_of(
        identity.catalog.module_versions.begin(),
        identity.catalog.module_versions.end(),
        [](const auto &row) { return row.first == "social_period_sources"; });
    if (social_exact)
      exact_keys.emplace("social_period_sources", "1");
    auto social_cut = selected_native_social_knowledge_cut(identity.catalog);
    if (!social_cut.ok)
      return {false, {}, std::move(social_cut.error)};
    if (social_cut.value) {
      if (!social_exact)
        fail("SocialKnowledgeCutExactSourcesRequired",
             "checkpoint/module_versions");
      exact_keys.emplace("social_knowledge_common_cut", "1");
    }
    auto original_social =
        selected_native_original_social_version(identity.catalog);
    if (!original_social.ok)
      return {false, {}, std::move(original_social.error)};
    auto propagation_cut =
        selected_native_social_propagation_cut(identity.catalog);
    if (!propagation_cut.ok)
      return {false, {}, std::move(propagation_cut.error)};
    if (original_social.value == 2) {
      if (!complete || !social_exact)
        fail("OriginalSocialCompleteSourcesRequired",
             "checkpoint/module_versions");
      exact_keys.emplace("social", "2");
    }
    if (propagation_cut.value) {
      if (!complete || !social_cut.value || original_social.value != 2)
        fail("SocialPropagationOriginalCutsRequired",
             "checkpoint/module_versions");
      exact_keys.emplace("social_propagation_adoption_cut", "1");
    }
    if (occupation_schema.value == 2)
      exact_keys.emplace("military_occupation", "2");
    if (growth_marker)
      exact_keys.emplace("growth_fixed_service_time2",
                         std::to_string(growth_phase));
    auto face_selected = selected_native_face_to_face_version(identity.catalog);
    if (!face_selected.ok)
      return {false, {}, std::move(face_selected.error)};
    if (face_selected.value) {
      if (!complete)
        fail("FaceToFaceCompleteProfileRequired", "checkpoint/module_versions");
      exact_keys.emplace("face_to_face_contacts", "3");
    }
    Integer social_occupancy = 0;
    for (const auto &row : identity.catalog.module_versions)
      if (row.first == "actor_social_occupancy") {
        if (social_occupancy ||
            (row.second != "1" && row.second != "2" && row.second != "3") ||
            !complete || !command_cuts)
          fail("CheckpointActorSocialOccupancyProfileMismatch", row.first);
        social_occupancy = row.second == "3" ? 3 : row.second == "2" ? 2 : 1;
      }
    if (social_occupancy)
      exact_keys.emplace("actor_social_occupancy",
                         std::to_string(social_occupancy));
    if (interaction_response.value) {
      if (!complete || !life4 || social_occupancy != 3)
        fail("InteractionResponseOriginalLifeAndOpeningRequired",
             "checkpoint/module_versions");
      exact_keys.emplace("interaction",
                         std::to_string(interaction_response.value));
    }
    Integer household_version = 0;
    bool retail_selected = false;
    for (const auto &row : identity.catalog.module_versions) {
      if (row.first == "household_purchases") {
        if (household_version || (row.second != "2" && row.second != "3"))
          fail("CheckpointHouseholdProfileMismatch", row.first);
        household_version = row.second == "2" ? 2 : 3;
      }
      if (row.first == "household_retail_settlements") {
        if (retail_selected || row.second != "1")
          fail("CheckpointHouseholdProfileMismatch", row.first);
        retail_selected = true;
      }
    }
    if (bool(household_version) != retail_selected ||
        (household_version && !complete))
      fail("CheckpointHouseholdProfileMismatch", "checkpoint/module_versions");
    if (household_version) {
      exact_keys.emplace("household_purchases",
                         std::to_string(household_version));
      exact_keys.emplace("household_retail_settlements", "1");
    }

    auto market_version =
        selected_native_growth_market_version(identity.catalog);
    if (!market_version.ok)
      return {false, {}, std::move(market_version.error)};
    if (market_version.value == 2) {
      std::size_t selected_resources = 0;
      for (const auto &row : identity.catalog.module_versions)
        if (row.first == "selected_resource_operations") {
          if (row.second != "1")
            fail("CheckpointMarketHistory2ProfileMismatch",
                 "checkpoint/module_versions");
          ++selected_resources;
        }
      if (!complete || selected_resources != 1)
        fail("CheckpointMarketHistory2ProfileMismatch",
             "checkpoint/module_versions");
    }
    if (complete) {
      exact_keys.emplace("facility_claims", "2");
      if (!social_exact)
        fail("CheckpointCompleteSocialSourceRequired",
             "checkpoint/module_versions");
    }
    if (inspection_version.value)
      exact_keys.emplace("public_supply_inspection", "1");
    if (field_passage_version.value) {
      exact_keys.emplace("field_group_passage_catalog", "1");
      exact_keys.emplace("political_passage", "3");
    }
    if (relations_version.value) {
      exact_keys.emplace("political_relations_catalog", "1");
      exact_keys.emplace("political_relations", "1");
    }
    bool exact = false;
    for (const auto &row : identity.catalog.module_versions)
      if (exact_keys.count(row.first))
        exact = true;
    if (exact) {
      CheckpointIdentity base = identity;
      std::set<StableId> seen;
      std::vector<std::pair<StableId, StableId>> kept;
      for (const auto &row : base.catalog.module_versions) {
        if (row.first == "field_battle")
          continue;
        auto key = exact_keys.find(row.first);
        if (key == exact_keys.end())
          kept.push_back(row);
        else if (key->second != row.second || !seen.insert(row.first).second)
          fail("CheckpointExactProfileMismatch", "checkpoint/module_versions");
      }
      if (seen.size() != exact_keys.size())
        fail("CheckpointExactProfileMismatch", "checkpoint/module_versions");
      if (!complete) {
        const std::map<StableId, StableId> allowed = {
            {"dynamic_world", "local_society_v1"},
            {"local_economy", "1"},
            {"organization", "2"},
            {"affairs", "1"},
            {"director", "2"},
            {"regional_life", "1"}};
        std::set<StableId> base_seen;
        for (const auto &row : kept) {
          auto it = allowed.find(row.first);
          if (it == allowed.end() || it->second != row.second ||
              !base_seen.insert(row.first).second)
            fail("CheckpointExactProfileMismatch",
                 "checkpoint/module_versions");
        }
        if (base_seen.size() != allowed.size())
          fail("CheckpointExactProfileMismatch", "checkpoint/module_versions");
      }
      // Full Time2 retains every original selected module. Their original
      // finite selector below still rejects unsupported versions and dependency
      // pairs. Resolve /4 over the unchanged original integration shape. The
      // actual catalogue still carries /4; this temporary identity only selects
      // the original finite domain order before restoring that one physical
      // tail.
      // 仅借原 /1 的域顺序，返回前恢复已锁定的 /2 物理格式。
      for (auto &row : kept)
        if ((row.first == "politics" || row.first == "political_fiscal" ||
             row.first == "political_takeover") &&
            row.second == "2")
          row.second = "1";
      if (exact_population) {
        const bool integration =
            std::any_of(kept.begin(), kept.end(), [](const auto &row) {
              return row.first == "siege_integration" && row.second == "1";
            });
        for (auto &row : kept)
          if (row.first == "population_military" && row.second == "4")
            row.second = integration ? "2" : "1";
      }
      if (life4)
        for (auto &row : kept)
          if (row.first == "family_life" && row.second == "4")
            row.second = "1";
      if (birth_version.value == 4)
        for (auto &row : kept)
          if (row.first == "family_birth" && row.second == "4")
            row.second = "1";
      if (organization_runtime.value)
        for (auto &row : kept)
          if (row.first == "organization")
            row.second =
                (organization_body == 5 || organization_body == 7) ? "3" : "2";
      if (obligation_versions.value.sources ||
          obligation_versions.value.named_birth_population)
        for (auto &row : kept)
          if (row.first == "local_economy")
            row.second = std::to_string(
                obligation_versions.value.local_economy -
                (obligation_versions.value.named_birth_population ? 10 : 0) -
                (obligation_versions.value.sources ? 5 : 0));
      // 原基形守卫仍只读 registry/1 + physical4；同根后继随后恢复显式5或6。
      if (director_subjects)
        for (auto &row : kept)
          if (row.first == "director")
            row.second = "4";
      // 只取原完整前缀顺序，实际声明版本与依赖已在上方逐项核定。
      if (interaction_sections.value.exact_sources())
        for (auto &row : kept) {
          if (row.first == "visit_messages" ||
              row.first == "personal_appointments" ||
              row.first == "contact_inquiries" ||
              row.first == "visit_appointment_invitations" ||
              row.first == "visit_appointment_meetings" ||
              row.first == visit_message_receipt_module)
            row.second = "1";
          else if (row.first == "visit_message_replies")
            row.second =
                visit_reply_authored_version(interaction_sections.value.replies)
                    ? "2"
                    : "1";
        }
      if (market_version.value == 2)
        for (auto &row : kept)
          if (row.first == "growth_market_history")
            row.second = "1";
      base.catalog.module_versions = std::move(kept);
      auto domains = selected_checkpoint_domains_impl(
          base, population_shape_only || exact_population);
      if (!domains.ok)
        return domains;
      if (market_version.value == 2)
        for (auto &domain : domains.value)
          if (domain.name == "growth_market_history")
            domain.codec_version = 2;
      if (interaction_response.value) {
        auto original =
            interaction_response_base_version(interaction_response.value);
        if (!original.ok)
          return {false, {}, std::move(original.error)};
        auto actual = std::find_if(
            domains.value.begin(), domains.value.end(),
            [](const auto &domain) { return domain.name == "interaction"; });
        if (actual == domains.value.end() ||
            actual->codec_version != static_cast<std::uint32_t>(original.value))
          fail("InteractionOriginalBaseVersionMismatch",
               "checkpoint/interaction");
        actual->codec_version =
            static_cast<std::uint32_t>(interaction_response.value);
      }
      for (auto &domain : domains.value)
        if (domain.name == "social")
          domain.codec_version =
              static_cast<std::uint32_t>(original_social.value);
      if (life4)
        for (auto &domain : domains.value)
          if (domain.name == "family_life")
            domain.codec_version = 4;
      if (birth_version.value == 4)
        for (auto &domain : domains.value)
          if (domain.name == "family_birth")
            domain.codec_version = 4;
      for (auto &domain : domains.value) {
        const auto &v = interaction_sections.value;
        if (domain.name == "visit_messages")
          domain.codec_version = static_cast<std::uint32_t>(v.messages);
        else if (domain.name == "personal_appointments")
          domain.codec_version = static_cast<std::uint32_t>(v.personal);
        else if (domain.name == "contact_inquiries")
          domain.codec_version = static_cast<std::uint32_t>(v.inquiry);
        else if (domain.name == "visit_appointment_invitations")
          domain.codec_version = static_cast<std::uint32_t>(v.invitations);
        else if (domain.name == "visit_appointment_meetings")
          domain.codec_version = static_cast<std::uint32_t>(v.meetings);
        else if (domain.name == visit_message_receipt_module)
          domain.codec_version = static_cast<std::uint32_t>(v.receipts);
        else if (domain.name == "visit_message_replies")
          domain.codec_version = static_cast<std::uint32_t>(v.replies);
      }

      for (auto &domain : domains.value) {
        if (domain.name == "politics")
          domain.codec_version =
              static_cast<std::uint32_t>(political_body.value);
        if (domain.name == "political_fiscal")
          domain.codec_version = static_cast<std::uint32_t>(fiscal_body.value);
        if (domain.name == "political_takeover")
          domain.codec_version =
              static_cast<std::uint32_t>(takeover_body.value);
      }
      if (director_subjects)
        for (auto &domain : domains.value)
          if (domain.name == "director")
            domain.codec_version =
                static_cast<std::uint32_t>(director_physical.value);
      if (exact_population)
        for (auto &domain : domains.value)
          if (domain.name == "population_military")
            domain.codec_version = 4;
      if (occupation_schema.value == 2)
        for (auto &domain : domains.value)
          if (domain.name == "military") {
            if (domain.codec_version < 3 || domain.codec_version > 17)
              fail("MilitaryOccupationCheckpointProfileMismatch", "military");
            domain.codec_version += 32;
          }
      for(auto& domain:domains.value)if(domain.name=="military")domain.codec_version+=64;
      for (auto &d : domains.value)
        if (d.name == "growth") {
          if (complete) {
            if (d.codec_version < 3 || d.codec_version > 34)
              fail("CheckpointExactGrowthBaseMismatch", "growth");
            d.codec_version += capability_chain ? 192
                               : rest_mouths    ? 128
                               : command_cuts   ? 96
                                                : 64;
          } else {
            if (d.codec_version != 4)
              fail("CheckpointExactGrowthBaseMismatch", "growth");
            d.codec_version = 36;
          }
        }
      for (const auto &d : std::initializer_list<CheckpointDomainView>{
               {"world_timeline", 2},
               {"action_elapsed", 2},
               {"actor_start_context", complete ? 3u : 2u},
               {"character_origin_records", 1}})
        domains.value.push_back({StableId{d.name}, d.codec_version});
      if (capability_chain) {
        domains.value.push_back({"growth_permanent_contributions", 3});
        domains.value.push_back({"trade-grant", 1});
      }
      domains.value.push_back(
          {"character_condition_extensions", injury_sources     ? 8u
                                             : capability_chain ? 7u
                                                                : 1u});
      if (injury_sources)
        domains.value.push_back({"character-injury-sources", 1});
      if (complete) {
        domains.value.push_back({"growth_terminal_publications", 1});
        domains.value.push_back({"facility_claims", 2});
      }
      if (household_version) {
        domains.value.push_back(
            {"household_purchases",
             static_cast<std::uint32_t>(household_version)});
        domains.value.push_back({"household_retail_settlements", 1});
      }
      if (social_exact) {
        for (auto &d : domains.value)
          if (d.name == "dynamic_social") {
            auto selected = select_social_time2_physical_version(
                d.codec_version, social_cut.value, propagation_cut.value);
            if (!selected.ok)
              return {false, {}, std::move(selected.error)};
            d.codec_version = static_cast<std::uint32_t>(selected.value);
          }
        domains.value.push_back({"social_period_sources", 1});
      }
      if (face_selected.value)
        domains.value.push_back({"face_to_face_contacts", 3});
      if (social_occupancy)
        domains.value.push_back({"actor_social_occupancy",
                                 static_cast<std::uint32_t>(social_occupancy)});
      if (age_checks)
        domains.value.push_back({"life_age_checks", 1});
      if (control_version.value)
        domains.value.push_back(
            {"control_continuation",
             static_cast<std::uint32_t>(control_version.value)});
      if (item_craft_versions.value.item_instances) {
        domains.value.push_back(
            {"item_instances", static_cast<std::uint32_t>(
                                   item_craft_versions.value.item_instances)});
        domains.value.push_back({"lot_properties", 1});
      }
      if (item_craft_versions.value.craft_jobs)
        domains.value.push_back({"craft_jobs", 1});
      if (facility_versions.value.facilities) {
        domains.value.push_back({"economic_facility_capacity_sources", 1});
        domains.value.push_back({"production_facility_conditions", 1});
      }
      if (facility_versions.value.work_expense)
        domains.value.push_back(
            {"governance_work_expense",
             static_cast<std::uint32_t>(facility_versions.value.work_expense)});
      if (organization_runtime.value) {
        for (auto &domain : domains.value)
          if (domain.name == "organization")
            domain.codec_version =
                static_cast<std::uint32_t>(organization_body);
        domains.value.push_back({"organization_runtime", 1});
      }
      if (organization_effects.value)
        domains.value.push_back({"organization_service_effects", 1});
      if (governance_versions.value.state)
        domains.value.push_back(
            {"governance_programs",
             static_cast<std::uint32_t>(governance_versions.value.state)});
      if (obligation_versions.value.sources ||
          obligation_versions.value.named_birth_population)
        for (auto &domain : domains.value)
          if (domain.name == "local_economy")
            domain.codec_version = static_cast<std::uint32_t>(
                obligation_versions.value.local_economy);
      if (obligation_versions.value.sources)
        domains.value.push_back({"economic_obligation_sources", 1});
      if (military_versions.value.dynamic_military)
        domains.value.push_back(
            {"dynamic_military",
             static_cast<std::uint32_t>(
                 military_versions.value.dynamic_military)});
      if (personal_lifecycle.value)
        domains.value.push_back({"personal_battle_lifecycle", 1});
      if (battle_lifecycle.value)
        domains.value.push_back({"military_battle_lifecycle", 1});
      if (field_version.value)
        domains.value.push_back(
            {"field_battle", static_cast<std::uint32_t>(
                                 field_battle_checkpoint_codec_revision)});
      if (relations_version.value)
        domains.value.push_back({"political_relations", 1});
      if (field_passage_version.value)
        domains.value.push_back({"political_passage", 3});
      if (inspection_version.value)
        domains.value.push_back({"public_supply_inspection", 1});
      if (military_versions.value.military_action_time)
        domains.value.push_back({"military_action_time", 1});
      if (military_versions.value.military_authority_time)
        domains.value.push_back({"military_authority_time", 1});
      if (military_versions.value.military_action_resources)
        domains.value.push_back({"military_action_resources", 1});
      if (military_versions.value.military_return_detachments)
        domains.value.push_back({"military_return_detachments", 1});
      if (military_versions.value.military_action_conditions)
        domains.value.push_back(
            {"military_action_conditions",
             static_cast<std::uint32_t>(
                 military_versions.value.military_action_conditions)});
      if (military_versions.value.military_training_completion_time)
        domains.value.push_back({"military_training_completion_time", 1});
      if (military_versions.value.military_formation_reservations)
        domains.value.push_back(
            {"military_formation_reservations",
             static_cast<std::uint32_t>(
                 military_versions.value.military_formation_reservations)});
      if (shop_versions.value.exact_operations()) {
        domains.value.push_back({"private_shop_operation_sources", 1});
        domains.value.push_back({"private_shop_operation_capabilities", 1});
      }
      if (custody_version.value)
        domains.value.push_back({"private_shop_household_custody_sources", 1});
      if (work_version.value)
        domains.value.push_back(
            {"task_work_execution",
             static_cast<std::uint32_t>(work_version.value)});
      if (production_version.value) {
        domains.value.push_back({"production_cycles", 3});
        domains.value.push_back({"civilian_transport_population", 3});
      }
      if (funding_version.value)
        domains.value.push_back({"private_transport_funding", 1});
      if (task_execution.value)
        domains.value.push_back(
            {"task_execution_time",
             static_cast<std::uint32_t>(task_execution.value)});
      auto transport =
          apply_native_transport_domains(domains.value, identity.catalog);
      if (!transport.ok)
        return {false, {}, std::move(transport.error)};
      return domains;
    }
    static const std::map<StableId, std::set<StableId>> supported = {
        {"field_battle", {"1"}},
        {"dynamic_world", {"local_society_v1"}},
        {"local_economy", {"1", "2", "3", "4", "5"}},
        {"organization", {"2", "3"}},
        {"affairs", {"1", "2", "3", "4"}},
        {"director", {"1", "2", "3", "4"}},
        {"population_military", {"1", "2"}},
        {"regional_life", {"1"}},
        {"siege_integration", {"1"}},
        {"named_membership", {"1"}},
        {"study_segments", {"1"}},
        {"player_plan", {"1", "2"}},
        {"director_registry", {"1"}},
        {"obligation_needs", {"1"}},
        {"skills_extended", {"1"}},
        {"growth_cards", {"1"}},
        {"politics", {"1"}},
        {"political_fiscal", {"1"}},
        {"treaty_passage", {"1"}},
        {"political_occupation_fiscal", {"1"}},
        {"occupation_fiscal_settlement", {"1"}},
        {"political_takeover", {"1"}},
        {"economic_takeover_execution", {"1"}},
        {"task_takeover_execution", {"1", "2"}},
        {"production_management", {"1"}},
        {"household_provisioning", {"1"}},
        {"fiscal_settlement", {"1"}},
        {"fiscal_taxpayer_continuity", {"1"}},
        {"politics_governance", {"1"}},
        {"governance_tasks", {"1"}},
        {"public_relief", {"1"}},
        {"task_offer_responses", {"1", "2"}},
        {"task_personal_advances", {"1"}},
        {"task_message_handoffs", {"1", "2", "3", "4"}},
        {"task_message_notices", {"1"}},
        {"task_static_assets", {"1"}},
        {"fiscal_need_policies", {"1"}},
        {"occupation_fiscal_needs", {"1"}},
        {"cohort_wage_need_policies", {"1"}},
        {"teaching_requests", {"1"}},
        {"study_teaching_requests", {"1"}},
        {"diplomacy", {"1", "2"}},
        {"envoy_tasks", {"1", "2"}},
        {"military_requests", {"1"}},
        {"military_officer_assignments", {"1"}},
        {"military_field_commands", {"1"}},
        {"military_wounded_recovery", {"1"}},
        {"military_supply_attrition", {"1"}},
        {"military_rout_retreat", {"1"}},
        {"military_command_shock", {"1"}},
        {"military_garrison_rest", {"1"}},
        {"military_combat_conditions", {"1"}},
        {"military_commanded_damage", {"1"}},
        {"military_sequential_rounds", {"1"}},
        {"military_field_terrain", {"1"}},
        {"military_command_resources", {"1"}},
        {"military_tactic_learning", {"1"}},
        {"military_siege_engineering", {"1"}},
        {"political_field_appointments", {"1"}},
        {"political_military_appointments", {"1"}},
        {"military_naval", {"1", "2", "3", "4", "5", "6"}},
        {"naval_gunnery", {"1"}},
        {"naval_repairs", {"1"}},
        {"naval_repair_charges", {"1"}},
        {"naval_navigation_wear", {"1"}},
        {"naval_tactical_navigation", {"1"}},
        {"household_control", {"1"}},
        {"household_provision_management", {"1"}},
        {"regional_outcome", {"1"}},
        {"personal_simultaneous_outcomes", {"1"}},
        {"party_invitations", {"1"}},
        {"party_recruitment_consents", {"1"}},
        {"selected_existing_actor", {"1", "2"}},
        {"actor_card_unlocks", {"1", "2"}},
        {"growth_insights", {"1"}},
        {"diplomatic_response", {"1"}},
        {"authored_task_continuation", {"1", "2"}},
        {"character_creation", {"1"}},
        {"production_disruptions", {"1"}},
        {"adult_adoption", {"1"}},
        {"voluntary_succession", {"1", "2"}},
        {"family_voluntary_paths", {"1"}},
        {"family_life", {"1"}},
        {"family_legacy", {"1", "2", "3"}},
        {"family_collateral_legacy", {"1"}},
        {"task_message_returns", {"1"}},
        {"task_personal_message_deliveries", {"1"}},
        {"task_personal_message_reports", {"1"}},
        {"task_personal_message_acceptances", {"1"}},
        {"task_personal_message_cancellations", {"1"}},
        {"family_biological_legacy", {"1"}},
        {"political_head_handovers", {"1"}},
        {"political_successor_rounds", {"1"}},
        {"political_term_handovers", {"1"}},
        {"political_office_receptions", {"1"}},
        {"political_office_resignations", {"1", "2"}},
        {"political_subordinate_appointments", {"1"}},
        {"political_dismissal_sources", {"1"}},
        {"political_subordinate_appointment_intents", {"1"}},
        {"political_office_resignation_responses", {"1"}},
        {"political_governance_agencies", {"1", "2"}},
        {"political_governance_agency_intents", {"1"}},
        {"political_successions", {"1"}},
        {"family_bequest", {"1"}},
        {"family_inheritance", {"1"}},
        {"family_marriage", {"1"}},
        {"estate_employment", {"1"}},
        {"economic_supply_policies", {"1"}},
        {"political_supply_adoptions", {"1"}},
        {"family_birth", {"1"}},
        {"visit_messages", {"1"}},
        {"visit_message_receipts", {"1"}},
        {"visit_reception_intents", {"1"}},
        {"visit_reception_claims", {"1"}},
        {"family_lineage", {"1"}},
        {"political_supply_renewals", {"1"}},
        {"family_innate_learning", {"1"}},
        {"family_care_relief", {"1"}},
        {"task_extensions", {"1"}},
        {"growth_market_history", {"1"}},
        {"political_shop_licensing", {"1", "2"}},
        {"private_shops", {"1", "2", "3"}},
        {"task_shop_orders", {"1"}},
        {"political_shop_registry_successions", {"1"}},
        {"political_shop_licensing_receipts", {"1"}},
        {"private_shop_liquidations", {"1"}},
        {"task_shop_order_estate_cancellations", {"1"}},
        {"family_apprenticeship", {"1"}},
        {"family_disciple_legacy", {"1"}},
        {"political_governance_continuations", {"1"}},
        {"political_governance_invitation_replies", {"1"}},
        {"private_shop_daily_rent", {"1"}},
        {"private_shop_command_results", {"1"}},
        {"political_governance_intents", {"1"}},
        {"personal_appointments", {"1"}},
        {"contact_inquiries", {"1"}},
        {"visit_message_replies", {"1", "2"}},
        {"visit_reply_intents", {"1"}},
        {"born_personal_dialogues", {"1"}},
        {"visit_appointment_invitations", {"1"}},
        {"visit_appointment_meetings", {"1"}},
        {"visit_appointment_intents", {"1"}},
        {"visit_appointment_attendance", {"1"}},
        {"growth_tactic_lessons", {"1"}},
        {"known_personal_dialogues", {"1"}},
        {"task_shop_order_contacts", {"1"}},
        {"personal_injury_recovery", {"1"}},
        {"political_shop_registry_intents", {"1"}},
        {"personal_appointment_attendance", {"1"}}};
    std::map<StableId, StableId> modules;
    for (const auto &marker : identity.catalog.module_versions) {
      const auto allowed = supported.find(marker.first);
      if (allowed == supported.end() || !allowed->second.count(marker.second) ||
          !modules.emplace(marker.first, marker.second).second)
        fail("CheckpointProfileMismatch", "checkpoint/module_versions");
    }
    const auto has = [&](const char *name) { return modules.count(name) != 0; };
    auto simultaneous = validate_personal_simultaneous_outcome_modules(
        identity.catalog.module_versions,
        has(personal_simultaneous_outcomes_module));
    if (!simultaneous.ok)
      return {false, {}, simultaneous.error};
    auto innate_layout = validate_innate_learning_layout(
        has("family_innate_learning"), identity.catalog.module_versions);
    if (!innate_layout.ok)
      return {false, {}, innate_layout.error};
    if (has("naval_gunnery") !=
        (has("military_naval") && modules.at("military_naval") == "6"))
      fail("CheckpointProfileMismatch", "checkpoint/naval_gunnery");
    if (has("naval_repairs") !=
        (has("military_naval") && (modules.at("military_naval") == "3" ||
                                   modules.at("military_naval") == "4" ||
                                   (modules.at("military_naval") == "5" ||
                                    modules.at("military_naval") == "6"))))
      fail("CheckpointProfileMismatch", "checkpoint/naval_repairs");
    if (has("naval_repair_charges") !=
        (has("military_naval") && (modules.at("military_naval") == "4" ||
                                   (modules.at("military_naval") == "5" ||
                                    modules.at("military_naval") == "6"))))
      fail("CheckpointProfileMismatch", "checkpoint/naval_repair_charges");
    if (has("naval_navigation_wear") !=
        (has("military_naval") && (modules.at("military_naval") == "5" ||
                                   modules.at("military_naval") == "6")))
      fail("CheckpointProfileMismatch", "checkpoint/naval_navigation_wear");
    if (has("naval_tactical_navigation") &&
        (!has("military_naval") || (modules.at("military_naval") != "2" &&
                                    modules.at("military_naval") != "3" &&
                                    modules.at("military_naval") != "4" &&
                                    modules.at("military_naval") != "5" &&
                                    modules.at("military_naval") != "6")))
      fail("CheckpointProfileMismatch", "checkpoint/naval_tactical_navigation");
    const bool dynamic = identity.catalog.ruleset_id == dynamic_ruleset_id;
    if (dynamic != has("dynamic_world"))
      fail("CheckpointProfileMismatch", "checkpoint/dynamic_world");
    if (!dynamic)
      for (const auto &marker : modules)
        if (marker.first != "study_segments")
          fail("CheckpointProfileMismatch", "checkpoint/module_versions");
    const bool population = has("population_military"),
               regional = has("regional_life");
    const bool siege = has("siege_integration"),
               named = has("named_membership");
    auto domains = dynamic ? (population ? population_checkpoint_domains
                                         : dynamic_checkpoint_domains)
                           : checkpoint_domains;
    if (regional)
      domains = regional_domains(std::move(domains));
    if (siege)
      domains = siege_domains(std::move(domains));
    if (named)
      domains = named_domains(std::move(domains));
    if (has("regional_outcome")) {
      if (!regional)
        fail("CheckpointProfileMismatch", "checkpoint/regional_outcome");
      for (auto &domain : domains)
        if (domain.name == "interaction")
          domain.codec_version = 4;
    }
    if (has("study_segments"))
      domains = study_domains(std::move(domains));
    if (has("player_plan")) {
      auto plan = read_player_plan_selected_version(identity.catalog);
      if (!plan.ok)
        return {false, {}, plan.error};
      if (plan.value == 2 &&
          (!has("world_timeline") || !has("action_elapsed") ||
           !has("facility_claims")))
        return {
            false, {}, common_failure("PlayerPlanTime2ProfileRequired").error};
      domains.push_back(
          {"player_plan", static_cast<std::uint32_t>(plan.value)});
    }
    if (has("director_registry"))
      domains = registry_domains(std::move(domains));
    if (has("obligation_needs"))
      domains = obligation_domains(std::move(domains));
    if (has("skills_extended"))
      domains = extended_skill_domains(std::move(domains));
    if (has("growth_cards"))
      domains = card_domains(std::move(domains));
    if (has("politics"))
      domains.push_back({"politics", 1});
    if (has("political_fiscal"))
      domains.push_back({"political_fiscal", 1});
    if (has("household_provisioning") || has("production_management"))
      domains = economic_domains(std::move(domains),
                                 has("household_provisioning") ? 5 : 4);
    if (has("fiscal_settlement"))
      domains.push_back({"fiscal_settlement", 1});
    auto continuity_layout = validate_fiscal_taxpayer_continuity_layout(
        has("fiscal_taxpayer_continuity"), identity.catalog.module_versions);
    if (!continuity_layout.ok)
      return {false, {}, continuity_layout.error};
    if (has("politics_governance"))
      domains = governance_domains(std::move(domains));
    if (has("task_offer_responses"))
      domains.push_back({"task_offer_responses",
                         modules.at("task_offer_responses") == "2" ? 2 : 1});
    if (has("task_personal_advances"))
      domains.push_back({"task_personal_advances", 1});
    if (has("task_message_handoffs")) {
      if (!dynamic)
        fail("TaskMessageHandoffProfileMismatch", "checkpoint/module_versions");
      domains.push_back({"task_message_handoffs",
                         modules.at("task_message_handoffs") == "4"   ? 4u
                         : modules.at("task_message_handoffs") == "3" ? 3u
                         : modules.at("task_message_handoffs") == "2" ? 2u
                                                                      : 1u});
    }
    if (has("fiscal_need_policies"))
      domains = fiscal_need_domains(std::move(domains));
    if (has("cohort_wage_need_policies"))
      domains = cohort_wage_need_domains(std::move(domains));
    if (has("teaching_requests"))
      domains = teaching_domains(std::move(domains));
    if (has("study_teaching_requests"))
      domains = study_teaching_domains(std::move(domains));
    if (has("diplomacy"))
      domains = diplomacy_domains(std::move(domains),
                                  modules.at("diplomacy") == "2" ? 2 : 1);
    if (has("military_requests"))
      domains = request_domains(std::move(domains));
    {
      auto pair = validate_military_siege_engineering_modules(
          identity.catalog.module_versions);
      if (!pair.ok)
        return {false, {}, pair.error};
    }
    if (has("military_officer_assignments")) {
      if (!dynamic)
        fail("MilitaryOfficerAssignmentProfileMismatch", "checkpoint/military");
      domains = officer_domains(std::move(domains));
    }
    if (has(military_field_command_module))
      domains = field_command_domains(std::move(domains));
    if (has(military_recovery_module))
      domains = military_recovery_domains(std::move(domains));
    if (has(military_supply_attrition_module))
      domains = military_supply_attrition_domains(std::move(domains));
    if (has(military_rout_module))
      domains = military_rout_domains(std::move(domains));
    if (has(military_command_shock_module))
      domains = military_command_shock_domains(std::move(domains));
    if (has(military_garrison_rest_module))
      domains = military_garrison_rest_domains(std::move(domains));
    if (has(military_combat_conditions_module))
      domains = military_combat_conditions_domains(std::move(domains));
    if (has(military_commanded_damage_module))
      domains = military_commanded_damage_domains(std::move(domains));
    if (has(military_command_resources_module))
      domains = military_command_resources_domains(std::move(domains));
    if (has(military_tactic_learning_module))
      domains = military_tactic_learning_domains(std::move(domains));
    if (has(military_siege_engineering_module))
      domains = military_siege_engineering_domains(std::move(domains));
    if (has("military_naval"))
      domains.push_back(
          {"military_naval", modules.at("military_naval") == "6" ? 6u
                             : (modules.at("military_naval") == "5" ||
                                modules.at("military_naval") == "6")
                                 ? 5u
                             : modules.at("military_naval") == "4" ? 4u
                             : modules.at("military_naval") == "3" ? 3u
                             : modules.at("military_naval") == "2" ? 2u
                                                                   : 1u});
    if (has("household_control"))
      domains = household_control_domains(std::move(domains));
    if (has("household_provision_management"))
      domains.push_back({"household_provision_management", 1});
    if (has("character_creation"))
      domains = character_creation_domains(std::move(domains));
    if (has("production_disruptions"))
      domains.push_back({"production_disruptions", 1});
    if (has("adult_adoption"))
      domains = adult_adoption_domains(std::move(domains));
    if (has("voluntary_succession"))
      domains = voluntary_succession_domains(std::move(domains));
    if (has("political_takeover") != has("economic_takeover_execution") ||
        has("political_takeover") != has("task_takeover_execution") ||
        (has("political_takeover") && (!dynamic || !has("politics"))))
      fail("PoliticalTakeoverProfileMismatch", "checkpoint/module_versions");
    if (has("political_takeover"))
      domains = takeover_domains(
          std::move(domains),
          modules.at("task_takeover_execution") == "2" ? 2 : 1);
    if (has("family_life"))
      domains.push_back({"family_life", 1});
    if (has("family_legacy"))
      domains.push_back(
          {"family_legacy", modules.at("family_legacy") == "3"   ? 3u
                            : modules.at("family_legacy") == "2" ? 2u
                                                                 : 1u});
    if (has("political_occupation_fiscal") !=
            has("occupation_fiscal_settlement") ||
        (has("political_occupation_fiscal") &&
         (!has("political_takeover") || !has("fiscal_settlement") ||
          !has("political_fiscal"))))
      fail("OccupationFiscalProfileMismatch", "checkpoint/module_versions");
    if (has("political_occupation_fiscal"))
      domains = occupation_fiscal_domains(std::move(domains));
    auto fiscal_needs_layout = validate_occupation_fiscal_need_layout(
        has("occupation_fiscal_needs"), identity.catalog.module_versions);
    if (!fiscal_needs_layout.ok)
      return {false, {}, fiscal_needs_layout.error};
    if (has("family_bequest"))
      domains.push_back({"family_bequest", 1});
    if (has("task_message_notices")) {
      if (!dynamic || !has("family_life") || !has("task_message_handoffs"))
        fail("TaskMessageNoticeProfileMismatch", "checkpoint/module_versions");
      domains.push_back({"task_message_notices", 1});
    }
    if (has("treaty_passage")) {
      if (!has("politics") || !has("diplomacy") || !has("envoy_tasks"))
        fail("TreatyPassageProfileMismatch", "checkpoint/module_versions");
      domains.push_back({"treaty_passage", 1});
    }
    if (has("family_inheritance")) {
      if (!dynamic || !has("family_bequest") || !has("task_message_notices"))
        fail("FamilyInheritanceProfileMismatch", "checkpoint/module_versions");
      domains.push_back({"family_inheritance", 1});
    }
    if (has("fiscal_taxpayer_continuity"))
      domains.push_back({"fiscal_taxpayer_continuity", 1});
    if (has(military_officer_political_module))
      domains.push_back({military_officer_political_module, 1});
    if (has("family_marriage")) {
      if (!dynamic || !has("family_life"))
        fail("FamilyMarriageProfileMismatch", "checkpoint/module_versions");
      domains.push_back({"family_marriage", 1});
    }
    if (has("task_static_assets")) {
      if (!dynamic)
        fail("TaskStaticAssetProfileMismatch", "checkpoint/module_versions");
      domains.push_back({"task_static_assets", 1});
    }
    auto employment_layout = validate_estate_employment_layout(
        has("estate_employment"), identity.catalog.module_versions);
    if (!employment_layout.ok)
      return {false, {}, employment_layout.error};
    if (has("estate_employment"))
      domains.push_back({"estate_employment", 1});
    if (has(military_field_command_political_module))
      domains.push_back({military_field_command_political_module, 1});
    auto supply_layout = validate_economic_supply_layout(
        has("economic_supply_policies"), identity.catalog.module_versions);
    if (!supply_layout.ok)
      return {false, {}, supply_layout.error};
    if (has("economic_supply_policies")) {
      domains.push_back({"economic_supply_policies", 1});
      domains.push_back({"political_supply_adoptions", 1});
    }
    if (has("family_birth") != has("family_lineage") ||
        (has("family_birth") &&
         (!dynamic || !has("family_life") || !has("skills_extended"))))
      fail("FamilyBirthProfileMismatch", "checkpoint/module_versions");
    if (has("family_birth"))
      domains.push_back({"family_birth", 1});

    auto renewal_layout = validate_political_supply_renewal_modules(
        has("political_supply_renewals"), identity.catalog.module_versions);
    if (!renewal_layout.ok)
      return {false, {}, renewal_layout.error};
    auto registry_intents_layout =
        validate_political_shop_registry_intent_modules(
            has("political_shop_registry_intents"),
            identity.catalog.module_versions);
    if (!registry_intents_layout.ok)
      return {false, {}, registry_intents_layout.error};
    if (has("visit_messages")) {
      if (!dynamic)
        fail("VisitMessageProfileMismatch", "checkpoint/module_versions");
      domains.push_back({"visit_messages", 1});
    }
    if (has("visit_reception_intents") != has("visit_reception_claims") ||
        (has("visit_reception_intents") &&
         (!dynamic || !has("visit_messages"))))
      fail("VisitReceptionProfileMismatch", "checkpoint/module_versions");
    if (has("visit_reception_claims"))
      domains.push_back({"visit_reception_claims", 1});
    if (has("family_care_relief")) {
      if (!dynamic || !has("family_birth") || !has("family_lineage") ||
          !has("family_life"))
        fail("ChildCareReliefProfileMismatch", "checkpoint/module_versions");
      domains.push_back({"family_care_relief", 1});
    }

    auto shop_pair = validate_private_shop_pair(
        has("private_shops"), identity.catalog.module_versions);
    if (!shop_pair.ok)
      return {false, {}, shop_pair.error};
    if ((has("growth_market_history") && (!dynamic || !has("regional_life"))) ||
        (has("private_shops") && !has("politics")) ||
        (has("task_shop_orders") && !has("private_shops")))
      fail("PrivateShopProfileMismatch", "checkpoint/module_versions");
    if (has("growth_market_history")) {
      auto selected = selected_native_growth_market_version(identity.catalog);
      if (!selected.ok)
        return {false, {}, std::move(selected.error)};
      if (!selected.value)
        return {false,
                {},
                common_failure("GrowthMarketHistoryProfileMismatch").error};
      domains.push_back({"growth_market_history",
                         static_cast<std::uint32_t>(selected.value)});
    }
    if (has("task_shop_orders"))
      domains.push_back({"task_shop_orders", 1});
    if (has("political_shop_licensing")) {
      auto selected =
          selected_native_political_shop_licensing_version(identity.catalog);
      if (!selected.ok)
        return {false, {}, std::move(selected.error)};
      if (!selected.value)
        return {false,
                {},
                common_failure("PoliticalShopLicensingPairRequired").error};
      domains.push_back({"political_shop_licensing",
                         static_cast<std::uint32_t>(selected.value)});
    }
    if (has("private_shops")) {
      auto selected =
          selected_native_private_shop_operation_versions(identity.catalog);
      if (!selected.ok)
        return {false, {}, std::move(selected.error)};
      domains.push_back(
          {"private_shops", static_cast<std::uint32_t>(selected.value.shops)});
    }
    if (has("task_extensions")) {
      if (!dynamic)
        fail("TaskExtensionProfileMismatch", "checkpoint/module_versions");
      domains.push_back({"task_extensions", 1});
    }
    if (has("political_shop_registry_successions")) {
      if (!dynamic || !has("political_shop_licensing") ||
          !has("private_shops") || !has("political_takeover"))
        fail("PoliticalShopRegistrySuccessionDependenciesRequired",
             "checkpoint/module_versions");
      domains.push_back({"political_shop_registry_successions", 1});
    }
    if (has("personal_appointments")) {
      if (!dynamic)
        fail("PersonalAppointmentProfileMismatch",
             "checkpoint/module_versions");
      domains.push_back({"personal_appointments", 1});
    }
    if (has("personal_appointment_attendance") &&
        (!dynamic || !has("personal_appointments")))
      fail("AppointmentAttendanceProfileMismatch",
           "checkpoint/module_versions");
    {
      auto attendance_layout = validate_visit_appointment_attendance_layout(
          identity.catalog.module_versions, dynamic);
      if (!attendance_layout.ok)
        return {false, {}, attendance_layout.error};
    }
    if (has("personal_injury_recovery")) {
      if (!dynamic || !has("regional_life") || !has("family_life"))
        fail("PersonalInjuryRecoveryProfileMismatch",
             "checkpoint/module_versions");
      domains.push_back({"personal_injury_recovery", 1});
    }
    if (has("political_shop_licensing_receipts")) {
      if (!dynamic || !has("political_shop_licensing") || !has("private_shops"))
        fail("PoliticalShopLicensingReceiptsDependenciesRequired",
             "checkpoint/module_versions");
      domains.push_back({"political_shop_licensing_receipts", 1});
    }
    {
      const bool estate = has("private_shop_liquidations");
      auto pair = validate_private_shop_liquidation_pair(
          estate, identity.catalog.module_versions);
      if (!pair.ok)
        return {false, {}, pair.error};
      if (estate) {
        if (!dynamic)
          fail("PrivateShopLiquidationProfileRequired",
               "checkpoint/module_versions");
        domains.push_back({"private_shop_liquidations", 1});
        domains.push_back({"task_shop_order_estate_cancellations", 1});
      }
    }
    if (has("family_apprenticeship")) {
      if (!dynamic || !has("family_life"))
        fail("ApprenticeshipDependenciesRequired",
             "checkpoint/module_versions");
      domains.push_back({"family_apprenticeship", 1});
    }
    auto continued_layout = validate_political_governance_continuation_modules(
        identity.catalog.module_versions,
        has("political_governance_continuations"));
    if (!continued_layout.ok)
      return {false, {}, continued_layout.error};
    if (has("political_governance_continuations")) {
      if (!dynamic)
        fail("GovernanceContinuationProfileRequired",
             "checkpoint/module_versions");
      domains.push_back({"political_governance_continuations", 1});
    }
    if (has("contact_inquiries")) {
      if (!dynamic ||
          (!has("personal_appointments") && !has("task_message_handoffs")))
        fail("ContactInquiryProfileMismatch", "checkpoint/module_versions");
      domains.push_back({"contact_inquiries", 1});
    }
    auto governance_intents_layout =
        validate_political_governance_intent_modules(
            has("political_governance_intents"),
            identity.catalog.module_versions);
    if (!governance_intents_layout.ok)
      return {false, {}, governance_intents_layout.error};
    if (has("visit_message_replies")) {
      if (!dynamic || !has("visit_messages"))
        fail("VisitReplyProfileMismatch", "checkpoint/module_versions");
      domains.push_back({"visit_message_replies",
                         modules.at("visit_message_replies") == "2" ? 2u : 1u});
    }
    if (has("visit_reply_intents") !=
            (has("visit_message_replies") &&
             modules.at("visit_message_replies") == "2") ||
        (has("visit_reply_intents") &&
         (!has("visit_reception_intents") || !has("visit_reception_claims"))))
      fail("VisitReplyAuthorshipRequired", "checkpoint/module_versions");
    if (has("family_disciple_legacy")) {
      if (!dynamic || !has("family_life") || !has("family_legacy") ||
          !has("family_apprenticeship"))
        fail("DiscipleControlDependenciesRequired",
             "checkpoint/module_versions");
      domains.push_back({"family_disciple_legacy", 1});
    }
    auto invitation_layout = validate_governance_invitation_reply_modules(
        identity.catalog.module_versions,
        has(governance_invitation_reply_module));
    if (!invitation_layout.ok)
      return {false, {}, invitation_layout.error};
    if (has(governance_invitation_reply_module)) {
      if (!dynamic)
        fail("GovernanceInvitationProfileRequired",
             "checkpoint/module_versions");
      domains.push_back({governance_invitation_reply_module, 1});
    }
    auto daily_layout = validate_private_shop_daily_rent_pair(
        has(private_shop_daily_rent_module), identity.catalog.module_versions);
    if (!daily_layout.ok)
      return {false, {}, daily_layout.error};
    if (has(private_shop_daily_rent_module)) {
      if (!dynamic)
        fail("PrivateShopDailyRentProfileRequired",
             "checkpoint/module_versions");
      domains.push_back({private_shop_daily_rent_module, 1});
    }
    if (has("born_personal_dialogues")) {
      if (!dynamic || !has("family_birth") || !has("family_lineage") ||
          !has("family_life"))
        fail("BornPersonalDialogueProfileMismatch",
             "checkpoint/module_versions");
      domains.push_back({"born_personal_dialogues", 1});
    }
    if ((has("family_biological_legacy") &&
         (!has("family_legacy") || (modules.at("family_legacy") != "2" &&
                                    modules.at("family_legacy") != "3"))) ||
        (!has("family_biological_legacy") && has("family_legacy") &&
         modules.at("family_legacy") == "2"))
      fail("BiologicalControlLegacyVersionRequired",
           "checkpoint/module_versions");
    if (has("family_biological_legacy")) {
      if (!dynamic || !has("family_life") || !has("family_birth") ||
          !has("family_lineage"))
        fail("BiologicalControlDependenciesRequired",
             "checkpoint/module_versions");
      domains.push_back({"family_biological_legacy", 1});
    }
    auto succession_version =
        political_succession_selected_version(identity.catalog.module_versions);
    if (!succession_version.ok)
      return {false, {}, succession_version.error};
    if (succession_version.value) {
      if (!dynamic)
        fail("PoliticalSuccessionProfileRequired",
             "checkpoint/module_versions");
      domains.push_back({political_succession_domain, 1});
    }
    if (has("visit_appointment_invitations") !=
            has("visit_appointment_meetings") ||
        (has("visit_appointment_invitations") &&
         (!dynamic || !has("visit_messages") ||
          !has("personal_appointments"))) ||
        (has("visit_appointment_intents") &&
         (!has("visit_appointment_invitations") ||
          !has("visit_reception_intents") || !has("visit_reception_claims"))))
      fail("VisitAppointmentProfileMismatch", "checkpoint/module_versions");
    if (has("visit_appointment_invitations")) {
      domains.push_back({"visit_appointment_invitations", 1});
      domains.push_back({"visit_appointment_meetings", 1});
    }
    auto handover_version = political_head_handover_selected_version(
        identity.catalog.module_versions);
    if (!handover_version.ok)
      return {false, {}, handover_version.error};
    if (handover_version.value) {
      if (!dynamic)
        fail("PoliticalHeadHandoverProfileRequired",
             "checkpoint/module_versions");
      domains.push_back({political_head_handover_domain, 1});
    }
    if (has(military_tactic_learning_module))
      domains.push_back({military_tactic_learning_module, 1});
    if (has("growth_tactic_lessons")) {
      if (!dynamic || !has("family_life") || !has("military_tactic_learning"))
        fail("GrowthTacticLessonProfileMismatch", "checkpoint/module_versions");
      domains.push_back({"growth_tactic_lessons", 1});
    }
    auto round_version = political_successor_round_selected_version(
        identity.catalog.module_versions);
    if (!round_version.ok)
      return {false, {}, round_version.error};
    if (round_version.value) {
      if (!dynamic)
        fail("PoliticalSuccessorRoundProfileRequired",
             "checkpoint/module_versions");
      domains.push_back({political_successor_rounds_domain, 1});
    }
    auto shop_results_layout = validate_private_shop_command_results_pair(
        has(private_shop_command_results_module), has(private_shop_module),
        identity.catalog.module_versions);
    if (!shop_results_layout.ok)
      return {false, {}, shop_results_layout.error};
    if (has(private_shop_command_results_module)) {
      if (!dynamic)
        fail("PrivateShopCommandResultsProfileRequired",
             "checkpoint/module_versions");
      domains.push_back({private_shop_command_results_module, 1});
    }
    auto term_version = political_term_handover_selected_version(
        identity.catalog.module_versions);
    if (!term_version.ok)
      return {false, {}, term_version.error};
    if (term_version.value) {
      if (!dynamic)
        fail("PoliticalTermHandoverProfileRequired",
             "checkpoint/module_versions");
      domains.push_back({political_term_handover_domain, 1});
    }
    auto message_receipts = visit_message_receipt_selected_version(
        identity.catalog.module_versions);
    if (!message_receipts.ok)
      return {false, {}, message_receipts.error};
    if (message_receipts.value) {
      if (!dynamic)
        fail("VisitMessageReceiptProfileMismatch",
             "checkpoint/module_versions");
      domains.push_back({visit_message_receipt_module, 1});
    }
    {
      auto selected =
          known_personal_dialogue_selected(identity.catalog.module_versions);
      if (!selected.ok)
        return {false, {}, selected.error};
      if (selected.value) {
        if (!dynamic)
          fail("KnownPersonalDialogueProfileMismatch",
               "checkpoint/module_versions");
        domains.push_back({"known_personal_dialogues", 1});
      }
    }
    auto reception_version = political_office_reception_selected_version(
        identity.catalog.module_versions);
    if (!reception_version.ok)
      return {false, {}, reception_version.error};
    if (reception_version.value) {
      if (!dynamic)
        fail("PoliticalOfficeReceptionProfileRequired",
             "checkpoint/module_versions");
      domains.push_back({political_office_reception_domain, 1});
    }
    {
      auto selected = task_shop_order_contact_selected_version(
          identity.catalog.module_versions);
      if (!selected.ok)
        return {false, {}, selected.error};
      if (selected.value) {
        if (!dynamic)
          fail("TaskShopOrderContactProfileMismatch",
               "checkpoint/module_versions");
        domains.push_back({task_shop_order_contact_module, 1});
      }
    }
    if (has("family_collateral_legacy") !=
        (has("family_legacy") && modules.at("family_legacy") == "3"))
      fail("CollateralControlLegacyVersionRequired",
           "checkpoint/module_versions");
    if (has("family_collateral_legacy")) {
      if (!dynamic || !has("family_life") || !has("family_lineage") ||
          !has("known_personal_dialogues"))
        fail("CollateralControlDependenciesRequired",
             "checkpoint/module_versions");
      domains.push_back({"family_collateral_legacy", 1});
    }
    auto agency_version = political_governance_agency_selected_version(
        identity.catalog.module_versions);
    if (!agency_version.ok)
      return {false, {}, agency_version.error};
    if (agency_version.value) {
      if (!dynamic)
        fail("PoliticalGovernanceAgencyProfileRequired",
             "checkpoint/module_versions");
      domains.push_back({political_governance_agency_domain,
                         static_cast<std::uint32_t>(agency_version.value)});
    }
    {
      auto selected = task_message_return_selected_version(
          identity.catalog.module_versions);
      if (!selected.ok)
        return {false, {}, selected.error};
      if (selected.value) {
        if (!dynamic)
          fail("TaskMessageReturnProfileMismatch",
               "checkpoint/module_versions");
        domains.push_back({task_message_return_module, 1});
      }
    }
    auto family_version = select_family_codec_version(
        {has("household_control"), has("character_creation"),
         has("adult_adoption"), has("voluntary_succession"),
         has("family_voluntary_paths")});
    if (!family_version.ok)
      return {false, {}, family_version.error};
    auto family_pair = family_modules_for(family_version.value,
                                          identity.catalog.module_versions);
    if (!family_pair.ok)
      return {false, {}, family_pair.error};
    if (has("family_voluntary_paths") && !dynamic)
      fail("VoluntaryPathSourcePairRequired", "checkpoint/module_versions");
    for (auto &d : domains)
      if (d.name == "family")
        d.codec_version = static_cast<std::uint32_t>(family_version.value);
    if (has("family_voluntary_paths"))
      domains.push_back({"family_voluntary_paths", 1});
    {
      auto selected = task_personal_message_selected_version(
          identity.catalog.module_versions);
      if (!selected.ok)
        return {false, {}, selected.error};
      if (selected.value) {
        if (!dynamic)
          fail("TaskPersonalMessageProfileMismatch",
               "checkpoint/module_versions");
        domains.push_back({task_personal_message_module, 1});
      }
    }
    auto resignation_version = political_office_resignation_selected_version(
        identity.catalog.module_versions);
    if (!resignation_version.ok)
      return {false, {}, resignation_version.error};
    if (resignation_version.value) {
      if (!dynamic)
        fail("PoliticalOfficeResignationProfileRequired",
             "checkpoint/module_versions");
      domains.push_back(
          {political_office_resignation_module,
           static_cast<std::uint32_t>(resignation_version.value)});
    }
    {
      auto selected = task_personal_report_selected_version(
          identity.catalog.module_versions);
      if (!selected.ok)
        return {false, {}, selected.error};
      if (selected.value) {
        if (!dynamic)
          fail("TaskPersonalReportProfileMismatch",
               "checkpoint/module_versions");
        domains.push_back({task_personal_report_module, 1});
      }
    }
    {
      auto selected = task_personal_cancellation_selected_version(
          identity.catalog.module_versions);
      if (!selected.ok)
        return {false, {}, selected.error};
      if (selected.value) {
        if (!dynamic)
          fail("TaskPersonalCancellationProfileMismatch",
               "checkpoint/module_versions");
        domains.push_back({task_personal_cancellation_module, 1});
      }
    }
    auto subordinate_version =
        political_subordinate_appointment_selected_version(
            identity.catalog.module_versions);
    if (!subordinate_version.ok)
      return {false, {}, subordinate_version.error};
    if (subordinate_version.value) {
      if (!dynamic)
        fail("PoliticalSubordinateAppointmentProfileRequired",
             "checkpoint/module_versions");
      domains.push_back(
          {political_subordinate_appointment_module,
           static_cast<std::uint32_t>(subordinate_version.value)});
    }
    {
      auto selected = task_personal_acceptance_selected_version(
          identity.catalog.module_versions);
      if (!selected.ok)
        return {false, {}, selected.error};
      if (selected.value) {
        if (!dynamic)
          fail("TaskPersonalAcceptanceProfileMismatch",
               "checkpoint/module_versions");
        domains.push_back({task_personal_acceptance_module, 1});
      }
    }
    auto dismissal_version = political_dismissal_source_selected_version(
        identity.catalog.module_versions);
    if (!dismissal_version.ok)
      return {false, {}, dismissal_version.error};
    if (dismissal_version.value) {
      if (!dynamic)
        fail("PoliticalDismissalSourceProfileRequired",
             "checkpoint/module_versions");
      domains.push_back({political_dismissal_sources_module,
                         static_cast<std::uint32_t>(dismissal_version.value)});
    }
    if (field_version.value)
      domains.push_back(
          {"field_battle",
           static_cast<std::uint32_t>(field_battle_checkpoint_codec_revision)});
    return {true, std::move(domains), {}};
  } catch (const boundary::DecodeFailure &error) {
    return {false, {}, error.error};
  }
}

Outcome<std::vector<CheckpointDomain>>
selected_checkpoint_domains(const CheckpointIdentity &identity) {
  return selected_checkpoint_domains_impl(identity, false);
}
