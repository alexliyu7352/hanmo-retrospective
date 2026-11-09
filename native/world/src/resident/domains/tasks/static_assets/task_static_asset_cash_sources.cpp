// 复用原Task2永久支付来源，不新造一份同值终结账；完整现金/资产冷校验为此交叉口前提。
#include "task_static_asset.hpp"
namespace hanmo::resident {
Status validate_task_static_asset_cash_sources(const CommonStore& common,const TaskState& tasks,const TaskStaticAssetState& assets){
    if(!tasks.takeover_execution||tasks.takeover_execution->version!=2)return common_success();
    const auto& order=common.state().facts.order();
    for(const auto& row:assets.receipts.rows()){
        const auto& receipt=row.second;const TaskStaticPaymentSource* source=nullptr;
        for(const auto& cash:tasks.takeover_execution->static_payments.rows())if(cash.second.task_id==receipt.task_id&&cash.second.stage!=TaskStaticPaymentStage::acceptance){if(source)return common_failure("TaskStaticAssetCashSourceMismatch",row.first);source=&cash.second;}
        const auto* head=source?common.state().facts.find(source->fact_id):nullptr;
        if(!source||!head||head->id!=source->fact_id||head->kind!="task_static_payment"||head->domain!="tasks"||source->tick!=receipt.tick||source->source_action_id!=receipt.source_action_id||source->stage!=(receipt.stage=="procurement_delivery"?TaskStaticPaymentStage::report_success:TaskStaticPaymentStage::cancellation))return common_failure("TaskStaticAssetCashSourceMismatch",row.first);
        const auto before=std::find(order.begin(),order.end(),source->fact_id),after=std::find(order.begin(),order.end(),row.first);
        if(before==order.end()||after==order.end()||before>=after)return common_failure("TaskStaticAssetCashOrderMismatch",row.first);
    }
    return common_success();
}
}
