// 新账仅证明真实围城参与名册和操作来源；完整军史/产权/原siege当前结构仍由既有领域门先验。
#pragma once
#include "military_siege_lifecycle.hpp"
#include "military_siege_engineering_source.hpp"
namespace hanmo::resident {
// 两种观察上限严格区分；不以今日army.unit_ids或当前统帅反填旧加入。
Status validate_military_siege_participation_history(
 const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitarySiegeEngineeringPolicy&,
 const MilitarySiegeLifecycleState&,const MilitarySiegeEngineeringState&,
 const DynamicStepInput* phase=nullptr,const PopulationMilitaryState* population=nullptr,const PopulationMilitaryCatalog* population_catalog=nullptr);
// 必须在上面的完整参与链与原Military历史通过后调用；仅读真实工程头前的最后参与状态。
Outcome<MilitarySiegeParticipationObservation> read_military_siege_participation_for_engineering(
 const CommonStore&,const MilitarySiegeLifecycleState&,const MilitarySiegeEngineeringPreparation&,const DynamicStepInput* phase=nullptr);
}
