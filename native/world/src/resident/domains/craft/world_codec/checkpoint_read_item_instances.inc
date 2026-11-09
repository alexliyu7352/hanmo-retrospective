// 制造检查点恢复分派：沿原 else-if 链读取 item_instances, lot_properties,
// craft_jobs；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "item_instances") {
  auto selected = selected_native_item_craft_versions(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (!selected.value.item_instances ||
      section.codec_version != selected.value.item_instances ||
      seed.item_instances)
    return {false,
            {},
            common_failure("ItemCraftCheckpointSelectionMismatch").error};
  auto state = make_state<Seed, ItemInstancesState>();
  r = visit_item_instances(reader, *state, selected.value.item_instances);
  seed.item_instances = std::move(state);
}
else if (section.domain == "lot_properties") {
  auto selected = selected_native_item_craft_versions(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (!selected.value.lot_properties || section.codec_version != 1 ||
      seed.lot_properties)
    return {false,
            {},
            common_failure("ItemCraftCheckpointSelectionMismatch").error};
  auto state = make_state<Seed, LotPropertiesState>();
  r = visit_lot_properties(reader, *state);
  seed.lot_properties = std::move(state);
}
else if (section.domain == "craft_jobs") {
  auto selected = selected_native_item_craft_versions(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (!selected.value.craft_jobs || section.codec_version != 1 ||
      seed.craft_jobs)
    return {false,
            {},
            common_failure("ItemCraftCheckpointSelectionMismatch").error};
  auto state = make_state<Seed, CraftJobsState>();
  r = visit_craft_jobs_checkpoint(reader, *state);
  seed.craft_jobs = std::move(state);
}