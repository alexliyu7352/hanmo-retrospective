// 政治世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (const auto *config = dynamic->find("politics")) {
  const auto &caps = required_capabilities();
  if (std::find(caps.begin(), caps.end(), "native_dynamic_world") == caps.end())
    fail("PoliticalCapabilityRequired", "dynamic_world/politics");
  auto decoded = decode_politics_catalog(*config);
  if (!decoded.ok)
    return {false, {}, decoded.error};
  if (!decoded.value.enabled)
    fail("PoliticalProfileRequired", "dynamic_world/politics");
  result->politics =
      checkpoint_make_shared<const PoliticsCatalog>(std::move(decoded.value));
}
if (const auto *config = dynamic->find("political_fiscal")) {
  const auto &caps = required_capabilities();
  if (std::find(caps.begin(), caps.end(), "native_dynamic_world") == caps.end())
    fail("PoliticalFiscalCapabilityRequired", "dynamic_world/political_fiscal");
  if (!politics_enabled(*result))
    fail("PoliticalFiscalRequiresPolitics", "dynamic_world/political_fiscal");
  auto body = read_political_author_body_version(scenario);
  if (!body.ok)
    return {false, {}, std::move(body.error)};
  auto decoded = [&] {
    std::optional<ScopedCheckpointJsonDecode> scope;
    if (ScopedCheckpointBoundaryDecode::current())
      scope.emplace(CheckpointCatalogJsonKind::politics);
    return body.value == 2 ? decode_political_fiscal_catalog2(*config)
                           : decode_political_fiscal_catalog(*config);
  }();
  if (!decoded.ok)
    return {false, {}, decoded.error};
  if (!decoded.value.enabled)
    fail("PoliticalFiscalProfileRequired", "dynamic_world/political_fiscal");
  result->political_fiscal =
      checkpoint_make_shared<const PoliticalFiscalCatalog>(
          std::move(decoded.value));
}
