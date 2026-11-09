// 只向承运本人列原受领和本人销约凭据；不显示远方Task物批次或收件者现况。
#pragma once
#include "task_personal_cancellation_contract.hpp"
namespace hanmo::resident {Outcome<BoundaryValue> query_task_personal_cancellations(const TaskPersonalCancellationServices&,Integer cursor=0);}
