// 只证明指定交割前从未关联本域责任的窄分支；有历史关联时明确不可用，不猜旧欠额。
#pragma once
#include "private_account_dependencies.hpp"
#include "../estate/estate_model.hpp"
namespace hanmo::resident {
struct FiscalSettlementCatalog;
// 全部引用必须来自同一已验证World；空指针只代表该模块没有启用，不能隐藏已启用账本。
struct EconomicPrivateHistoryContext {
 const CommonStore& common;const CommonCatalog& common_catalog;
 const EstateState& estate;const EstateCatalog& estate_catalog;
 const LocalEconomyState* economy=nullptr;const LocalEconomyCatalog* economy_catalog=nullptr;
 const FiscalSettlementCatalog* fiscal_catalog=nullptr;const FiscalSettlementState* fiscal=nullptr;
 const OccupationFiscalSettlementState* occupation=nullptr;const PublicReliefState* relief=nullptr;
 const EstateAccess* estate_access=nullptr;
 const EstateEmploymentState* employment=nullptr;
 const PrivateShopState* shops=nullptr;
};
struct EconomicPrivateHistoricalClearance {WorldIdentity source_identity;StableId actor_id,before_fact_id;Integer tick=0;};
// 报价按当前真实事实尾界预检同一窄历史可证性，不分配ID、不伪造未来交割事实。
Outcome<EconomicPrivateHistoricalClearance> preview_economic_private_account_historical_clearance(
 const EconomicPrivateHistoryContext&,const EconomicPrivateAccountSelection&);
// before_fact只认family_inheritance: / family_inheritance_settled / family_inheritance。
// tick须由S4已验证的该交割载荷提供；本口不认证死亡、受益人、交割金额或其他域债务。
Outcome<EconomicPrivateHistoricalClearance> read_economic_private_account_historical_clearance(
 const EconomicPrivateHistoryContext&,const EconomicPrivateAccountSelection&,Integer tick,const StableId& before_fact_id);
}
