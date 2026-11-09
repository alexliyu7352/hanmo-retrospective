// 原静态宽限不变；最短采购加交接也赶不上窗口时只关新公款支出，不回收或瞬移既有资产。
#include "task_execution_gate.hpp"
#include "resident/model/procurement_timing.hpp"
namespace hanmo::resident {
namespace {
// 动态合同只借原execution_stopped权威；不把静态宽限覆盖到动态冻结条款。
bool authored_window_open(const CommonRead& common,const TaskRecord& task){
    if(task.contract_id)return true;
    return task_finish_within(common.tick(),trade_action_ticks+task_handover_ticks,task.deadline_tick,authored_task_late_grace_ticks);
}
}
// 原人物、任务类型、物品与动态停止权限保留；本任务公款不能新增已注定赶不上交接的采购。
bool task_procurement_authorized_now(const CommonRead& common,const TaskState& tasks,const TaskCatalog& catalog,const StableId& actor,const StableId& id,const StableId& item){
    if(!task_procurement_authorized(tasks,catalog,actor,id,item))return false;
    const auto* task=tasks.tasks.find(id);return task&&authored_window_open(common,*task);
}
// 付款列表与实际交易准入使用同一入口，保原任务顺序；不把过期经费伪装成个人零钱。
std::vector<std::pair<StableId,StableId>> task_procurement_payers_now(const CommonRead& common,const TaskState& tasks,const TaskCatalog& catalog,const StableId& actor){
    std::vector<std::pair<StableId,StableId>> rows;
    for(const auto& row:tasks.tasks.rows()){
        const auto* offer=resolve_task_definition(tasks,catalog,row.second);
        if(offer&&task_procurement_authorized_now(common,tasks,catalog,actor,row.first,offer->goal.item_id))rows.emplace_back(row.second.offer_id,row.second.budget_wallet_id);
    }
    return rows;
}
}
