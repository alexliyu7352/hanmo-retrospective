// 入战的军级映射只读取原真实Army形成/接任；冷读精确原入口前，不用今天统帅替换历史。
#pragma once
#include "resident/domains/military/military_refs.hpp"
#include "military_command_shock.hpp"
namespace hanmo::resident {
Outcome<MilitaryCommandShockState> prepare_military_command_shock_entry(
 const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const MilitaryBattleRecord&);
Status validate_military_command_shock_entry(const CommonStore&,const MilitaryState&,
 const MilitaryAccess&,const MilitaryRoutEntryFact&,const MilitaryCommandShockState&);
}
