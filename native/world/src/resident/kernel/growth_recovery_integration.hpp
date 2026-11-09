// 完整个人休养World薄桥；所有读写借本候选原域，临时桥自持回调寿命，不跨clone保存。
#pragma once
#include "resident/domains/growth/growth_recovery_services.hpp"
#include "resident/domains/growth/growth_recovery_completion.hpp"
#include "resident/domains/growth/growth_personal_injury_history.hpp"
#include "command_types.hpp"
namespace hanmo::resident {
struct WorldBundle;
// S2稳定/严格phase具名回队口；无本profile返回空，调用方必须拒而非默许伤员回队。
std::unique_ptr<GrowthRecoveredActorRead> make_growth_recovered_actor_read(const WorldBundle&,const DynamicStepInput* phase=nullptr);
// Personal2只借稳定同World原伤因/Recovery；临时Read回调不逃出本同步调用。
Outcome<GrowthPersonalInjuryObservation> read_growth_personal_entry_injury_world(const WorldBundle&,const StableId& actor);
Outcome<GrowthPersonalInjuryObservation> read_growth_personal_entry_injury_before_world(const WorldBundle&,const StableId& actor,Integer entry_tick,const StableId& actual_entry_fact);
std::shared_ptr<GrowthPersonalRecoveryServices> make_growth_personal_recovery_services(WorldBundle&,const DynamicStepInput* phase=nullptr);
Status validate_growth_recovery_world(const WorldBundle&);
Outcome<BoundaryValue> query_growth_recovery_world(const WorldBundle&,Integer page);
// 通用释放前保原服务真实终态；未选本域Action不作任何写入。
Status prepare_growth_recovery_release(WorldBundle&,const StableId& action_id);
// 原陆战伤亡后的actions_step前或稳定零时命令尾；返回真实失败Completion供原等待对白/Plan消费。
Outcome<std::vector<std::pair<StableId,ActionCompletion>>> terminate_growth_recovery_world(WorldBundle&,const DynamicStepInput* phase=nullptr);
Status finish_growth_recovery_boundary_world(WorldBundle&);
// 只投影真实本半日新closure；补NPC取消/末尾死亡缺失的精确完成，不猜已消失Action。
Status complete_growth_recovery_advance_world(WorldBundle&,const std::vector<StableId>& old_closure_ids,AdvanceDetails&);
// 借提交前原账，含本次命令与尾段新死亡的精确closure差分；不补签旧未绑定源。
Status bind_growth_recovery_result_world(WorldBundle&,const GrowthRecoveryState* before,const StableId& actor,Integer sequence,const CommitResult&);
}
