// 具名释放调用者必须另供实际step/control/rescue或军战原因，不能用本只读值自证终结。
#include "travel_release_capture.hpp"
namespace hanmo::resident {
Outcome<TravelReleaseSnapshot> capture_travel_before_release(const CommonRead& common,const TravelState& state,const StableId& id){
    const auto fail=[&](const StableId& code)->Outcome<TravelReleaseSnapshot>{return {false,{},common_failure(code,id).error};};
    const auto* action=common.action(id);const auto* actor=action?common.actor(action->actor_id):nullptr;
    if(!action||!actor||actor->action_id!=id)return fail("TravelReleaseOriginalActionRequired");
    TravelReleaseSnapshot out;out.action=*action;out.source_tick=common.tick();out.actual_location=actor->location;
    if(action->kind=="travel"){
        if(state.rescues.find(id))return fail("TravelReleaseOriginalPayloadAmbiguous");
        auto original=capture_original_travel_plan_snapshot(common,state,id);if(!original.ok)return {false,{},original.error};if(!original.value.params)return fail("TravelReleaseOriginalPayloadRequired");
        std::visit([&](const auto& params){out.original_payload=params;},*original.value.params);out.terrain_plan=std::move(original.value.terrain_plan);
    }else if(action->kind=="rescue"){
        const auto* rescue=state.rescues.find(id);if(!rescue||rescue->origin.empty())return fail("TravelReleaseOriginalPayloadRequired");if(state.actions.find(id)||state.road_actions.find(id))return fail("TravelReleaseOriginalPayloadAmbiguous");out.original_payload=*rescue;
    }else return fail("TravelReleaseOriginalKindRequired");
    return {true,std::move(out),{}};
}
}
