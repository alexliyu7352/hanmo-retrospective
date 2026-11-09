// 原围城确认的实际退出来源；仅解除此事实以后的占用，不补造此前加入/撤军历史。
#pragma once
#include "resident/domains/military/military_refs.hpp"
#include "resident/model/dynamic_contract.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
Status record_military_siege_release(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const SiegeRecord&,const StableId& actor_id);
Status validate_military_siege_release_shape(const MilitaryRecoveryState&);
// 原围城、形成与接任史先验；此门核本次原确认者、最终结果及精确Common先后。
Status validate_military_siege_release_history(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const DynamicStepInput* phase=nullptr);
}
