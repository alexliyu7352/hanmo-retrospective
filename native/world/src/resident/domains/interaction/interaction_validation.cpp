// 完整交互边界检查真实引用与输入继续点；不会修复损坏状态或重放事件。
#include "interaction_model.hpp"
#include "interaction_actions.hpp"
#include "regional_life_dialogue.hpp"
#include "resident/model/common_model.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
namespace {
template<class T> const T* find_id(const std::vector<T>& rows,const StableId& id){for(auto& r:rows)if(r.id==id)return &r;return nullptr;}
template<class T> bool has(const std::vector<T>& a,const T& b){return std::find(a.begin(),a.end(),b)!=a.end();}
bool same_location(const Location& a,const Location& b){if(a.index()!=b.index())return false;if(auto p=std::get_if<WildernessLocation>(&a)){auto& q=std::get<WildernessLocation>(b);return p->map_id==q.map_id&&p->origin==q.origin&&p->point.x==q.point.x&&p->point.y==q.point.y;}if(auto p=std::get_if<OnRouteLocation>(&a)){auto& q=std::get<OnRouteLocation>(b);return p->edge_id==q.edge_id&&p->origin==q.origin&&p->progress==q.progress;}return false;}
}
Status validate_interaction_state(const CommonRead& c,const InteractionState& s,const InteractionCatalog& cat,const InteractionAccess& access){
    if(s.knowledge.size()!=c.actor_order().size())return common_failure("InvalidKnowledge");
    if(!s.encounters.empty()&&!cat.road_encounters)return common_failure("UnsupportedEncounters");std::set<StableId> ids,occupied;
    for(auto& e:s.encounters){if(e.id.empty()||!ids.insert(e.id).second||(e.status!="pending"&&e.status!="resolved"))return common_failure("InvalidEncounter");if(e.leaders[0]==e.leaders[1])return common_failure("InvalidEncounterMembers");if(e.created_tick<0||e.created_tick>c.tick())return common_failure("InvalidEncounterTime");if(e.status=="pending"?(e.resolved_tick!=-1||!e.result_id.empty()):(e.resolved_tick<e.created_tick||e.resolved_tick>c.tick()||e.result_id.empty()))return common_failure("InvalidEncounterResult");std::vector<StableId> expected;
        for(std::size_t i=0;i<2;++i){auto actor=c.actor(e.leaders[i]);if(!actor)return common_failure("UnknownEncounterActor");auto w=std::get_if<WildernessLocation>(&e.anchors[i]);auto route=std::get_if<OnRouteLocation>(&e.anchors[i]);if(w){if(!has(cat.map_ids,w->map_id)||!has(cat.settlement_ids,w->origin)||!access.terrain_point_valid||!access.terrain_point_valid(w->map_id,w->point))return common_failure("InvalidEncounterAnchor");}else if(route){auto d=find_id(cat.routes,route->edge_id);if(!d||route->origin!=d->origin||route->progress<1||route->progress>=d->duration_ticks*100)return common_failure("InvalidEncounterAnchor");}else return common_failure("InvalidEncounterAnchor");
            if(e.status=="pending"){auto action=c.action(e.action_ids[i]);if(!action||action->kind!="travel"||action->actor_id!=e.leaders[i])return common_failure("EncounterWithoutJourney");if(!access.journey_encounter)return common_failure("UnsupportedNativeCapability","journey_encounter",ErrorKind::unsupported_capability);auto j=access.journey_encounter(e.action_ids[i]);if(!j.ok)return {false,{},j.error};if(j.value.first!=e.id||!j.value.second||!same_location(actor->location,e.anchors[i]))return common_failure("EncounterJourneyMismatch");auto members=c.members(e.leaders[i]);if(!members)return common_failure("UnknownEncounterActor");expected.insert(expected.end(),members->begin(),members->end());}
        }
        if(!anchors_meet(cat,e.anchors[0],e.anchors[1]))return common_failure("EncounterAnchorsApart");if(e.status=="pending"&&e.participants!=expected)return common_failure("EncounterParticipantMismatch");std::set<StableId> seen;for(auto& person:e.participants){if(!c.actor(person)||!seen.insert(person).second)return common_failure("InvalidEncounterParticipant");if(e.status=="pending"&&!occupied.insert(person).second)return common_failure("DoubleEncounterOccupation");}for(auto& leader:e.leaders)if(!seen.count(leader))return common_failure("MissingEncounterLeader");
    }
    for(auto& id:c.action_order()){auto action=c.action(id);if(!action||action->kind!="travel")continue;if(!access.journey_encounter)return common_failure("UnsupportedNativeCapability","journey_encounter",ErrorKind::unsupported_capability);auto j=access.journey_encounter(id);if(!j.ok)return {false,{},j.error};if(!j.value.first.empty()){auto e=pending_encounter(s,action->actor_id);if(!e||e->id!=j.value.first)return common_failure("DanglingEncounter");}}
    ids.clear();for(auto& f:s.flags)if(!ids.insert(f.id).second||!has(cat.flags,f.id))return common_failure("InvalidFlag");
    ids.clear();for(auto& k:s.knowledge){if(!c.actor(k.actor_id)||!ids.insert(k.actor_id).second)return common_failure("InvalidKnowledge");std::set<StableId> known;for(auto& offer:k.offers)if(!has(cat.offer_ids,offer.offer_id)||!known.insert(offer.offer_id).second||offer.discovered_tick<0||offer.discovered_tick>c.tick())return common_failure("InvalidKnowledge");}
    ids.clear();for(auto& se:s.interactions){auto dialogue=find_id(cat.dialogues,se.dialogue_id);if(!c.actor(se.actor_id)||!ids.insert(se.actor_id).second||!dialogue)return common_failure("InvalidInteractionSession");auto role=cat.common?cat.common->roles.find(se.context.role_id):nullptr;if((!role&&!(se.context.kind&&*se.context.kind=="narrative"))||!c.actor(se.context.provider_actor_id)||(!(se.context.kind&&*se.context.kind=="encounter")&&!has(cat.place_ids,se.context.place_id)))return common_failure("InvalidSessionContext");
        if(se.context.kind&&*se.context.kind=="encounter"){auto checked=validate_interaction(c,s,cat,access,se.actor_id,se.context);StableId prototype=se.context.provider_actor_id;if(auto store=dynamic_cast<const CommonStore*>(&c))if(auto provider=store->state().actors.find(prototype))prototype=provider->prototype_id.value_or(prototype);auto ad=std::find_if(cat.actor_dialogues.begin(),cat.actor_dialogues.end(),[&](auto& d){return d.actor_id==prototype;});if(!checked.ok||ad==cat.actor_dialogues.end()||!ad->road_dialogue_id||*ad->road_dialogue_id!=se.dialogue_id)return common_failure("InvalidRoadSession");}
        // A stepping node is cold-readable only when the original selected
        // response already has its real Common result and exact retained tail.
        // A private pre-seal choice has no such source and is never publishable.
        if(se.phase=="stepping"){
            const auto* actual=dynamic_cast<const CommonStore*>(&c);
            if(!actual||!interaction_response_version(cat.response_sources_version))return common_failure("InvalidDialogueContinuePoint");
            auto tail=read_interaction_selected_response_tail(*actual,s,cat,se.id);
            if(!tail.ok)return {false,{},tail.error};if(!tail.value)return common_failure("InteractionSealedSelectedTailRequired",se.id);
            continue;
        }
        auto node=find_id(dialogue->nodes,se.node_id);if(!node||(se.phase!="say"&&se.phase!="choice"&&se.phase!="action"&&se.phase!="waiting"&&se.phase!="trade"))return common_failure("InvalidDialogueContinuePoint");if((se.phase=="say"||se.phase=="choice")&&node->kind!=se.phase)return common_failure("DialoguePhaseMismatch");if(se.phase=="action"||se.phase=="waiting"||se.phase=="trade"){if(node->kind!="action"||!node->command||!se.pending_command||!same_dialogue_command(*node->command,*se.pending_command))return common_failure("DialogueCommandMismatch");}if(se.phase=="waiting"){auto action=c.action(se.pending_action_id);if(!action||action->actor_id!=se.actor_id)return common_failure("MissingDialogueAction");}else if(!se.pending_action_id.empty())return common_failure("UnexpectedDialogueAction");if(se.phase=="trade"&&(!node->command||node->command->kind!="open_trade"))return common_failure("InvalidTradeContinuePoint");
    }
    ids.clear();for(auto& e:s.events){auto d=find_id(cat.events,e.event_id);if(!ids.insert(e.key).second||!d||!c.actor(e.actor_id)||d->place_id!=e.place_id||(e.status!="queued"&&e.status!="shown"&&e.status!="cancelled")||e.last_tick<0||e.last_tick>c.tick())return common_failure("InvalidSceneEvent");}
    return validate_regional_interaction(c,s,cat,&access);
}
}
