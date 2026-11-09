// 原服务候选内登记真实休养；调用方持Common/Growth/恢复账总保存点，本域不另开或提交保存点。
#pragma once
#include "growth_recovery_history.hpp"
namespace hanmo::resident {
// 原付款、Action与LifeActivity成功后绑定；未选中的role保持原普通服务且不生成恢复来源。
Status bind_growth_recovery_service_in_candidate(CommonStore&,const GrowthState&,const CommonCatalog&,const GrowthRecoveryCatalog&,GrowthRecoveryState&,const GrowthRecoveryHistoryAccess&,const ActionCoreView&);
// 仅原existing_action的本次真实半日，在原remaining_ticks递减之前调用；返回空ID表示不属所选服务。
Outcome<StableId> step_growth_recovery_service_in_candidate(CommonStore&,const GrowthState&,const CommonCatalog&,const GrowthRecoveryCatalog&,GrowthRecoveryState&,const GrowthRecoveryHistoryAccess&,const ActionCoreView&,const DynamicStepInput&);
// 原LifeActivity已完成/取消后登记实供期；失败即丢弃整个候选，不清责任或补未来疗效。
Outcome<StableId> close_growth_recovery_service_in_candidate(CommonStore&,const GrowthState&,const GrowthRecoveryCatalog&,GrowthRecoveryState&,const GrowthRecoveryHistoryAccess&,const StableId& action_id,const StableId& reason,const StableId& actual_cause={},const DynamicStepInput* phase=nullptr);
}
