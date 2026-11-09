// 交易无状态复用原Growth的货腿和双钱包recipe；不持余额、报价、订单或行动。
#pragma once
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
struct VerifiedTradeTerms {
    StableId from_inventory_id,to_inventory_id,item_id,from_wallet_id,to_wallet_id;
    Owner seller,buyer;Integer quantity=0,total=0;std::vector<CargoSelection> selected_cargo;
};
struct VerifiedTradeLot {StableId original_lot_id,destination_lot_id;Integer quantity=0;};
struct VerifiedTradeExecution {std::vector<VerifiedTradeLot> lots;};
// prepare只读，不分配ID、不预留；调用域仍须核真实接触、用途与冻结价格。
Outcome<ResourceRecipe> prepare_verified_trade_recipe(const CommonStore&,const VerifiedTradeTerms&);
// 同候选只调用Common原recipe；返回实写后的原批次→新批次，不另执行交易算法。
Outcome<VerifiedTradeExecution> execute_verified_trade_recipe_in_candidate(CommonStore&,const VerifiedTradeTerms&);
}
