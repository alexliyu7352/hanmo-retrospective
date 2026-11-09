// 两种真正承诺在原事实序合并核冲突；旧约或新邀请只有当时已真实终止才可释放，当前空状态不倒填。
#pragma once
#include "resident/kernel/read_stamp.hpp"
#include "visit_appointment_meeting_state.hpp"
namespace hanmo::resident {
// 原当面预约、三源、实际会面及各自结果必须先完整验证；这是显式最后交叉门，不从冷load写派生账。
Status validate_visit_appointment_commitment_history(const CommonStore&,const PersonalAppointmentState&,const VisitAppointmentState&,const VisitAppointmentMeetingState&,const CurrentMomentRead* actual_clock=nullptr);
// 本人现在的邀请承诺；用于新旧两个accept写口的最后重新报价，不读远方接受状态作为inviter释放条件。
Status check_visit_appointment_current_commitment(const CommonStore&,const VisitAppointmentState&,const VisitAppointmentMeetingState*,const StableId& actor,Integer earliest,Integer latest,const StableId& excluding_invitation={},const CurrentMomentRead* actual_clock=nullptr);
}
