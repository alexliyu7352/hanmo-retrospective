// 使者责任来源只核冻结政治批准与唯一Task；当前外交意愿、实际呈递事实由政治域持有。
#pragma once
#include "envoy_task_admission.hpp"
namespace hanmo::resident {
class DiplomaticEnvoyReportRead;
// 历史受领不重用当前接受窗或在场/存活要求；不把今日失权、死亡或期限过期当历史不存在。
Outcome<EnvoyTaskAcceptance> read_envoy_task_acceptance(const CommonStore&,const TaskState&,const EnvoyTaskState&,const DiplomaticMissionRead&,const StableId& task_id);
// 只校验接受来源及唯一绑定；后继全生命周期校验还须核递交/返报/终态和政治presentation。
Status validate_envoy_task_acceptances(const CommonStore&,const TaskState&,const EnvoyTaskState&,const DiplomaticMissionRead&);
// 同候选内部读取必须同时借政治的唯一批准/呈递记录；不是玩家任意任务ID查询。
class EnvoyTaskRead final:public TaskEnvoyRead {
public:
    EnvoyTaskRead(const CommonStore&,const TaskState&,const EnvoyTaskState&,const DiplomaticMissionRead&,const DiplomacyPresentationRead&);
    // v2显式借用政治唯一报告读取口；旧五参数入口不能凭报告ID自行完成返报验证。
    EnvoyTaskRead(const CommonStore&,const TaskState&,const EnvoyTaskState&,const DiplomaticMissionRead&,const DiplomacyPresentationRead&,const DiplomaticEnvoyReportRead&);
    Outcome<EnvoyTaskSource> envoy_task_source(const StableId& task_id)const override;
    Outcome<EnvoyTaskDelivery> envoy_task_delivery(const StableId& fact_id)const override;
    Outcome<EnvoyTaskCompletion> envoy_task_completion(const StableId& fact_id)const override;
private:
    const CommonStore& common_;const TaskState& tasks_;const EnvoyTaskState& envoy_;
    const DiplomaticMissionRead& missions_;const DiplomacyPresentationRead& presentations_;
    const DiplomaticEnvoyReportRead* reports_=nullptr;
};
}
