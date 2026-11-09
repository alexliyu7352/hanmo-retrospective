// 请求只接受原Task ID，不能从参数选择退款金额、sender或库存。
#pragma once
#include "task_personal_cancellation_contract.hpp"
namespace hanmo::resident {Outcome<TaskPersonalCancel> decode_task_personal_cancel(const StableId&,const BoundaryValue&);}
