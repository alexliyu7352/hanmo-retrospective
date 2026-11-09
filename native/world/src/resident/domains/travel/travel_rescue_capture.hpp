// 原救助请求替换原Travel行动时的唯一同栈快照；它不表示已经返城或已发生货损。
#pragma once
#include "travel_model.hpp"
#include "../../model/common_operations.hpp"
namespace hanmo::resident {
// 原writer在改kind/删载荷前拥有值，成功后交回；无新ID、付款或永久事实。
struct TravelRescueReplacementDraft {
    ActionCoreView original_action,replacement_action;
    Integer source_tick=0;
    Location original_location;
    std::variant<TerrainTravelParams,RoadTravelParams> original_travel;
    RescueParams rescue;
};
// 原完成writer的成功值快照，保个人每批损失和未受影响产权；不表示外层已提交。
struct TravelRescueCompletionDraft {
    ActionCoreView action;
    Integer source_tick=0;
    RescueParams rescue;
    Location before_location,after_location;
    InventoryView before_inventory,after_inventory;
    AdjustLots applied_adjustment;
};
}
