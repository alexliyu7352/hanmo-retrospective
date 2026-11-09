// 成长世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto permanent_catalog = validate_native_growth_permanent_catalog(catalog);
if (!permanent_catalog.ok)
  return {false, {}, std::move(permanent_catalog.error)};
if (catalog.growth->complete_capability_application_chain) {
  auto decoded = [&] {
    std::optional<ScopedCheckpointJsonDecode> section;
    if (owned_budget)
      section.emplace(*owned_budget, *owned_job, "growth_permanent_contributions", current_checkpoint_memory());
    return decode_growth_permanent_section(
        field(source, "growth_permanent_contributions", "state"),
        GrowthPermanentSectionVersion::trade_grants);
  }();
  if (!decoded.ok)
    return {false, {}, std::move(decoded.error)};
  result.growth_permanent = checkpoint_make_shared<const GrowthPermanentState>(
      std::move(decoded.value.permanent));
  result.card_acquisitions = checkpoint_make_shared<const CardAcquisitionState>(
      std::move(decoded.value.cards));
} else if (source.find("growth_permanent_contributions"))
  fail("UnexpectedGrowthPermanentState", "state");
