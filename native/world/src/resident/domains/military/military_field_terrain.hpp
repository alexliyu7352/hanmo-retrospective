// 地形报价与本轮实际移动共用同一具名条件，不从今天地图补写过去会战。
#pragma once
#include "battle_model.hpp"
namespace hanmo::resident {
Status validate_military_field_terrain_troop(const MilitaryFieldTerrainRule&,const TroopDefinition&);
Status validate_military_field_terrain_policy(const MilitaryBattleCatalog&);
Integer military_field_attack_maximum_range(const BattlefieldDefinition&,const TroopDefinition&,HexPoint from);
bool military_field_retreat_required(const MilitaryBattleRecord&,const MilitaryBattleCatalog&,const StableId& unit_id,const StableId& tactic_kind);
}
