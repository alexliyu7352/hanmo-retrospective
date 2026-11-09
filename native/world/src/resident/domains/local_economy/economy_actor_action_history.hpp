// 经济原 Action 的 source-local 身份/应用历史；不重入完整身体或 Knowledge。
#pragma once
#include "resident/kernel/action_time_actor_history.hpp"
#include "resident/kernel/action_time_validation.hpp"
namespace hanmo::resident {
struct CraftJobReadContext;
struct RepairReadContext;
struct HouseholdPurchaseContext;
struct PrivateShopOperationSourceContext;
struct FacilityConstructionServiceContext;
// 只借原不可变参与者及实际取得/释放点；完整业务准入仍归各原 validator。
Status visit_craft_actor_action_history(const CraftJobReadContext &,
                                        const ActionTimeRegistration &,
                                        const StableId &,
                                        ActionTimeActorHistorySink &);
Status visit_repair_actor_action_history(const RepairReadContext &,
                                         const ActionTimeRegistration &,
                                         const StableId &,
                                         ActionTimeActorHistorySink &);
Status visit_household_actor_action_history(const HouseholdPurchaseContext &,
                                            const ActionTimeRegistration &,
                                            const StableId &,
                                            ActionTimeActorHistorySink &);
Status visit_private_shop_actor_action_history(
    const PrivateShopOperationSourceContext &, const ActionTimeRegistration &,
    const StableId &, ActionTimeActorHistorySink &);
Status visit_construction_actor_action_history(
    const FacilityConstructionServiceContext &, const ActionTimeRegistration &,
    const StableId &, ActionTimeActorHistorySink &);
} // namespace hanmo::resident
