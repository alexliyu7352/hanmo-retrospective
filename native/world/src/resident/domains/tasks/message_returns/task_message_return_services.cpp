// 仅同步借用，地址必须同一Common/Task/目录及原独立尾，不能用另一候选的报价或来源落款。
#include "task_message_return_services.hpp"
namespace hanmo::resident {
Status validate_task_message_return_services(const CommonStore& c,const TaskState& tasks,const TaskCatalog& catalog,const TaskMessageReturnState* state,const TaskMessageReturnServices* service){if(bool(state)!=(service!=nullptr))return common_failure("TaskMessageReturnServicesMismatch");if(!service)return common_success();if(state!=&service->sources.state||&c!=&service->sources.common||&tasks!=&service->sources.tasks||&catalog!=&service->sources.task_catalog)return common_failure("TaskMessageReturnCandidateRequired");return common_success();}
bool task_message_return_report_can_interrupt(const StableId& code){return code=="TaskMessageReturnDeclarationChanged"||code=="TaskMessageReturnOriginalLotMissing"||code=="TaskMessageReturnAssetAuthorityMismatch"||code=="RecipientCapacity";}
}
