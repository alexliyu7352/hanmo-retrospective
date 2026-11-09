// 玩家计划世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (catalog.player_plans) {
  auto plan = decode_player_plan_state(field(source, "player_plan", "state"));
  if (!plan.ok)
    return {false, {}, plan.error};
  auto selected = validate_player_plan_selected_version(
      plan.value, catalog.identity, catalog.player_plans,
      catalog.exact_time && exact_complete_selected(*catalog.exact_time));
  if (!selected.ok)
    return {false, {}, selected.error};
  result.player_plan =
      std::make_shared<const PlayerPlanState>(std::move(plan.value));
} else if (source.find("player_plan"))
  fail("UnexpectedPlayerPlanState", "player_plan");
