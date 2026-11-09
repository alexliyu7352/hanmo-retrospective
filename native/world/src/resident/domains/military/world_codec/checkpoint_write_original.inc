// 军事世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
r = section("military", [&](auto &c) {
  return visit_military_checkpoint(
      c, w.military, w.catalog->selected_military_occupation_schema);
});
if (!r.ok)
  return {false, {}, r.error};
{
  auto layout = validate_native_military_battle_lifecycle_layout(w);
  if (!layout.ok)
    return layout;
  auto selected =
      selected_native_military_battle_lifecycle(w.catalog->identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (selected.value) {
    r = section("military_battle_lifecycle", [&](auto &c) {
      return visit_military_battle_lifecycle(c, w.military, selected.value);
    });
    if (!r.ok)
      return r;
  }
}
