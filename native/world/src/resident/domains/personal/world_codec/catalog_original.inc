// 个人战斗世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto personal = decode_personal_catalog(data);
if (!personal.ok)
  return {false, {}, personal.error};
auto simultaneous =
    configure_personal_simultaneous_outcomes(data, scenario, personal.value);
if (!simultaneous.ok)
  return {false, {}, simultaneous.error};
result->personal =
    checkpoint_make_shared<const PersonalCatalog>(std::move(personal.value));
