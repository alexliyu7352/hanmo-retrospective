// 玩家生活意向只保存有限来源、授权与实际引用；行动、合同和钱物仍属原领域。
#pragma once
#include "resident/storage/common_codec.hpp"
#include "hanmo/resident/world_moment.hpp"
#include "resident/domains/travel/travel_model.hpp"
#include "resident/domains/interaction/interaction_model.hpp"
namespace hanmo::resident {
inline constexpr Integer player_plan_version=1;
inline constexpr Integer player_plan_time2_version=2;
inline constexpr std::size_t player_plan_max_steps=8;
// 来源仅说明玩家如何知道目标，不授予远方查询或新合同权限。
enum class PlayerPlanSourceKind { authored,local_observation,accepted_task,existing_action,own_estate };
struct PlayerPlanSource { PlayerPlanSourceKind kind=PlayerPlanSourceKind::authored;StableId source_id;Integer observed_tick=0; };
struct PlayerPlanMove { StableId place_id; };
struct PlayerPlanTravel { TravelCommand request; };
struct PlayerPlanWork { InteractionContext context; };
struct PlayerPlanCourse { InteractionContext context;StableId skill_id;Integer ticks=2;StableId challenge_mode="automatic"; };
struct PlayerPlanRest { StableId estate_id;Integer duration=1; };
struct PlayerPlanContinue { StableId action_id; };
struct PlayerPlanTaskReminder { StableId task_id,place_id; };
using PlayerPlanStepData=std::variant<PlayerPlanMove,PlayerPlanTravel,PlayerPlanWork,PlayerPlanCourse,PlayerPlanRest,PlayerPlanContinue,PlayerPlanTaskReminder>;
// 冻结的是原条款和资格规则，不是余额、疲劳、库存或随机题目实例。
struct PlayerPlanTerms {
    StableId provider_actor_id,role_id,place_id,payer_wallet_id,recipient_wallet_id;
    StableId mode,skill_id,qualification_key,challenge_kind,challenge_id,challenge_mode;
    std::optional<StableId> settlement_policy,challenge_source_sha256;
    Integer duration=0,cost=0,wage=0,hp_gain=0,fatigue_delta=0,xp_limit=0,target_level=0;
};
enum class PlayerPlanStepPhase { queued,active,awaiting_confirmation,completed,blocked };
// 启动和完成各留原回执引用；删除未来队列不能抹掉仍在进行的原行动。
struct PlayerPlanStep {
    StableId id;PlayerPlanStepData data;PlayerPlanSource source;
    bool authorized=false;std::optional<PlayerPlanTerms> terms;
    PlayerPlanStepPhase phase=PlayerPlanStepPhase::queued;
    std::optional<StableId> action_id,start_result_id,completion_result_id;
    std::optional<Integer> started_tick,completed_tick;
    StableId stop_reason;
};
// /2 exact_used_duration是唯一实际时间累计；/1必须缺失，used_ticks只在/2为floor投影。
// 一名当前玩家只有一个列表；本状态不保存可复活的驱动许可或第二个预算。
// 批次预算只记本人批准的范围与实际消耗，保存它不等于恢复驱动许可。
struct PlayerPlanBudget { Integer max_additional_ticks=0,max_gross_spend=0,used_ticks=0,gross_spend=0,approved_tick=0;std::optional<HalfDayDuration> exact_used_duration; };
struct PlayerPlan { StableId id,actor_id;Integer created_tick=0;std::vector<PlayerPlanStep> steps;StableId stop_reason;std::optional<PlayerPlanBudget> budget;Integer approved_scope_revision=0; };
struct PlayerPlanState { Integer version=player_plan_version;std::optional<PlayerPlan> current; };
// 新增请求不接受由调用者自称已执行的phase/action/result字段。
struct PlayerPlanAppend { PlayerPlanStepData data;PlayerPlanSource source;bool authorized=false;std::optional<PlayerPlanTerms> terms; };
struct PlayerPlanRemove { StableId step_id; };
struct PlayerPlanReorder { std::vector<StableId> step_ids; };
struct PlayerPlanClear {};
struct PlayerPlanPause {};
struct PlayerPlanAuthorize { StableId step_id;bool authorized=false;std::optional<PlayerPlanTerms> terms; };
using PlayerPlanEdit=std::variant<PlayerPlanAppend,PlayerPlanRemove,PlayerPlanReorder,PlayerPlanClear,PlayerPlanPause,PlayerPlanAuthorize>;
// 真实承诺仅为原owner引用；截止时刻、状态和完成判定不在计划持久化。
enum class PlayerCommitmentKind { task,course };
struct PlayerCommitmentRef { PlayerCommitmentKind kind=PlayerCommitmentKind::task;StableId source_id; };
StableId player_plan_step_kind(const PlayerPlanStepData&);
StableId player_plan_phase_name(PlayerPlanStepPhase);
bool same_player_plan_terms(const PlayerPlanTerms&,const PlayerPlanTerms&);
Status validate_player_plan_shape(const PlayerPlanState&);
// 新教学先核精确Common结果绑定，完整课程/人物来源仍由World晚门强制核验。
struct GrowthTacticLessonState;
struct TaskState;
// 显式个人Task来源只在其全史后传入；旧调用仍保原通用回执门。
Status validate_player_plan(const CommonStore&,const PlayerPlanState&,const GrowthTacticLessonState* lessons=nullptr,const TaskState* personal_tasks=nullptr);
} // 领域函数只改未发布候选，由中央在原命令事务内统一采纳。
