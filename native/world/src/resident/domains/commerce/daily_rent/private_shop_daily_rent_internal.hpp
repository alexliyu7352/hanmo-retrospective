// 日租唯一写源内部入口；调用者负责热门准入与完整外层保存集，不在这里再次创建 Common 点。
#pragma once
#include "private_shop_daily_rent_business.hpp"
namespace hanmo::resident::private_shop_rent_detail {
Outcome<StableId> append(CommonStore&,const StableId& prefix,const StableId& kind,const StableId& domain);
Outcome<StableId> pay(CommonStore&,LocalEconomyState&,PrivateShopDailyRentState&,const PrivateShopRecord&,const PrivateShopRentAssessment&,const StableId& funding_kind,const StableId& actor,const StableId& payer,const StableId& mandate={},const StableId& notice={});
Outcome<StableId> assess(CommonStore&,LocalEconomyState&,PrivateShopState&,PrivateShopDailyRentState&,const StableId& shop, Integer period_index,const StableId& trigger,const StableId& resume={});
}
