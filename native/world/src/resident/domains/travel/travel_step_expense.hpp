// 原成功Travel step的同栈观察值；不是永久实付证明，也不授权运输成本或第二次资源操作。
#pragma once
#include "travel_model.hpp"
#include "../../model/common_operations.hpp"
namespace hanmo::resident {
// 每行恰来自原step的一次fatigue字段写，HP仅观察前后，绝不表示该步写了HP。
// 成员順序取实际common.members，零增量/已达100仍保原同值写；不是永久成员或旅程证明。
struct TravelStepFatigueWrite {
    StableId actor_id;
    Integer hp_before=0,hp_after=0,fatigue_before=0,fatigue_after=0,fatigue_per_tick=0;
};
// 原writer只在全部step写入成功后交付；失败清空调用者槽，外层仍承担整个候选回滚。
// applied_payment来自实际成功recipe中的WalletDelta；免费收尾为空，零价移动保delta=0。
struct TravelStepExpenseDraft {
    ActionCoreView action;
    Integer source_tick=0;
    Location before_location,after_location;
    std::variant<TerrainTravelParams,RoadTravelParams> before_params,after_params;
    std::optional<WalletDelta> applied_payment;
    StableId charged_road_id;
    bool completed=false,free_endpoint_finalization=false;
    // 免费端点没有条件腿；付款为零的真实移动仍有逐成员原写。
    std::vector<TravelStepFatigueWrite> fatigue_writes;
};
}
