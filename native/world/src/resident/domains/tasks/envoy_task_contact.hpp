// 已开始使者交接的完成时接触来源；与尚未接受动作的say/choice会话报价证明分开。
#pragma once
#include "envoy_task_state.hpp"
namespace hanmo::resident {
struct TaskState;
// 只在当前actions_step候选读取；政治域据此复核代表/Grant并写唯一presentation，不能跨提交复用。
struct EnvoyDeliveryContactProof {
    WorldIdentity source_identity;
    EnvoyTaskAcceptance acceptance;
    StableId action_id;
    InteractionContext context;
    Integer completed_tick=0;
};
// 此接口须由真实同候选Task/Common适配器实现，不接受边界调用者直接提交一份自报Proof。
class TaskEnvoyContactRead {
public:
    virtual ~TaskEnvoyContactRead()=default;
    virtual Outcome<EnvoyDeliveryContactProof> envoy_delivery_contact(const StableId& task_id,const StableId& action_id,Integer completed_tick)const=0;
};
// 仅在原Action已经减到零且尚未释放参与者时成立；内部采用原完成中行动例外核真正当面。
Outcome<EnvoyDeliveryContactProof> read_envoy_delivery_contact(const CommonStore&,const TaskState&,const EnvoyTaskState&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const StableId& task_id,const StableId& action_id,Integer completed_tick);
}
