// 一次驱动最多准备一个原命令，真实结果只在共同候选提交尾段登记。
#pragma once
#include "plan_model.hpp"
#include "resident/kernel/command_types.hpp"
namespace hanmo::resident {
class CompleteExactCommandCapture;
struct WorldBundle;
enum class PlayerPlanExecutionKind { start,advance,complete,reminder,stop };
// 此结构仅活在一次prepare/apply/commit调用中，绝不能编码进计划或冷快照。
struct PlayerPlanExecution {
    WorldIdentity source;std::optional<WorldMoment> exact_source_moment;Integer approved_scope_revision=0;StableId actor_id,plan_id,step_id,reason;
    PlayerPlanExecutionKind kind=PlayerPlanExecutionKind::stop;
    std::optional<StableId> action_id;
    std::shared_ptr<const PreparedDomainOp> nested;
    bool needs_confirmation=false;
};
Outcome<PlayerPlanExecution> prepare_player_plan_execution(const WorldBundle&,const StableId& actor_id,const PlayerPlanState&);
// 原apply可以交中央直接调用；本入口额外核同版本和固定命令绑定，不提交世界。
Outcome<ReceiptDetails> apply_player_plan_execution(WorldBundle&,const PlayerPlanState&,const StableId& actor_id,const PlayerPlanExecution&,CompleteExactCommandCapture* capture=nullptr);
// commit_tail取得原真实result_id后调用；外部手动命令只能核已绑定行动完成并停止驱动。
Status reconcile_player_plan_commit(WorldBundle&,PlayerPlanState&,const StableId& actor_id,const PlayerPlanExecution*,const ReceiptDetails&,const StableId& result_id);
} // 开工、副作用、一次完成、预算累计共同成功或共同丢弃，不另建模拟时钟。
