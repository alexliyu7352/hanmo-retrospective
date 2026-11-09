// 成长世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto growth = [&] {
  // 原 Growth 内部 typed 字段也借同一候选 job；本段结束后再开独立终态段。
  std::optional<ScopedCheckpointJsonDecode> section;
  if (owned_budget && catalog.exact_time &&
      exact_complete_selected(*catalog.exact_time))
    section.emplace(*owned_budget, *owned_job, "growth",
                    current_checkpoint_memory());
  return catalog.exact_time && exact_complete_selected(*catalog.exact_time)
             ? (catalog.growth->complete_capability_application_chain
                    ? decode_growth_trade_grant_seed(source,
                                                          *catalog.growth)
                : catalog.growth->complete_exact_rest_mouth_sources
                    ? decode_growth_exact_rest_mouth_seed(source,
                                                          *catalog.growth)
                : catalog.growth->complete_exact_command_cuts
                    ? decode_growth_exact_command_cut_seed(source,
                                                           *catalog.growth)
                    : decode_growth_exact_period_seed(source, *catalog.growth))
         : catalog.exact_time
             ? decode_growth_fixed_service_seed(
                   source, catalog.regional_life, catalog.growth->cards.enabled,
                   catalog.growth->teaching_requests.enabled,
                   catalog.growth->study_teaching_requests.enabled,
                   catalog.growth->innate_learning)
             : decode_growth_seed(
                   source, catalog.regional_life, catalog.growth->cards.enabled,
                   catalog.growth->teaching_requests.enabled,
                   catalog.growth->study_teaching_requests.enabled,
                   catalog.growth->innate_learning);
}();
if (!growth.ok)
  return {false, {}, growth.error};
// 原终态发表独立 section 的唯一 JSON 解码；旧 Growth 解码调用与字节不动。
if (catalog.exact_time && exact_complete_selected(*catalog.exact_time)) {
  auto terminal = [&] {
    const auto &original =
        field(source, "growth_terminal_publications", "state");
    std::optional<ScopedCheckpointJsonDecode> section;
    if (owned_budget)
      section.emplace(*owned_budget, *owned_job, "growth_terminal_publications",
                      current_checkpoint_memory());
    return decode_growth_terminal_publications_v1(original);
  }();
  if (!terminal.ok)
    return {false, {}, std::move(terminal.error)};
  if (!growth.value.terminal_publications.empty())
    return common_failure("GrowthTerminalPublicationSectionMismatch",
                          "growth_terminal_publications");
  growth.value.terminal_publications = std::move(terminal.value);
} else if (source.find("growth_terminal_publications") ||
           !growth.value.terminal_publications.empty())
  return common_failure("UnexpectedGrowthTerminalPublications");
// grant 拥有实录只在原 phase4 选择；不进入旧 Growth body，也不由终态反推。
if (catalog.growth->complete_capability_application_chain) {
  auto grants = [&] {
    const auto &original = field(source, "trade-grant", "state");
    std::optional<ScopedCheckpointJsonDecode> section;
    if (owned_budget)
      section.emplace(*owned_budget, *owned_job, "trade-grant", current_checkpoint_memory());
    return decode_growth_trade_grants_v1(original);
  }();
  if (!grants.ok) return {false, {}, std::move(grants.error)};
  if (!growth.value.trade_grants.empty())
    return common_failure("GrowthTradeGrantSectionMismatch", "trade-grant");
  growth.value.trade_grants = std::move(grants.value);
} else if (source.find("trade-grant") || !growth.value.trade_grants.empty())
  return common_failure("UnexpectedGrowthTradeGrants", "trade-grant");
result.growth = std::make_shared<const GrowthState>(std::move(growth.value));
