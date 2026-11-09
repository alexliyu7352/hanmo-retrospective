// 新康复尾永久绑定真实请求与结果；首history裁剪不能让旧同tick终止冒充本次行动。
#pragma once
#include "growth_recovery_completion.hpp"
namespace hanmo::resident {
struct ActionCompletion;
// 原Advance内已有完成必须逐字段匹配；允许普通service原completed缺省，不放宽失败形状。
Status validate_growth_recovery_action_completion(const GrowthRecoveryCompletion&,const ActionCompletion&);
// 仅取提交前后真实新增closure；旧源及其绑定必须原样，禁止追认旧未绑定坏档。
Outcome<std::vector<StableId>> growth_recovery_new_closure_sources(const GrowthRecoveryState& before,const GrowthRecoveryState& after);
// 外层Common/Growth/康复同候选已持完整事务；实际cache/history生成后、Plan消费前调用，不另开保存点。
Status bind_growth_recovery_result_in_candidate(CommonStore&,const GrowthState&,GrowthRecoveryState&,const std::vector<StableId>& exact_new_sources,const StableId& request_actor,Integer request_seq,const CommitResult&);
// 完整服务/伤因/康复史先验；最终World再核每个closure与永久提交绑定，允许原cache/history正常裁剪。
Status validate_growth_recovery_result_bindings(const CommonStore&,const GrowthState&,const GrowthRecoveryState&);
// 只读真正Common缓存对象；返回该次结果所有原Action终止，供零时死亡及Advance精确Plan消费。
Outcome<std::vector<GrowthRecoveryCompletion>> read_growth_recovery_completions_for_result(const CommonStore&,const GrowthState&,const GrowthRecoveryState&,const CommitResult& actual_result);
// 保原取消读签名；现必须精确永久结果绑定，previous仅是相邻保留历史的附加次序约束。
Outcome<StableId> validate_growth_recovery_cancel_receipt(const CommonStore&,const GrowthState&,const GrowthRecoveryState&,const StableId& actor,Integer tick,const GrowthEffect&,const StableId& actual_result_id,const StableId& previous_result_id={});
}
