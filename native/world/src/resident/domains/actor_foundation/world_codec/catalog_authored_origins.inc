// 人物底座世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto authored = decode_authored_actor_foundation_catalog(
    field(scenario, "actor_foundation", "scenario"),
    field(scenario, "initial", "scenario"), id,
    get_string(state, "scenario_id", ""), result->growth);
if (!authored.ok)
  return {false, {}, authored.error};
// DeclaredAgeAtStart是原作者整岁，不是生日事实；仅完整准入新来源后去掉旧-age*720派生投影。
// 保留原actor键/作者data/age_basis/锁身份，Interaction等既有只读引用仍指同一未发布对象。
for (const auto &row : authored.value.actors.rows()) {
  if (row.second.age_basis.kind !=
      AuthoredActorAgeBasis::Kind::declared_age_at_start)
    unsupported("UnsupportedAuthoredAgeBasis", row.first);
  auto *legacy_projection =
      authored_common_candidate->actor_initial_birth.find(row.first);
  if (!legacy_projection)
    fail("MissingOriginalAuthoredActor", row.first);
  legacy_projection->reset();
}
if (exact_complete_selected(*result->exact_time)) {
  authored.value.original_long_rest_rule = OriginalLongRestConditionRule{
      {"original_estate_long_rest", 1, id.content_digest},
      bool(result->personal_recovery)};
  auto training = register_original_personal_training_purpose(authored.value);
  if (!training.ok)
    return {false, {}, training.error};
  auto service_work =
      register_original_growth_service_work_purpose(authored.value);
  if (!service_work.ok)
    return {false, {}, service_work.error};
  // 仅真实History2的原贸易参与桥；原服务、休息与工作purpose均不代用。
  if (std::any_of(id.module_versions.begin(), id.module_versions.end(),
                  [](const auto &module) {
                    return module.first == "growth_market_history" &&
                           module.second == "2";
                  })) {
    auto trade =
        register_original_growth_trade_participation_purpose(authored.value);
    if (!trade.ok)
      return {false, {}, std::move(trade.error)};
  }
}
auto shop_selected = selected_native_private_shop_operation_versions(id);
if (!shop_selected.ok)
  return {false, {}, std::move(shop_selected.error)};
if (shop_selected.value.exact_operations()) {
  auto purpose =
      register_original_private_shop_operation_purpose(authored.value);
  if (!purpose.ok)
    return {false, {}, std::move(purpose.error)};
}
auto craft_purpose_selected = selected_native_item_craft_versions(id);
if (!craft_purpose_selected.ok)
  return {false, {}, std::move(craft_purpose_selected.error)};
if (craft_purpose_selected.value.craft_jobs == 1) {
  auto purpose = register_original_craft_job_work_purpose(authored.value);
  if (!purpose.ok)
    return {false, {}, std::move(purpose.error)};
}
// Field1 的原 Opening 缺粮后绑定必须具有真实 C8 规则，不以空表免除依赖。
if (const auto *dynamic = scenario.find("dynamic_world"))
  if (dynamic->find("field_battle")) {
    auto supply_rule = register_original_military_supply_shortage_rule(authored.value);
    if (!supply_rule.ok) return {false, {}, std::move(supply_rule.error)};
  }
result->actor_foundation = seal_actor_foundation_catalog(
    std::move(authored.value));
