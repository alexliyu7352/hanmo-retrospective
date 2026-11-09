// 交互后尾借原行动完成清单与入场前快照，不创建独立时钟或复制领域结果。
#pragma once
#include "interaction_model.hpp"
namespace hanmo::resident {
struct ActorPlaceSnapshot { StableId actor_id,place_id; };
std::vector<ActorPlaceSnapshot> snapshot_actor_places(const CommonRead&);
Status resume_completed_dialogues(CommonWrite&,InteractionState&,const InteractionCatalog&,const InteractionAccess&,const std::vector<std::pair<StableId,std::shared_ptr<const ActionCompletion>>>& completed_actions);
Status enter_changed_places(const CommonRead&,InteractionState&,const InteractionCatalog&,const InteractionAccess&,const std::vector<ActorPlaceSnapshot>&);
Status after_local_move(const CommonRead&,InteractionState&,const InteractionCatalog&,const InteractionAccess&,const StableId& actor_id);
}
