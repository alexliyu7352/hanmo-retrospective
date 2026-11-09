// 动态暂拒复用原TaskCommand和报价；组合只读知识/当前会面，不认领Need或另建任务钟。
#pragma once
#include "resident/domains/tasks/task_runtime_offer_view.hpp"
namespace hanmo::resident {
struct WorldBundle;
// 只有显式回应v2允许新入口；本人来源和原共同守卫仍由实际预览链核验。
Outcome<TaskQuote> preview_runtime_task_response(const WorldBundle&,const StableId&,const TaskCommand&);
// 调用方已持完整世界候选；晚失败必须丢弃包含响应子域在内的整个候选。
Status apply_runtime_task_response(WorldBundle&,const StableId&,const TaskCommand&,const TaskQuote&);
// 观察者固定当前玩家；分页只读取当前会面实际已知的机会和本人历史回应。
Outcome<BoundaryValue> query_runtime_interaction_tasks(const WorldBundle&,Integer cursor=0);
}
