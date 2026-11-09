// 真实contact同栈冻结；历史只在完整入场结构和独立习得账早门后重建，晚教学源仍必须通过。
#pragma once
#include "military_tactic_entry.hpp"
#include "military_tactic_learning_source.hpp"
#include "resident/domains/military/military_refs.hpp"
namespace hanmo::resident {
Outcome<MilitaryTacticEntryState> prepare_military_tactic_entry(const CommonStore&,const CommonCatalog&,const MilitaryState&,const MilitaryCatalog&,const MilitaryTacticLearningState&,const MilitaryBattleRecord&,const DynamicStepInput&);
Status validate_military_tactic_entry_history(const CommonStore&,const CommonCatalog&,const MilitaryState&,const MilitaryCatalog&,const MilitaryTacticLearningState&,const MilitaryRoutEntryFact&,const DynamicStepInput* phase=nullptr);
}
