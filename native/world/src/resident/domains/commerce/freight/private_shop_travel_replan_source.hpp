// 原改道/返程的同捕获计划来源；安装零距据点位置不等于原Action已终结或到店。
#pragma once
#include "resident/domains/travel/travel_plan_capture.hpp"
#include "resident/model/common_model.hpp"
#include <memory>
namespace hanmo::resident {
class CommonStore;struct TravelCatalog;
class PrivateShopTravelReplanBoundary {
public:PrivateShopTravelReplanBoundary()=default;
private:
 struct State;std::shared_ptr<const State> state_;
 friend Outcome<PrivateShopTravelReplanBoundary> capture_private_shop_travel_replan_boundary(const CommonStore&,const TravelState&,const TravelCatalog&,const PreparedTravelOp&);
 friend struct PrivateShopTravelReplanAccess;
};
struct PrivateShopTravelReplanSource {WorldIdentity source_identity;TravelControlDraft actual_replan;};
// 原prepare已成功、原Travel仍活动时取值；只接具名Reroute/Return和其真正原报价。
Outcome<PrivateShopTravelReplanBoundary> capture_private_shop_travel_replan_boundary(const CommonStore&,const TravelState&,const TravelCatalog&,const PreparedTravelOp&);
// 同候选核原payer/钱货不变、旧原计划保留、新实际路径与唯一地形头；没有退款或预付。
// 原Action释放、在途范围以及商业资格均不在本口；返回仍待原coverage成功。
Outcome<PrivateShopTravelReplanSource> read_private_shop_travel_replan(const CommonStore&,const TravelState&,const TravelCatalog&,const PrivateShopTravelReplanBoundary&,const TravelControlDraft&);
}
