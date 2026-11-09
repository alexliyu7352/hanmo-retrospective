// 共同编解码世界编解码片段：保持原调用位置、字段与首错顺序；仅由
// world_codec.cpp 登记包含。
auto exact = decode_exact_time_profile(scenario);
if (!exact.ok)
  return {false, {}, exact.error};
if (exact.value) {
  auto selected = append_exact_time_modules(id, &*exact.value);
  if (!selected.ok)
    return {false, {}, selected.error};
  result->exact_time =
      checkpoint_make_shared<const ExactTimeProfile>(std::move(*exact.value));
  if (exact_complete_selected(*result->exact_time)) {
    auto growth = checkpoint_make_shared<GrowthCatalog>(*result->growth);
    growth->complete_study_sources = true;
    growth->complete_exact_period_sources = true;
    auto selected_growth =
        read_complete_growth_phase_version(*result->exact_time);
    if (!selected_growth.ok)
      return {false, {}, std::move(selected_growth.error)};
    growth->complete_exact_command_cuts = selected_growth.value >= 2;
    growth->complete_exact_rest_mouth_sources = selected_growth.value >= 3;
    growth->complete_capability_application_chain = selected_growth.value == 4;
    result->growth = std::move(growth);
  }
  auto age_module = append_native_life_age_module(scenario, id);
  if (!age_module.ok)
    return {false, {}, std::move(age_module.error)};
  auto production_module = append_native_production_cycles_module(scenario, id);
  if (!production_module.ok)
    return {false, {}, std::move(production_module.error)};
  auto funding =
      prepare_native_private_transport_funding_selection(scenario, *result);
  if (!funding.ok)
    return {false, {}, std::move(funding.error)};
  auto transport = prepare_native_transport_selection(
      scenario, *result, *authored_common_candidate);
  if (!transport.ok)
    return {false, {}, std::move(transport.error)};
  auto item_craft = prepare_native_item_craft_catalogs(scenario, *result);
  if (!item_craft.ok)
    return {false, {}, std::move(item_craft.error)};
  auto facility_catalogs =
      prepare_native_economic_facility_catalogs(scenario, *result);
  if (!facility_catalogs.ok)
    return {false, {}, std::move(facility_catalogs.error)};
  auto social_cut = prepare_native_social_source_selection(
      data, scenario, *result, *authored_social_candidate);
  if (!social_cut.ok)
    return {false, {}, std::move(social_cut.error)};
  // 原 native 准入会先独立核 admitted，稍后才装载真实 Field 资源；本目录投影
  // 也必须在原人物作者编译之前具有同一明确 Field/source1 选择，不能等已拒的
  // map3 解码返回后才改 C7。这里不装载地形、不构造世界，也不自动选新 profile。
  if (const auto *dynamic = scenario.find("dynamic_world")) {
    if (const auto *selected = dynamic->find("field_battle")) {
      auto parameters = decode_field_battle_parameters(*selected);
      if (!parameters.ok)
        return {false, {}, std::move(parameters.error)};
      for (const auto &entry : id.module_versions)
        if (entry.first == "field_battle")
          return {false,
                  {},
                  common_failure("DuplicateFieldBattleModuleSelection").error};
      id.module_versions.emplace_back("field_battle", "1");
    }
  }
  auto injury_module = append_native_character_injury_module(
      scenario, id, result->exact_time.get(), *result->personal);
  if (!injury_module.ok)
    return {false, {}, std::move(injury_module.error)};
#include "resident/domains/actor_foundation/world_codec/catalog_authored_origins.inc"
  auto shop_catalog = validate_native_private_shop_operation_catalog(*result);
  if (!shop_catalog.ok)
    return {false, {}, std::move(shop_catalog.error)};
  if (result->actor_social_occupancy_selected) {
    auto compiled = compile_native_actor_social_occupancy_seed(
        data, field(scenario, "initial", "scenario"), id,
        get_string(state, "scenario_id", ""));
    if (!compiled.ok)
      return {false, {}, std::move(compiled.error)};
    result->actor_social_occupancy_initial =
        checkpoint_make_shared<const ActorSocialOccupancyInitialCatalog>(
            std::move(compiled.value));
  }
  auto military_catalog =
      prepare_native_military_state_catalog(scenario, *result);
  if (!military_catalog.ok)
    return {false, {}, std::move(military_catalog.error)};
  auto control = prepare_native_control_continuation_catalog(scenario, *result);
  if (!control.ok)
    return {false, {}, std::move(control.error)};
  auto life_age = prepare_native_life_age_catalog(scenario, *result);
  if (!life_age.ok)
    return {false, {}, std::move(life_age.error)};
  auto production = prepare_native_production_cycles_catalog(scenario, *result);
  if (!production.ok)
    return {false, {}, std::move(production.error)};
  auto permanent =
      prepare_native_growth_permanent_catalog(data, scenario, *result);
  if (!permanent.ok)
    return {false, {}, std::move(permanent.error)};
  auto organization_catalog =
      prepare_native_organization_runtime_catalog(scenario, *result);
  if (!organization_catalog.ok)
    return {false, {}, std::move(organization_catalog.error)};
  auto obligation_catalog =
      validate_native_economic_obligation_catalog(*result);
  if (!obligation_catalog.ok)
    return {false, {}, std::move(obligation_catalog.error)};
  auto governance_catalog =
      prepare_native_governance_program_catalog(scenario, *result);
  if (!governance_catalog.ok)
    return {false, {}, std::move(governance_catalog.error)};
  auto birth_catalog =
      prepare_native_dependent_provision_catalog(data, *result);
  if (!birth_catalog.ok)
    return {false, {}, std::move(birth_catalog.error)};
  auto population_catalog = validate_native_born_population_catalog(*result);
  if (!population_catalog.ok)
    return {false, {}, std::move(population_catalog.error)};
  if (exact_complete_selected(*result->exact_time)) {
    auto legacy = read_native_complete_selected_legacy_writer_keys(*result);
    if (!legacy.ok)
      return {false, {}, legacy.error};
    auto holders = read_native_complete_facility_holder_registrations(*result);
    if (!holders.ok)
      return {false, {}, holders.error};
    auto work = read_native_complete_task_work_factory(*result);
    if (!work.ok)
      return {false, {}, work.error};
    auto social = read_native_complete_social_dispatch(*result);
    if (!social.ok)
      return {false, {}, social.error};
    auto runtime = prepare_native_complete_exact_runtime(
        *result, legacy.value, holders.value, work.value, social.value);
    if (!runtime.ok)
      return {false, {}, runtime.error};
  }
} else {
  auto military_catalog =
      prepare_native_military_state_catalog(scenario, *result);
  if (!military_catalog.ok)
    return {false, {}, std::move(military_catalog.error)};
  auto control = prepare_native_control_continuation_catalog(scenario, *result);
  if (!control.ok)
    return {false, {}, std::move(control.error)};
  auto production = prepare_native_production_cycles_catalog(scenario, *result);
  if (!production.ok)
    return {false, {}, std::move(production.error)};
  auto funding =
      prepare_native_private_transport_funding_selection(scenario, *result);
  if (!funding.ok)
    return {false, {}, std::move(funding.error)};
  auto transport = prepare_native_transport_selection(
      scenario, *result, *authored_common_candidate);
  if (!transport.ok)
    return {false, {}, std::move(transport.error)};
  auto item_craft = prepare_native_item_craft_catalogs(scenario, *result);
  if (!item_craft.ok)
    return {false, {}, std::move(item_craft.error)};
  auto facility_catalogs =
      prepare_native_economic_facility_catalogs(scenario, *result);
  if (!facility_catalogs.ok)
    return {false, {}, std::move(facility_catalogs.error)};
  auto social_cut = prepare_native_social_source_selection(
      data, scenario, *result, *authored_social_candidate);
  if (!social_cut.ok)
    return {false, {}, std::move(social_cut.error)};
  if (scenario.find("actor_foundation"))
    fail("ActorFoundationExactProfileRequired", "scenario/actor_foundation");
  if (scenario.find("life_age_checks"))
    unsupported("LifeAgeCompleteOriginalSourcesRequired",
                "scenario/life_age_checks");
  auto life = validate_native_life_age_catalog(*result);
  if (!life.ok)
    return {false, {}, std::move(life.error)};
  auto permanent =
      prepare_native_growth_permanent_catalog(data, scenario, *result);
  if (!permanent.ok)
    return {false, {}, std::move(permanent.error)};
  auto organization_catalog =
      prepare_native_organization_runtime_catalog(scenario, *result);
  if (!organization_catalog.ok)
    return {false, {}, std::move(organization_catalog.error)};
  auto obligation_catalog =
      validate_native_economic_obligation_catalog(*result);
  if (!obligation_catalog.ok)
    return {false, {}, std::move(obligation_catalog.error)};
  auto governance_catalog =
      prepare_native_governance_program_catalog(scenario, *result);
  if (!governance_catalog.ok)
    return {false, {}, std::move(governance_catalog.error)};
  auto birth_catalog =
      prepare_native_dependent_provision_catalog(data, *result);
  if (!birth_catalog.ok)
    return {false, {}, std::move(birth_catalog.error)};
  auto population_catalog = validate_native_born_population_catalog(*result);
  if (!population_catalog.ok)
    return {false, {}, std::move(population_catalog.error)};
}
