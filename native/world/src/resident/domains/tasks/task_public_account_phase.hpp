// 仅真实共享半日阶段可读尚未clock_commit的目标tick；冷读始终以Common已提交时间为界。
#pragma once
#include "task_model.hpp"
#include <limits>
namespace hanmo::resident {
inline Outcome<Integer> task_public_account_observed_tick(const CommonStore& common,const DynamicStepInput* phase){
    if(!phase)return {true,common.tick(),{}};
    if(phase->from_tick!=common.tick()||phase->from_tick<0||phase->from_tick==std::numeric_limits<Integer>::max()||phase->to_tick!=phase->from_tick+1)return {false,{},common_failure("TaskTakeoverPhaseMismatch").error};
    return {true,phase->to_tick,{}};
}
}
