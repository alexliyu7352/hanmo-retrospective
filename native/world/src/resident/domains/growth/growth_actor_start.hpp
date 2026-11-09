// 新局人物卡政策只决定收藏资格；真正人物资格仍由原选择与初态叶核实。
#pragma once
#include "growth_actor_cards.hpp"
namespace hanmo::resident {
struct GrowthActorStartPolicy {
    GrowthActorCardCatalog cards;
    std::vector<StableId> playable_actor_ids,initially_available_actor_ids;
    StableId default_actor_id;
};
Outcome<GrowthActorStartPolicy> decode_growth_actor_start_policy(const BoundaryValue& definitions,const BoundaryValue& config,const CommonCatalog&,const std::vector<StableId>& playable_actor_ids,const StableId& default_actor_id);
// 仅用于新局，读档不会重新执行；unlocked来自专属跨局资格文件而非图鉴。
Status validate_growth_actor_start_selection(const GrowthActorStartPolicy&,const StableId& selected_actor_id,const std::vector<StableId>& unlocked_actor_ids);
}
