// 原服务终止的精确完成源，供ActionCompletion/等待对白/Plan读取；不把Action消失当作成功。
#pragma once
#include "growth_recovery_services.hpp"
namespace hanmo::resident {
struct GrowthRecoveryCompletion {StableId source_fact_id,action_id,actor_id,provider_actor_id,reason,code;Integer tick=0,provided_half_days=0;bool success=false;};
// 调用前完整原Growth/伤因/康复史已通过；此读只核指定唯一终态，不递归World。
Outcome<GrowthRecoveryCompletion> read_growth_recovery_completion(const CommonStore&,const GrowthState&,const GrowthRecoveryState&,const StableId& exact_closure_fact_id);
// 原军事伤亡之后、任何本期服务之前调用；stable用于零时水战，phase用于陆战先伤亡后actions。
// release仅接原通用Action释放，调用者仍持整World保存点，并负责按返回精确完成恢复等待对白。
Outcome<std::vector<GrowthRecoveryCompletion>> terminate_dead_growth_recovery_services_in_candidate(CommonStore&,GrowthState&,const GrowthRecoveryCatalog&,GrowthRecoveryState&,const GrowthRecoveryHistoryAccess&,const std::function<Status(const StableId&)>& release,const DynamicStepInput* phase=nullptr);
}
