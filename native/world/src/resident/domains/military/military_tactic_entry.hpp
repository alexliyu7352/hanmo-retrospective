// 入场本领只读原唯一习得账；战斗轮次只消费冻结引用，不向Growth反向授权。
#pragma once
#include "battle_model.hpp"
#include "military_tactic_learning.hpp"
namespace hanmo::resident {
Status validate_military_tactic_entry_shape(const MilitaryTacticEntryState&);
Status validate_military_tactic_entry_bindings(const MilitaryBattleRecord&,const MilitaryBattleCatalog&,const MilitaryTacticEntryState&);
Status validate_military_learned_tactic_binding(const MilitaryTactic&,const MilitaryTacticLearningCatalog&);
// 原无16会话不增加资格门；显式16必须在该部原入场清单中具名取得。
Status require_military_learned_tactic(const MilitaryBattleRecord&,const MilitaryBattleCatalog&,const StableId& unit,const MilitaryTactic&);
}
