#pragma once
#include "production_disruptions.hpp"
#include "../views/production_history.hpp"
namespace hanmo::resident {
// 仅在最终身份边界派生；查询不另扫全世界，也不新增游标序列。
void index_production_disruptions(const CommonStore&,const ProductionDisruptionCatalog&,const ProductionDisruptionState&,LocalEconomyIndex&);
// 页面须已在同候选通过原经济或历史权限门；仅返回原因、时窗、系数和物品数量。
Outcome<BoundaryValue> encode_economy_with_production_disruptions(const CommonStore&,const ProductionDisruptionCatalog&,const ProductionDisruptionState&,const LocalEconomyIndex&,const LocalEconomyPage&);
Outcome<BoundaryValue> encode_history_with_production_disruptions(const CommonStore&,const ProductionDisruptionCatalog&,const ProductionDisruptionState&,const LocalEconomyIndex&,const ProductionHistoryPage&);
}
