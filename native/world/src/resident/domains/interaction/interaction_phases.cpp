// 入场与收养/对白完成守卫保原后尾时点；未实现结果类型不被伪造为对白成功。
#include "interaction_phases.hpp"
#include "interaction_actions.hpp"
#include "resident/model/common_model.hpp"
#include <algorithm>
namespace hanmo::resident {
std::vector<ActorPlaceSnapshot> snapshot_actor_places(const CommonRead& c){std::vector<ActorPlaceSnapshot> out;for(auto& id:c.actor_order()){auto actor=c.actor(id);auto loc=actor?std::get_if<SettlementLocation>(&actor->location):nullptr;out.push_back({id,loc?loc->place_id:""});}return out;}
// 行动已经完成后，失去原说话人的会话直接结束；不能回滚成功行动或冒用身份。
Status resume_completed_dialogues(CommonWrite& common,InteractionState& state,const InteractionCatalog& catalog,const InteractionAccess& access,const std::vector<std::pair<StableId,std::shared_ptr<const ActionCompletion>>>& completed){
    std::vector<StableId> actors;for(auto& session:state.interactions)actors.push_back(session.actor_id);
    for(auto& actor:actors){
        InteractionSession* current=nullptr;for(auto& session:state.interactions)if(session.actor_id==actor)current=&session;
        if(!current||current->phase!="waiting")continue;
        std::shared_ptr<const ActionCompletion> outcome;for(auto& row:completed)if(row.first==current->pending_action_id)outcome=row.second;
        if(!outcome)continue;
        DialogueOutcome result;result.completion=outcome;
        auto resolved=resolve_dialogue(state,catalog,actor,result);if(!resolved.ok)return resolved;
        auto resumed=pump_dialogue(common,state,catalog,access,actor);
        if(!resumed.ok){if(resumed.error.kind!=ErrorKind::rule)return resumed;const InteractionSession*remaining=nullptr;for(const auto&session:state.interactions)if(session.actor_id==actor)remaining=&session;if(!remaining)continue;const auto original=remaining->id;auto before=capture_actor_social_occupancy(common,state,access,actor);if(!before.ok)return {false,{},before.error};state.interactions.erase(std::remove_if(state.interactions.begin(),state.interactions.end(),[&](const auto& session){return session.actor_id==actor;}),state.interactions.end());auto recorded=record_captured_actor_social_occupancy(access,actor,before.value,ActorSocialOccupancyCause::completed_dialogue_unavailable,original,actor);if(!recorded.ok)return recorded;}
    }
    return common_success();
}

Status enter_changed_places(const CommonRead& c,InteractionState& s,const InteractionCatalog& cat,const InteractionAccess& access,const std::vector<ActorPlaceSnapshot>& old){for(auto& id:c.actor_order()){auto actor=c.actor(id);auto loc=actor?std::get_if<SettlementLocation>(&actor->location):nullptr;if(!loc)continue;StableId previous;for(auto& snapshot:old)if(snapshot.actor_id==id){previous=snapshot.place_id;break;}if(previous!=loc->place_id){auto r=enter_place_events(c,s,cat,access,id);if(!r.ok)return r;}}return common_success();}
Status after_local_move(const CommonRead& c,InteractionState& s,const InteractionCatalog& cat,const InteractionAccess& access,const StableId& actor){auto before=capture_actor_social_occupancy(c,s,access,actor);if(!before.ok)return {false,{},before.error};StableId original;for(const auto&session:s.interactions)if(session.actor_id==actor)original=session.id;s.interactions.erase(std::remove_if(s.interactions.begin(),s.interactions.end(),[&](auto& session){return session.actor_id==actor;}),s.interactions.end());if(!original.empty()){auto recorded=record_captured_actor_social_occupancy(access,actor,before.value,ActorSocialOccupancyCause::local_move,original,actor);if(!recorded.ok)return recorded;}return enter_place_events(c,s,cat,access,actor);}
}
