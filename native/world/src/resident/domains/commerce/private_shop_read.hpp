// 本人有界商铺读：只能列自己的合同，收入来自实际完成/中断账，不让前端扫描别人的经营史。
#pragma once
#include "private_shop.hpp"
namespace hanmo::resident {
// 完整Shop钱货/批次历史先验后的派生明细；不是库存或资产凭证，不写第二份成本余额。
struct PrivateShopPurchaseCostView {Integer sold_quantity=0,recognized_cost=0,unrecognized_quantity=0,unrecognized_cost=0;};
Outcome<PrivateShopPurchaseCostView> derive_private_shop_purchase_cost_view(const PrivateShopState&,const PrivateShopPurchaseFact&);
// 仅供完整商铺验证后的知情投影；私人所有权不等于实际观察来源。
Outcome<bool> private_shop_observer_is_local(const CommonStore&,const StableId& actor,const StableId& place);
bool private_shop_history_is_firsthand(const PrivateShopState&,const StableId& actor,const StableId& fact);
class PrivateShopAccess final:public PrivateShopRead {
public:
    explicit PrivateShopAccess(const PrivateShopContext& context):context_(context){}
    Outcome<std::vector<StableId>> private_shop_ids(const StableId& observer,Integer offset,Integer limit)const override;
    Outcome<std::optional<PrivateShopSource>> private_shop_source(const StableId& observer,const StableId& shop_id)const override;
private:const PrivateShopContext& context_;
};
// 仅本人页读取Common实有余额/存量；供跨域准入的Source不携带整个状态或可写对象。
struct PrivateShopStockView {StableId item_id;Integer quantity=0;};
struct PrivateShopPageRow {PrivateShopSource source;Integer cash=0,spendable=0,inventory_capacity=0,inventory_weight=0;std::vector<PrivateShopStockView> stock;StableId active_action_id;Integer remaining_ticks=0;};
struct PrivateShopPage {WorldIdentity source_identity;StableId observer_actor_id;std::vector<PrivateShopPageRow> shops;Integer next_offset=0;bool has_more=false;};
Outcome<PrivateShopPage> query_private_shops(const PrivateShopContext&,const StableId& observer,Integer offset=0,Integer limit=16);
}
