// 回书接取复用原Common半日行动；报告与取消仍交原Task唯一资产及结酬写者。
#pragma once
#include "task_message_return_contract.hpp"
namespace hanmo::resident {
// 两份作者政策不能对同一原Offer并行改截止；后继须具名事实前期限口，首版明确不混用。
Status validate_task_message_return_extension_pair(const TaskMessageReturnCatalog&,const TaskExtensionCatalog*);
// 新Task返回段的全史先验；稳定与真实from→to共用同一来源标准。
Status validate_task_message_returns(const TaskMessageReturnSources&);
// 原报告来源的内部严格门，完整入口同时核去程/接取、该门和实际结果。
Status validate_task_message_return_reports(const TaskMessageReturnSources&);
Outcome<TaskMessageReturnQuote> preview_task_message_return_collection(const TaskMessageReturnSources&,const InteractionState&,const InteractionAccess&,const StableId& actor,const TaskMessageReturnCollect&);
// 外层须同时保护Common、Task及本段；返回原Action ID，实际result绑定在原commit尾完成。
Outcome<StableId> begin_task_message_return_collection_in_candidate(CommonStore&,TaskMessageReturnState&,const TaskMessageReturnSources&,const InteractionState&,const InteractionAccess&,const TaskMessageReturnQuote&);
// phase先原ActionMutation减至0，本口完成或精确中断；最终统一release仍由原phase拥有。
Outcome<TaskEffect> finish_task_message_return_collection_in_candidate(CommonStore&,TaskMessageReturnState&,const TaskMessageReturnSources&,const InteractionState&,const InteractionAccess&,const StableId& action_id,const DynamicStepInput&);
}
