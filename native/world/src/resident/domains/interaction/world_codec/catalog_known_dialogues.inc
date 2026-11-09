// 交互世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto known_dialogues =
    decode_known_personal_dialogue_profile(scenario, required_capabilities());
if (!known_dialogues.ok)
  return {false, {}, known_dialogues.error};
if (known_dialogues.value) {
  if (!result->family || !result->family_life)
    return {false,
            {},
            common_failure("KnownPersonalDialogueLifeProfileRequired").error};
  auto parsed = decode_known_personal_dialogue_catalog(
      field(field(scenario, "dynamic_world", "scenario"),
            "known_personal_dialogues", "dynamic_world"),
      *result->interaction, *result->family,
      result->family_birth ? *result->family_birth : BirthArrangementCatalog{});
  if (!parsed.ok)
    return {false, {}, parsed.error};
  result->known_personal_dialogues =
      checkpoint_make_shared<const KnownPersonalDialogueCatalog>(
          std::move(parsed.value));
}
