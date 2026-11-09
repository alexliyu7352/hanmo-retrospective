// 具名人生资格只读同一共同人物与时钟；不替政治授职、本人意愿或资源权限作决定。
#pragma once
#include "resident/storage/common_store.hpp"

namespace hanmo::resident {
// 独立公职依03§3.15要求成年；未知出生不猜年龄，旧已授职责任不由此函数追溯取消。
inline Status growth_independent_office_age(const CommonStore& common,const StableId& actor_id){
    const auto* actor=common.state().actors.find(actor_id);
    if(!actor)return common_failure("UnknownActor",actor_id);
    constexpr Integer maximum_tick=2147483647,ticks_per_year=720,adult_years=18;
    const auto tick=common.tick();
    if(tick<0||tick>maximum_tick||!actor->birth_tick||*actor->birth_tick< -maximum_tick||*actor->birth_tick>tick)return common_failure("ActorAgeUnknown",actor_id);
    if(tick-*actor->birth_tick<adult_years*ticks_per_year)return common_failure("OfficeRequiresAdult",actor_id);
    return common_success();
}
}
