// 常驻core私有持久外壳；世界权威只有一个typed bundle，run许可与检查点身份分离。
#pragma once
#include "hanmo/resident_world_api.hpp"
#include "world_codec.hpp"
namespace hanmo::resident {
struct ResidentWorld::Impl : CheckpointAllocatedObject {
    // 每个session至多一个未释放capture；旧世界保留量随真正lease持费。
    std::weak_ptr<const WorldCheckpointCapture> active_checkpoint_capture;
    std::weak_ptr<const WorldRestoreRetention> active_storage_restore;
    std::shared_ptr<WorldBundle> world; // 封闭只读capture lease保同一实际allocation owner
    bool fail_after_full_advance_once=false;
    std::vector<std::pair<StableId,Integer>> last_submission_timing;
    struct ActiveRun { BeginRun request;RunProgress progress; StableId action_kind,wallet_id; Integer action_start_tick=0,started_tick=0; std::optional<WorkUnits> exact_max_units,exact_spent_units; std::optional<WorldMoment> exact_started_at; std::optional<HalfDayDuration> exact_committed_duration;std::optional<ContinuationStartedSource> exact_action_started_source; };
    // Ready开期是同原串行worker的完整内部候选，不冒玩家命令/请求seq或花费时间。
    Status open_exact_period();
    Outcome<RunProgress> resume_exact_run(ResidentWorld&,const ResumeRun&);
    std::optional<ActiveRun> run;
    // 计划与单行动共用唯一worker，但每份许可只能驱动其中一种有限范围。
    struct ActivePlanRun { BeginRun request; RunProgress progress; };
    std::optional<ActivePlanRun> plan_run;
    Outcome<RunProgress> begin_plan_run(const BeginRun&);
    Outcome<RunProgress> resume_plan_run(ResidentWorld&,const ResumeRun&);
    Outcome<RunProgress> poll_plan_run(const RunRef&);
    // 支出观察仅在一个未发布候选内存在，不复制为世界账本或带入下次手动命令。
    struct RunSpendGuard { StableId wallet_id; Integer remaining=0,observed=0; };
    std::optional<RunSpendGuard> run_spend_guard;
    CheckpointJobReservation restored_continuation_memory;
    CheckpointUniquePtr<ContinuationSnapshot> restored_continuation;
    std::vector<StableId> used_run_ids;
};
} // 唯一worker串行访问；Bridge锁只保护队列/有限结果，不包围主线程长调用。
