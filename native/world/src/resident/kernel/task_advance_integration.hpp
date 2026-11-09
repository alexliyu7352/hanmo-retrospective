// 原受领Action、私人付款与clock后清算的薄组合；所有状态仍属于同一World候选。
#pragma once
#include "resident/domains/tasks/task_personal_advance_command.hpp"
namespace hanmo::resident {
struct WorldBundle;struct TaskExecutionServices;
Outcome<TaskPersonalAdvanceQuote> preview_task_advance_world(const WorldBundle&,const StableId& actor,const TaskPersonalAdvanceCommand&);
Status apply_task_advance_world(WorldBundle&,const StableId& actor,const TaskPersonalAdvanceCommand&,const TaskPersonalAdvanceQuote&);
Status authorize_task_advance_after_accept(WorldBundle&,const StableId& action_id);
Status settle_task_advances_after_clock(WorldBundle&,const TaskExecutionServices* execution=nullptr);
Outcome<BoundaryValue> query_task_advances_world(const WorldBundle&,Integer cursor=0);
}
