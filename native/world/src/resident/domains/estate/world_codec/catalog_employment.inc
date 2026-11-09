// 庄园世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (const auto *config = dynamic->find("estate_employment")) {
  const auto &caps = required_capabilities();
  for (const auto *cap : {"estate_employment_v1", "native_dynamic_world",
                          "estates", "economy", "dialogue", "multi_actor"})
    if (std::find(caps.begin(), caps.end(), cap) == caps.end())
      unsupported("EstateEmploymentCapabilityRequired", cap);
  ScopedCheckpointJsonDecode original(
      CheckpointCatalogJsonKind::estate_employment);
  auto parsed = decode_estate_employment_catalog(*config);
  if (!parsed.ok)
    return {false, {}, parsed.error};
  result->estate_employment =
      checkpoint_make_shared<const EstateEmploymentCatalog>(
          std::move(parsed.value));
}
