// 本人伤势和实际已提供恢复点的有限页；不查询远处人物身体或推定未发生的疗效。
#pragma once
#include "growth_recovery_history.hpp"
namespace hanmo::resident {
Outcome<BoundaryValue> growth_recovery_view(const CommonStore&,const GrowthState&,const GrowthRecoveryCatalog&,const GrowthRecoveryState&,const GrowthRecoveryHistoryAccess&,const StableId& observer,Integer page);
}
