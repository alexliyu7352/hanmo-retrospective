// 薄接原Task报价/资源writer：本人声明只授权这一封原信，不能复制原任务开工与结酬。
#pragma once
#include "task_personal_message_contract.hpp"
namespace hanmo::resident {
// 原Task已创建Action并ReserveLots后调用；报价来自占用前真正Known读取，随后按自身实际头重验。
Status record_task_personal_message_start_in_candidate(CommonStore&,TaskPersonalMessageState&,const TaskPersonalMessageServices&,const TaskPersonalMessageQuote&,const ActionCoreView&);
// 原deliver_message完工分支使用；只准备原TransferLots收据，不自行写库存或结酬。
Outcome<TaskMessageHandoffPreparation> prepare_task_personal_message_handoff(const TaskPersonalMessageServices&,const ActionCoreView&,Integer tick);
// 原Task返回实际成功/失败后、统一release前记录；成功必须已有唯一真实handoff3。
Status record_task_personal_message_completion_in_candidate(CommonStore&,TaskPersonalMessageState&,const TaskPersonalMessageSources&,const ActionCoreView&,Integer tick,const TaskEffect&);
// 原Task完整结构/钱货先验之后使用；此门核本段业务史，实际result单独门还须随后执行。
Status validate_task_personal_message_business(const TaskPersonalMessageSources&);
// handoff3唯一roleless分支的有界结构交叉读，不调用Known/完整Task或World总验。
Status validate_task_personal_message_handoff_link(const CommonStore&,const TaskPersonalMessageState&,const TaskMessageHandoff&);
}
