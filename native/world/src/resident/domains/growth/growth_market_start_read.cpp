// 已批准首批市场关账：本文件具名 plan 仅停内存计量，原数量、来源、复制及租约流程不变。
// 原开始时刻只有真实capture这一个持值；纯reader不执行交易、不造FSL边界或补历史。
#include "growth_market_start_read.hpp"
#include "growth_trade_terminal_codec.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include <algorithm>
#include <charconv>
namespace hanmo::resident {
namespace {
// 借原Common序和全局ID；不以查询缓存、当前Action存在或相同Tick替代源。
std::optional<Integer> allocated(const CommonStore &common, const StableId &id,
                                 std::string_view prefix) {
  if (id.size() <= prefix.size() + 1 ||
      id.compare(0, prefix.size(), prefix) != 0 || id[prefix.size()] != ':')
    return {};
  const char *first = id.data() + prefix.size() + 1;
  const char *last = id.data() + id.size();
  if (*first < '1' || *first > '9')
    return {};
  Integer value = 0;
  const auto parsed = std::from_chars(first, last, value);
  if (parsed.ec != std::errc{} || parsed.ptr != last || value < 1 ||
      value >= common.state().next_instance_id)
    return {};
  return value;
}
std::size_t position(const CommonStore &common, const StableId &id) {
  const auto &ids = common.state().facts.order();
  return static_cast<std::size_t>(std::find(ids.begin(), ids.end(), id) -
                                  ids.begin());
}
bool selected(const CatalogIdentity &identity, std::string_view name,
              std::string_view version) {
  std::size_t count = 0;
  for (const auto &item : identity.module_versions)
    if (item.first == name) {
      if (item.second != version)
        return false;
      ++count;
    }
  return count == 1;
}
// 原历史接触完整相等，不以现任店员或同地点替换真实已接受角色。
bool same_context(const InteractionContext &a, const InteractionContext &b) {
  return a.place_id == b.place_id &&
         a.provider_actor_id == b.provider_actor_id && a.role_id == b.role_id &&
         a.kind == b.kind && a.encounter_id == b.encounter_id &&
         !a.input_error && !b.input_error;
}
} // namespace
Outcome<GrowthScalarMarketStartView> read_growth_scalar_market_start_v2(
    const CommonStore &common, const CommonCatalog &shared,
    const GrowthCatalog &catalog, const GrowthState &growth,
    const ActorFoundationCatalog &foundation,
    const GrowthMarketHistoryCatalog &history_catalog,
    const GrowthMarketHistoryState &history, const SourceRef &source,
    Integer schema, const WorldMoment &now, ExactArithmeticBudget budget) {
  const auto bad =
      [&](const char *code) -> Outcome<GrowthScalarMarketStartView> {
    return {false, {}, common_failure(code, source.id).error};
  };
  if (schema != 2 || source.domain != "growth_market_history" ||
      source.kind != "growth_market_movement" ||
      !allocated(common, source.id, "market_history"))
    return bad("GrowthScalarStartSourceRequired");
  if (history_catalog.selected_version != 2 || history.version != 2 ||
      !catalog.regional_life || !catalog.economy ||
      !catalog.complete_study_sources ||
      foundation.original_growth.get() != &catalog ||
      !selected(foundation.identity, "growth_market_history", "2") ||
      !selected(foundation.identity, "world_timeline", "2") ||
      !selected(foundation.identity, "action_elapsed", "2") ||
      !selected(foundation.identity, "selected_resource_operations", "1"))
    return bad("GrowthScalarStartSelectedWorldRequired");
  const auto *head = common.state().facts.find(source.id);
  const auto *movement = history.movements.find(source.id);
  if (!head || head->id != source.id || head->domain != source.domain ||
      head->kind != source.kind || !movement ||
      movement->kind != "growth_market_movement" ||
      movement->business_kind != MarketCauseKind::growth_trade ||
      movement->business_source_schema != 1 ||
      !movement->original_business_execution)
    return bad("GrowthScalarStartMovementMismatch");
  const auto *activity =
      growth.life_activities.find(movement->business_fact_id);
  const auto *original = common.state().facts.find(movement->business_fact_id);
  if (!allocated(common, movement->business_fact_id, "action") || !activity ||
      !original || original->id != movement->business_fact_id ||
      original->kind != "life_activity" || original->domain != "growth" ||
      growth.facts.find(movement->business_fact_id) ||
      position(common, movement->business_fact_id) >=
          position(common, source.id) ||
      position(common, source.id) >= common.state().facts.size())
    return bad("GrowthScalarStartOriginalSourceMissing");
  for (const auto &row : history.movements.rows())
    if (row.first != source.id &&
        row.second.business_fact_id == movement->business_fact_id)
      return bad("GrowthScalarStartSourceRepeated");
  const auto &view = activity->view;
  const auto *terms = std::get_if<TradeTerms>(&activity->terms);
  const auto *shop = terms ? catalog.shops.find(terms->shop_id) : nullptr;
  const auto *actor = common.actor(view.actor_id);
  const auto *role = shared.roles.find(activity->context.role_id);
  if (!terms || !shop || !actor || !role ||
      view.kind != LifeActivityKind::trade ||
      view.fact_id != movement->business_fact_id ||
      view.action_id != movement->business_fact_id ||
      (view.phase != LifeActivityPhase::active &&
       view.phase != LifeActivityPhase::completed &&
       view.phase != LifeActivityPhase::cancelled) ||
      view.activity != terms->side ||
      (terms->side != "buy" && terms->side != "sell") ||
      view.item_id != terms->item_id || !shared.items.find(terms->item_id) ||
      std::find(catalog.tradeable_items.begin(), catalog.tradeable_items.end(),
                terms->item_id) == catalog.tradeable_items.end() ||
      terms->quantity < 1 || terms->quantity > 500 ||
      view.quantity != terms->quantity || terms->total < 0 ||
      view.amount != terms->total || view.start_tick < 0 ||
      view.start_tick != terms->valid_tick ||
      view.settlement_tick != view.start_tick ||
      movement->source_tick != view.start_tick ||
      movement->observed_tick != view.start_tick || activity->cost_paid ||
      activity->wage_paid || activity->selected_trade_settlement ||
      std::find(history_catalog.shop_ids.begin(),
                history_catalog.shop_ids.end(),
                terms->shop_id) == history_catalog.shop_ids.end())
    return bad("GrowthScalarStartTermsMismatch");
  const bool buy = terms->side == "buy";
  if (view.from_inventory_id !=
          (buy ? shop->inventory_id : actor->inventory_id) ||
      view.to_inventory_id !=
          (buy ? actor->inventory_id : shop->inventory_id) ||
      view.from_inventory_id == view.to_inventory_id ||
      !common.inventory(view.from_inventory_id) ||
      !common.inventory(view.to_inventory_id) ||
      view.payer_wallet_id !=
          (buy ? terms->payer_wallet_id : shop->wallet_id) ||
      view.payee_wallet_id !=
          (buy ? shop->wallet_id : terms->payer_wallet_id) ||
      view.payer_wallet_id == view.payee_wallet_id ||
      !common.wallet(view.payer_wallet_id) ||
      !common.wallet(view.payee_wallet_id) ||
      terms->context.place_id != shop->place_id ||
      activity->context.place_id != shop->place_id ||
      activity->context.place_id != role->place_id || activity->context.kind ||
      activity->context.encounter_id ||
      !same_context(activity->context, terms->context) ||
      activity->context.provider_actor_id != view.provider_actor_id ||
      view.participant_actor_ids.size() != 2 ||
      view.participant_actor_ids[0] != view.actor_id ||
      view.participant_actor_ids[1] != view.provider_actor_id ||
      view.actor_id == view.provider_actor_id ||
      !common.actor(view.provider_actor_id))
    return bad("GrowthScalarStartParticipantsMismatch");
  const auto &execution = *movement->original_business_execution;
  const auto *at = std::get_if<MarketScalarActualStart>(&execution.time_basis);
  if (execution.schema_version != 1 ||
      execution.adapter_rule_ref.id != "growth_trade_original_fifo" ||
      execution.adapter_rule_ref.version != 1 ||
      execution.adapter_rule_ref.catalog_digest !=
          foundation.identity.content_digest ||
      foundation.identity.content_digest.empty() || !at ||
      at->action_ref != movement->business_fact_id ||
      at->started_at.half_day_tick != view.start_tick ||
      execution.stages.size() != 1 || movement->steps.size() != 1)
    return bad("GrowthScalarStartExecutionMismatch");
  const auto &stage = execution.stages[0];
  const auto &step = movement->steps[0];
  const auto *binding =
      std::get_if<MarketOriginalBusinessFifo>(&step.source_binding);
  if (!binding || binding->business_fact_id != movement->business_fact_id ||
      binding->execution_stage_ordinal != 0 ||
      stage.stage != MarketCaptureStage::trade ||
      step.stage != MarketCaptureStage::trade ||
      stage.next_instance_before < 1 ||
      stage.next_instance_after < stage.next_instance_before ||
      stage.next_instance_after >
          *allocated(common, source.id, "market_history") ||
      step.next_instance_before != stage.next_instance_before ||
      step.next_instance_after != stage.next_instance_after ||
      stage.inventory_frames.size() != 2 || stage.item_operations.size() != 1 ||
      stage.wallet_postings.size() != 2 || step.inventories.size() != 1 ||
      step.inventories[0].inventory_id != shop->inventory_id ||
      stage.wallet_postings[0].wallet_id != view.payer_wallet_id ||
      stage.wallet_postings[1].wallet_id != view.payee_wallet_id ||
      stage.wallet_postings[0].delta_qian != -terms->total ||
      stage.wallet_postings[1].delta_qian != terms->total)
    return bad("GrowthScalarStartCaptureMismatch");
  if (now.half_day_tick < common.tick())
    return bad("GrowthScalarStartClockBeforeWorld");
  if (view.phase == LifeActivityPhase::active) {
    const auto *action = common.action(view.action_id);
    if (!action || action->kind != "trade" ||
        action->actor_id != view.actor_id ||
        action->start_tick != view.start_tick ||
        !growth.actions.find(view.action_id))
      return bad("GrowthScalarStartActiveActionMissing");
  } else {
    const bool completed = view.phase == LifeActivityPhase::completed;
    if (view.success != completed ||
        (completed ? !view.code.empty() : view.code.empty()) ||
        common.action(view.action_id) || growth.actions.find(view.action_id))
      return bad("GrowthScalarStartTerminalResultMismatch");
  }
  auto ordered = compare_world_moments(at->started_at, now, budget);
  if (!ordered.ok)
    return {false, {}, ordered.error};
  if (ordered.value > 0)
    return bad("GrowthScalarStartAfterBoundary");
  auto tail = growth_trade_terminal_codec::validate_activity(*activity, true);
  if (!tail.ok)
    return {false, {}, tail.error};
  if (activity->trade_terminal_time) {
    auto current = compare_world_moments(
        activity->trade_terminal_time->ended_at, now, budget);
    if (!current.ok)
      return {false, {}, current.error};
    if (current.value > 0)
      return bad("GrowthScalarStartTerminalAfterBoundary");
    auto relation = compare_world_moments(
        at->started_at, activity->trade_terminal_time->ended_at, budget);
    if (!relation.ok)
      return {false, {}, relation.error};
    if (relation.value > 0)
      return bad("GrowthScalarTerminalBeforeStart");
  }
  return {true, {movement, activity, &at->started_at, &head->id}, {}};
}
Outcome<GrowthScalarMarketStartView>
read_growth_scalar_market_start_for_action_v2(
    const CommonStore &common, const CommonCatalog &shared,
    const GrowthCatalog &catalog, const GrowthState &growth,
    const ActorFoundationCatalog &foundation,
    const GrowthMarketHistoryCatalog &history_catalog,
    const GrowthMarketHistoryState &history, const StableId &action,
    const WorldMoment &now, ExactArithmeticBudget budget) {
  const StableId *id = nullptr;
  for (const auto &row : history.movements.rows())
    if (row.second.business_fact_id == action) {
      if (id)
        return {
            false,
            {},
            common_failure("GrowthScalarStartSourceRepeated", action).error};
      id = &row.first;
    }
  if (!id)
    return {
        false,
        {},
        common_failure("GrowthScalarStartOriginalSourceMissing", action).error};


  auto memory = CheckpointMemoryCharge(0);
  const SourceRef source{"growth_market_history", "growth_market_movement",
                         *id};
  return read_growth_scalar_market_start_v2(common, shared, catalog, growth,
                                            foundation, history_catalog,
                                            history, source, 2, now, budget);
}
} // namespace hanmo::resident
