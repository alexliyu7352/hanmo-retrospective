// 原Task保有全部钱货和Action写权；本持者只将同一Known会话、生命与控制来源借入原办理栈。
#pragma once
#include "task_personal_message_services.hpp"
#include "task_personal_message_profile.hpp"
#include "task_personal_message_receipts.hpp"
#include "task_personal_message_view.hpp"
#include "task_personal_message_progress.hpp"
#include "resident/domains/interaction/personal_dialogues/known_personal_dialogue_world.hpp"
namespace hanmo::resident {
struct WorldBundle;class CharacterConditionWorldRead;
class TaskPersonalMessageWorldAccess final {
public:
 static Outcome<std::shared_ptr<TaskPersonalMessageWorldAccess>> create(const WorldBundle&,const DynamicStepInput* phase=nullptr,const CharacterConditionWorldRead* original_body=nullptr);
 TaskPersonalMessageWorldAccess(const TaskPersonalMessageWorldAccess&)=delete;
 TaskPersonalMessageWorldAccess& operator=(const TaskPersonalMessageWorldAccess&)=delete;
 const TaskPersonalMessageServices& services()const{return services_;}
private:
 TaskPersonalMessageWorldAccess(const WorldBundle&,const DynamicStepInput*,std::shared_ptr<KnownPersonalDialogueWorldAccess>);
 std::shared_ptr<KnownPersonalDialogueWorldAccess> personal_;
 InteractionAccess interaction_access_;
 TaskPersonalMessageServices services_;
};
Status validate_task_personal_message_world(const WorldBundle&,const CharacterConditionWorldRead* original_body=nullptr);
Status task_personal_message_npc_command_allowed(const StableId& kind);
Outcome<TaskPersonalMessageQuote> preview_task_personal_message_world(const WorldBundle&,const StableId&,const TaskPersonalMessageDeliver&,const CharacterConditionWorldRead* original_body=nullptr);
// 原begin_action在分配前调用；占用后不能把当前busy误认为原报价不存在。
Status revalidate_task_personal_message_quote_world(const WorldBundle&,const StableId&,const TaskPersonalMessageDeliver&,const TaskPersonalMessageQuote&,const CharacterConditionWorldRead* original_body=nullptr);
// 原begin_action刚创建并占用这个Action后调用，唯一start_task_action原writer先预约原信。
Status start_task_personal_message_world(WorldBundle&,const ActionCoreView&,const TaskPersonalMessageQuote&,const TaskExecutionServices*,const CharacterConditionWorldRead* original_body=nullptr);
Outcome<BoundaryValue> query_task_personal_messages_world(const WorldBundle&,Integer cursor=0,const CharacterConditionWorldRead* original_body=nullptr);
struct TaskPersonalMessageResultBatch {Integer before_tick=0;std::vector<StableId> facts;};
Outcome<TaskPersonalMessageResultBatch> task_personal_message_new_result_world(const WorldBundle& before,const WorldBundle& after,const StableId& actor,const StableId& committed_kind,const ReceiptDetails&);
Status bind_task_personal_message_result_world(WorldBundle&,const TaskPersonalMessageResultBatch&,const StableId& actor,Integer request,const CommitResult&,const CharacterConditionWorldRead* original_body=nullptr);
}
