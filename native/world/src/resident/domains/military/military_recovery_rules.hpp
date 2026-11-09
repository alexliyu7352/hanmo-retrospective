// 恢复阶段只写原伤员/可战分类和永久事实；钱粮已由前一原供给阶段处理。
#pragma once
#include "military_recovery_eligibility.hpp"
#include "military_recovery_actor_contract.hpp"
namespace hanmo::resident {
struct MilitaryRecoveryEffects {std::vector<StableId> fact_ids;};
// 必须位于整World未发布候选，原军事供给之后、围城和会战之前；晚ID失败由原候选整体丢弃。
Outcome<MilitaryRecoveryEffects> step_military_wounded_recovery(
 CommonStore&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,MilitaryRecoveryState&,
 const MilitaryRecoveryPolicy&,const MilitaryActorRecoveryRead*,const DynamicStepInput&,const PopulationMilitaryState* population=nullptr);
// 早门只核真实军史、供给、分类及额度；不得反入尚待人物历史总验的康复读。
Status validate_military_recovery_structure(
 const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const MilitaryRecoveryState&,const MilitaryRecoveryPolicy&,const PopulationMilitaryState* population=nullptr);
// 原人口消费只开放严格同phase结构验证，不能直接承担人物康复晚门。
Status validate_military_recovery_structure(
 const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const MilitaryRecoveryState&,const MilitaryRecoveryPolicy&,const DynamicStepInput&,const PopulationMilitaryState* population=nullptr);
// 必须在同候选Family/Growth康复原账通过后执行的晚门；不是可省略的bool模式。
Status validate_military_recovery_actor_sources(
 const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const MilitaryRecoveryState&,const MilitaryRecoveryPolicy&,const MilitaryActorRecoveryRead*,const PopulationMilitaryState* population=nullptr);
// 原军籍当前分类必须等于初态和永久事件投影，不能凭当前fit补造恢复效果。
Status validate_military_recovery_current_rosters(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,
 const PopulationMilitaryState* population=nullptr,const PopulationMilitaryCatalog* population_catalog=nullptr);
// 完整独立域门依次执行两项；World可用两个具名门安排无递归的同候选先后。
Status validate_military_wounded_recovery(
 const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const MilitaryRecoveryState&,
 const MilitaryRecoveryPolicy&,const MilitaryActorRecoveryRead*,
 const PopulationMilitaryState* population=nullptr,const PopulationMilitaryCatalog* population_catalog=nullptr);
Status validate_military_recovery_shape(const MilitaryRecoveryState&);
}
