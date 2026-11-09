// 原参与者仅读亲自办理的回书来源；本人页的收回书按钮再走相同当面报价。
#pragma once
#include "task_message_return.hpp"
namespace hanmo::resident {
class TaskMessageReturnAccess final:public TaskMessageReturnRead {
public:explicit TaskMessageReturnAccess(TaskMessageReturnSources sources):sources_(std::move(sources)){}
 Outcome<std::optional<TaskMessageReturnKnownSource>> known_task_message_return(const StableId& observer,const StableId& task)const override;
private:TaskMessageReturnSources sources_;
};
Outcome<BoundaryValue> query_task_message_returns(const TaskMessageReturnSources&,const InteractionState&,const InteractionAccess&,Integer cursor=0);
}
