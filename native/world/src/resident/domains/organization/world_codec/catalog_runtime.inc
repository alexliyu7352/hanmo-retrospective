// 组织世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto organization = decode_native_organization_catalog(
    scenario, field(*dynamic, "organization", "dynamic_world"), id,
    result->named_membership);
if (!organization.ok)
  return {false, {}, organization.error};
result->organization = checkpoint_make_shared<const OrganizationCatalog>(
    std::move(organization.value));
