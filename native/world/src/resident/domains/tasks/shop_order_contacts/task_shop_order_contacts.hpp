// 开工同候选追加，原完成/失败继续唯一旧writer；全部永久来源按新独立profile明确配对。
#pragma once
#include "task_shop_order_contact_contract.hpp"
#include "resident/domains/tasks/shop_orders/task_shop_order_execution.hpp"
#include "resident/domains/family/actor_presence_read.hpp"
namespace hanmo::resident {
enum class TaskShopOrderContactSubmitterMode {unavailable,controlled_player,legacy_actor};
struct TaskShopOrderContactContext {
 const CommonStore& common;
 const TaskState& tasks;
 const TaskShopOrderState& orders;
 const TaskShopOrderContactState& contacts;
 const InteractionCatalog& interaction_catalog;
 const PrivateShopOrderRead& issued_orders;
 const DynamicStepInput* phase=nullptr;
 // 原完整Family/Creation/Birth及Military/Life先验，不借今日alive填旧接触。
 std::function<Outcome<ActorPresenceRead>(const StableId&,Integer,const StableId&)> presence_before;
 TaskShopOrderContactSubmitterMode submitter_mode=TaskShopOrderContactSubmitterMode::unavailable;
 std::function<Outcome<StableId>(Integer,const StableId&)> player_before;
};
// 只原begin成功后立即调用；原Common/Task整保存点须包住原Action与新头，失败整体撤回。
Outcome<StableId> record_task_shop_order_contact_in_candidate(CommonStore&,TaskShopOrderContactState&,const TaskShopOrderContactContext&,const StableId& actual_action_id,const TaskShopOrderDeliveryQuote& actual_quote);
Status validate_task_shop_order_contacts(const TaskShopOrderContactContext&);
class TaskShopOrderContactAccess final:public TaskShopOrderContactRead {
public:
 TaskShopOrderContactAccess(const CommonStore&,const TaskShopOrderState&,const TaskShopOrderContactState&,const DynamicStepInput* phase=nullptr);
 Outcome<std::optional<TaskShopOrderDeliveryContactSource>> task_shop_order_delivery_contact(const StableId&,const StableId&)const override;
private:
 const CommonStore& common_;const TaskShopOrderState& orders_;const TaskShopOrderContactState& contacts_;const DynamicStepInput* phase_;
};
}
