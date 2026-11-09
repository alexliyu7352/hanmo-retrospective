// 已有成人的新局接管只核本人真实状态；不更换资产、不撤销合同，也不执行NPC意图。
#pragma once
#include "resident/model/common_model.hpp"
#include "resident/domains/travel/travel_model.hpp"

namespace hanmo::resident {
// 仅供selected_existing_actor_v1冷初始化使用；未开始的作者行旅意图原样保留，真实忙碌或在途拒绝。
Status prepare_native_player_control(const CommonState&,const TravelState&,const StableId& selected_actor);
}
