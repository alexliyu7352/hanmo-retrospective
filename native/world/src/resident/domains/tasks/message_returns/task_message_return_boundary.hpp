// 唯一新命令只提交本人已知Task ID；双方、节点、期限、回书物品和原报酬都不可由参数覆盖。
#pragma once
#include "task_message_return_contract.hpp"
namespace hanmo::resident {
Outcome<TaskMessageReturnCollect> decode_task_message_return_collect(const StableId& kind,const BoundaryValue& params);
BoundaryValue encode_task_message_return_quote(const TaskMessageReturnQuote&);
Outcome<BoundaryValue> encode_task_message_return_action(const TaskMessageReturnState&,const StableId& action_id);
}
