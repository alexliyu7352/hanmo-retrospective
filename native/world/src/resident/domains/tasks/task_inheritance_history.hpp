// 遗产冷史只准未涉及或有显式Task2终态收据的责任；当前已结清不能单独倒填历史免责。
#pragma once
#include "resident/domains/tasks/task_model.hpp"
namespace hanmo::resident {
// 前提：同候选Task、Task2支付、信件实交及静态货物全验证/现金交叉源已通过；当前交割与事后冷审用同一来源标准。
Status validate_task_inheritance_history_provable_now(const CommonStore&,const TaskState&,const TaskCatalog&,const StableId& deceased_actor_id);
// caller先核S4实际family_inheritance_settled payload，传原fact_id及其原tick；禁止任意header借用。
Status validate_task_inheritance_history_before_fact(const CommonStore&,const TaskState&,const TaskCatalog&,const StableId& deceased_actor_id,const StableId& inheritance_fact_id,Integer tick);
}
