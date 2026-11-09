// 受领工厂直接持Known，不反借须先有受领史的Q01或返报全验，保证冷史有向。
#pragma once
#include "task_personal_acceptance_services.hpp"
#include "task_personal_acceptance_profile.hpp"
#include "task_personal_acceptance_receipts.hpp"
#include "task_personal_acceptance_view.hpp"
#include "task_personal_acceptance_progress.hpp"
#include "resident/domains/interaction/personal_dialogues/known_personal_dialogue_world.hpp"
namespace hanmo::resident {
struct WorldBundle;class CharacterConditionWorldRead;
class TaskPersonalAcceptanceWorldAccess final {
public:
 static Outcome<std::shared_ptr<TaskPersonalAcceptanceWorldAccess>> create(const WorldBundle&,const DynamicStepInput* phase=nullptr,const CharacterConditionWorldRead* original_body=nullptr);
 TaskPersonalAcceptanceWorldAccess(const TaskPersonalAcceptanceWorldAccess&)=delete;
 TaskPersonalAcceptanceWorldAccess& operator=(const TaskPersonalAcceptanceWorldAccess&)=delete;
 const TaskPersonalAcceptanceServices& services()const{return services_;}
private:
 TaskPersonalAcceptanceWorldAccess(const WorldBundle&,const DynamicStepInput*,std::shared_ptr<KnownPersonalDialogueWorldAccess>);
 std::shared_ptr<KnownPersonalDialogueWorldAccess> personal_;
 InteractionAccess interaction_access_;
 TaskPersonalAcceptanceServices services_;
};
Status validate_task_personal_acceptance_world(const WorldBundle&,const CharacterConditionWorldRead* original_body=nullptr);
Status task_personal_acceptance_npc_command_allowed(const StableId&);
Outcome<TaskPersonalAcceptanceQuote> preview_task_personal_acceptance_world(const WorldBundle&,const StableId&,const TaskPersonalAcceptance&,const CharacterConditionWorldRead* original_body=nullptr);
Status revalidate_task_personal_acceptance_quote_world(const WorldBundle&,const StableId&,const TaskPersonalAcceptance&,const TaskPersonalAcceptanceQuote&,const CharacterConditionWorldRead* original_body=nullptr);
Status start_task_personal_acceptance_world(WorldBundle&,const ActionCoreView&,const TaskPersonalAcceptanceQuote&,const TaskExecutionServices*,const CharacterConditionWorldRead* original_body=nullptr);
Outcome<BoundaryValue> query_task_personal_acceptances_world(const WorldBundle&,Integer cursor=0,const CharacterConditionWorldRead* original_body=nullptr);
struct TaskPersonalAcceptanceResultBatch {Integer before_tick=0;std::vector<StableId> facts;};
Outcome<TaskPersonalAcceptanceResultBatch> task_personal_acceptance_new_result_world(const WorldBundle&,const WorldBundle&,const StableId&,const StableId&,const ReceiptDetails&);
Status bind_task_personal_acceptance_result_world(WorldBundle&,const TaskPersonalAcceptanceResultBatch&,const StableId&,Integer,const CommitResult&,const CharacterConditionWorldRead* original_body=nullptr);
}
