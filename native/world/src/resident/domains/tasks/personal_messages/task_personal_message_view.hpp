// 本人已受领原信件列表；不枚举远方收件人的位置或未获知任务。
#pragma once
#include "task_personal_message_contract.hpp"
namespace hanmo::resident {
Outcome<BoundaryValue> query_task_personal_messages(const TaskPersonalMessageSources&,const InteractionState&,const InteractionAccess&,Integer cursor=0);
}
