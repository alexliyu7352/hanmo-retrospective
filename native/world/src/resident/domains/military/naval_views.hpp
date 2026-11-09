// 舟师页仅投影本人所在真实军团/同泊地有权实船；不替代命令preview或导出远方私产。
#pragma once
#include "naval_world.hpp"
#include "naval_battle_state.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident {
Outcome<BoundaryValue> naval_camp_page(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const NavalAssetState&,const NavalAssetCatalog&,const StableId& actor_id,bool combat_enabled=false);
// 新格式只给既有舟师页追加具名修缮投影，旧Naval1/2形状保持。
Outcome<BoundaryValue> naval_camp_page(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const NavalState&,const NavalCatalog&,const StableId& actor_id,bool combat_enabled=true);
}
