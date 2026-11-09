// 通知绑定独立于旧Task与文书实交段；本账只连原信件和S4证明，不保存第二份死亡或知情真值。
#pragma once
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/domains/family/death_notice_source.hpp"
#include <map>
namespace hanmo::resident {
inline constexpr const char* task_death_notice_bound_kind="task_death_notice_bound";
struct TaskMessageNoticeBinding {
    StableId fact_id,task_id,offer_id,accept_action_id,carrier_actor_id,source_lot_id,carrier_inventory_id;
    Integer tick=0;WitnessedDeathNoticeSource source;
};
struct TaskMessageNoticeState {
    Integer version=1;OrderedTable<TaskMessageNoticeBinding> bindings;
    CheckpointMap<StableId,StableId> task_to_binding;
};
// 派生索引不进入存档；实际重复Task绑定由完整验证拒绝，不能靠覆盖索引洗掉。
void rebuild_task_message_notice_index(TaskMessageNoticeState&);
Status visit_task_message_notices_checkpoint(CodecVisitor&,TaskMessageNoticeState&);
BoundaryValue encode_task_message_notices(const TaskMessageNoticeState&);
Outcome<TaskMessageNoticeState> decode_task_message_notices(const BoundaryValue&);
}
