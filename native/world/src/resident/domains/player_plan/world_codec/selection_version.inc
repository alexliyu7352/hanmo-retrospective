// 玩家计划世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。 原目录身份已锁的Plan版本是唯一section
// selector，不从保存state反授能力。
Integer player_plan_version_for(const PreparedCatalog &catalog) {
  auto selected = read_player_plan_selected_version(catalog.identity);
  if (!selected.ok || catalog.player_plans != (selected.value != 0))
    boundary::fail("PlayerPlanModuleSelectionMismatch",
                   "checkpoint/player_plan");
  return selected.value;
}
