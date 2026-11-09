// 已受领的静态信件/护送只提出下一条原命令；不会代办、发现委托或分配资产。
#pragma once
#include "resident/domains/director/director_model.hpp"
#include "task_authored_continuation_catalog.hpp"
namespace hanmo::resident {
struct CommonCatalog;
// 空目录保持旧等待；显式v1仅继续本人的既有责任，采用仍沿原预算及完整命令准入。
Outcome<std::optional<DecisionProposal>> propose_authored_task_continuation(
    const CommonStore&,const TaskState&,const TaskCatalog&,const CommonCatalog&,
    const TravelCatalog&,const KnownSubjectView&,const StableId& task_id,
    const DecisionProposal&,const TaskAuthoredContinuationCatalog*);
}
