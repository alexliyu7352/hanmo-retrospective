// 原Task单一writer借用同候选来源；服务不持第二余额、不写原Task表，也不回调World全验。
#pragma once
#include "task_message_return_report.hpp"
namespace hanmo::resident {
struct TaskMessageReturnServices {
 TaskMessageReturnSources sources;
 const InteractionState& interaction;
 const InteractionAccess& interaction_access;
};
Status validate_task_message_return_services(const CommonStore&,const TaskState&,const TaskCatalog&,const TaskMessageReturnState*,const TaskMessageReturnServices*);
// 只有完工现场真实变化属于原报告可中断结果；坏历史/坏保存段继续硬拒并撤回候选。
bool task_message_return_report_can_interrupt(const StableId& code);
}
