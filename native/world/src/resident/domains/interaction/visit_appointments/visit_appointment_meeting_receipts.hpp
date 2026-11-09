#pragma once
#include "visit_appointment_meeting.hpp"
namespace hanmo::resident {
struct ReceiptDetails;struct CommitResult;
Outcome<std::vector<StableId>> visit_appointment_meeting_new_result_sources(const VisitAppointmentMeetingState* before,const VisitAppointmentMeetingState* after,const StableId& actor,const StableId& kind,const ReceiptDetails&,Integer tick);
Status bind_visit_appointment_meeting_result(CommonStore&,VisitAppointmentMeetingState&,const std::vector<StableId>& sources,const StableId& actor,Integer request,const CommitResult& actual);
Status validate_visit_appointment_meeting_cached_results(const CommonStore&,const VisitAppointmentMeetingState*);
}
