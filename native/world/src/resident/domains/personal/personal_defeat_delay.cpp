// 原普通遭遇败北的两半日行旅延误；只从永久战果派生hold，不存第二倒计时或动钱货。
#include "personal_model.hpp"
#include <set>
namespace hanmo::resident {namespace {
// 来源比较忽略字段顺序但保全部有类型业务值，坏来源不能形成移动处罚。
bool same_source(const PersonalSource& a,const PersonalSource& b) {
    if(a.kind!=b.kind||a.encounter_id!=b.encounter_id||bool(a.context)!=bool(b.context))return false;
    if(!a.context)return true;
    const auto& x=*a.context;const auto& y=*b.context;
    return x.place_id==y.place_id&&x.provider_actor_id==y.provider_actor_id&&x.role_id==y.role_id&&x.kind==y.kind&&x.encounter_id==y.encounter_id;
}
// 只接受实际道路小规模战败的完整永久证明；形状/来源不符安全忽略，不能代替世界校验。
std::vector<StableId> affected_participants(const CommonStore& common,const PersonalState& state,const PersonalCatalog& catalog,const PersonalBattle& battle) {
    if((battle.phase!="result"&&battle.phase!="closed")||battle.outcome!="defeat"||!battle.applied)return {};
    if(battle.source.kind!="encounter"||battle.source.encounter_id.empty())return {};
    if(battle.completed_tick<0||battle.completed_tick>common.tick()||common.tick()-battle.completed_tick>=2)return {};
    const auto* objective=catalog.objectives.find(battle.objective_id);
    if(!objective||objective->kind!="skirmish"||battle.result_id.empty())return {};
    const auto* controller=battle.units.find(battle.controller_id);
    if(!controller||controller->side<0||controller->side>1)return {};
    const auto* header=common.state().facts.find(battle.id);
    const auto* fact=state.facts.find(battle.id);
    if(!header||header->kind!="personal_battle"||!fact||fact->battle_id!=battle.id||fact->result_id!=battle.result_id||fact->outcome!="defeat"||fact->tick!=battle.completed_tick||!same_source(fact->source,battle.source))return {};
    auto actual=fact->participants;auto recorded=battle.units.order();
    std::sort(actual.begin(),actual.end());std::sort(recorded.begin(),recorded.end());
    if(actual!=recorded)return {};
    std::vector<StableId> affected;
    for(const auto& id:recorded) {
        const auto& unit=*battle.units.find(id);
        if(unit.actor_id!=id||unit.side<0||unit.side>1)return {};
        if(unit.side==controller->side)affected.push_back(id);
    }
    return affected;
}
}
// 旧tick T和T+1各hold一次，T+2自然失效；只阻止当前承载原参败人物的travel领队。
std::vector<StableId> personal_held_travel_actors(const CommonStore& common,const PersonalState& state,const PersonalCatalog& catalog) {
    if(common.tick()<0)return {};
    std::set<StableId> affected;
    for(const auto& row:state.battles.rows())for(const auto& id:affected_participants(common,state,catalog,row.second))affected.insert(id);
    std::set<StableId> held;
    for(const auto& id:common.action_order()) {
        const auto* action=common.action(id);
        if(!action||action->kind!="travel"||action->id!=id)continue;
        const auto* leader=common.actor(action->actor_id);
        if(!leader||leader->action_id!=id||std::find(action->participants.begin(),action->participants.end(),action->actor_id)==action->participants.end())continue;
        for(const auto& participant:action->participants)if(affected.count(participant)) {
            held.insert(action->actor_id);
            break;
        }
    }
    return {held.begin(),held.end()};
}
}
