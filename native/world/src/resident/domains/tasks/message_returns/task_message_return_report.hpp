// 原Task报告的薄接头：原report仍唯一占用/结酬，新段仅预约并实际交回原回书。
#pragma once
#include "task_message_return.hpp"
namespace hanmo::resident {
struct TaskMessageReturnReportPreparation {TaskMessageReturnReportStart source;ResourceRecipe reservation;};
struct TaskMessageReturnDeliveryPreparation {TaskMessageReturnDelivery receipt;ResourceRecipe transfer;InventoryView source_after,destination_before;};
// 在原Task原门通过后追加新回书门；非选中Offer不新增限制。首次报告需原sender本人当前SAY。
Status preview_task_message_return_report(const TaskMessageReturnSources&,const InteractionState&,const InteractionAccess&,const StableId& actor,const TaskCommand&,const TaskQuote&);
// 原Action已占用，TaskAction/资源尚未写入时准备；真实ReserveLots并入原同一recipe。
Outcome<std::optional<TaskMessageReturnReportPreparation>> prepare_task_message_return_report_start(const TaskMessageReturnSources&,const InteractionState&,const InteractionAccess&,const ActionCoreView&,const TaskCommand&,const TaskQuote&);
Status record_task_message_return_report_start_after_resources(CommonStore&,TaskMessageReturnState&,const TaskMessageReturnSources&,const TaskMessageReturnReportPreparation&);
// 原report完成重新核真会面后，TransferLots并入其原现金recipe；末失败由原完整保存点撤回。
Outcome<std::optional<TaskMessageReturnDeliveryPreparation>> prepare_task_message_return_delivery(const TaskMessageReturnSources&,const InteractionState&,const InteractionAccess&,const ActionCoreView&,Integer tick);
Status record_task_message_return_delivery_after_report(CommonStore&,TaskMessageReturnState&,const TaskMessageReturnSources&,const TaskMessageReturnDeliveryPreparation&);
// 原report规则返回success=false才追加本次中断；保原TaskEffect，不提前改任务或发退款。
Status record_task_message_return_report_interruption(CommonStore&,TaskMessageReturnState&,const TaskMessageReturnSources&,const ActionCoreView&,Integer tick,const TaskEffect&);
}
