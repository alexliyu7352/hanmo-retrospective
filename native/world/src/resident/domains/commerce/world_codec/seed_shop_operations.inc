// 商业世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto shop_versions =
    selected_native_private_shop_operation_versions(catalog.identity);
if (!shop_versions.ok)
  return {false, {}, std::move(shop_versions.error)};
if (shop_versions.value.exact_operations()) {
  {
    ScopedCheckpointJsonDecode original(
        CheckpointWorldJsonSection::private_shop_operation_sources);
    auto sources = decode_private_shop_operation_sources(
        field(source, "private_shop_operation_sources", "state"));
    if (!sources.ok)
      return {false, {}, std::move(sources.error)};
    result.private_shop_operation_sources =
        checkpoint_make_shared<const PrivateShopOperationSourcesState>(
            std::move(sources.value));
  }
  {
    ScopedCheckpointJsonDecode original(
        CheckpointWorldJsonSection::private_shop_operation_capabilities);
    auto capabilities = decode_private_shop_operation_capabilities(
        field(source, "private_shop_operation_capabilities", "state"));
    if (!capabilities.ok)
      return {false, {}, std::move(capabilities.error)};
    result.private_shop_operation_capabilities =
        checkpoint_make_shared<const PrivateShopOperationCapabilitiesState>(
            std::move(capabilities.value));
  }
} else if (source.find("private_shop_operation_sources") ||
           source.find("private_shop_operation_capabilities"))
  fail("UnexpectedShopOperationState", "private_shop_operation_sources");
