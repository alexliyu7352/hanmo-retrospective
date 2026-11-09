// 本人零时销约只绑定本请求真正新增的一份来源；缓存裁剪不能删掉永久实际结果。
#pragma once
#include "task_personal_cancellation_contract.hpp"
namespace hanmo::resident {
struct ReceiptDetails;struct CommitResult;
Outcome<StableId> task_personal_cancellation_new_result_source(const TaskPersonalCancellationState&,const TaskPersonalCancellationState&,const StableId& actor,const StableId& kind,const ReceiptDetails&,Integer tick);
Status bind_task_personal_cancellation_result(CommonStore&,TaskPersonalCancellationState&,const StableId& fact_id,const StableId& actor,Integer request,const CommitResult&);
Status validate_task_personal_cancellation_results(const CommonStore&,const TaskPersonalCancellationState*);
}
