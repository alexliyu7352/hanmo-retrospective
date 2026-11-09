// 行旅世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto b = decode_travel_catalog(data, get_string(state, "scenario_id", ""));
if (!b.ok)
  return {false, {}, b.error};
result->travel =
    checkpoint_make_shared<const TravelCatalog>(std::move(b.value));
