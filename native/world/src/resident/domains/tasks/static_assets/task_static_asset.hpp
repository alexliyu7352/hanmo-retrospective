// 仅借原静态任务准入后的同候选资源结果；本口既不搬货也不批准取消、交付或货权转属。
#pragma once
#include "task_static_asset_state.hpp"
#include "resident/domains/tasks/task_model.hpp"
namespace hanmo::resident {
// 临时快照只活在原World候选内，资源成功后逐批核原容器变化，不持久化副本。
struct TaskStaticAssetPreparation {
    TaskStaticAssetReceipt receipt;
    InventoryView source_after,destination_before;
};
// stage只准procurement_delivery/cancellation_return；原report Action或无Action取消来源各自严格。
Outcome<TaskStaticAssetPreparation> prepare_task_static_asset_transfer(const CommonStore&,const TaskState&,const TaskCatalog&,const CommonCatalog&,const TaskStaticAssetState&,const StableId& task_id,const StableId& stage,const ActionCoreView*,Integer tick,const DynamicStepInput* phase=nullptr);
// 原TransferLots成功后核准确拆批和剩余货物，再分配唯一事实；晚失败交原Common/Task保存点撤回。
Status record_task_static_asset_transfer_after_resources(CommonStore&,TaskStaticAssetState&,const TaskStaticAssetPreparation&);
// 全段冷校验：原责任/目录、规范实例顺序、逐腿数量、双方容器、Common事实及Task终态双向覆盖。
Status validate_task_static_assets(const CommonStore&,const TaskState&,const TaskCatalog&,const CommonCatalog&,const TaskStaticAssetState&,const DynamicStepInput* phase=nullptr);
// 若已启Task2，原永久终结支付提供独立时刻/Action锚；无Task2不凭本段授予历史免责。
Status validate_task_static_asset_cash_sources(const CommonStore&,const TaskState&,const TaskStaticAssetState&);
Status validate_task_static_asset_layout(const CommonStore&,bool enabled,const std::vector<std::pair<StableId,StableId>>& modules,const TaskStaticAssetState*);
Outcome<bool> decode_task_static_asset_profile(const BoundaryValue& scenario,const std::vector<StableId>& capabilities);
}
