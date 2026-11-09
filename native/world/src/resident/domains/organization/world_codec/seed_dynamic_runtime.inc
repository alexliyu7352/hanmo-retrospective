// 组织世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto organization = [&]() {
  ScopedCheckpointJsonDecode original(CheckpointWorldJsonSection::organization);
  return organization_runtime.value
             ? decode_organization_runtime_selected_body(
                   boundary::field(source, "organization", ""),
                   catalog.identity)
             : decode_organization_state(
                   boundary::field(source, "organization", ""),
                   catalog.named_membership);
}();
if (!organization.ok)
  return {false, {}, std::move(organization.error)};
if (organization_runtime.value) {
  ScopedCheckpointJsonDecode original(
      CheckpointWorldJsonSection::organization_runtime);
  auto runtime = decode_organization_runtime(
      field(source, "organization_runtime", "state"), organization.value,
      catalog.named_membership);
  if (!runtime.ok)
    return {false, {}, std::move(runtime.error)};
  result.organization_runtime =
      checkpoint_make_shared<const OrganizationRuntime>(
          std::move(runtime.value));
}
if (organization_effects.value) {
  ScopedCheckpointJsonDecode original(
      CheckpointWorldJsonSection::organization_service_effects);
  auto attached = decode_organization_service_effects(
      field(source, "organization_service_effects", "state"),
      organization.value, organization_effects.value);
  if (!attached.ok)
    return {false, {}, std::move(attached.error)};
}
auto effects_layout = validate_organization_service_effects(
    organization.value, organization_effects.value);
if (!effects_layout.ok)
  return {false, {}, std::move(effects_layout.error)};
result.organization = checkpoint_make_shared<const OrganizationState>(
    std::move(organization.value));
