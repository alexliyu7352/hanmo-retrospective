// 只读指挥来源给政治签发限定唯一目标；不因死亡、继承或同营关系生成调任许可。
#pragma once
#include "military_officer_contract.hpp"
#include "resident/domains/military/military_refs.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/model/dynamic_contract.hpp"
namespace hanmo::resident {
// 调用方先完成原Military历史验证；此处仍逐项核原作者/真实编军头及当前指挥者一致。
Outcome<MilitaryUnitCommandObservation> query_military_unit_command_source(
    const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const StableId& unit_id);
// 仅返回真实Common事实之前的指挥绑定，不用当前营地/现职填补过去位置。
Outcome<MilitaryUnitCommandBinding> query_military_unit_command_source_before(
    const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const StableId& unit_id,const StableId& before_fact_id);
// 只允许from等于当前、to恰下一半日；旧稳定读口不接受未来事实。
Outcome<MilitaryUnitCommandObservation> query_military_unit_command_source(
    const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const StableId& unit_id,const DynamicStepInput&);
Outcome<MilitaryUnitCommandBinding> query_military_unit_command_source_before(
    const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const StableId& unit_id,const StableId& before_fact_id,const DynamicStepInput&);
}
