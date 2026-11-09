#pragma once
#include "visit_appointment_access.hpp"
#include "visit_appointment_meeting.hpp"
namespace hanmo::resident {
struct VisitAppointmentMeetingTime2Context;
// 当前本人有限页；未实际领回的邀请只在原custodian真实声明处出现领回动作，不露时间/地点正文。
Outcome<BoundaryValue> query_visit_appointments(const CommonStore&,const VisitMessageState&,const VisitMessageCatalog&,const PersonalAppointmentCatalog&,const PersonalAppointmentState&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const VisitAppointmentCatalog&,const VisitAppointmentState&,const VisitAppointmentMeetingState&,Integer cursor=0,Integer offer_cursor=0,const VisitReplyCatalog* ordinary=nullptr,const VisitAppointmentMeetingTime2Context* exact=nullptr);
}
