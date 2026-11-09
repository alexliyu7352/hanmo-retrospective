// 原供给阶段内的休整入口；安全/人手/训练资格均读实际供给事实前军籍。
#pragma once
#include "military_recovery_eligibility.hpp"
#include "military_conditions.hpp"
namespace hanmo::resident {
struct MilitaryGarrisonRestEligibility {
 MilitaryRecoveryEligibility supplied;
 bool qualified=false;
 StableId pause_reason;
};
Status validate_military_garrison_rest_policy(const MilitaryGarrisonRestPolicy&,const MilitaryCatalog&);
// stable只读已提交半日；phase仅原from→to，旧半日仍以各自供给头为边界。
Outcome<MilitaryGarrisonRestEligibility> query_military_garrison_rest_period(
 const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,
 const StableId& unit_id,const StableId& supply_fact_id,const DynamicStepInput* phase=nullptr,
 const PopulationMilitaryState* population=nullptr);
Status validate_military_garrison_rest_shape(const MilitaryGarrisonRestState&);
// 供给/逃散/伤员恢复之后、actions_step之前，同一整World候选；晚失败由外层恢复全候选。
Outcome<std::vector<StableId>> step_military_garrison_rest(
 CommonStore&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const DynamicStepInput&,
 const PopulationMilitaryState* population=nullptr);
// 冷核逐源并反向枚举应得合格半日；删去整条头/payload仍不能抹掉历史减疲劳。
Status validate_military_garrison_rest_history(
 const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,
 const DynamicStepInput* phase=nullptr,const PopulationMilitaryState* population=nullptr);
}
