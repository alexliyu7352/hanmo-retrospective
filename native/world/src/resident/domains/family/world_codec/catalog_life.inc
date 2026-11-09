// 家庭世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (const auto *config = scenario.find("family_life")) {
  auto unique = validate_succession_json_unique(catalog);
  if (!unique.ok)
    return {false, {}, unique.error};
  auto parsed = decode_native_family_life_policy(*config);
  if (!parsed.ok)
    return {false, {}, parsed.error};
  const auto &caps = required_capabilities();
  // 作者两种生命策略共用已登记能力；实际版本仍由原策略和目录校验锁定。
  const StableId capability = "family_life_v1";
  if (!scenario.find("dynamic_world") ||
      std::find(caps.begin(), caps.end(), capability) == caps.end())
    unsupported("FamilyLifeCapabilityRequired", "scenario/family_life");
  result->family_life =
      checkpoint_make_shared<const FamilyDeathPolicy>(std::move(parsed.value));
}
