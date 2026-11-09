// 原经济阶段使用真实政治读与独立执行关联；不增阶段或复制钱粮状态。
#pragma once
#include "execution_materials.hpp"
namespace hanmo::resident {
struct EconomicTakeoverPhaseContext {
    EconomicTakeoverReadContext read;
    EconomicTakeoverExecutionState& state;
};
// 发生额只核原责任来源；付出资产另核当前逐责任裁定。
Status validate_taken_obligation_accrual(const EconomicTakeoverReadContext&,
    const RecurringObligation&,Integer amount,Integer due_tick);
}
