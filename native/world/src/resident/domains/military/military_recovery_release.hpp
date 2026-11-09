// Military8补原确认战果的永久边界；原命令授权不变，旧档没有来源仍不能倒填。
#pragma once
#include "resident/domains/military/military_refs.hpp"
#include "resident/model/dynamic_contract.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
// 只在原military_close完整候选内、result→closed前调用；不另造完成命令或搬动人物。
Status record_military_battle_release(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const MilitaryBattleRecord&,const StableId& actor_id);
Status validate_military_battle_release_shape(const MilitaryRecoveryState&);
// 完整原会战史先验；此门精确绑定最终伤亡、原控制者、唯一实际关闭头和Common顺序。
Status validate_military_battle_release_history(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const DynamicStepInput* phase=nullptr);
}
