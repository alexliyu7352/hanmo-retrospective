#pragma once
#include "military_duty_detail.hpp"
#include "resident/storage/common_store.hpp"

namespace hanmo::resident::duty_detail {

// These source-local checks borrow only retained Common/Duty rows. They do not
// call OriginalRead, ActionTime readers, source_moment, or execution consumers.
// They establish structural association, not original authority, safe release,
// per-actor occupation, or historical qualification. Callers preserve those
// independent checks and keep both roots stable throughout each synchronous use.
struct MilitaryStationOriginalAssociation {
    const MilitaryStationActionSource& source;
    const MilitaryDutyTerms& accepted_terms;
    const MilitaryDutyExecution& execution;
    const MilitaryDutyAcceptedSource& accepted_source;
    const DutyStationAction& binding;
    const FactRecord& source_header;
};

inline const MilitaryStationActionSource& military_station_source_for_action(
    const MilitaryDutyState& state,const StableId& action) {
    const MilitaryStationActionSource* result=nullptr;
    for(const auto& row:state.station_sources.rows())if(row.second.actual_action.id==action){
        if(result)movement_detail::fail("DuplicateMilitaryStationAction",action);
        result=&row.second;
    }
    if(!result)movement_detail::fail("MilitaryStationActionSourceRequired",action);
    return *result;
}

inline MilitaryStationOriginalAssociation read_military_station_original_association(
    const CommonStore& common,const MilitaryDutyState& state,const StableId& action) {
    const auto& source=military_station_source_for_action(state,action);
    const auto& t=terms(state,source.duty_action_id);
    const auto* e=state.executions.find(t.duty_action_id);
    const auto* accepted=e?state.accepted_sources.find(e->accepted_source_ref):nullptr;
    const auto* binding=t.execution_binding?std::get_if<DutyStationAction>(&*t.execution_binding):nullptr;
    const auto* head=common.state().facts.find(source.id);
    if(!e||!accepted||!binding||binding->action_id!=action||!head||head->domain!="military"||head->kind!="military_station_source"||source.actual_action.kind!="military_station"||source.actual_action.actor_id!=t.leader_actor_id||source.actual_presence_sources.empty())
        movement_detail::fail("MilitaryStationOriginalBindingRequired",action);
    return {source,t,*e,*accepted,*binding,*head};
}

// Keep this after the original terminal/cause/expiry proof; absence of a
// current Common action is not by itself a terminal or safe-release proof.
inline void require_military_station_action_released(
    const CommonStore& common,const StableId& action) {
    if(common.action(action))movement_detail::fail("MilitaryStationActionNotReleased",action);
}

// Used only in the original no-terminal branch. This is a current CommonAction
// relation, not a historical occupancy read or a per-actor action_id check.
inline ActionTimeOwnerPhase read_military_station_current_action_phase(
    const CommonStore& common,const MilitaryStationActionSource& source,
    const MilitaryDutyTerms& t,const StableId& action) {
    const auto* current=common.action(action);
    if(!current||current->kind!=source.actual_action.kind||current->actor_id!=source.actual_action.actor_id||current->participants!=source.actual_action.participants||current->start_tick!=source.started_at.half_day_tick||t.state==MilitaryDutyPhase::released||t.state==MilitaryDutyPhase::assembling)
        movement_detail::fail("MilitaryStationActualActionRequired",action);
    return t.state==MilitaryDutyPhase::active?ActionTimeOwnerPhase::working:ActionTimeOwnerPhase::suspended;
}

}
