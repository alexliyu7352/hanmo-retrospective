// Q03回书仍属原送信Task；作者只固定原收件人的明确托回声明，不另造合同或报酬。
#pragma once
#include "task_message_return_state.hpp"
#include "resident/domains/tasks/task_model.hpp"
#include "resident/domains/interaction/interaction_model.hpp"
namespace hanmo::resident {
inline constexpr const char* task_message_return_profile="private_round_trip_letter_v1";
inline constexpr const char* task_message_return_capability="task_message_returns_v1";
inline constexpr const char* task_message_return_collect_command="task_message_return_collect";
// 同一原Offer至多一个回书条款；回书物件独立于去程信件，不能拿原已交信件当回书。
struct TaskMessageReturnPolicy {
 StableId id,offer_id,reply_item_id,recipient_role_id,dialogue_id,node_id;
};
struct TaskMessageReturnCatalog {Integer version=1;std::vector<TaskMessageReturnPolicy> policies;};
// 只支持私人静态message与原初本人岗位；公约、动态任务和更换接待人另须完整权源。
Status validate_task_message_return_catalog(const TaskMessageReturnCatalog&,const TaskCatalog&,const InteractionCatalog&);
Outcome<TaskMessageReturnCatalog> decode_task_message_return_catalog(const BoundaryValue&,const TaskCatalog&,const InteractionCatalog&);
BoundaryValue encode_task_message_return_catalog(const TaskMessageReturnCatalog&);
// 显式新段必须配原实交/返报v2、原实物返还及真实生命来源；能力并集不能暗中开启。
Outcome<bool> decode_task_message_return_profile(const BoundaryValue& scenario,const std::vector<StableId>& capabilities);
const TaskMessageReturnPolicy* task_message_return_policy(const TaskMessageReturnCatalog&,const StableId& offer_id);
}
