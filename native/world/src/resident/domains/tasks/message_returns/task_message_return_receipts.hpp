// 回书的真实提交后绑定；所有首次冷史都要求永久result，不借已裁缓存或当前控制者补授权。
#pragma once
#include "task_message_return_contract.hpp"
namespace hanmo::resident {
struct ReceiptDetails;struct CommitResult;
Status validate_task_message_return_results(const TaskMessageReturnSources&);
Outcome<std::vector<StableId>> task_message_return_new_result_sources(const TaskMessageReturnState&,const TaskMessageReturnState&,const StableId& request_actor,const StableId& kind,const ReceiptDetails&,Integer tick,Integer before_tick);
Status bind_task_message_return_results(CommonStore&,TaskMessageReturnState&,const TaskMessageReturnSources&,const std::vector<StableId>& facts,const StableId& request_actor,Integer request,const CommitResult&,Integer before_tick);
}
