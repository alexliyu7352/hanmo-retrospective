// 私信承包人向原sender本人明确销约；没有新付款、追缴、免责或远方知情授权。
#pragma once
#include "task_personal_cancellation_catalog.hpp"
#include "task_personal_cancellation_state.hpp"
#include "resident/domains/tasks/personal_messages/task_personal_message_contract.hpp"
#include "resident/domains/tasks/static_assets/task_static_asset.hpp"
namespace hanmo::resident {
struct TaskPersonalCancel {StableId task_id;};
struct TaskPersonalCancellationQuote {
 WorldIdentity source_identity;
 StableId task_id,policy_id,previous_task_status;
 KnownPersonalDialogueProof sender_declaration;
 Integer refunded_budget=0;
};
struct TaskPersonalCancellationSources {
 TaskPersonalMessageSources delivery;
 const TaskPersonalCancellationCatalog& catalog;
 const TaskPersonalCancellationState& state;
 // 只解释本取消头；原Q01 callback继续供完整交信先验使用。
 std::function<Outcome<StableId>(Integer,const StableId&)> player_before;
};
struct TaskPersonalCancellationServices {
 TaskPersonalCancellationSources sources;
 const InteractionState& interaction;
 const InteractionAccess& interaction_access;
 // 仅借原Task2上下文；未选时仍null，不在私人取消中另解释政治权限。
 const TaskExecutionServices* execution=nullptr;
};
// 完整Known、原Task/Q01及static_assets须在本段全史之前，不以今日空库存证明过去清偿。
Status validate_task_personal_cancellation_business(const TaskPersonalCancellationSources&);
Status validate_task_personal_cancellations(const TaskPersonalCancellationSources&);
Outcome<TaskPersonalCancellationQuote> preview_task_personal_cancellation(const TaskPersonalCancellationServices&,const StableId& actor,const TaskPersonalCancel&);
BoundaryValue encode_task_personal_cancellation_quote(const TaskPersonalCancellationQuote&);
// 唯一执行口先重报价，再原cancel_task返还，最后记录精确原settlement/asset来源；调用者拥有完整候选保存点。
Outcome<StableId> apply_task_personal_cancellation_in_candidate(CommonStore&,TaskState&,TaskPersonalCancellationState&,const TaskPersonalCancellationServices&,const StableId& actor,const TaskPersonalCancel&,const TaskPersonalCancellationQuote&);
}
