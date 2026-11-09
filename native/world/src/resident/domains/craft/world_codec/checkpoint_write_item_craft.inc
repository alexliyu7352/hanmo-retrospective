// 制造世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
r = validate_native_item_craft_layout(w);
if (!r.ok)
  return r;
auto item_craft_versions =
    selected_native_item_craft_versions(w.catalog->identity);
if (!item_craft_versions.ok)
  return {false, {}, std::move(item_craft_versions.error)};
if (w.item_instances) {
  r = section("item_instances", [&](auto &c) {
    return visit_item_instances(c, *w.item_instances,
                                item_craft_versions.value.item_instances);
  });
  if (!r.ok)
    return r;
}
if (w.lot_properties) {
  r = section("lot_properties", [&](auto &c) {
    return visit_lot_properties(c, *w.lot_properties);
  });
  if (!r.ok)
    return r;
}
if (w.craft_jobs) {
  r = section("craft_jobs", [&](auto &c) {
    return visit_craft_jobs_checkpoint(c, *w.craft_jobs);
  });
  if (!r.ok)
    return r;
}
