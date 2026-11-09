// 信件交接只记录实际参与者与原任务来源；发信人不会因同一世界事实自动获知远方送达。
#pragma once
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/storage/common_codec.hpp"
#include <map>
namespace hanmo::resident {
inline constexpr const char* task_message_handoff_kind="task_message_handoff";
// 一份原静态信件只发生一次交接，文书内容与其他领域事实不由这里推断或伪造。
struct TaskMessageHandoff {
    StableId fact_id,task_id,offer_id,source_action_id;
    Owner original_issuer;
    StableId carrier_actor_id,recipient_actor_id,item_id,source_lot_id,recipient_lot_id;
    StableId from_inventory_id,to_inventory_id,place_id,role_id;
    Integer tick=0;
};
inline constexpr const char* task_message_report_kind="task_message_report";
// v2返报只登记原任务当面成功结酬时实际听取报告的人；组织身份不自动通知其全部成员。
struct TaskMessageReport {
    StableId fact_id,task_id,handoff_fact_id,source_action_id,settlement_id;
    StableId carrier_actor_id,report_actor_id,place_id,role_id;
    Integer tick=0;
};
// observed_by是可重建的当前索引，不能独立持久化成另一份消息/知情账。
struct TaskMessageHandoffState {
    Integer version=1;
    OrderedTable<TaskMessageHandoff> receipts;
    CheckpointMap<StableId,std::vector<StableId>> observed_by;
    OrderedTable<TaskMessageReport> reports;
    CheckpointMap<StableId,std::vector<StableId>> reports_observed_by;
};
// 冷解码或录入后按真实交、收两主体重建；同人不得重复，原issuer不自动加入。
void rebuild_task_message_handoff_index(TaskMessageHandoffState&);
Status visit_task_message_handoffs_checkpoint(CodecVisitor&,TaskMessageHandoffState&);
BoundaryValue encode_task_message_handoffs(const TaskMessageHandoffState&);
Outcome<TaskMessageHandoffState> decode_task_message_handoffs(const BoundaryValue&);
}
