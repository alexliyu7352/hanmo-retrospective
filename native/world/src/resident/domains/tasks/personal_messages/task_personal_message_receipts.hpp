// 个人送信的真实提交后绑定；所有首次冷史都要求永久result，不借已裁缓存或当前控制者补授权。
#pragma once
#include "task_personal_message_contract.hpp"
namespace hanmo::resident {
struct ReceiptDetails;struct CommitResult;
Status validate_task_personal_message_results(const TaskPersonalMessageSources&);
Outcome<std::vector<StableId>> task_personal_message_new_result_sources(const TaskPersonalMessageState&,const TaskPersonalMessageState&,const StableId& request_actor,const StableId& kind,const ReceiptDetails&,Integer tick,Integer before_tick);
Status bind_task_personal_message_results(CommonStore&,TaskPersonalMessageState&,const TaskPersonalMessageSources&,const std::vector<StableId>& facts,const StableId& request_actor,Integer request,const CommitResult&,Integer before_tick);
}
