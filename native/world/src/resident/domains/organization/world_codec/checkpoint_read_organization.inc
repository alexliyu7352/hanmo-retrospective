// 组织检查点恢复分派：沿原 else-if 链读取 organization, organization_runtime,
// organization_service_effects；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "organization") {
  if (organization_body_decoded)
    return {false, {}, common_failure("DuplicateOrganizationBody").error};
  auto selected = selected_native_organization_runtime(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  r = selected.value ? visit_organization_runtime_selected_body(
                           reader, *organization, catalog.identity)
                     : visit_organization_checkpoint(reader, *organization,
                                                     catalog.named_membership);
  if (r.ok)
    organization_body_decoded = true;
}
else if (section.domain == "organization_runtime") {
  auto selected = selected_native_organization_runtime(catalog.identity);
  if (!selected.ok || selected.value != 1 || seed.organization_runtime)
    return {false,
            {},
            common_failure("OrganizationRuntimeSelectedRootMismatch").error};
  auto runtime = make_state<Seed, OrganizationRuntime>();
  r = visit_organization_runtime(reader, *organization, *runtime,
                                 catalog.named_membership);
  seed.organization_runtime = std::move(runtime);
}
else if (section.domain == "organization_service_effects") {
  auto selected =
      selected_native_organization_service_effects(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (selected.value != 1 || section.codec_version != 1 ||
      !organization_body_decoded || organization_effects_decoded)
    return {false,
            {},
            common_failure("OrganizationServiceEffectsSelectedLayoutMismatch")
                .error};
  r = visit_organization_service_effects_checkpoint(reader, *organization,
                                                    selected.value);
  if (r.ok)
    organization_effects_decoded = true;
}