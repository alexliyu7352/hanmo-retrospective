// 同一原私人送信责任的回程物件；提交不能改原收件人、回书条款、期限或报酬。
#pragma once
#include "task_message_return_catalog.hpp"
#include "resident/domains/interaction/dialogue_provider_source.hpp"
#include "resident/domains/family/actor_presence_read.hpp"
#include "resident/domains/tasks/message_handoffs/task_message_report.hpp"
#include "resident/domains/tasks/static_assets/task_static_asset.hpp"
namespace hanmo::resident {
struct TaskMessageReturnCollect {StableId task_id;};
struct TaskMessageReturnQuote {
 WorldIdentity source_identity;
 StableId task_id,policy_id,handoff_fact_id,reply_item_id,carrier_inventory_id;
 AuthoredProviderSourceProof declaration;
 Integer duration=1;
};
enum class TaskMessageReturnSubmitterMode {unavailable,controlled_player,legacy_actor};
// World先验原Task/去程返报/实物与Family全史；本服务不反调完整World或另做生命账。
struct TaskMessageReturnSources {
 const CommonStore& common;const CommonCatalog& common_catalog;
 const TaskState& tasks;const TaskCatalog& task_catalog;const TaskMessageHandoffState& messages;
 const TaskStaticAssetState& assets;const InteractionCatalog& interaction_catalog;
 const TaskMessageReturnCatalog& catalog;const TaskMessageReturnState& state;
 const DynamicStepInput* phase=nullptr;
 std::function<Outcome<ActorPresenceRead>(const StableId&,Integer,const StableId&)> presence_before;
 TaskMessageReturnSubmitterMode submitter_mode=TaskMessageReturnSubmitterMode::unavailable;
 std::function<Outcome<StableId>(Integer,const StableId&)> player_before;
};
// 只向实际承运人/发回书人/已实际收到回书的原sender投影；到城或远方Task状态不授知情。
struct TaskMessageReturnKnownSource {
 WorldIdentity source_identity;
 StableId observer_actor_id,task_id,policy_id,source_fact_id,stage;
 StableId carrier_actor_id,other_actor_id,reply_item_id,known_place_id;
 Integer observed_tick=0;
};
class TaskMessageReturnRead {
public:
 virtual ~TaskMessageReturnRead()=default;
 virtual Outcome<std::optional<TaskMessageReturnKnownSource>> known_task_message_return(const StableId& observer_actor_id,const StableId& task_id)const=0;
};
}
