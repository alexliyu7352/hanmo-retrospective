#include "resident/domains/growth/growth_model.hpp"
// 原正常服务完工的条件来源与保留口；此DTO不证明完成时伤势上限。
#pragma once
#include "growth_personal_condition_contract.hpp"
#include "resident/model/dynamic_contract.hpp"
namespace hanmo::resident {
struct GrowthServiceConditionSource {
    GrowthPersonalConditionCause cause;
    StableId activity;
    ServiceTerms original_terms;
};
// 前置完整原Growth/生活结果。取消、活动中、贸易均不产生服务HP/疲劳完成写。
// 完整HP复核还必须读该实际写入点的伤势，以及原启用的恢复政策，不能取今日injury。
Outcome<GrowthServiceConditionSource> read_growth_service_condition_source(
    const CommonState&,const GrowthState&,const GrowthCatalog&,const StableId& actual_life_activity,
    const DynamicStepInput* actual_phase=nullptr);
// 仅新条件profile的未来World绑定可消费：全部原服务载荷必须保留，包括已零变化/旧取消者。
// 空旧profile仍原16条策略；此函数本身不修改prune或启用profile。
Outcome<std::vector<StableId>> read_growth_condition_service_pins(
    const CommonState&,const GrowthState&,const GrowthCatalog&);
}
