// 个人战斗世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto personal_catalog =
    validate_native_personal_battle_lifecycle_catalog(catalog);
if (!personal_catalog.ok)
  return personal_catalog;
auto personal_lifecycle =
    selected_native_personal_battle_lifecycle(catalog.identity);
if (!personal_lifecycle.ok)
  return {false, {}, std::move(personal_lifecycle.error)};
const BoundaryValue *lifecycle_source = nullptr;
for (const auto &row : object(source, "state"))
  if (row.first == "personal_battle_lifecycle") {
    if (lifecycle_source)
      fail("DuplicatePersonalBattleLifecycleSection",
           "personal_battle_lifecycle");
    lifecycle_source = &row.second;
  }
if (bool(lifecycle_source) != bool(personal_lifecycle.value))
  fail("PersonalBattleLifecycleSelectedSectionMismatch",
       "personal_battle_lifecycle");
auto personal = decode_personal_seed(source);
if (!personal.ok)
  return {false, {}, personal.error};
if (lifecycle_source) {
  ScopedCheckpointJsonDecode original(
      CheckpointWorldJsonSection::personal_battle_lifecycle);
  auto attached = decode_personal_battle_lifecycle(
      *lifecycle_source, personal.value, personal_lifecycle.value);
  if (!attached.ok)
    return attached;
}
auto personal_layout = validate_personal_battle_lifecycle_layout(
    personal.value, personal_lifecycle.value);
if (!personal_layout.ok)
  return personal_layout;
result.personal =
    std::make_shared<const PersonalState>(std::move(personal.value));
