// 市场JSON按实际锁版本成对恢复；物理2只借当前同job预算，不造中间树。
if (catalog.growth_market_history) {
  auto layout = validate_native_growth_market_catalog(catalog);
  if (!layout.ok) return {false, {}, std::move(layout.error)};
  auto parsed = [&]() -> Outcome<GrowthMarketHistoryState> {
    const auto &input = field(source, "growth_market_history", "state");
    if (catalog.growth_market_history->selected_version == 2) {
      if (!checkpoint_boundary_decode_active())
        return {false, {}, common_failure("GrowthMarketOwnedJsonDecodeRequired").error};
      ScopedCheckpointJsonDecode section(CheckpointWorldJsonSection::growth_market_history);
      return decode_growth_market_history_v2(input, catalog.growth_market_history->resource_codec, section);
    }
    return decode_growth_market_history(input);
  }();
  if (!parsed.ok) return {false, {}, std::move(parsed.error)};
  result.growth_market_history = checkpoint_make_shared<const GrowthMarketHistoryState>(std::move(parsed.value));
} else if (source.find("growth_market_history"))
  fail("UnexpectedShopMarketDomain", "growth_market_history");
