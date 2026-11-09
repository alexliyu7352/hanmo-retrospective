// 政治世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (const auto *config = dynamic->find("diplomacy")) {
  const auto &caps = required_capabilities();
  if (std::find(caps.begin(), caps.end(), "native_dynamic_world") == caps.end())
    fail("DiplomacyCapabilityRequired", "dynamic_world/diplomacy");
  auto decoded = decode_diplomacy_catalog(*config);
  if (!decoded.ok)
    return {false, {}, decoded.error};
  auto profile = validate_diplomacy_runtime_profile(decoded.value);
  if (!profile.ok)
    return {false, {}, profile.error};
  auto valid = validate_diplomacy_catalog(*result->common,
                                          *result->organization, decoded.value);
  if (!valid.ok)
    return {false, {}, valid.error};
  result->diplomacy =
      checkpoint_make_shared<const DiplomacyCatalog>(std::move(decoded.value));
  result->envoy_tasks_enabled = true;
}
if (const auto *config = dynamic->find("treaty_passage")) {
  if (!result->diplomacy || !result->politics)
    fail("TreatyPassageDependenciesRequired", "dynamic_world/treaty_passage");
  const auto &caps = required_capabilities();
  if (std::find(caps.begin(), caps.end(), "treaty_passage_v1") == caps.end())
    fail("TreatyPassageCapabilityRequired", "dynamic_world/treaty_passage");
  auto decoded = decode_treaty_passage_catalog(*config);
  if (!decoded.ok)
    return {false, {}, decoded.error};
  if (!decoded.value.enabled)
    fail("TreatyPassageProfileRequired", "dynamic_world/treaty_passage");
  auto valid = validate_treaty_passage_catalog(
      *result->common, *result->organization, *result->politics,
      *result->diplomacy, decoded.value);
  if (!valid.ok)
    return {false, {}, valid.error};
  result->treaty_passage = checkpoint_make_shared<const TreatyPassageCatalog>(
      std::move(decoded.value));
}
