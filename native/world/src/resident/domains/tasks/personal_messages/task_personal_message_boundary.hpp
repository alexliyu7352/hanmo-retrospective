// 原任务ID是唯一可写参数；收件者、地点、说话节点与原信件产权都来自真实已受领来源。
#pragma once
#include "task_personal_message_contract.hpp"
namespace hanmo::resident {
Outcome<TaskPersonalMessageDeliver> decode_task_personal_message_deliver(const StableId&,const BoundaryValue& params);
BoundaryValue encode_task_personal_message_quote(const TaskPersonalMessageQuote&);
}
