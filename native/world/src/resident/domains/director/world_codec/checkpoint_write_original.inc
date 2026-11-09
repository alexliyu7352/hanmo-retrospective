// 决策世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
r = section("director", [&](auto &c) {
  return visit_native_director_checkpoint(c, w.director, *w.catalog);
});
if (!r.ok)
  return {false, {}, r.error};
