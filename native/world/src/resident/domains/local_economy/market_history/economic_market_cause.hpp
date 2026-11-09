// 只给受保护专用市场的原经济业务量；不凭今日库存、产权或市场回执反推历史。
#pragma once
#include "resident/domains/growth/growth_market_history_contract.hpp"
#include "resident/domains/local_economy/local_economy_model.hpp"
#include <span>
namespace hanmo::resident {
class MarketOriginalFifoSourceSink;
struct EconomicTakeoverExecutionState;
// World先全验原LE及接管史；本口只验精确本账头/不可变条款，不反调市场历史或政治全量。
// 新profile永久禁止所选market改Owner；已接管业务首片明确拒绝，不能用原Owner冒今日权源。
Outcome<MarketBusinessCause> read_economic_market_cause(
    const CommonStore &, const CommonCatalog &, const LocalEconomyState &,
    const LocalEconomyCatalog &, const StableId &exact_fact_id,
    const EconomicTakeoverExecutionState *takeover = nullptr,
    const DynamicStepInput *phase = nullptr);
// 反向枚举本账触及专用库存的全部非零原因，供冷重放拒绝删掉回执后自洽的余额。
Outcome<std::vector<StableId>> read_economic_market_cause_ids(
    const CommonStore &, const CommonCatalog &, const LocalEconomyState &,
    const LocalEconomyCatalog &,
    const std::vector<StableId> &protected_inventory_ids,
    const EconomicTakeoverExecutionState *takeover = nullptr,
    const DynamicStepInput *phase = nullptr);
struct EconomicMarketCauseBorrow {
  MarketCauseKind kind = MarketCauseKind::local_production;
  Integer source_tick = 0, business_due_tick = 0;
  const StableId *original_business_key = nullptr;
};
// 原来源身份及全部腿校验不变，只借原收据/目录字段供纯核使用。
Outcome<EconomicMarketCauseBorrow> read_economic_market_cause_borrow(
    const CommonStore &, const CommonCatalog &, const LocalEconomyState &,
    const LocalEconomyCatalog &, const StableId &exact_fact_id,
    const EconomicTakeoverExecutionState *takeover = nullptr,
    const DynamicStepInput *phase = nullptr);
// 同一原业务校验正文的借用反枚举，不复制来源IDs、Owner或整笔钱货腿。
Status visit_economic_market_cause_sources(
    const CommonStore &, const CommonCatalog &, const LocalEconomyState &,
    const LocalEconomyCatalog &,
    std::span<const InventoryView *const> protected_initial,
    MarketOriginalFifoSourceSink &,
    const EconomicTakeoverExecutionState *takeover = nullptr,
    const DynamicStepInput *phase = nullptr);

} // namespace hanmo::resident
