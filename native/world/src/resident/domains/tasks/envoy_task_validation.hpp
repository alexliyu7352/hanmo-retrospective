// 发布/冷恢复时校验使者sidecar、唯一Task与Common行动/事实；阶段内临时来源不能直接过冷门。
#pragma once
#include "envoy_task_rules.hpp"
namespace hanmo::resident {
struct GovernanceTaskState;
Status validate_envoy_tasks(const CommonStore&,const TaskState&,const EnvoyTaskState&,const CommonCatalog&,const DiplomaticMissionRead&,const DiplomacyPresentationRead&);
// 返报v2完整冷验证必须显式传入政治报告源；模块marker/section是否启用仍由公共入口先核。
Status validate_envoy_tasks_with_reports(const CommonStore&,const TaskState&,const EnvoyTaskState&,const CommonCatalog&,const DiplomaticMissionRead&,const DiplomacyPresentationRead&,const DiplomaticEnvoyReportRead&);
// 只对完整已登记责任分流；旧入口与普通任务的作者/钱包守卫不变。
Status validate_tasks_with_responsibilities(const CommonStore&,const TaskState&,const TaskCatalog&,const CommonCatalog&,const GovernanceTaskState&,bool governance_registered,const EnvoyTaskState&,bool envoy_registered,const DiplomaticMissionRead&,const DiplomacyPresentationRead&,const TaskBattleRead& battles={},const DiplomaticEnvoyReportRead* reports=nullptr,const PoliticalTakeoverRead* authority=nullptr);
}
