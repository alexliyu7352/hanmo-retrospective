// 家庭世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (const auto *config = scenario.find("family_voluntary_paths")) {
  const auto &caps = required_capabilities();
  if (std::find(caps.begin(), caps.end(), "family_voluntary_paths_v1") ==
      caps.end())
    fail("VoluntaryPathCapabilityRequired", "scenario/family_voluntary_paths");
  if (!result->family || !result->family->voluntary_succession ||
      result->family->voluntary_source_mode !=
          FamilyVoluntarySourceMode::living_paths ||
      !result->known_personal_dialogues ||
      result->dynamic_profile != DynamicProfile::local_society_v1)
    fail("VoluntaryPathSourcePairRequired", "scenario/family_voluntary_paths");
  auto parsed = decode_voluntary_path_catalog(*config);
  if (!parsed.ok)
    return {false, {}, parsed.error};
  result->family_voluntary_paths =
      checkpoint_make_shared<const VoluntaryPathCatalog>(
          std::move(parsed.value));
}
