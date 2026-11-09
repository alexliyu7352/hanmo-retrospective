// M14早门仅验原军事入口/军权；晚门独立借完整Family/Birth/Growth历史，不用布尔跳过。
#pragma once
#include "military_commanded_damage.hpp"
#include "military_commanded_damage_contract.hpp"
#include "resident/domains/military/military_refs.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/model/dynamic_contract.hpp"
namespace hanmo::resident {
Outcome<MilitaryCommandedDamageState> prepare_military_commanded_damage_entry(
 const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryBattleRecord&,const MilitaryCommandedSkillRead&,const DynamicStepInput&);
Status validate_military_commanded_damage_bindings(
 const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryRoutEntryFact&,const MilitaryCommandedDamageState&,const DynamicStepInput* phase=nullptr);
Status validate_military_commanded_damage_actor_sources(
 const CommonStore&,const MilitaryRoutEntryFact&,const MilitaryCommandedDamageState&,const MilitaryCommandedSkillRead&);
}
