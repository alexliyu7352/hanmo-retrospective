#include "resident/domains/military/battle_model.hpp"
// 既有军事页有限公开DTO；选中来源来自常驻候选，不含AI草案、RNG、完整人物或全库存。
#pragma once
#include "resident/domains/military/military_refs.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident {
StableId active_military_battle(const MilitaryState&,const StableId& actor_id);
Outcome<BoundaryValue> military_battle_page(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const CommonCatalog&,const StableId& battle_id={});
Outcome<BoundaryValue> military_battle_draft(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const StableId& battle_id,const StableId& selected_id,const MilitaryOrders&);
}
