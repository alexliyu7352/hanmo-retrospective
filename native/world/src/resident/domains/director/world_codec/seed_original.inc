// 决策世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto director = decode_native_director_state(
    boundary::field(source, "director", ""), catalog);
if (!director.ok)
  return {false, {}, director.error};
result.director =
    std::make_shared<const DirectorState>(std::move(director.value));
