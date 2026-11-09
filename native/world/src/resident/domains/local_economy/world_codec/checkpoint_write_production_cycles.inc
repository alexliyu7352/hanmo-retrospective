// 地方经济世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
r = validate_native_production_cycles_world(w);
if (!r.ok)
  return r;
if (w.production_cycles) {
  r = section("production_cycles", [&](auto &c) {
    return visit_economic_work_sources(
        c, *w.production_cycles,
        EconomicWorkSourceSchema::production_calendar3);
  });
  if (!r.ok)
    return r;
}
