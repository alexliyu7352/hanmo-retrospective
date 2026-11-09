// 原军战/航损终止的薄投影；只复用所属域已存在的稳定钟读，不添加第二份战史或行动账。
#pragma once
#include "private_shop_travel_expense_contract.hpp"
namespace hanmo::resident {
class CommonStore;struct MilitaryState;struct MilitaryCatalog;struct NavalState;
// action/actor必须来自原释放前快照；source必须来自实际caller，不能按最新头搜索。
struct PrivateShopTravelMilitaryEndRequest {
    PrivateShopTravelExpenseEndKind kind=PrivateShopTravelExpenseEndKind::military_casualties;
    StableId source_fact_id,action_id,actor_id;
};
struct PrivateShopTravelMilitaryEndSource {
    WorldIdentity source_identity;
    PrivateShopTravelMilitaryEndRequest request;
    StableId battle_id,casualty_fact_id,original_reason;
    Integer tick=0;
};
// 必须先完整核原Military/Naval/Navigation历史；本口不回调Shop或重跑全World。
// 原payload/head完成且Common时钟提交后才调用，preclock pending ID不能冒已登记源。
// Naval读沿原更早navigation去重。缺域、缺真实来源或非五类军事kind均返回error。
Outcome<PrivateShopTravelMilitaryEndSource> read_private_shop_travel_military_end(
    const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const NavalState*,
    const PrivateShopTravelMilitaryEndRequest&);
}
