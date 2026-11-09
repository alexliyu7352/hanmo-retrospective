// Naval6只保存真实入战技艺来源；船体和兵员仍由原Naval/Military唯一账拥有。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
// 射程由船型显式作者给出；火攻/武器生产均不在此合同中隐式增加。
struct NavalGunneryWeapon {StableId hull_id;Integer minimum_range=1,maximum_range=1;};
struct NavalGunneryPolicy {StableId profile="entry_naval_skill_v1";std::vector<NavalGunneryWeapon> weapons;};

// 每条只对应原战场的一组真实unit/vessel/captain；经验因果由成长域验证，不另建XP账。
struct NavalGunneryCaptain {
 StableId unit_id,vessel_id,fleet_id,army_id,captain_actor_id,authored_actor_id;
 Integer experience=0,level=0;std::vector<StableId> causal_fact_ids;FieldOrder field_order;
};
// 一场只在真实建场后、首次AI/玩家计划前冻结一次；后来获得经验不改此场火力。
struct NavalGunneryPreparation {
 StableId id,battle_id,contact_source_id;Integer tick=0;
 std::vector<NavalGunneryCaptain> captains;FieldOrder field_order;
};
struct NavalGunneryState {std::vector<NavalGunneryPreparation> preparations;FieldOrder field_order;};
}
