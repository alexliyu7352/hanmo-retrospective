// 共同编解码世界编解码片段：保持原调用位置、字段与首错顺序；仅由
// world_codec.cpp 登记包含。
auto officer_pair =
    validate_military_siege_engineering_modules(id.module_versions);
if (!officer_pair.ok)
  return {false, {}, officer_pair.error};
auto household_catalogs = prepare_native_household_catalogs(scenario, *result);
if (!household_catalogs.ok)
  return {false, {}, std::move(household_catalogs.error)};
auto face_module = append_native_face_to_face_module(scenario, id);
if (!face_module.ok)
  return {false, {}, std::move(face_module.error)};
auto occupancy =
    prepare_native_actor_social_occupancy_selection(scenario, *result);
if (!occupancy.ok)
  return {false, {}, std::move(occupancy.error)};
auto occupation =
    prepare_native_military_occupation_selection(scenario, *result);
if (!occupation.ok)
  return {false, {}, std::move(occupation.error)};
auto item_craft_modules = append_native_item_craft_modules(scenario, id);
if (!item_craft_modules.ok)
  return {false, {}, std::move(item_craft_modules.error)};
auto facility_modules = append_native_economic_facility_modules(scenario, id);
if (!facility_modules.ok)
  return {false, {}, std::move(facility_modules.error)};
auto military_module = append_native_military_state_modules(scenario, id);
if (!military_module.ok)
  return {false, {}, std::move(military_module.error)};
// 与原生准入同序登记已启用的个人物理身份；旧未选身份与物理段表不变。
if (result->personal->enabled && scenario.find("personal_battle_lifecycle"))
  id.module_versions.emplace_back("personal", "1");
auto personal_lifecycle_module =
    append_native_personal_battle_lifecycle_module(scenario, id);
if (!personal_lifecycle_module.ok)
  return {false, {}, std::move(personal_lifecycle_module.error)};
auto battle_lifecycle_module =
    append_native_military_battle_lifecycle_module(scenario, id);
if (!battle_lifecycle_module.ok)
  return {false, {}, std::move(battle_lifecycle_module.error)};
auto organization_module =
    append_native_organization_runtime_module(scenario, id);
if (!organization_module.ok)
  return {false, {}, std::move(organization_module.error)};
auto obligation_module = append_native_economic_obligation_module(scenario, id);
if (!obligation_module.ok)
  return {false, {}, std::move(obligation_module.error)};
auto population_module = append_native_born_population_module(scenario, id);
if (!population_module.ok)
  return {false, {}, std::move(population_module.error)};
auto shop_modules = append_native_private_shop_operation_modules(scenario, id);
if (!shop_modules.ok)
  return {false, {}, std::move(shop_modules.error)};
auto original_shop_pair =
    validate_private_shop_pair(bool(result->private_shops), id.module_versions);
if (!original_shop_pair.ok)
  return {false, {}, std::move(original_shop_pair.error)};
auto control_module = append_native_control_continuation_module(scenario, id);
if (!control_module.ok)
  return {false, {}, std::move(control_module.error)};
auto interaction_response_module =
    append_native_interaction_response_module(scenario, id);
if (!interaction_response_module.ok)
  return {false, {}, std::move(interaction_response_module.error)};
auto task_execution_module = append_native_task_execution_module(scenario, id);
if (!task_execution_module.ok)
  return {false, {}, std::move(task_execution_module.error)};
auto task_work_module = append_native_task_work_module(scenario, id);
if (!task_work_module.ok)
  return {false, {}, std::move(task_work_module.error)};
auto governance_module = append_native_governance_program_modules(scenario, id);
if (!governance_module.ok)
  return {false, {}, std::move(governance_module.error)};
