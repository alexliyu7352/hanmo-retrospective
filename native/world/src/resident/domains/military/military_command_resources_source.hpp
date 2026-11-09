// 早门核军旅原入口与机械形状；晚门在同候选完整人物/Study史之后核真实兵法资格。
#pragma once
#include "resident/domains/military/military_refs.hpp"
#include "military_command_resources.hpp"
namespace hanmo::resident {
Outcome<MilitaryCommandResourcesState> prepare_military_command_resources_entry(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryBattleRecord&,const MilitaryCommandResourcesRule&,const MilitaryTacticsEntryRead&,const DynamicStepInput&);
Status validate_military_command_resources_bindings(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryRoutEntryFact&,const MilitaryCommandResourcesState&,const DynamicStepInput* phase=nullptr);
Status validate_military_command_resources_actor_sources(const CommonStore&,const MilitaryRoutEntryFact&,const MilitaryCommandResourcesState&,const MilitaryTacticsEntryRead&);
}
