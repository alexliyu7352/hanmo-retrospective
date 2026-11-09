// 地方经济世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto production = validate_native_production_cycles_world(w);
if (!production.ok)
  return production;
if (w.production_cycles)
  WORLD_WHOLE(P::section, "production_cycles",
              emit_economic_work_sources(
                  out, *w.production_cycles,
                  EconomicWorkSourceSchema::production_calendar3));
