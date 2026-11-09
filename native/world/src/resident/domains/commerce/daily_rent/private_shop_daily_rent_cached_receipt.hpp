// 完整日租/原 LE 先验后核缓存；永久绑定不随 Common history/results 裁剪而丢失。
#pragma once
#include "private_shop_daily_rent.hpp"
#include <functional>
namespace hanmo::resident {
class ProjectionWriter;
bool private_shop_daily_rent_command_kind(const StableId&);
bool private_shop_daily_rent_fact_id(const StableId&);
// 原记录逐字段保持，旧手动来源必须已绑定，只返回本次新增的自愿偿租或本人复业。
Outcome<std::vector<StableId>> private_shop_daily_rent_new_command_sources(const PrivateShopDailyRentState& before,const PrivateShopDailyRentState& after);
// 真正 Common 缓存及 history 已分配后调用，actual_result 必须就是缓存对象。
Status bind_private_shop_daily_rent_result_in_candidate(CommonStore&,PrivateShopDailyRentState&,const std::vector<StableId>& exact_new_sources,const StableId& actor,Integer request_seq,const CommitResult& actual_result);
Status validate_private_shop_daily_rent_cached_results(const CommonStore&,const PrivateShopDailyRentState*,ProjectionWriter* projection=nullptr);
// 完整 Family/Legacy 控制史之后逐真实付款/复业事实前界复验受控本人；自动结租不消费本人许可。
using PrivateShopRentHistoricalPlayerRead=std::function<Outcome<StableId>(Integer,const StableId&)>;
Status validate_private_shop_daily_rent_player_history(const CommonStore&,const PrivateShopDailyRentState*,const PrivateShopRentHistoricalPlayerRead&,ProjectionWriter* projection=nullptr);
}
