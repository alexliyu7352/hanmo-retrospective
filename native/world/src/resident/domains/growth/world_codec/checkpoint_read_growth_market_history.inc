// 市场binary只按目录显式1/2选唯一正文，缺布局或错配不猜读。
else if (section.domain == "growth_market_history") {
  auto layout = validate_native_growth_market_catalog(catalog);
  if (!layout.ok) return {false, {}, std::move(layout.error)};
  if (!catalog.growth_market_history || section.codec_version != catalog.growth_market_history->selected_version)
    return {false, {}, common_failure("GrowthMarketPhysicalVersionMismatch").error};
  auto state = make_state<Seed, GrowthMarketHistoryState>();
  r = catalog.growth_market_history->selected_version == 2
      ? visit_growth_market_history_v2_checkpoint(reader, *state, catalog.growth_market_history->resource_codec)
      : visit_growth_market_history_checkpoint(reader, *state);
  seed.growth_market_history = std::move(state);
}
