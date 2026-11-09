#include "resident/domains/military/military_command_contract.hpp"
// 原四半日训练与军阵题面的唯一运行入口；中央负责公共人物守卫、行动释放和时钟提交。
#pragma once
#include "resident/domains/military/military_refs.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident {
Outcome<MilitaryQuote> preview_military_training(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId&,const MilitaryCommand&,const StableId& own_action={});
Status start_military_training(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const ActionCoreView&,const MilitaryTrain&,const MilitaryQuote&);
Outcome<MilitaryTrainingStep> step_military_training(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const ActionCoreView&,Integer finish_tick);
Outcome<MilitaryEffect> finish_military_training(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const ActionCoreView&,Integer finish_tick);
Outcome<MilitaryEffect> apply_military_training(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId&,const MilitaryCommand&);
Status validate_military_training_action(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const ActionCoreView&);
Status validate_military_training_state(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&);
Outcome<std::vector<StableId>> military_training_mouths(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const MilitaryUnitRecord&,const StableId& garrison_id);
Status validate_training_drill_binding(const MilitaryCatalog&,const MilitaryTrainingFact&);
Outcome<MilitaryConditionProfile> fold_military_condition(const MilitaryState&,const MilitaryCatalog&,const StableId& unit,Integer through_tick);
BoundaryValue military_training_active_view(const CommonStore&,const MilitaryState&);
bool military_manual_window(const CommonRead&,const MilitaryState&,const StableId& actor);
Outcome<BoundaryValue> military_drill_view(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const StableId& actor,const StableId& action);
}
