// 家庭世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (const auto *config = scenario.find("family_biological_legacy")) {
  const auto &caps = required_capabilities();
  if (std::find(caps.begin(), caps.end(), "family_biological_legacy_v1") ==
      caps.end())
    fail("BiologicalControlCapabilityRequired",
         "scenario/family_biological_legacy");
  if (!result->family_legacy ||
      (result->family_legacy->version != 2 &&
       result->family_legacy->version != 3) ||
      !result->family_birth || !result->family_lineage ||
      !result->family_life || !result->family->enabled ||
      result->dynamic_profile != DynamicProfile::local_society_v1)
    fail("BiologicalControlDependenciesRequired",
         "scenario/family_biological_legacy");
  auto parsed = decode_biological_control_catalog(*config);
  if (!parsed.ok)
    return {false, {}, parsed.error};
  BiologicalControlTemplateRead templates =
      [&](const StableId &name) -> Outcome<BiologicalControlTemplateIdentity> {
    const auto *item = result->born_personal_dialogues
                           ? born_personal_dialogue_template(
                                 *result->born_personal_dialogues, name)
                           : nullptr;
    if (!item)
      return {false,
              {},
              common_failure("BiologicalControlTemplateSourceRequired", name)
                  .error};
    return {true, {item->id, item->prototype_id, item->dialogue_id}, {}};
  };
  auto checked = validate_biological_control_catalog(
      parsed.value, *result->family_legacy, *result->common,
      *result->interaction, templates);
  if (!checked.ok)
    return {false, {}, checked.error};
  result->family_biological_legacy =
      checkpoint_make_shared<const BiologicalControlCatalog>(
          std::move(parsed.value));
}
if (const auto *config = scenario.find("family_collateral_legacy")) {
  const auto &caps = required_capabilities();
  if (std::find(caps.begin(), caps.end(), "family_collateral_legacy_v1") ==
      caps.end())
    fail("CollateralControlCapabilityRequired",
         "scenario/family_collateral_legacy");
  if (!result->family_legacy || result->family_legacy->version != 3 ||
      !result->family_lineage || !result->family_life ||
      !result->family->enabled || !result->known_personal_dialogues ||
      result->dynamic_profile != DynamicProfile::local_society_v1)
    fail("CollateralControlDependenciesRequired",
         "scenario/family_collateral_legacy");
  auto parsed = decode_collateral_control_catalog(*config);
  if (!parsed.ok)
    return {false, {}, parsed.error};
  CollateralControlTemplateRead templates =
      [&](const StableId &name) -> Outcome<KnownPersonalDialogueTemplate> {
    const auto *t = known_personal_dialogue_template(
        *result->known_personal_dialogues, name);
    if (!t)
      return {false,
              {},
              common_failure("CollateralControlTemplateSourceRequired", name)
                  .error};
    return {true, *t, {}};
  };
  auto checked = validate_collateral_control_catalog(
      parsed.value, *result->family_legacy, *result->interaction, templates);
  if (!checked.ok)
    return {false, {}, checked.error};
  for (const auto &row : parsed.value.definitions.rows())
    if (result->family_biological_legacy &&
        result->family_biological_legacy->definitions.find(row.first))
      fail("CollateralControlRequestCollision", row.first);
  result->family_collateral_legacy =
      checkpoint_make_shared<const CollateralControlCatalog>(
          std::move(parsed.value));
}
