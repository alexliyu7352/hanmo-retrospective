// 延期命令只提交本人原Task与原同意政策，截止和增量均不能由调用者填写。
#pragma once
#include "task_extension.hpp"
namespace hanmo::resident {
inline constexpr const char* task_extension_command_kind="task_request_extension";
BoundaryValue encode_task_extension_command(const TaskExtensionCommand&);
Outcome<TaskExtensionCommand> decode_task_extension_command(const StableId& kind,const BoundaryValue&);
// 原InteractionEffect仅引用本次新同意事实；冷验以前序完整Task/延期账为前提，不重放业务。
Status validate_task_extension_cached_results(const CommonStore&,const TaskExtensionState*);
}
