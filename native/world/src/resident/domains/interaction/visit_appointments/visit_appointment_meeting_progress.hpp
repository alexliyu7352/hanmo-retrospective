// Continue只消费本人已经真的开工的同一会面；书面邀请/实领/接受本身均不是Action。
#pragma once
#include "visit_appointment_meeting_state.hpp"
namespace hanmo::resident {
struct ReceiptDetails;struct VisitAppointmentMeetingTime2Context;
struct VisitAppointmentMeetingActionSource{WorldIdentity source_identity;StableId actor_id,invitation_fact_id,acceptance_fact_id,action_id,started_fact_id,finished_fact_id,status,reason;PersonalAppointmentContact contact;Integer start_tick=0,remaining_ticks=0,finished_tick=-1;bool success=false;std::optional<WorldMoment> actual_started_at,actual_finished_at;};
// 调用前完整同World三源、会面来源与结果先验；未知/别人Action保持空，本域缺来源返回错误。
Outcome<std::optional<VisitAppointmentMeetingActionSource>> query_visit_appointment_meeting_action(const CommonStore&,const VisitAppointmentMeetingState&,const StableId& actor,const StableId& action,const VisitAppointmentMeetingTime2Context* exact=nullptr);
Outcome<std::optional<VisitAppointmentMeetingActionSource>> query_visit_appointment_meeting_completion(const CommonStore&,const VisitAppointmentMeetingState&,const StableId& actor,const StableId& action,const ReceiptDetails&,Integer completed_tick,const StableId& actual_result_id,const VisitAppointmentMeetingTime2Context* exact=nullptr);
}
