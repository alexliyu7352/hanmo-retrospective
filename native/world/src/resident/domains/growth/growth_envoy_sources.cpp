#include "growth_envoy_sources.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
Outcome<std::optional<GrowthEnvoyRoundTripEvidence>> read_verified_growth_envoy_round_trip(
    const CommonState& common,const EnvoyTaskState& envoy,const TaskEnvoyRead& read,const StableId& actor){
    const auto fail=[](const StableId& code,const StableId& id=StableId{})->Outcome<std::optional<GrowthEnvoyRoundTripEvidence>>{return {false,{},common_failure(code,id).error};};
    if(actor.empty())return fail("UnknownActor",actor);
    if(!common.actors.find(actor))return fail("UnknownActor",actor);
    if(common.identity.tick<0||common.identity.tick>2147483647||common.metadata.scenario_id.empty()||std::all_of(common.identity.world_id.begin(),common.identity.world_id.end(),[](auto byte){return byte==0;}))return fail("InvalidActorCardWorld");
    if(envoy.version!=1&&envoy.version!=2)return fail("UnsupportedEnvoyTaskVersion");
    std::optional<GrowthEnvoyRoundTripEvidence> earliest;
    for(const auto& row:envoy.envoy_contracts.rows()){
        if(row.second.acceptance.envoy_actor_id!=actor)continue;
        auto source=read.envoy_task_source(row.first);if(!source.ok)return {false,{},source.error};
        const auto& value=source.value;const auto& accepted=value.acceptance;
        if(accepted.envoy_actor_id!=actor||accepted.task_id!=row.first||accepted.contract_id.empty()||std::count_if(envoy.envoy_contracts.rows().begin(),envoy.envoy_contracts.rows().end(),[&](const auto& other){return other.second.acceptance.envoy_actor_id==actor&&other.second.acceptance.contract_id==accepted.contract_id;})!=1)return fail("ActorCardEnvoySourceMismatch",row.first);
        if(envoy.version==1||!accepted.return_report_required||value.task_status!="success")continue;
        if(!value.delivery||!value.completion||value.completion->result!=EnvoyTaskResult::completed||!value.completion->diplomatic_report_fact_id||value.completion->diplomatic_report_fact_id->empty())return fail("ActorCardEnvoyReportRequired",row.first);
        const auto& delivered=*value.delivery;const auto& completed=*value.completion;
        if(accepted.accepted_tick<0||delivered.completed_tick<accepted.accepted_tick||completed.completed_tick<delivered.completed_tick||completed.completed_tick>common.identity.tick)return fail("ActorCardSourceTimeMismatch",row.first);
        if(std::count_if(envoy.envoy_contracts.rows().begin(),envoy.envoy_contracts.rows().end(),[&](const auto& other){return other.second.acceptance.envoy_actor_id==actor&&other.second.completion&&(other.second.completion->fact_id==completed.fact_id||other.second.completion->diplomatic_report_fact_id==completed.diplomatic_report_fact_id);})!=1)return fail("DuplicateActorCardEnvoySource",row.first);
        GrowthEnvoyRoundTripEvidence evidence;
        evidence.actor_id=actor;evidence.task_id=accepted.task_id;evidence.contract_id=accepted.contract_id;
        evidence.acceptance_fact_id=accepted.fact_id;evidence.mission_authority_fact_id=accepted.mission_authority_fact_id;evidence.proposal_id=accepted.proposal_id;
        evidence.delivery_fact_id=delivered.fact_id;evidence.presentation_fact_id=delivered.diplomatic_presentation_fact_id;
        evidence.completion_fact_id=completed.fact_id;evidence.report_fact_id=*completed.diplomatic_report_fact_id;
        evidence.world_id=common.identity.world_id;evidence.scenario_id=common.metadata.scenario_id;
        evidence.accepted_tick=accepted.accepted_tick;evidence.delivered_tick=delivered.completed_tick;evidence.completed_tick=completed.completed_tick;
        if(!earliest||evidence.completed_tick<earliest->completed_tick||(evidence.completed_tick==earliest->completed_tick&&evidence.task_id<earliest->task_id))earliest=std::move(evidence);
    }
    return {true,std::move(earliest),{}};
}
// 公开旧读口保持本人权限；内部历史读取不借篡改metadata绕过此门。
Outcome<std::optional<GrowthEnvoyRoundTripEvidence>> first_growth_envoy_round_trip(const CommonState& c,const EnvoyTaskState& s,const TaskEnvoyRead& r,const StableId& actor){if(actor!=c.metadata.player_actor_id)return {false,{},common_failure("NotPlayerActor",actor).error};return read_verified_growth_envoy_round_trip(c,s,r,actor);}
}
