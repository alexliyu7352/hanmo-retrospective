// 原施工选批与付款主体的纯读准入，供 preview/实际 writer 共用，不保许可token。
#pragma once
#include "facility_construction_adoption.hpp"
#include <span>
namespace hanmo::resident {
struct FacilityConstructionCostSelectionRead {
  const ActorCoreView *actor = nullptr;
  const WalletView *payer = nullptr;
  std::size_t inventory_count = 0;
};
// 原私人物批的逐项检查；返回同 Common 借值，任何写入后必须重读。
Outcome<const LotView *> read_facility_construction_selected_material(
    const CommonStore &, const CargoSelection &, const StableId &actual_actor);
// 同锁真实条款先由调用方验证；本口不分配ID、不预约、不扣钱货、不写结果。
Outcome<FacilityConstructionCostSelectionRead>
read_facility_construction_cost_selection(const CommonStore &,
                                          const ConstructionTerms &,
                                          const StableId &actual_actor,
                                          std::span<const CargoSelection>,
                                          const StableId &error_context);
} // namespace hanmo::resident
