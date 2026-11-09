// 交互世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto born_dialogues =
    decode_born_personal_dialogue_profile(scenario, required_capabilities());
if (!born_dialogues.ok)
  return {false, {}, born_dialogues.error};
if (born_dialogues.value) {
  if (!result->family_birth || !result->family)
    return {false,
            {},
            common_failure("BornPersonalDialogueBirthProfileRequired").error};
  auto decoded = decode_born_personal_dialogue_catalog(
      field(field(scenario, "dynamic_world", "scenario"),
            "born_personal_dialogues", "dynamic_world"),
      *result->interaction, *result->family, *result->family_birth);
  if (!decoded.ok)
    return {false, {}, decoded.error};
  result->born_personal_dialogues =
      checkpoint_make_shared<const BornPersonalDialogueCatalog>(
          std::move(decoded.value));
}
