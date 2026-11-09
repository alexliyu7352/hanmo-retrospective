// 原pause/wait/resume的同捕获来源；这里的wait本身不推进钟，也不产生付款或终结。
#pragma once
#include "resident/domains/travel/travel_plan_capture.hpp"
#include "resident/model/common_model.hpp"
#include <memory>
namespace hanmo::resident {
class CommonStore;
// 只读原prepare已成功的三种无费控制，边界不允许调用者改换原命令或前态。
class PrivateShopTravelControlBoundary {
public:PrivateShopTravelControlBoundary()=default;
private:
 struct State;std::shared_ptr<const State> state_;
 friend Outcome<PrivateShopTravelControlBoundary> capture_private_shop_travel_control_boundary(const CommonStore&,const TravelState&,const PreparedTravelOp&);
 friend struct PrivateShopTravelControlAccess;
};
struct PrivateShopTravelNoFeeControlSource {WorldIdentity source_identity;TravelControlDraft actual_control;};
// 须在原prepare后、apply前调用；改道/返程另有计划来源，不作为这三个无费控制放行。
Outcome<PrivateShopTravelControlBoundary> capture_private_shop_travel_control_boundary(const CommonStore&,const TravelState&,const PreparedTravelOp&);
// 仅接受同捕获、同原主体/行动/载荷，全部资源、位置、钟及ID不变；保原可选paused字段序。
// 必须在外层TravelWait推进之前读。暂态结果仍须原coverage成功，不授永久在途或商业归属。
Outcome<PrivateShopTravelNoFeeControlSource> read_private_shop_travel_no_fee_control(const CommonStore&,const TravelState&,const PrivateShopTravelControlBoundary&,const TravelControlDraft&);
}
