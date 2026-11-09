// 原05§5.7地形表只由显式场景选择，复用原战场与回放，不新增军队或保存段。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct MilitaryFieldTerrainRule {StableId profile="terrain_and_zones_05_v1";};
// 接近场仍属于原围城拓扑；仅野外接敌映射到独立地形场，避免倒改城门/侧道。
struct MilitaryFieldTerrainBinding {StableId approach_battlefield_id,field_battlefield_id;};
struct MilitaryFieldTerrainPolicy {MilitaryFieldTerrainRule rule;std::vector<MilitaryFieldTerrainBinding> battlefields;};
inline constexpr const char* military_field_terrain_module="military_field_terrain";
// 固定原规则，不允许作者把沼泥改成道路、把水面伪装成步行地面。
Status validate_military_field_terrain_rule(const MilitaryFieldTerrainRule&);
Outcome<StableId> select_military_field_contact_battlefield(const MilitaryFieldTerrainPolicy&,const StableId& approach_battlefield_id);
Outcome<Integer> military_field_terrain_cost(const MilitaryFieldTerrainRule&,const StableId& category,const StableId& terrain);
}
