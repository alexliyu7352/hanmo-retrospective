// 三个具名输入只引用原政策或预约ID；绝不允许页面提交自选时窗、替代人物或直接完成标志。
#pragma once
#include "personal_appointment.hpp"
namespace hanmo::resident {
using PersonalAppointmentCommand=std::variant<PersonalAppointmentAccept,PersonalAppointmentCancel,PersonalAppointmentMeet>;
bool is_personal_appointment_command(const StableId&);
StableId personal_appointment_command_kind(const PersonalAppointmentCommand&);
Outcome<PersonalAppointmentCommand> decode_personal_appointment_command(const StableId&,const BoundaryValue&);
BoundaryValue encode_personal_appointment_command(const PersonalAppointmentCommand&);
BoundaryValue encode_personal_appointment_quote(const PersonalAppointmentQuote&,bool begins_action);
// 继续沿原完成DTO；具名成功码使缺整个新段时也不能把原会面完成伪装为普通无来源结果。
ActionCompletion personal_appointment_completion(const PersonalAppointmentCompletion&);
struct ReceiptDetails;
// 前后同一候选唯一新增来源，真实result生成后才能永久绑定；NPC零时命令不借玩家结果。
Outcome<std::vector<StableId>> personal_appointment_new_result_sources(const PersonalAppointmentState* before,const PersonalAppointmentState* after,const StableId& actor,const StableId& kind,const ReceiptDetails&,Integer tick);
Status bind_personal_appointment_player_result(CommonStore&,PersonalAppointmentState&,const std::vector<StableId>& sources,const StableId& actor,Integer request_sequence,const CommitResult&);
Status validate_personal_appointment_player_bindings(const CommonStore&,const PersonalAppointmentState&);
const PersonalAppointmentPlayerReceipt* personal_appointment_player_receipt(const PersonalAppointmentState&,const StableId& fact_id);
Status validate_personal_appointment_cached_results(const CommonStore&,const PersonalAppointmentState*);
}
