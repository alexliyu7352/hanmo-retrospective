// 粮食和生产只关联原实物收据；不另存库存、投入或产出数量。
#pragma once
#include "execution_receipts.hpp"
namespace hanmo::resident {
std::vector<EconomicResponsibilityRef> economic_production_responsibilities(const LocalEconomyCatalog&,const StableId& unit);
Status validate_economic_operating_permissions(const EconomicTakeoverReadContext&,
    const EconomicOperatingReceiptSource&,const std::vector<EconomicResponsibilityRef>&,Integer tick,bool performed);
// 本域原结算已经产生真实收据后调用；同候选失败必须一并回滚原资产操作。
Status record_economic_taken_ration_source(const EconomicTakeoverReadContext&,
    EconomicTakeoverExecutionState&,const EconomicTakeoverExecutionPlan&,const StableId& receipt_id);
Status record_economic_taken_production_source(const EconomicTakeoverReadContext&,
    EconomicTakeoverExecutionState&,const std::vector<EconomicTakeoverExecutionPlan>&,
    const StableId& receipt_id);
// 赈济只关联已实际发给原家庭的整份收据；任务工作与原受领条款仍归原域。
Status record_economic_taken_relief_source(const EconomicTakeoverReadContext&,
    EconomicTakeoverExecutionState&,const EconomicTakeoverExecutionPlan&,const StableId& receipt_id);
Status validate_economic_taken_relief_sources(const EconomicTakeoverReadContext&,const EconomicTakeoverExecutionState&);
// 执行fact之前真实登记决定是否必需关联；删除整个关联也不能变成旧无接管收据。
Status validate_economic_taken_material_sources(const EconomicTakeoverReadContext&,
    const EconomicTakeoverExecutionState&);
}
