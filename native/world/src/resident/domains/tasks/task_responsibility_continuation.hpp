// 特殊责任仍由原Task与具名侧账持有；Director只借当前同候选的有限来源，不伪造普通Offer。
#pragma once
#include "governance_task_contract.hpp"
#include "envoy_task_contract.hpp"
#include "resident/domains/politics/diplomacy/diplomacy_mission_contract.hpp"
#include "envoy_task_report_step.hpp"
#include <variant>
namespace hanmo::resident {
class CommonStore;struct TaskState;struct GovernanceTaskState;struct EnvoyTaskState;
class DiplomaticEnvoyReportRead;
// 这些引用只能在本次读/提案调用中使用；复制或提交World后必须重建。
struct TaskResponsibilityContinuationRead {
    const GovernanceTaskState* governance=nullptr;
    const EnvoyTaskState* envoy=nullptr;
    const DiplomaticMissionRead* missions=nullptr;
    const DiplomacyPresentationRead* presentations=nullptr;
    const DiplomaticEnvoyReportRead* reports=nullptr;
    // 原返报责任不沿提议有效期取消，必须借S6已递交后的专用连续性口。
    EnvoyReportContinuityRead report_continuity;
};
// 不保存第二份进度；action与pending都来自实际侧账，治理和使者不互相降级。
struct TaskResponsibilityContinuationSource {
    WorldIdentity source_identity;
    StableId actor_id,task_id,action_id,pending_code;
    Integer module_version=1;
    std::variant<GovernanceTaskSource,EnvoyTaskSource> responsibility;
    std::optional<DiplomaticMissionContinuity> mission_continuity;
};
// 空值仅表示该真实本人Task不属于已登记两侧账；缺来源、双重归属与他人任务均报错。
Outcome<std::optional<TaskResponsibilityContinuationSource>> read_task_responsibility_continuation(
    const CommonStore&,const TaskState&,const TaskResponsibilityContinuationRead&,
    const StableId& observer_actor_id,const StableId& task_id);
}
