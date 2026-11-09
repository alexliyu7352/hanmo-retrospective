// 地方经济世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto production_catalog = validate_native_production_cycles_catalog(catalog);
if (!production_catalog.ok)
  return {false, {}, std::move(production_catalog.error)};
if (catalog.production_cycles) {
  ScopedCheckpointJsonDecode original_decode(
      CheckpointWorldJsonSection::production_cycles);
  auto parsed = decode_economic_work_sources(
      field(source, "production_cycles", "state"),
      EconomicWorkSourceSchema::production_calendar3);
  if (!parsed.ok)
    return {false, {}, std::move(parsed.error)};
  result.production_cycles =
      checkpoint_make_shared<const EconomicWorkSourcesState>(
          std::move(parsed.value));
} else if (source.find("production_cycles"))
  fail("UnexpectedProductionCyclesState", "production_cycles");
