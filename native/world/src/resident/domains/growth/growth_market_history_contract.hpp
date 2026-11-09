// 选定市场历史的最窄原业务原因读；不储存钱包余额、库存副本或再造可花资产。
#pragma once
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
// 首片只有原成交、宏观生产与实际民耗；其余资源写者没有隐式通行类别。
enum class MarketCauseKind { growth_trade=0,local_production=1,local_ration=2 };
// 原业务的真实有序腿，数量带符号且非零；同库投入和产出不可净额合并掩盖先耗后产。
struct MarketQuantityLeg { StableId inventory_id,item_id;Owner owner;Integer signed_quantity=0; };
struct MarketBusinessCause {
    WorldIdentity source_identity;
    StableId source_fact_id;
    MarketCauseKind kind=MarketCauseKind::growth_trade;
    Integer source_tick=0;
    std::vector<MarketQuantityLeg> legs;
    // 热scope精确绑定原业务身份；生产/口粮分别为unit/obligation与原due_tick。
    StableId original_business_key;
    Integer business_due_tick=0;
};
// 读取精确永久业务ID，先验该原业务完整历史；Common当前identity是读戳，不固化为旧revision。
// 返回legs只能来自原TradeTerms/ProductionReceipt/RationReceipt，绝不由今日库存或市场回执反推。
// 热写permit另由中央Common保护原语持有，持此只读DTO不能修改受保护库存。
class MarketBusinessCauseRead {
public:
    virtual ~MarketBusinessCauseRead()=default;
    virtual Outcome<MarketBusinessCause> market_business_cause(MarketCauseKind,const StableId& exact_source_fact_id)const=0;
};
}
