// 新局有限外貌只影响明确获准人物的原AppearanceProfile；目录与草案不进入热状态。
#pragma once
#include "native_initial_state.hpp"
namespace hanmo::resident {
// 旧fixed_actor拒绝候选表；旧外貌政策核十二项，新组合核192项，选人政策再核逐人许可。
Status validate_native_appearance_choices(const BoundaryValue& effective_data,const BoundaryValue& scenario);
// 返回已核完整档案或空指针以保留旧初始化；失败不分配人物或初货。
Outcome<std::shared_ptr<const AppearanceProfile>> select_native_appearance(const BoundaryValue& effective_data,const BoundaryValue& scenario,const std::optional<BoundaryValue>& request);
// 新选人政策显式携带已选原人物；逐人许可决定组合或完整保留原外貌，旧三参入口仍独立保留。
Outcome<std::shared_ptr<const AppearanceProfile>> select_native_appearance(const BoundaryValue& effective_data,const BoundaryValue& scenario,const std::optional<BoundaryValue>& request,const StableId& selected_actor_id);
}
