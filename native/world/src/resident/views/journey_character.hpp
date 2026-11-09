// 现有行旅屏幕选择过的人物才能进入A公开投影；观察者始终来自唯一共同状态。
#pragma once
#include "character_presentation.hpp"
#include "resident/domains/interaction/interaction_model.hpp"
namespace hanmo::resident {
// 当前self/peer/speaker与无会面时原party同行；不枚举场景、不授权敌军名册或扩展知闻。
Outcome<CharacterPresentationView> query_selected_character(const CommonState&,const InteractionState&,const InteractionCatalog&,const CharacterPresentationRead&,const StableId& requested_actor_id);
} // C只核当前选择，A唯一提供公开六+五字段；失败不调用其它人物的projector。
