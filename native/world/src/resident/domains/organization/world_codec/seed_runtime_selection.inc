// 组织世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto organization_catalog =
    validate_native_organization_runtime_catalog(catalog);
if (!organization_catalog.ok)
  return {false, {}, std::move(organization_catalog.error)};
auto organization_runtime =
    selected_native_organization_runtime(catalog.identity);
if (!organization_runtime.ok)
  return {false, {}, std::move(organization_runtime.error)};
auto organization_effects =
    selected_native_organization_service_effects(catalog.identity);
if (!organization_effects.ok)
  return {false, {}, std::move(organization_effects.error)};
