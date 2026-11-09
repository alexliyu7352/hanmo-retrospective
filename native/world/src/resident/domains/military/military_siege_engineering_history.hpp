// 工程准备与真实半日执行一一对应；早结构/军籍门和晚完整人物XP门明确分离。
#pragma once
#include "military_siege_lifecycle_history.hpp"
namespace hanmo::resident {
// 只在原siege_step候选内写一次准备，失败由原整个World候选恢复。
Outcome<MilitarySiegeEngineeringPreparation> prepare_military_siege_engineering(
 CommonStore&,const MilitaryState&,const MilitaryCatalog&,const SiegeRecord&,
 MilitarySiegeEngineeringState&,const MilitarySiegeEngineeringSkillRead&,const DynamicStepInput&);
// 含反向Common头、每围城前驱链、恰一次真正advanced消费和原参与/名册源。
Status validate_military_siege_engineering_history(
 const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitarySiegeEngineeringPolicy&,
 const MilitarySiegeEngineeringState&,const MilitarySiegeLifecycleState&,
 const DynamicStepInput* phase=nullptr,const PopulationMilitaryState* population=nullptr,const PopulationMilitaryCatalog* population_catalog=nullptr);
// 此门只能在同候选完整Family/Birth/Growth先验之后调用，不能用可选bool绕过。
Status validate_military_siege_engineering_history_actor_sources(
 const CommonStore&,const MilitarySiegeEngineeringState&,const MilitarySiegeEngineeringSkillRead&);
}
