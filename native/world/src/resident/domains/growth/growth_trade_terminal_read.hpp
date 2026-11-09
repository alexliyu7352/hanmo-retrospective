#include "resident/kernel/source_ref.hpp"
#include "hanmo/resident/world_moment.hpp"
// 原标量交易的只读终局源；借用期不超过同一候选，不借中央累计补造时刻。
#pragma once
#include "resident/domains/growth/growth_refs.hpp"
namespace hanmo::resident {
// 两个指针均指同一原LifeActivity拥有值，不持有第二份终局或资源历史。
struct GrowthTradeTerminalView {
  const LifeActivityRecord* activity=nullptr;
  const TradeTerminalTime* terminal=nullptr;
};
// 只接受growth/life_activity原地址及schema2；活动中、缺尾或残留占用明确失败。
// 实际开始源与唯一累计由上层组合另核，不能在本reader反调ActionElapsed。
Outcome<GrowthTradeTerminalView> read_growth_trade_terminal_v2(
    const CommonStore&,const GrowthState&,const GrowthCatalog&,const CommonCatalog&,
    const SourceRef&,Integer source_schema,const WorldMoment& actual_now,
    ExactArithmeticBudget budget={});
} // namespace hanmo::resident
