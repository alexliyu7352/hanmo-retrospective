// 独立Known发信人返报。所有来源只读原已受领Task与原实交，不重新发信或生成第二份结酬。
#pragma once
#include "task_personal_report_catalog.hpp"
#include "task_personal_report_state.hpp"
#include "resident/domains/tasks/personal_messages/task_personal_message_contract.hpp"
#include "resident/domains/tasks/message_handoffs/task_message_report.hpp"
namespace hanmo::resident {
struct TaskPersonalReport {StableId task_id;};
struct TaskPersonalReportQuote {
 WorldIdentity source_identity;
 StableId task_id,policy_id,handoff_fact_id;
 KnownPersonalDialogueProof sender_declaration;
 Integer duration=1,reward=0;
};
// 持值复制引用集合，World持者须活到实际报价/结算完成；phase与控制模式沿同一个原Q01候选。
struct TaskPersonalReportSources {
 TaskPersonalMessageSources delivery;
 const TaskPersonalReportCatalog& catalog;
 const TaskPersonalReportState& state;
 // 返报自己的actual事实边界；原delivery.player_before始终只解释Q01。
 std::function<Outcome<StableId>(Integer,const StableId&)> player_before;
};
struct TaskPersonalReportServices {
 TaskPersonalReportSources sources;
 const InteractionState& interaction;
 const InteractionAccess& interaction_access;
};
// 完整Known/人物/原送达先验后才核本段；原report总结构可链接本段，但不能反调World。
Status validate_task_personal_reports(const TaskPersonalReportSources&);
Outcome<TaskPersonalReportQuote> preview_task_personal_report(const TaskPersonalReportSources&,const InteractionState&,const InteractionAccess&,const StableId& actor,const TaskPersonalReport&);
// 原start_task_action已写report_offer和原reward预约后紧接记录，不能从本口直接拨款。
Status record_task_personal_report_start_in_candidate(CommonStore&,TaskPersonalReportState&,const TaskPersonalReportServices&,const TaskPersonalReportQuote&,const ActionCoreView&);
// 完成时再次核同一发信人SAY，准备原report载荷；实际支付和report写入仍由原Task recipe负责。
Outcome<TaskMessageReportPreparation> prepare_task_personal_report_completion(const TaskPersonalReportServices&,const ActionCoreView&,Integer tick);
Status record_task_personal_report_completion_in_candidate(CommonStore&,TaskPersonalReportState&,const TaskPersonalReportSources&,const ActionCoreView&,Integer tick,const TaskEffect&);
}
