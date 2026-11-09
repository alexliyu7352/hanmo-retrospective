// 原军旅校验消费已完整验证的Naval永久事实；该读口不建立第二份俘虏或败军资产账。
#pragma once
#include "naval_battle_state.hpp"
namespace hanmo::resident {
std::optional<StableId> naval_custodian(const NavalState&,const StableId& person_id);
std::optional<StableId> naval_defeat_source(const NavalState&,const MilitaryState&,const StableId& army_id);
}
