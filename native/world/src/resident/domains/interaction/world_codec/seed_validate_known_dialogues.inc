// 交互世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
{
  CommonStore view(result.common, catalog.common);
  auto checked = validate_known_personal_dialogue_json_payloads(
      view,
      result.interaction && result.interaction->known_personal_dialogues
          ? &*result.interaction->known_personal_dialogues
          : nullptr,
      source);
  if (!checked.ok)
    return {false, {}, checked.error};
}
