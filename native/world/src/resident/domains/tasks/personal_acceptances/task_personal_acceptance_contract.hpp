// 原已知私人委托的真实sender本人声明；只缩小离岗受领入口，不复制Task预算、lot或接受writer。
#pragma once
#include "resident/domains/tasks/task_exact_read_clock.hpp"
#include "task_personal_acceptance_catalog.hpp"
#include "task_personal_acceptance_state.hpp"
#include "resident/domains/tasks/personal_messages/task_personal_message_contract.hpp"
namespace hanmo::resident {
inline constexpr const char* task_personal_acceptance_source_kind="personal_message_acceptance";
struct TaskPersonalAcceptance {StableId offer_id;};
struct TaskPersonalAcceptanceQuote {
 WorldIdentity source_identity;StableId offer_id,policy_id;
 KnownPersonalDialogueProof sender_declaration;
 TaskPersonalAcceptanceKnowledge known_offer;
 Integer duration=1,budget=0;
};
// 单向来源先完整Known/人物，再本接受史，再Q01/返报/销约；不能回调后面的Task交信全验。
struct TaskPersonalAcceptanceSources {
 const CommonStore& common;const CommonCatalog& common_catalog;
 const TaskState& tasks;const TaskCatalog& task_catalog;
 const InteractionState& interaction;const InteractionCatalog& interaction_catalog;
 const TaskPersonalAcceptanceCatalog& catalog;const TaskPersonalAcceptanceState& state;
 const TaskPersonalMessageCatalog& delivery_catalog;const KnownPersonalDialogueCatalog& personal_catalog;
 const KnownPersonalDialogueRead& personal;const DynamicStepInput* phase=nullptr;
 std::function<Outcome<ActorPresenceRead>(const StableId&,Integer,const StableId&)> presence_before;
 TaskPersonalMessageSubmitterMode submitter_mode=TaskPersonalMessageSubmitterMode::unavailable;
 // 只接受本段三个精确事实头；不得借用只解释Q01或返报事实的历史控制callback。
 std::function<Outcome<StableId>(Integer,const StableId&)> player_before;
 std::optional<TaskExactReadClock> exact_clock;
};
struct TaskPersonalAcceptanceServices {TaskPersonalAcceptanceSources sources;const InteractionAccess& interaction_access;};
Status validate_task_personal_acceptance_business(const TaskPersonalAcceptanceSources&);
Status validate_task_personal_acceptances(const TaskPersonalAcceptanceSources&);
Outcome<TaskPersonalAcceptanceQuote> preview_task_personal_acceptance(const TaskPersonalAcceptanceSources&,const InteractionAccess&,const StableId& actor,const TaskPersonalAcceptance&);
// 原start_task_action已生成accepting Task、钱包及预算预约后才紧接记源；本口不创建或拨款。
Status record_task_personal_acceptance_start_in_candidate(CommonStore&,TaskPersonalAcceptanceState&,const TaskPersonalAcceptanceServices&,const TaskPersonalAcceptanceQuote&,const ActionCoreView&);
// 钱货写入前只核同一原SAY；成功/中断仍经原Task writer与统一Action释放，不生成独立结算。
Status prepare_task_personal_acceptance_completion(const TaskPersonalAcceptanceServices&,const ActionCoreView&,Integer tick);
Status record_task_personal_acceptance_completion_in_candidate(CommonStore&,TaskPersonalAcceptanceState&,const TaskPersonalAcceptanceSources&,const ActionCoreView&,Integer tick,const TaskEffect&);
}
