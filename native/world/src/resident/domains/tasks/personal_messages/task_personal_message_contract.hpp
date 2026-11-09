// 新私人接信只缩小原Task的真实办理条件；公共Role服务、原钱货及已有回执来源不从本口借权。
#pragma once
#include "resident/domains/tasks/task_exact_read_clock.hpp"
#include "task_personal_message_catalog.hpp"
#include "task_personal_message_state.hpp"
#include "resident/domains/tasks/message_handoffs/task_message_handoff.hpp"
namespace hanmo::resident {
struct TaskPersonalMessageDeliver {StableId task_id;};
struct TaskPersonalMessageQuote {
 WorldIdentity source_identity;
 StableId task_id,policy_id,original_lot_id,from_inventory_id,to_inventory_id;
 KnownPersonalDialogueProof recipient_declaration;
 Integer duration=1;
};
enum class TaskPersonalMessageSubmitterMode {unavailable,controlled_player,legacy_actor};
struct TaskPersonalMessageSources {
 const CommonStore& common;const CommonCatalog& common_catalog;
 const TaskState& tasks;const TaskCatalog& task_catalog;
 const TaskMessageHandoffState& handoffs;const TaskPersonalMessageState& state;
 const TaskPersonalMessageCatalog& catalog;const KnownPersonalDialogueCatalog& personal_catalog;
 const InteractionCatalog& interaction_catalog;const KnownPersonalDialogueRead& personal;
 const DynamicStepInput* phase=nullptr;
 std::function<Outcome<ActorPresenceRead>(const StableId&,Integer,const StableId&)> presence_before;
 // 原World不可变目录派生外层推进权限；新接信本身另核Known本人及此业务头前的实际carrier。
 TaskPersonalMessageSubmitterMode submitter_mode=TaskPersonalMessageSubmitterMode::unavailable;
 std::function<Outcome<StableId>(Integer,const StableId&)> player_before;
 std::optional<TaskExactReadClock> exact_clock;
};
struct TaskPersonalMessageServices {
 TaskPersonalMessageSources sources;
 const InteractionState& interaction;
 const InteractionAccess& interaction_access;
};
// 完整Family/生命/Known opening先验，随后本段全史，再交资格/继产/Plan；不能从纯handoff结构反调World。
Status validate_task_personal_messages(const TaskPersonalMessageSources&);
Outcome<TaskPersonalMessageQuote> preview_task_personal_message(const TaskPersonalMessageSources&,const InteractionState&,const InteractionAccess&,const StableId& actor,const TaskPersonalMessageDeliver&);
}
