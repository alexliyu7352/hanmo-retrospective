// 返报v2的完工当面证明；原外交呈递与新的政治报告收据仍由政治域唯一持有。
#pragma once
#include "envoy_task_state.hpp"
namespace hanmo::resident {
struct TaskState;
// 仅供同一actions_step候选：引用真实已交付记录，不能把旧会话或已经释放的Action当成报告事实。
struct EnvoyReportContactProof {
    WorldIdentity source_identity;
    EnvoyTaskAcceptance acceptance;
    EnvoyTaskDelivery delivery;
    StableId action_id;
    InteractionContext context;
    Integer completed_tick=0;
};
// 政治域取本证明后仍须复核原report Office/site、当前接收Grant和完整历史使命绑定。
class TaskEnvoyReportContactRead {
public:
    virtual ~TaskEnvoyReportContactRead()=default;
    virtual Outcome<EnvoyReportContactProof> envoy_report_contact(const StableId& task_id,const StableId& action_id,Integer completed_tick)const=0;
};
// 只支持显式v2候选；此读取不启用v2模块、不更改旧v1载荷，也不推进时间或写入成果。
Outcome<EnvoyReportContactProof> read_envoy_report_contact(const CommonStore&,const TaskState&,const EnvoyTaskState&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const StableId& task_id,const StableId& action_id,Integer completed_tick);
}
