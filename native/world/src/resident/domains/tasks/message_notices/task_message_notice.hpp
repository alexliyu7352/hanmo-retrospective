// 原信件接受/交付的业务接缝，只借S4亲历来源，不提供任意fact_id附件或消息总线。
#pragma once
#include "task_message_notice_catalog.hpp"
#include "task_message_notice_state.hpp"
#include "resident/domains/tasks/message_handoffs/task_message_handoff.hpp"
namespace hanmo::resident {
using WitnessedDeathNoticeValidationRead=std::function<Status(const WitnessedDeathNoticeSource&,const StableId& binding_fact_id,const DynamicStepInput* phase)>;
// 只在当前World候选调用栈存活；read与validate都须来自同一S4权威对象，不能从旧World缓存。
struct TaskMessageNoticeServices {
    const TaskMessageNoticeCatalog* catalog=nullptr;const TaskMessageNoticeState* state=nullptr;
    WitnessedDeathNoticeRead read;WitnessedDeathNoticeValidationRead validate;const DynamicStepInput* phase=nullptr;
};
struct TaskMessageNoticePreparation {TaskMessageNoticeBinding binding;StableId item_id;std::vector<StableId> previous_lot_ids;};
// 预览先拒尚未证实的死亡与非亲历发送者，不把玩家留在永远不能完成的accept Action里。
Status preview_task_message_notice_acceptance(const CommonStore&,const TaskState&,const TaskCatalog&,const CommonCatalog&,const TaskMessageNoticeServices*,const StableId& offer_id);
Outcome<std::optional<TaskMessageNoticePreparation>> prepare_task_message_notice_acceptance(const CommonStore&,const TaskState&,const TaskCatalog&,const CommonCatalog&,const TaskMessageNoticeServices*,const ActionCoreView&,Integer tick);
// 原AddLot完成后绑定其唯一新task-owned实物；不另造信、不转钱、不代收件人获知。
Status record_task_message_notice_after_resources(CommonStore&,TaskMessageNoticeState&,const TaskMessageNoticePreparation&);
Status validate_task_message_notices(const CommonStore&,const TaskState&,const TaskCatalog&,const CommonCatalog&,const TaskMessageNoticeCatalog&,const TaskMessageNoticeState&,const TaskMessageHandoffState&,const WitnessedDeathNoticeValidationRead&,const DynamicStepInput* phase=nullptr);
// 只有同一封实交给本人的原信，才形成可永久引用的通知来源；当前identity只是纯读版本戳。
struct DeliveredTaskDeathNotice {
    WorldIdentity identity;StableId scenario_id,binding_fact_id,task_id,source_lot_id,handoff_fact_id;
    StableId death_fact_id,decedent_actor_id,sender_actor_id,recipient_actor_id;
    Integer issued_tick=0,received_tick=0;
};
using TaskDeliveredDeathNoticeRead=std::function<Outcome<std::optional<DeliveredTaskDeathNotice>>(const StableId& observer,const StableId& binding_fact_id)>;
// 必须精确选binding，不能按同死者“最新一封”替换S4已经永久引用的原来源。
Outcome<std::optional<DeliveredTaskDeathNotice>> read_delivered_task_death_notice(const CommonStore&,const TaskState&,const TaskCatalog&,const CommonCatalog&,const TaskMessageNoticeCatalog&,const TaskMessageNoticeState&,const TaskMessageHandoffState&,const WitnessedDeathNoticeValidationRead&,const StableId& observer,const StableId& binding_fact_id);
Outcome<BoundaryValue> query_delivered_task_death_notices(const CommonStore&,const TaskState&,const TaskCatalog&,const CommonCatalog&,const TaskMessageNoticeCatalog&,const TaskMessageNoticeState&,const TaskMessageHandoffState&,const WitnessedDeathNoticeValidationRead&,Integer cursor=0);
}
