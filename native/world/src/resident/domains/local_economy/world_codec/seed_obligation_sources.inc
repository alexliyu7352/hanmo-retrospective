// 地方经济世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (obligation_versions.value.sources) {
  ScopedCheckpointJsonDecode original(
      CheckpointWorldJsonSection::economic_obligation_sources);
  auto state = decode_economic_obligation_sources(
      field(source, "economic_obligation_sources", "state"));
  if (!state.ok)
    return {false, {}, std::move(state.error)};
  result.economic_obligation_sources =
      checkpoint_make_shared<const EconomicObligationSourcesState>(
          std::move(state.value));
} else if (source.find("economic_obligation_sources"))
  fail("UnexpectedEconomicObligationSources", "economic_obligation_sources");
