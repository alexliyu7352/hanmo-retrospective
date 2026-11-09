// 原05同时倒地终局的显式目录规则；沿原同轮扣血和唯一终局，不产生第二份战斗状态。
#pragma once
#include "personal_model.hpp"
namespace hanmo::resident {
inline constexpr const char* personal_simultaneous_outcomes_module="personal_simultaneous_outcomes";
inline constexpr const char* personal_simultaneous_outcomes_capability="personal_simultaneous_outcomes_v1";
// 只从已选情景读取有限原目标集合；旧情景不补新策略，重复字段/未知目标拒绝。
Status configure_personal_simultaneous_outcomes(const BoundaryValue& data,const BoundaryValue& scenario,PersonalCatalog&);
// typed目录调用者也须过同一门；不能给护送目标覆盖其关键人物优先级。
Status validate_personal_simultaneous_outcomes(const PersonalCatalog&);
// 标题和完整World共用严格模块选择；不新增Personal段或复用他域位。
Status validate_personal_simultaneous_outcome_modules(const std::vector<std::pair<StableId,StableId>>&,bool enabled);
bool personal_simultaneous_outcome_selected(const PersonalCatalog&,const StableId& objective_id);
// 控制者和每个敌方都真正倒地才命中；撤离、仍存活敌人和空敌方不构成双方倒地。
bool personal_mutual_defeat_draw(const PersonalBattle&,const PersonalCatalog&);
}
