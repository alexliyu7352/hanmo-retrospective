#include "resident/domains/growth/growth_fixed_service_model.hpp"
// FSL 输入变化与唯一原源闭包；ActionTime
// 只核引用/时序结构，本口必须在安装/冷读/稳定片同次核验。
#pragma once
#include "resident/domains/growth/growth_refs.hpp"
#include "resident/domains/actor_foundation/character_capability.hpp"
namespace hanmo::resident {
// 不产生成功缓存或外部 skip flag。先由原身体 validator 核全部真实 cause，再核
// FSL 引用和反向覆盖。 原有 source 没有完整 reader 的有限支仍拒绝；新增
// producer 不靠纯 tag 准入。
Status
validate_growth_fixed_condition_input_closure(const CharacterConditionSources &,
                                              const GrowthState &);
// 用于 ActionTime 的有限结构层；不等价于上面的实际来源闭包。
Status
validate_growth_fixed_input_change_reference(const LifeFixedInputChange &);
// Bounded actual source closure for one already-authored boundary, not a flag.
Status
validate_growth_fixed_input_change_source(const CharacterConditionSources &,
                                          const GrowthState &,
                                          const StableId &actual_boundary_id);
} // namespace hanmo::resident
namespace hanmo::resident {
// 完整安装/cold
// restore/稳定片出口必须统一调用本门；本栈先结构后实际源，禁止只核局部Condition。
struct GrowthMarketExactSources;
Status validate_complete_growth_fixed_sources(
    const CharacterConditionSources &, const GrowthState &,
    const GrowthMarketExactSources *market_exact = nullptr);
} // namespace hanmo::resident
