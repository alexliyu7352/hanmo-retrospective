// 只读区分尚未执行的NPC行旅意图与真实占用；失败不清除任何工作、行旅、钱货或承诺。
#include "native_player_control.hpp"
#include <algorithm>

namespace hanmo::resident {
// 新局只接管同一已选人物；旧政策和冷恢复不调用本适配器，时钟、ID及随机均不改变。
Status prepare_native_player_control(const CommonState& common,const TravelState& travel,const StableId& selected_actor){
    if(common.identity.tick!=0||common.identity.revision!=0)return common_failure("NativePlayerControlRequiresInitialState",selected_actor);
    const auto* actor=common.actors.find(selected_actor);
    if(selected_actor.empty()||common.metadata.player_actor_id!=selected_actor||!actor||actor->id!=selected_actor||!actor->alive||!actor->available)return common_failure("InvalidPlayableActor",selected_actor);
    if(!actor->action_id.empty())return common_failure("PlayableActorBusy",selected_actor);
    // 除本人行动头外，也拒绝作为参与者被另一条真实行动占用的候选，不能靠空action_id放行。
    for(const auto& row:common.actions.rows())if(row.second.actor_id==selected_actor||std::find(row.second.participants.begin(),row.second.participants.end(),selected_actor)!=row.second.participants.end())return common_failure("PlayableActorBusy",selected_actor);
    if(!std::holds_alternative<SettlementLocation>(actor->location))return common_failure("PlayableActorTravelling",selected_actor);
    if(const auto* itinerary=travel.itineraries.find(selected_actor)){
        if(itinerary->travelling)return common_failure("PlayableActorTravelling",selected_actor);
        if(itinerary->index!=0||itinerary->ready_tick<0||itinerary->ready_tick>2147483647)return common_failure("InvalidPlayableItinerary",selected_actor);
    }
    // Travel调度已显式排除metadata玩家；保留未开始意图才满足原作者计划与检查点的精确对应。
    return common_success();
}
}
