// 家庭世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (const auto *config = scenario.find("family_care_relief")) {
  const auto &caps = required_capabilities();
  if (!result->family_birth || !result->family_lineage ||
      !result->family_life ||
      std::find(caps.begin(), caps.end(), "family_care_relief_v1") ==
          caps.end())
    unsupported("ChildCareReliefProfileRequired",
                "scenario/family_care_relief");
  auto care = decode_child_care_relief_catalog(*config);
  if (!care.ok)
    return {false, {}, care.error};
  auto valid = validate_child_care_relief_catalog(care.value, *result->common,
                                                  *result->interaction);
  if (!valid.ok)
    return {false, {}, valid.error};
  result->family_care_relief =
      checkpoint_make_shared<const ChildCareReliefCatalog>(
          std::move(care.value));
}

if (const auto *config = scenario.find("family_legacy")) {
  const bool biological = scenario.find("family_biological_legacy") != nullptr;
  const bool collateral = scenario.find("family_collateral_legacy") != nullptr;
  auto unique = validate_succession_json_unique(catalog);
  if (!unique.ok)
    return {false, {}, unique.error};
  const auto &caps = required_capabilities();
  if (std::find(caps.begin(), caps.end(),
                collateral   ? "family_legacy_v3"
                : biological ? "family_legacy_v2"
                             : "family_legacy_v1") == caps.end())
    fail("FamilyLegacyCapabilityRequired", "scenario/family_legacy");
  if (result->dynamic_profile != DynamicProfile::local_society_v1 ||
      !result->family_life || !result->family || !result->family->enabled)
    fail("FamilyLegacyDependenciesRequired", "scenario/family_legacy");
  BoundaryValue empty{BoundaryValue::Dict{}};
  const auto *definitions = data.find("legacy_control_consent_requests");
  if (!definitions && !biological && !collateral)
    fail("MissingField", "legacy_control_consent_requests");
  auto parsed =
      collateral ? decode_legacy_control_consent_catalog_v3(
                       *config, definitions ? *definitions : empty)
      : biological
          ? decode_legacy_control_consent_catalog_v2(
                *config, definitions ? *definitions : empty)
          : decode_legacy_control_consent_catalog(*config, *definitions);
  if (!parsed.ok)
    return {false, {}, parsed.error};
  auto valid = validate_legacy_control_consent_catalog(
      parsed.value, *result->common, *result->interaction);
  if (!valid.ok)
    return {false, {}, valid.error};
  result->family_legacy =
      checkpoint_make_shared<const LegacyControlConsentCatalog>(
          std::move(parsed.value));
}
