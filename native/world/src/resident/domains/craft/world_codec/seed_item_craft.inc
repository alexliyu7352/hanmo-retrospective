// 制造世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto item_craft_catalog = validate_native_item_craft_catalogs(catalog);
if (!item_craft_catalog.ok)
  return {false, {}, std::move(item_craft_catalog.error)};
auto item_craft_versions =
    selected_native_item_craft_versions(catalog.identity);
if (!item_craft_versions.ok)
  return {false, {}, std::move(item_craft_versions.error)};
if (item_craft_versions.value.item_instances) {
  {
    ScopedCheckpointJsonDecode original(
        CheckpointWorldJsonSection::item_instances);
    auto items =
        decode_item_instances(field(source, "item_instances", "state"),
                              item_craft_versions.value.item_instances);
    if (!items.ok)
      return {false, {}, std::move(items.error)};
    result.item_instances = checkpoint_make_shared<const ItemInstancesState>(
        std::move(items.value));
  }
  {
    ScopedCheckpointJsonDecode original(
        CheckpointWorldJsonSection::lot_properties);
    auto properties =
        decode_lot_properties(field(source, "lot_properties", "state"));
    if (!properties.ok)
      return {false, {}, std::move(properties.error)};
    result.lot_properties = checkpoint_make_shared<const LotPropertiesState>(
        std::move(properties.value));
  }
} else if (source.find("item_instances") || source.find("lot_properties"))
  fail("UnexpectedItemServiceState", "item_instances");
if (item_craft_versions.value.craft_jobs) {
  ScopedCheckpointJsonDecode original(CheckpointWorldJsonSection::craft_jobs);
  auto jobs = decode_craft_jobs(field(source, "craft_jobs", "state"));
  if (!jobs.ok)
    return {false, {}, std::move(jobs.error)};
  result.craft_jobs =
      checkpoint_make_shared<const CraftJobsState>(std::move(jobs.value));
} else if (source.find("craft_jobs"))
  fail("UnexpectedCraftJobsState", "craft_jobs");
