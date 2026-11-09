// 起行与控制的原参数值快照；只来自本次实际writer，不授予持久来源或报销资格。
#pragma once
#include "travel_model.hpp"
namespace hanmo::resident {
struct TravelOriginalPlanSnapshot {
    ActionCoreView action;
    Location location;
    std::optional<std::variant<TerrainTravelParams,RoadTravelParams>> params;
    std::optional<TerrainJourneyFact> terrain_plan;
};
struct TravelPlanInstallationDraft {
    Integer source_tick=0;
    TravelQuote original_quote;
    std::optional<TravelCommand> original_command;
    TravelOriginalPlanSnapshot before,after;
};
struct TravelControlDraft {
    Integer source_tick=0;
    PreparedTravelOp original_control;
    TravelOriginalPlanSnapshot before,after;
};
// 原当前行动及唯一typed载荷的拥有值；无载荷只供真正初次begin的写前边界。
inline Outcome<TravelOriginalPlanSnapshot> capture_original_travel_plan_snapshot(
    const CommonRead& common,const TravelState& state,const StableId& id){
    const auto* action=common.action(id);const auto* actor=action?common.actor(action->actor_id):nullptr;
    if(!action||action->kind!="travel"||!actor||actor->action_id!=id)return {false,{},common_failure("TravelPlanCaptureActionRequired",id).error};
    const auto* terrain=state.actions.find(id);const auto* road=state.road_actions.find(id);
    if(terrain&&road)return {false,{},common_failure("TravelPlanCaptureAmbiguousPayload",id).error};
    TravelOriginalPlanSnapshot out;out.action=*action;out.location=actor->location;
    if(terrain){const auto* source=state.journey_facts.find(terrain->plan_id);if(!source)return {false,{},common_failure("TravelPlanCaptureOriginalPlanRequired",id).error};out.params=*terrain;out.terrain_plan=*source;}
    else if(road)out.params=*road;
    return {true,std::move(out),{}};
}
}
