// 已受领店单沿原Common Action实际办理，完成只以真实两腿和本域永久receipt为依据。
#pragma once
#include "task_shop_order.hpp"
#include "resident/domains/interaction/dialogue_provider_source.hpp"
namespace hanmo::resident {
struct TaskShopOrderDeliveryQuote {WorldIdentity source_identity;StableId task_id;AuthoredProviderSourceProof contact;Integer duration=1;};
Outcome<TaskShopOrderDeliveryQuote> preview_task_shop_order_delivery(const CommonStore&,const TaskState&,const TaskShopOrderState&,const PrivateShopOrderRead&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const StableId& actor_id,const StableId& task_id);
Outcome<StableId> begin_task_shop_order_delivery_in_candidate(CommonStore&,const TaskState&,TaskShopOrderState&,const PrivateShopOrderRead&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const TaskShopOrderDeliveryQuote&);
// phase只准原from/to单步，货物不足返回原Task失败效果并保未交责任，不假写成功收据。
Outcome<TaskEffect> finish_task_shop_order_delivery_in_candidate(CommonStore&,TaskState&,TaskShopOrderState&,const PrivateShopOrderRead&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const StableId& action_id,const DynamicStepInput&);
Outcome<PrivateShopOrderExecution> cancel_task_shop_order_in_candidate(CommonStore&,TaskState&,TaskShopOrderState&,const PrivateShopOrderRead&,const StableId& actor_id,const StableId& task_id);
// 仅已提交时钟超过冻结期限时取消未交责任；先由原阶段处理实际Action，再交S5逐项capture。
using TaskShopOrderExecutionCapture=std::function<Status(const PrivateShopOrderExecution&)>;
Outcome<std::vector<PrivateShopOrderExecution>> expire_task_shop_orders_in_candidate(CommonStore&,TaskState&,TaskShopOrderState&,const PrivateShopOrderRead&,const TaskShopOrderExecutionCapture& capture);
Status validate_task_shop_order_ledger(const CommonStore&,const TaskState&,const TaskShopOrderState&,const DynamicStepInput* phase=nullptr);
Status validate_task_shop_order_execution(const CommonStore&,const TaskState&,const TaskShopOrderState&,const PrivateShopOrderRead&,const DynamicStepInput* phase=nullptr);
class TaskShopOrderExecutionAccess final:public PrivateShopOrderExecutionRead {
public:
    TaskShopOrderExecutionAccess(const CommonStore& c,const TaskState& t,const TaskShopOrderState& s,const DynamicStepInput* phase=nullptr):common_(c),tasks_(t),state_(s),phase_(phase){}
    Outcome<std::optional<PrivateShopOrderExecution>> private_shop_order_execution(const StableId& task_id)const override;
private:const CommonStore& common_;const TaskState& tasks_;const TaskShopOrderState& state_;const DynamicStepInput* phase_;
};
}
