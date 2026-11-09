#include "resident/domains/military/battle_model.hpp"
// 永久来源直接连接原contact与实际round/casualty；不重算真实世界，也不接受今日墙体代替过去。
#pragma once
#include "resident/domains/military/military_refs.hpp"
namespace hanmo::resident {
// 与原入战共用增广匹配；不分配ID、不变名册，只在给定candidate写局部战场入口。
Status place_military_battle_at_original_spawns(MilitaryBattleRecord&,const MilitaryBattleCatalog&);
Outcome<StableId> record_military_rout_entry(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryBattleRecord&);
Outcome<StableId> record_military_rout_round(CommonWrite&,MilitaryState&,const MilitaryCatalog&,
 const MilitaryBattleRecord& before,const MilitaryBattleRecord& resolved,
 const StableId& battle_fact_id,const StableId& casualty_fact_id,const MilitaryDamageSources&,
 const StableId& termination_reason={});
Status validate_military_rout_history(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,
 const MilitaryAccess&,const DynamicStepInput* phase=nullptr);
}
