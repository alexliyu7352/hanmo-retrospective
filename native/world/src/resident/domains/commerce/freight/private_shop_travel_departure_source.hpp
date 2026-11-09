// 原首次起行的同捕获安装匹配；不授携运委托、历史货权或费用资本化。
#pragma once
#include "resident/domains/travel/travel_plan_capture.hpp"
#include "resident/model/common_model.hpp"
#include <memory>
namespace hanmo::resident {
class CommonStore;struct TravelCatalog;
// 只由原Action已安装、Travel尚无载荷的真实前界构造；不提供可修改/序列化字段。
class PrivateShopTravelDepartureBoundary {
public: PrivateShopTravelDepartureBoundary()=default;
private:
 struct State;std::shared_ptr<const State> state_;
 friend Outcome<PrivateShopTravelDepartureBoundary> capture_private_shop_travel_departure_boundary(const CommonStore&,const TravelState&,const TravelCatalog&,const StableId&,const TravelQuote&,const TravelCommand*,const std::vector<StableId>&);
 friend struct PrivateShopTravelDepartureAccess;
};
// 所选批次取原行囊全值，安装值取真实begin；此值只在本次原coverage成功后可采用。
struct PrivateShopTravelDepartureSource {
 WorldIdentity source_identity;
 TravelPlanInstallationDraft actual_installation;
 StableId personal_wallet_id,personal_inventory_id;
 std::vector<LotView> original_selected_lots;
};
// 调用者先完成原Travel报价和Action占用。本口不替报价或作者分摊规则；首片仅本人付款/货批。
Outcome<PrivateShopTravelDepartureBoundary> capture_private_shop_travel_departure_boundary(
 const CommonStore&,const TravelState&,const TravelCatalog&,const StableId& action_id,
 const TravelQuote&,const TravelCommand* original_command,const std::vector<StableId>& original_lot_ids);
// 原begin成功后立即读，须仍同capture。只准原计划头分配和位置安装，钱货/批序不可夹改。
// 无永久事实来源、无party费用分摊；货批被指定不证明其原采购或单店独运归属。
Outcome<PrivateShopTravelDepartureSource> read_private_shop_travel_departure(
 const CommonStore&,const TravelState&,const TravelCatalog&,
 const PrivateShopTravelDepartureBoundary&,const TravelPlanInstallationDraft&);
}
