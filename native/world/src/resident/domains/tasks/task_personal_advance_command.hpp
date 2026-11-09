// 本人只提交具体任务与自愿金额；付款主体、授权和收款钱包都从真实冻结合同派生。
#pragma once
#include "task_personal_advance.hpp"
namespace hanmo::resident {
struct TaskPersonalAdvanceCommand {StableId task_id;Integer amount=0;};
Outcome<TaskPersonalAdvanceCommand> decode_task_personal_advance_command(const BoundaryValue&);
BoundaryValue encode_task_personal_advance_quote(const TaskPersonalAdvanceQuote&);
}
