// 商业检查点恢复分派：沿原 else-if 链读取 private_shop_operation_sources,
// private_shop_operation_capabilities；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "private_shop_operation_sources") {
  auto selected =
      selected_native_private_shop_operation_versions(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (!selected.value.sources || section.codec_version != 1 ||
      seed.private_shop_operation_sources)
    return {false,
            {},
            common_failure("ShopOperationCheckpointSelectionMismatch").error};
  auto state = make_state<Seed, PrivateShopOperationSourcesState>();
  r = visit_private_shop_operation_sources(reader, *state);
  seed.private_shop_operation_sources = std::move(state);
}
else if (section.domain == "private_shop_operation_capabilities") {
  auto selected =
      selected_native_private_shop_operation_versions(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (!selected.value.capabilities || section.codec_version != 1 ||
      seed.private_shop_operation_capabilities)
    return {false,
            {},
            common_failure("ShopOperationCheckpointSelectionMismatch").error};
  auto state = make_state<Seed, PrivateShopOperationCapabilitiesState>();
  r = visit_private_shop_operation_capabilities(reader, *state);
  seed.private_shop_operation_capabilities = std::move(state);
}