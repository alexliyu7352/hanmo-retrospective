// World薄桥只借同候选持久表；调用栈持有真实InteractionAccess与phase，不向本域回传完整World验证。
#pragma once
#include "task_message_return_services.hpp"
#include "task_message_return_profile.hpp"
#include "task_message_return_receipts.hpp"
#include "task_message_return_view.hpp"
#include "task_message_return_progress.hpp"
namespace hanmo::resident {
struct WorldBundle;
class TaskMessageReturnWorldAccess {
public:
 static Outcome<std::shared_ptr<TaskMessageReturnWorldAccess>> create(const WorldBundle&,const DynamicStepInput* phase=nullptr);
 TaskMessageReturnWorldAccess(const TaskMessageReturnWorldAccess&)=delete;
 TaskMessageReturnWorldAccess& operator=(const TaskMessageReturnWorldAccess&)=delete;
 const TaskMessageReturnServices& services()const{return services_;}
private:
 TaskMessageReturnWorldAccess(const WorldBundle&,const DynamicStepInput*);
 InteractionAccess interaction_access_;
 TaskMessageReturnServices services_;
};
Status validate_task_message_return_world(const WorldBundle&);
Status task_message_return_npc_command_allowed(const WorldBundle&,const StableId& kind,const TaskCommand* original_task=nullptr);
Outcome<TaskMessageReturnQuote> preview_task_message_return_world(const WorldBundle&,const StableId&,const TaskMessageReturnCollect&);
Outcome<StableId> commit_task_message_return_world(WorldBundle&,const StableId&,const TaskMessageReturnCollect&,const TaskMessageReturnQuote&);
Outcome<TaskEffect> finish_task_message_return_world(WorldBundle&,const StableId& action,const DynamicStepInput&);
Outcome<BoundaryValue> query_task_message_returns_world(const WorldBundle&,Integer cursor=0);
struct TaskMessageReturnResultBatch {Integer before_tick=0;std::vector<StableId> facts;};
Outcome<TaskMessageReturnResultBatch> task_message_return_new_result_world(const WorldBundle& before,const WorldBundle& after,const StableId& actor,const StableId& committed_kind,const ReceiptDetails&);
Status bind_task_message_return_result_world(WorldBundle&,const TaskMessageReturnResultBatch&,const StableId& actor,Integer request,const CommitResult& actual_result);
}
