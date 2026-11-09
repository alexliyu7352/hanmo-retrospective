#include "resident/domains/growth/growth_model.hpp"
// 原活动就是成本和转货来源；永久选择包含他人、公约采购及已取消半日的实际成交，不能只保私店本人。
#include "growth_market_trade_pins.hpp"
#include "growth_market_fifo_read.hpp"
#include "growth_market_history.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include <algorithm>
#include <span>
namespace hanmo::resident {
namespace {
template <class Sink>
Status visit_pins(const CommonStore &c, const GrowthState &s,
                  const GrowthCatalog &catalog,
                  std::span<const StableId> selected, Sink &&sink) {
  auto bad = [](const StableId &code, const StableId &id) {
    return common_failure(code, id);
  };
  const auto contains = [](const auto &values, std::string_view id) {
    return std::find(values.begin(), values.end(), id) != values.end();
  };
  if (selected.empty())
    return common_success();
  if (!catalog.regional_life || !catalog.economy)
    return bad("GrowthMarketHistoryProfileRequired", {});
  CheckpointVector<std::string_view> shops, inventories;
  shops.reserve(selected.size());
  inventories.reserve(selected.size());
  for (const auto &id : selected) {
    const auto *shop = catalog.shops.find(id);
    if (contains(shops, id) || !shop || shop->id != id ||
        contains(inventories, shop->inventory_id) ||
        !c.inventory(shop->inventory_id))
      return bad("GrowthMarketHistorySelectionInvalid", id);
    shops.push_back(id);
    inventories.push_back(shop->inventory_id);
  }
  CheckpointVector<std::string_view> needed;
  needed.reserve(s.life_activities.size());
  for (const auto &row : s.life_activities.rows()) {
    const auto &v = row.second.view;
    const auto *t = std::get_if<TradeTerms>(&row.second.terms);
    const bool touches = contains(inventories, v.from_inventory_id) ||
                         contains(inventories, v.to_inventory_id);
    if (!t) {
      if (touches)
        return bad("GrowthMarketHistorySourceInvalid", row.first);
      continue;
    }
    const auto *shop = catalog.shops.find(t->shop_id);
    if (!contains(shops, t->shop_id)) {
      if (touches)
        return bad("GrowthMarketHistorySourceInvalid", row.first);
      continue;
    }
    const auto *head = c.state().facts.find(row.first);
    const auto *actor = c.actor(v.actor_id);
    const bool buy = t->side == "buy";
    if (!shop || !head || head->id != row.first ||
        head->kind != "life_activity" || head->domain != "growth" || !actor ||
        v.fact_id != row.first || v.action_id != row.first ||
        v.kind != LifeActivityKind::trade ||
        (t->side != "buy" && t->side != "sell") || v.activity != t->side ||
        v.item_id != t->item_id || v.quantity != t->quantity ||
        v.amount != t->total || t->quantity < 1 || t->quantity > 500 ||
        t->total < 0 || v.start_tick != t->valid_tick ||
        v.settlement_tick != v.start_tick || v.start_tick < 0 ||
        v.start_tick > c.tick() ||
        (buy ? v.from_inventory_id : v.to_inventory_id) != shop->inventory_id ||
        (buy ? v.to_inventory_id : v.from_inventory_id) != actor->inventory_id)
      return bad("GrowthMarketHistorySourceInvalid", row.first);
    // 半日后来取消不撤回开工已经发生的真实转货与付款，原来源仍永久保留。
    if (v.phase != LifeActivityPhase::active &&
        v.phase != LifeActivityPhase::completed &&
        v.phase != LifeActivityPhase::cancelled)
      return bad("GrowthMarketHistorySourceInvalid", row.first);
    needed.push_back(row.first);
  }
  std::size_t visited = 0;
  for (const auto &id : c.state().facts.order())
    if (contains(needed, id)) {
      auto status = sink(id);
      if (!status.ok)
        return status;
      ++visited;
    }
  if (visited != needed.size()) {
    for (const auto id : needed)
      if (std::find(c.state().facts.order().begin(),
                    c.state().facts.order().end(),
                    id) == c.state().facts.order().end())
        return bad("GrowthMarketHistorySourceOrderInvalid", StableId(id));
    return bad("GrowthMarketHistorySourceOrderInvalid", {});
  }
  return common_success();
}
} // namespace
Outcome<std::vector<StableId>>
read_growth_market_trade_pins(const CommonStore &c, const GrowthState &s,
                              const GrowthCatalog &catalog,
                              const std::vector<StableId> &selected) {
  std::vector<StableId> result;
  auto status = visit_pins(c, s, catalog, selected, [&](const StableId &id) {
    result.push_back(id);
    return common_success();
  });
  if (!status.ok)
    return {false, {}, std::move(status.error)};
  return {true, std::move(result), {}};
}
Status visit_growth_market_trade_pins(const CommonStore &c,
                                      const GrowthState &s,
                                      const GrowthCatalog &catalog,
                                      const GrowthMarketHistoryCatalog &history,
                                      MarketOriginalFifoSourceSink &sink) {
  if (history.selected_version != 2)
    return common_failure("GrowthMarketHistory2Required");
  return visit_pins(c, s, catalog, history.shop_ids, [&](const StableId &id) {
    return sink.visit_original_fifo_source(MarketCauseKind::growth_trade, id,
                                           1);
  });
}
} // namespace hanmo::resident
