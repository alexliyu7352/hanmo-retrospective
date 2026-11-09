// 陆战军官热收据和冷来源共用同一真实名册；不新增第二份人物伤亡账。
#pragma once
#include "resident/domains/military/military_refs.hpp"
namespace hanmo::resident {
// 只返回原轮次新增的独立军官伤亡；已经是兵卒的真人由原五分类收据负责。
Outcome<std::vector<MilitaryActorChange>> military_commander_actor_changes(const CommonRead&,
 const MilitaryState&,const MilitaryBattleRecord& before,const MilitaryBattleRecord& after);
// 在原会战/收据结构校验之后核永久标记、实际本部兵损、唯一首次死亡及已应用Common。
Status validate_military_commander_history(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,
 const MilitaryAccess&,const MilitaryBattleRecord&);
}
