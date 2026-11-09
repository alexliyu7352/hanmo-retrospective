// 制造世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto item_craft_layout = validate_native_item_craft_layout(w);
if (!item_craft_layout.ok)
  return item_craft_layout;
auto item_craft_versions =
    selected_native_item_craft_versions(w.catalog->identity);
if (!item_craft_versions.ok)
  return {false, {}, std::move(item_craft_versions.error)};
if (w.item_instances)
  WORLD_WHOLE(P::section, "item_instances",
              emit_item_instances(out, *w.item_instances,
                                  item_craft_versions.value.item_instances));
if (w.lot_properties)
  WORLD_WHOLE(P::section, "lot_properties",
              emit_lot_properties(out, *w.lot_properties));
if (w.craft_jobs)
  WORLD_WHOLE(P::section, "craft_jobs",
              emit_craft_jobs_projection(out, "", *w.craft_jobs));
