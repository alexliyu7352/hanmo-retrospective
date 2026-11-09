// 冷恢复与热采纳均核已绑定行动的原参数，不让计划目标替换真正正在执行的事项。
#pragma once
#include "hanmo/resident/model_contract.hpp"
#include "resident/domains/military/naval_repair_model.hpp"
#include "resident/domains/growth/growth_recovery_completion.hpp"
#include "resident/domains/growth/growth_tactic_lesson_completion.hpp"
#include "resident/domains/growth/growth_tactic_lesson_receipts.hpp"
#include "resident/domains/growth/growth_recovery_receipts.hpp"
#include "resident/domains/interaction/appointments/personal_appointment_progress.hpp"
#include "resident/domains/interaction/contact_inquiries/contact_inquiry_progress.hpp"
#include "resident/domains/interaction/visit_appointments/visit_appointment_meeting_progress.hpp"
#include "resident/domains/family/family_child_care_completion.hpp"
#include "resident/domains/family/child_care_relief_completion.hpp"
#include "resident/domains/tasks/shop_orders/task_shop_order_progress.hpp"
#include "resident/domains/tasks/message_returns/task_message_return_progress.hpp"
#include "resident/domains/tasks/personal_messages/task_personal_message_progress.hpp"
#include "resident/domains/tasks/personal_reports/task_personal_report_progress.hpp"
#include "resident/domains/tasks/personal_acceptances/task_personal_acceptance_progress.hpp"
namespace hanmo::resident {
struct WorldBundle;
struct PlayerPlanStep;
struct ActionCoreView;
struct ReceiptDetails;
class CommonStore;
struct NavalState;
struct NavalCatalog;
// 舟行失败只取真实本轮/原命令终止来源；无该行动来源返回空，不以行动缺失推断完成。
Outcome<std::optional<StableId>> player_plan_naval_termination_reason(const WorldBundle&,const StableId& actor_id,const StableId& action_id,const ReceiptDetails&,Integer tick);
// 实际Naval5舟行失能只能由原半日step证明；完整Naval历史在外层先验。
Outcome<std::optional<StableId>> player_plan_naval_navigation_termination_reason(const CommonStore&,const NavalState&,const NavalCatalog&,const StableId& actor_id,const StableId& action_id,const ReceiptDetails&,Integer tick);
Outcome<std::optional<StableId>> player_plan_naval_navigation_termination_reason(const WorldBundle&,const StableId& actor_id,const StableId& action_id,const ReceiptDetails&,Integer tick);
// 陆战只沿原Advance失败完成追实际伤亡，不把普通到达或舟战收据套用。
Outcome<std::optional<StableId>> player_plan_military_termination_reason(const WorldBundle&,const StableId& actor_id,const StableId& action_id,const ReceiptDetails&,Integer tick);
// 继续仅接受同一原修缮工单；原半日仍独立核工期、人手、材料与接敌。
bool player_plan_naval_repair_continuation(const WorldBundle&,const ActionCoreView&);
// 完成绑定独立结果事实，不把Common命令result_id当工单结果，也不按删除Action猜成功。
Outcome<std::optional<NavalRepairReceipt>> player_plan_naval_repair_completion(const WorldBundle&,const StableId& actor_id,const StableId& action_id,const ReceiptDetails&,Integer tick);
// 无World递归的同源窄门，供必要坏回执组件直接验证；不代替完整Naval历史准入。
Outcome<std::optional<NavalRepairReceipt>> player_plan_naval_repair_completion(const CommonStore&,const NavalState&,const NavalCatalog&,const StableId& actor_id,const StableId& action_id,const ReceiptDetails&,Integer tick);
// 已接受照护只能继续原Action；终态绑定原照护事实及真实回执，零时取消命令成功不等于照护成功。
bool player_plan_child_care_continuation(const WorldBundle&,const ActionCoreView&);
Outcome<std::optional<ChildCareCompletionSource>> player_plan_child_care_completion(const CommonStore&,const FamilyBirthState&,const StableId& actor_id,const StableId& action_id,const ReceiptDetails&,Integer tick);
Outcome<std::optional<ChildCareCompletionSource>> player_plan_child_care_completion(const WorldBundle&,const StableId& actor_id,const StableId& action_id,const ReceiptDetails&,Integer tick);
// 志愿接手仅续原提供者Action；完成消费本合同私粮，不替原出生合同解绑。
bool player_plan_child_care_relief_continuation(const WorldBundle&,const ActionCoreView&);
Outcome<std::optional<ChildCareReliefCompletionSource>> player_plan_child_care_relief_completion(const CommonStore&,const ChildCareReliefState&,const StableId& actor_id,const StableId& action_id,const ReceiptDetails&,Integer tick);
Outcome<std::optional<ChildCareReliefCompletionSource>> player_plan_child_care_relief_completion(const WorldBundle&,const StableId& actor_id,const StableId& action_id,const ReceiptDetails&,Integer tick);
// 仅本人已经实际开工的店单交付可继续；不自动接受店单或代客户确认。
bool player_plan_shop_order_continuation(const WorldBundle&,const ActionCoreView&);
Outcome<std::optional<TaskShopOrderActionSource>> player_plan_shop_order_completion(const WorldBundle&,const StableId& actor_id,const StableId& action_id,const ReceiptDetails&,Integer tick);
// 本人已付款且实际开工的康复服务只能继续原Action；闭合来源须先经完整身体/恢复历史。
bool player_plan_recovery_continuation(const WorldBundle&,const ActionCoreView&);
Outcome<std::optional<GrowthRecoveryCompletion>> player_plan_recovery_completion(const CommonStore&,const GrowthState&,const GrowthRecoveryState&,const StableId& actor_id,const StableId& action_id,const ReceiptDetails&,Integer tick);
Outcome<std::optional<GrowthRecoveryCompletion>> player_plan_recovery_completion(const WorldBundle&,const StableId& actor_id,const StableId& action_id,const CommitResult& actual_result);
// 私人约会只续本人已开工的一半日，精确来源不授受约或赴约开始权限。
bool player_plan_appointment_continuation(const WorldBundle&,const ActionCoreView&);
// 真原来源失败向上传播，不折成普通不可继续。
Outcome<bool> read_player_plan_appointment_continuation(const WorldBundle&,const ActionCoreView&);
Outcome<std::optional<PersonalAppointmentActionSource>> player_plan_appointment_completion(const WorldBundle&,const StableId& actor_id,const StableId& action_id,const ReceiptDetails&,Integer tick,const StableId& actual_result_id={});
// 当面询问只继续已开工的本人一半日；提供者或新题目不授自动开始权限。
bool player_plan_contact_inquiry_continuation(const CommonStore&,const ContactInquiryState&,const ActionCoreView&);
bool player_plan_contact_inquiry_continuation(const WorldBundle&,const ActionCoreView&);
// 真原来源失败向上传播，不折成普通不可继续。
Outcome<bool> read_player_plan_contact_inquiry_continuation(const WorldBundle&,const ActionCoreView&);
// 同原say上下文是唯一交互例外；不提前读取或投影回答。
bool player_plan_contact_inquiry_meeting_matches(const ContactInquiryActionSource&,const InteractionSession&);
// 精确实际result绑定先于计划终态；缺Action或删reason不能退成旅行成功。
Outcome<std::optional<ContactInquiryActionSource>> player_plan_contact_inquiry_completion(const WorldBundle&,const StableId& actor_id,const StableId& action_id,const ReceiptDetails&,Integer tick,const StableId& actual_result_id);
// 书面承诺先形成真实本人会面Action，才允许Continue及同原say的有限例外。
bool player_plan_visit_appointment_meeting_continuation(const CommonStore&,const VisitAppointmentMeetingState&,const ActionCoreView&);
bool player_plan_visit_appointment_meeting_continuation(const WorldBundle&,const ActionCoreView&);
// 真原来源失败向上传播，不折成普通不可继续。
Outcome<bool> read_player_plan_visit_appointment_meeting_continuation(const WorldBundle&,const ActionCoreView&);
bool player_plan_visit_appointment_meeting_matches(const VisitAppointmentMeetingActionSource&,const InteractionSession&);
Outcome<std::optional<VisitAppointmentMeetingActionSource>> player_plan_visit_appointment_meeting_completion(const WorldBundle&,const StableId& actor_id,const StableId& action_id,const ReceiptDetails&,Integer tick,const StableId& actual_result_id);
// 私信只续本人已开始的原交信Action；不自动交信、返报或旅行。
bool player_plan_personal_message_action(const WorldBundle&,const StableId& action_id);
bool player_plan_personal_message_continuation(const CommonStore&,const TaskState&,const TaskPersonalMessageState&,const ActionCoreView&);
bool player_plan_personal_message_continuation(const WorldBundle&,const ActionCoreView&);
// 本人恰一原SAY才有确定续办；缺会面、重复或另一个本人会话均停住。
bool player_plan_personal_message_meetings_match(const TaskPersonalMessageActionSource&,const std::vector<InteractionSession>&);
// 只消费原提交尾已绑定的本次真实结果，六种原推进形状均须精确来源。
Outcome<std::optional<TaskPersonalMessageActionSource>> player_plan_personal_message_completion(const WorldBundle&,const StableId& actor_id,const StableId& action_id,const CommitResult& actual_result);
// 冷计划终态核本人、原观察时点、实际结果及业务失败；供局部坏档门复用。
Status validate_player_plan_personal_message_completion(const CommonStore&,const TaskState&,const TaskPersonalMessageState&,const StableId& actor_id,const PlayerPlanStep&,const CommitResult&);
// 私人返报只续本人已开始的原返报Action；不自动返报、返报或旅行。
bool player_plan_personal_report_action(const WorldBundle&,const StableId& action_id);
bool player_plan_personal_report_continuation(const CommonStore&,const TaskState&,const TaskPersonalReportState&,const ActionCoreView&);
bool player_plan_personal_report_continuation(const WorldBundle&,const ActionCoreView&);
// 本人恰一原SAY才有确定续办；缺会面、重复或另一个本人会话均停住。
bool player_plan_personal_report_meetings_match(const TaskPersonalReportActionSource&,const std::vector<InteractionSession>&);
// 只消费原提交尾已绑定的本次真实结果，六种原推进形状均须精确来源。
Outcome<std::optional<TaskPersonalReportActionSource>> player_plan_personal_report_completion(const WorldBundle&,const StableId& actor_id,const StableId& action_id,const CommitResult& actual_result);
// 冷计划终态核本人、原观察时点、实际结果及业务失败；供局部坏档门复用。
Status validate_player_plan_personal_report_completion(const CommonStore&,const TaskState&,const TaskPersonalReportState&,const StableId& actor_id,const PlayerPlanStep&,const CommitResult&);
// 原受领Continue只消费本人既存Action和实际提交结果。
bool player_plan_personal_acceptance_action(const WorldBundle&,const StableId& action_id);
bool player_plan_personal_acceptance_continuation(const CommonStore&,const TaskState&,const TaskPersonalAcceptanceState&,const ActionCoreView&);
bool player_plan_personal_acceptance_continuation(const WorldBundle&,const ActionCoreView&);
// 本人恰一原SAY才有确定续办；缺会面、重复或另一个本人会话均停住。
bool player_plan_personal_acceptance_meetings_match(const TaskPersonalAcceptanceActionSource&,const std::vector<InteractionSession>&);
// 只消费原提交尾已绑定的本次真实结果，六种原推进形状均须精确来源。
Outcome<std::optional<TaskPersonalAcceptanceActionSource>> player_plan_personal_acceptance_completion(const WorldBundle&,const StableId& actor_id,const StableId& action_id,const CommitResult& actual_result);
// 冷计划终态核本人、原观察时点、实际结果及业务失败；供局部坏档门复用。
Status validate_player_plan_personal_acceptance_completion(const CommonStore&,const TaskState&,const TaskPersonalAcceptanceState&,const StableId& actor_id,const PlayerPlanStep&,const CommitResult&);
// 回书只续本人已经开工的一半日；实际接取来源及原SAY上下文不授权开始新任务。
bool player_plan_message_return_continuation(const CommonStore&,const TaskMessageReturnState&,const ActionCoreView&);
bool player_plan_message_return_continuation(const WorldBundle&,const ActionCoreView&);
// 完成必须绑定同一实际Advance结果；原收信失败不能解释为旅行到达。
Outcome<std::optional<TaskMessageReturnActionSource>> player_plan_message_return_completion(const WorldBundle&,const StableId& actor_id,const StableId& action_id,const ReceiptDetails&,Integer tick,const StableId& actual_result_id);
// 教学唯一原Action及原教师say例外；真实提交绑定后才消费取消/推进/死亡终态。
bool player_plan_tactic_lesson_action(const WorldBundle&,const StableId& action_id);
bool player_plan_tactic_lesson_continuation(const WorldBundle&,const ActionCoreView&);
bool player_plan_tactic_lesson_meeting_matches(const GrowthTacticLessonActionSource&,const InteractionSession&);
Outcome<std::optional<GrowthTacticLessonCompletion>> player_plan_tactic_lesson_completion(const CommonStore&,const GrowthTacticLessonState&,const StableId& actor_id,const StableId& action_id,const CommitResult& actual_result);
Outcome<std::optional<GrowthTacticLessonCompletion>> player_plan_tactic_lesson_completion(const WorldBundle&,const StableId& actor_id,const StableId& action_id,const CommitResult& actual_result);
Status validate_player_plan_world(const WorldBundle&);
} // 只读同一候选；不重报当前价格或为旧行动重算路径。
