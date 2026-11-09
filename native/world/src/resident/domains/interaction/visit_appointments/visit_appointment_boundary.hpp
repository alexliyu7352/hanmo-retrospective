#pragma once
#include "visit_appointment_view.hpp"
#include "visit_appointment_authored.hpp"
#include "visit_appointment_receipts.hpp"
#include "visit_appointment_meeting_receipts.hpp"
namespace hanmo::resident {
using VisitAppointmentMeetingCommand=std::variant<MeetVisitAppointment,CancelVisitAppointment>;
bool is_visit_appointment_command(const StableId&);
bool is_visit_appointment_meeting_command(const StableId&);
StableId visit_appointment_meeting_command_kind(const VisitAppointmentMeetingCommand&);
Outcome<VisitAppointmentMeetingCommand> decode_visit_appointment_meeting_command(const StableId&,const BoundaryValue&);
BoundaryValue encode_visit_appointment_quote(const VisitAppointmentQuote&);
BoundaryValue encode_visit_appointment_meeting_quote(const VisitAppointmentMeetingQuote&,bool action);
// 单一显式profile配对邀请及真实会面两段；NPC意向另需原template13/claims。
Outcome<Integer> decode_visit_appointment_profile_version(const BoundaryValue& scenario,const std::vector<StableId>& capabilities);
Outcome<bool> decode_visit_appointment_profile(const BoundaryValue& scenario,const std::vector<StableId>& capabilities);
Status validate_visit_appointment_layout(const CommonStore&,const std::vector<std::pair<StableId,StableId>>&,const VisitAppointmentCatalog*,const VisitAppointmentState*,const VisitAppointmentMeetingState*,const VisitAppointmentIntentCatalog*);
Outcome<BoundaryValue> visit_appointment_fact_payloads(const CommonStore&,const VisitAppointmentState*,const VisitAppointmentMeetingState*);
Outcome<BoundaryValue> visit_appointment_action_payloads(const CommonStore&,const VisitAppointmentMeetingState*);
Status validate_visit_appointment_json_payloads(const CommonStore&,const VisitAppointmentState*,const VisitAppointmentMeetingState*,const BoundaryValue&);
}
