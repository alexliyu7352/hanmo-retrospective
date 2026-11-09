// 销约持有原Q01/Known服务，原政治结算只在同一次热办理栈借入，不改变原Task writer。
#pragma once
#include "task_personal_cancellation_contract.hpp"
#include "task_personal_cancellation_profile.hpp"
#include "task_personal_cancellation_receipts.hpp"
#include "task_personal_cancellation_view.hpp"
#include "resident/domains/tasks/personal_messages/task_personal_message_world.hpp"
namespace hanmo::resident {
struct WorldBundle;class CharacterConditionWorldRead;
class TaskPersonalCancellationWorldAccess final {
public:
 static Outcome<std::shared_ptr<TaskPersonalCancellationWorldAccess>> create(const WorldBundle&,const DynamicStepInput* phase=nullptr,const CharacterConditionWorldRead* original_body=nullptr);
 TaskPersonalCancellationWorldAccess(const TaskPersonalCancellationWorldAccess&)=delete;
 TaskPersonalCancellationWorldAccess& operator=(const TaskPersonalCancellationWorldAccess&)=delete;
 const TaskPersonalCancellationServices& services()const{return services_;}
private:
 TaskPersonalCancellationWorldAccess(const WorldBundle&,std::shared_ptr<TaskPersonalMessageWorldAccess>);
 std::shared_ptr<TaskPersonalMessageWorldAccess> delivery_;
 TaskPersonalCancellationServices services_;
};
Status validate_task_personal_cancellation_world(const WorldBundle&,const CharacterConditionWorldRead* original_body=nullptr);
Status task_personal_cancellation_npc_command_allowed(const StableId&);
Outcome<TaskPersonalCancellationQuote> preview_task_personal_cancellation_world(const WorldBundle&,const StableId&,const TaskPersonalCancel&,const CharacterConditionWorldRead* original_body=nullptr);
Outcome<StableId> apply_task_personal_cancellation_world(WorldBundle&,const StableId&,const TaskPersonalCancel&,const TaskPersonalCancellationQuote&,const CharacterConditionWorldRead* original_body=nullptr);
Outcome<BoundaryValue> query_task_personal_cancellations_world(const WorldBundle&,Integer cursor=0,const CharacterConditionWorldRead* original_body=nullptr);
Outcome<StableId> task_personal_cancellation_new_result_world(const WorldBundle& before,const WorldBundle& after,const StableId&,const StableId&,const ReceiptDetails&);
Status bind_task_personal_cancellation_result_world(WorldBundle&,const StableId&,const StableId&,Integer,const CommitResult&,const CharacterConditionWorldRead* original_body=nullptr);
}
