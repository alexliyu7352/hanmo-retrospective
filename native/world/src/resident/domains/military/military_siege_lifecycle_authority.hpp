// 新围城参与账的历史军令/本人控制门；不拿当前任职或当前活着回填旧命令。
#pragma once
#include "military_siege_lifecycle_history.hpp"
namespace hanmo::resident {
// 原完整Military/Naval历史先验后调用；与参与结构门同一候选，不是新的政治授权口。
Status validate_military_siege_lifecycle_authority(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,
 const MilitarySiegeLifecycleState&,const DynamicStepInput* phase=nullptr);
}
