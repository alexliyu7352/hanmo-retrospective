// 只允许当前单步借用已核from/to；普通冷读仍严格停在Common.tick。
#pragma once
#include "resident/storage/common_store.hpp"
#include "resident/model/dynamic_contract.hpp"
#include <limits>
#include "resident/domains/organization/office_authority.hpp"
namespace hanmo::resident {
inline Outcome<Integer> political_takeover_evaluation_tick(const CommonStore& common,const DynamicStepInput* phase){
    if(!phase)return {true,common.tick(),{}};
    if(phase->from_tick<0||phase->from_tick!=common.tick()||phase->from_tick==INT64_MAX||phase->to_tick!=phase->from_tick+1)return {false,0,common_failure("PoliticalTakeoverPhaseBoundaryMismatch").error};
    return {true,phase->to_tick,{}};
}
// /2仅读同世界真实阶段钟；不能从旧phase整数窗补offset。
inline Outcome<Integer> political_takeover_evaluation_tick(const CommonStore& common,const DynamicStepInput* phase,Integer version,const OrganizationAuthorityClock* clock){
    if(version==1)return political_takeover_evaluation_tick(common,phase);
    if(version!=2||!clock)return {false,0,common_failure("PoliticalTakeoverActualClockRequired").error};
    auto now=read_organization_authority_now(common,clock);return {now.ok,now.ok?now.value.half_day_tick:0,now.error};
}
}
