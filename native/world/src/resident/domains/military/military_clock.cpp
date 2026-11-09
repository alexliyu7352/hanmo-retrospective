#include "resident/domains/military/military_model.hpp"
// 军中普通人物沿同一世界时钟前进；本人仍保留InArmy原位置和既有行动引用。
#include "resident/domains/military/military_clock.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
// 名册只判断当前军团的具名引用，不读取远方部队或复制军团状态。
bool contains(const std::vector<StableId>& ids,const StableId& id){return std::find(ids.begin(),ids.end(),id)!=ids.end();}
// 所有失败沿原规则错误出口返回，调用者不可把拒绝当作独立人物重试。
Outcome<StableId> reject(const StableId& code){return {false,{},common_failure(code).error};}
}
// 资格只证明当前军籍与本人真实占用；独立忙碌的统帅不阻止部将让世界继续。
Outcome<StableId> military_clock_leader(const CommonRead& common,const MilitaryState& military,const StableId& actor_id){
    const auto* actor=common.actor(actor_id);
    if(!actor)return reject("UnknownActor");
    const auto* attachment=std::get_if<InArmyLocation>(&actor->location);
    if(!attachment)return reject("NotArmyMember");
    const ArmyRecord* army=nullptr;
    for(const auto& candidate:military.armies)if(candidate.id==attachment->army_id){army=&candidate;break;}
    if(!army||army->status!="active"||!contains(army->member_actor_ids,actor_id)||contains(army->prisoner_ids,actor_id))return reject("ArmyMembershipRequired");
    if(!actor->alive||!actor->available)return reject("ActorUnavailable");
    const auto* leader=common.actor(army->commander_actor_id);
    if(!leader||leader->id==actor_id||!contains(army->member_actor_ids,leader->id)||contains(army->prisoner_ids,leader->id))return reject("ArmyCommanderUnavailable");
    if(std::holds_alternative<FollowingLocation>(leader->location)||std::holds_alternative<InArmyLocation>(leader->location))return reject("ArmyCommanderLocationInvalid");
    if(!actor->action_id.empty()){
        const auto* action=common.action(actor->action_id);
        if(!action||!contains(action->participants,actor_id))return reject("ArmyMemberActionMismatch");
    }
    return {true,leader->id,{}};
}
}
