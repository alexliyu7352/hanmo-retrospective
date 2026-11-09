// 显式新模块与其唯一检查点段配对；旧情景的Common回执不能夹带新裁定。
#pragma once
#include "politics_takeover_state.hpp"
#include "hanmo/resident/command_contract.hpp"
namespace hanmo::resident {
// 三个独立检查点段同启用；其中任一段缺失都不能把真实钱款履责退化为旧域。
inline Status validate_political_takeover_layout(bool enabled,const std::vector<std::pair<StableId,StableId>>& modules,const PoliticalTakeoverState* state,std::optional<Integer> execution_version={},std::optional<Integer> task_execution_version={},Integer expected_task_execution_version=1,Integer expected_takeover_version=1){
    if(expected_takeover_version!=1&&expected_takeover_version!=2)return common_failure("UnsupportedPoliticalTakeoverVersion");
    if(expected_task_execution_version!=1&&expected_task_execution_version!=2)return common_failure("UnsupportedPoliticalTakeoverVersion");
    unsigned count=0,execution_count=0,task_count=0;
    for(const auto& module:modules)if(module.first=="political_takeover"||module.first=="economic_takeover_execution"||module.first=="task_takeover_execution"){
        if(module.second!=(module.first=="task_takeover_execution"?std::to_string(expected_task_execution_version):(module.first=="political_takeover"?std::to_string(expected_takeover_version):StableId{"1"})))return common_failure("UnsupportedPoliticalTakeoverVersion");
        auto& found=module.first=="political_takeover"?count:(module.first=="economic_takeover_execution"?execution_count:task_count);
        if(++found!=1)return common_failure("DuplicatePoliticalTakeoverModule");
    }
    if(!enabled)return count||execution_count||task_count||state||execution_version||task_execution_version?common_failure("UnexpectedPoliticalTakeoverProfile"):common_success();
    if(count!=1||execution_count!=1||task_count!=1||!state||!execution_version||!task_execution_version)return common_failure("MissingPoliticalTakeoverState");
    return state->version==expected_takeover_version&&*execution_version==1&&*task_execution_version==expected_task_execution_version?common_success():common_failure("UnsupportedPoliticalTakeoverVersion");
}
inline Status validate_political_takeover_command_history(const CommonStore& common,bool enabled){
    if(enabled)return common_success();
    for(const auto& row:common.state().results.rows())if(row.second&&row.second->kind==political_takeover_command_kind)return common_failure("PoliticalTakeoverProfileRequired","results/"+row.first);
    for(const auto& row:common.state().history)if(row.kind==political_takeover_command_kind)return common_failure("PoliticalTakeoverProfileRequired","history/"+row.result_id);
    for(const auto& row:common.state().facts.rows())if(row.second.kind==political_takeover_pending_fact_kind||row.second.kind==political_takeover_fact_kind||row.second.kind==political_takeover_decision_fact_kind)return common_failure("PoliticalTakeoverProfileRequired","facts/"+row.first);
    return common_success();
}
}
