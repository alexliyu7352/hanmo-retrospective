// 家庭世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto family = decode_family_catalog(data, get_string(state, "scenario_id", ""));
if (!family.ok)
  return {false, {}, family.error};
result->family =
    checkpoint_make_shared<const FamilyCatalog>(std::move(family.value));
if (const auto *config = scenario.find("family_bequest")) {
  auto unique = validate_succession_json_unique(catalog);
  if (!unique.ok)
    return {false, {}, unique.error};
  const auto &caps = required_capabilities();
  if (!result->family_life || !result->family->enabled ||
      std::find(caps.begin(), caps.end(), "family_bequest_v1") == caps.end())
    unsupported("FamilyBequestProfileRequired", "scenario/family_bequest");
  auto policy = decode_family_bequest_policy(*config);
  if (!policy.ok)
    return {false, {}, policy.error};
  result->family_bequest = checkpoint_make_shared<const FamilyBequestPolicy>(
      std::move(policy.value));
}
if (const auto *config = scenario.find("family_inheritance")) {
  auto unique = validate_succession_json_unique(catalog);
  if (!unique.ok)
    return {false, {}, unique.error};
  const auto &caps = required_capabilities();
  if (!result->family_bequest || !result->task_message_notices ||
      std::find(caps.begin(), caps.end(), "family_inheritance_v1") ==
          caps.end())
    unsupported("FamilyInheritanceProfileRequired",
                "scenario/family_inheritance");
  auto policy = decode_family_inheritance_policy(*config);
  if (!policy.ok)
    return {false, {}, policy.error};
  result->family_inheritance =
      checkpoint_make_shared<const FamilyInheritancePolicy>(
          std::move(policy.value));
}
if (const auto *config = scenario.find("family_marriage")) {
  const auto &caps = required_capabilities();
  if (!result->family_life || !result->family || !result->family->enabled ||
      std::find(caps.begin(), caps.end(), "family_marriage_v1") == caps.end())
    unsupported("MarriageProfileRequired", "scenario/family_marriage");
  auto policy = decode_marriage_source_catalog(*config);
  if (!policy.ok)
    return {false, {}, policy.error};
  auto valid = validate_marriage_source_catalog(policy.value, *result->common,
                                                *result->interaction);
  if (!valid.ok)
    return {false, {}, valid.error};
  result->family_marriage = checkpoint_make_shared<const MarriageSourceCatalog>(
      std::move(policy.value));
}
if (bool(scenario.find("family_birth")) !=
    bool(scenario.find("family_lineage")))
  fail("FamilyBirthProfileMismatch", "scenario");
if (const auto *config = scenario.find("family_birth")) {
  const auto &caps = required_capabilities();
  if (!result->family_life || !result->family->enabled ||
      !result->common->extended_skills ||
      std::find(caps.begin(), caps.end(), "family_birth_v1") == caps.end())
    unsupported("BirthProfileRequired", "scenario/family_birth");
  auto lineage = decode_family_lineage_catalog(
      field(scenario, "family_lineage", "scenario"));
  if (!lineage.ok)
    return {false, {}, lineage.error};
  auto birth = decode_native_family_birth_catalog(scenario, id);
  if (!birth.ok)
    return {false, {}, birth.error};
  auto valid = validate_birth_arrangement_catalog(
      birth.value, *result->common, *result->interaction, *result->family);
  if (!valid.ok)
    return {false, {}, valid.error};
  result->family_lineage = checkpoint_make_shared<const FamilyLineageCatalog>(
      std::move(lineage.value));
  result->family_birth = checkpoint_make_shared<const BirthArrangementCatalog>(
      std::move(birth.value));
}
