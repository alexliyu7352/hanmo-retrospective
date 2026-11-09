// 本人取消使者责任保留接受/实际呈递/已做交接历史；不撤回提议，也不授外交接受。
#pragma once
#include "envoy_task_rules.hpp"
namespace hanmo::resident {
// 公共busy初门只允许本人当前使者交接；实际命令目标仍由preview/apply严格重验。
bool envoy_cancellation_busy_allowed(const CommonStore&,const TaskState&,const EnvoyTaskState&,const StableId& actor_id);
using EnvoyReleaseAction=std::function<Status(const StableId& action_id)>;
Outcome<EnvoyTaskSource> preview_envoy_cancellation(const CommonStore&,const TaskState&,const EnvoyTaskState&,const DiplomaticMissionRead&,const DiplomacyPresentationRead&,const StableId& actor_id,const StableId& task_id,Integer expected_world_revision);
// 只释放本任务正在进行的原交接；其它任务/旅行占用仍拒，失败须由整个World候选撤回。
Outcome<EnvoyTaskCompletion> cancel_envoy_task_in_candidate(CommonStore&,TaskState&,EnvoyTaskState&,const DiplomaticMissionRead&,const DiplomacyPresentationRead&,const StableId& actor_id,const StableId& task_id,Integer expected_world_revision,const EnvoyReleaseAction&);
}
