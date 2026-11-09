#include "resident/domains/growth/growth_model.hpp"
// 原trade writer的逐笔技能净值反验；市场金额、今日差额与交易次数都不是授予来源。
#include "growth_trade_grant_read.hpp"
#include "growth_trade_terminal_read.hpp"
#include <algorithm>
namespace hanmo::resident {
Outcome<GrowthTradeGrantView> read_growth_trade_grant_v1(
    const CommonStore &common, const GrowthState &growth,
    const GrowthCatalog &catalog, const CommonCatalog &shared,
    const GrowthTradeGrantRef &ref, const WorldMoment &now) {
  const auto bad = [&](const char *code) -> Outcome<GrowthTradeGrantView> {
    return {false, {}, common_failure(code, ref.action_id).error};
  };
  if (!catalog.complete_capability_application_chain ||
      !catalog.complete_study_sources)
    return bad("GrowthTradeGrantProfileRequired");
  const auto *grant = growth.trade_grants.find(ref.action_id);
  if (!grant || grant->action_id != ref.action_id ||
      grant->actor_id.empty() || grant->skill_id != "commerce" ||
      grant->original_trade_result_ref.domain != "growth" ||
      grant->original_trade_result_ref.kind != "life_activity" ||
      grant->original_trade_result_ref.id != ref.action_id ||
      grant->original_trade_source_schema != 2)
    return bad("GrowthTradeGrantOriginalSourceRequired");
  auto original = read_growth_trade_terminal_v2(
      common, growth, catalog, shared, grant->original_trade_result_ref,
      grant->original_trade_source_schema, now);
  if (!original.ok)
    return {false, {}, std::move(original.error)};
  const auto &view = original.value.activity->view;
  const auto *actor = common.state().actors.find(grant->actor_id);
  if (view.phase != LifeActivityPhase::completed || !view.success ||
      !view.code.empty() || view.actor_id != grant->actor_id || !actor)
    return bad("GrowthTradeGrantSuccessfulTradeRequired");
  if (grant->xp_before < 0 || grant->xp_before > 1200 ||
      grant->actual_xp_gain != std::min<Integer>(10, 1200 - grant->xp_before) ||
      grant->xp_after != grant->xp_before + grant->actual_xp_gain ||
      actor->skills.commerce < grant->xp_after)
    return bad("GrowthTradeGrantActualNetMismatch");
  return {true, {grant, original.value.terminal}, {}};
}

Status validate_growth_trade_grants_v1(
    const CommonStore &common, const GrowthState &growth,
    const GrowthCatalog &catalog, const CommonCatalog &shared,
    const WorldMoment &now) {
  if (!catalog.complete_capability_application_chain)
    return growth.trade_grants.empty()
               ? common_success()
               : common_failure("UnexpectedGrowthTradeGrant");
  for (const auto &row : growth.trade_grants.rows()) {
    auto source = read_growth_trade_grant_v1(common, growth, catalog, shared,
                                           {row.first}, now);
    if (!source.ok)
      return {false, {}, std::move(source.error)};
  }
  for (const auto &row : growth.life_activities.rows()) {
    const auto &view = row.second.view;
    const bool required = view.kind == LifeActivityKind::trade &&
                          view.phase == LifeActivityPhase::completed;
    if (bool(growth.trade_grants.find(row.first)) != required)
      return common_failure(required ? "GrowthTradeGrantRequired"
                                     : "UnexpectedGrowthTradeGrant",
                            row.first);
  }
  return common_success();
}
} // namespace hanmo::resident
