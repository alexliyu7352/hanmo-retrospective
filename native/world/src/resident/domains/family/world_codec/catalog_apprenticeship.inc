// 家庭世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。 本人师承依实际家户、培养和已核死亡来源准入，不要求继产或出生能力。
if (const auto *config = scenario.find("family_apprenticeship")) {
  const auto &caps = required_capabilities();
  if (std::find(caps.begin(), caps.end(), "family_apprenticeship_v1") ==
      caps.end())
    fail("ApprenticeshipCapabilityRequired", "scenario/family_apprenticeship");
  if (result->dynamic_profile != DynamicProfile::local_society_v1 ||
      !result->family || !result->family->enabled || !result->family_life ||
      !result->growth || !result->growth->cultivation || !result->interaction)
    fail("ApprenticeshipDependenciesRequired",
         "scenario/family_apprenticeship");
  auto parsed = decode_apprenticeship_catalog(*config);
  if (!parsed.ok)
    return {false, {}, parsed.error};
  auto checked = validate_apprenticeship_catalog(parsed.value, *result->common,
                                                 *result->interaction);
  if (!checked.ok)
    return {false, {}, checked.error};
  result->family_apprenticeship =
      checkpoint_make_shared<const ApprenticeshipCatalog>(
          std::move(parsed.value));
}
if (const auto *config = scenario.find("family_disciple_legacy")) {
  const auto &caps = required_capabilities();
  if (std::find(caps.begin(), caps.end(), "family_disciple_legacy_v1") ==
      caps.end())
    fail("DiscipleControlCapabilityRequired",
         "scenario/family_disciple_legacy");
  if (!result->family_apprenticeship || !result->family_legacy ||
      !result->family_life ||
      result->dynamic_profile != DynamicProfile::local_society_v1)
    fail("DiscipleControlDependenciesRequired",
         "scenario/family_disciple_legacy");
  auto parsed = decode_disciple_control_catalog(*config);
  if (!parsed.ok)
    return {false, {}, parsed.error};
  auto checked =
      validate_disciple_control_catalog(parsed.value, *result->family_legacy,
                                        *result->common, *result->interaction);
  if (!checked.ok)
    return {false, {}, checked.error};
  result->family_disciple_legacy =
      checkpoint_make_shared<const DiscipleControlCatalog>(
          std::move(parsed.value));
}
