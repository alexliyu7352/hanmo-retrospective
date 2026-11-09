// 静态委托公账v2保原预算/结酬算法；计划只在当前World候选内暂借，不成为另一份可花账。
#pragma once
#include "task_public_account_execution.hpp"
namespace hanmo::resident {
struct TaskStaticWalletResult { StableId wallet_id;Integer before=0,after=0; };
struct TaskStaticPaymentPreparation {
    TaskStaticPaymentSource receipt;
    ResourceRecipe cash_recipe;
    std::vector<TaskStaticWalletResult> wallets;
};
// 原finish/cancel先取本计划，把现金操作并入原货物/预约Recipe；未启v2或暂停必须明确拒绝。
Outcome<TaskStaticPaymentPreparation> prepare_task_static_payment(
    const CommonStore&,const TaskState&,const TaskCatalog&,const TaskPublicAccountExecutionState&,
    const PoliticalTakeoverRead&,const StableId& task_id,const StableId& command_kind,
    const ActionCoreView* completing_action,Integer tick,const DynamicStepInput* phase=nullptr);
// 仅在原Recipe成功后核每个实际钱包终值，再登记同候选事实；后续Task/ID失败仍由World整体回滚。
Status record_task_static_payment_after_resources(
    CommonStore&,TaskPublicAccountExecutionState&,const TaskStaticPaymentPreparation&);
// 本能力来源由真实Task/Catalog/profile派生，供S6放行最窄静态现金责任，不接受作者自报标记。
Outcome<bool> task_static_payment_responsibility_supported(
    const TaskState&,const TaskCatalog&,const StableId& task_id);
// v2每个真实静态受领/终态与Common事实双向覆盖；原奖励算法和原账户ID不能由坏档改写。
Status validate_task_static_payments(const CommonStore&,const TaskState&,const TaskCatalog&,
    const TaskPublicAccountExecutionState&,const PoliticalTakeoverRead&,const DynamicStepInput* phase=nullptr);
}
