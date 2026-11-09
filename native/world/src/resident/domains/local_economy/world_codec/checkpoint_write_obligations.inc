// 地方经济世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (w.economic_obligation_sources) {
  r = section("economic_obligation_sources", [&](auto &c) {
    return visit_economic_obligation_sources(c, *w.economic_obligation_sources);
  });
  if (!r.ok)
    return {false, {}, r.error};
}
