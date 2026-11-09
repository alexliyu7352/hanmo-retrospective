// 清算原事实完整验后，只核原命令缓存；原经营与此新段按精确命令/来源分流。
#pragma once
#include "private_shop_liquidation.hpp"
namespace hanmo::resident {
bool private_shop_liquidation_command_kind(const StableId&);
bool private_shop_liquidation_fact_id(const StableId&);
// 真正提交前后差分只取本次新增主源，先前每项永久记录必须原样且已绑定。
Outcome<std::vector<StableId>> private_shop_liquidation_new_command_sources(const PrivateShopLiquidationState& before,const PrivateShopLiquidationState& after);
// 实际Common结果入缓存/history后才调用；不另开Common保存点，不追认旧未绑定源。
Status bind_private_shop_liquidation_result_in_candidate(CommonStore&,PrivateShopLiquidationState&,const std::vector<StableId>& exact_new_sources,const StableId& actor,Integer request_seq,const CommitResult& actual_result);
Status validate_private_shop_liquidation_cached_results(const CommonStore&,const PrivateShopLiquidationState*);
}
