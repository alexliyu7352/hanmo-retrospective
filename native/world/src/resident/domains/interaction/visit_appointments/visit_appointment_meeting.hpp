#pragma once
#include "visit_appointment_meeting_state.hpp"
#include "visit_appointment.hpp"
namespace hanmo::resident {
struct VisitAppointmentMeetingTime2Context;struct ActionElapsedStore;
struct MeetVisitAppointment{StableId invitation_fact_id,acceptance_fact_id;};
struct CancelVisitAppointment{StableId invitation_fact_id,acceptance_fact_id;};
struct VisitAppointmentMeetingQuote{AuthoredProviderSourceProof declaration;StableId invitation_fact_id,acceptance_fact_id;VisitAppointmentTerms terms;std::optional<WorldMoment> actual_at;};
struct VisitAppointmentMeetingCompletion{StableId fact_id;bool success=false;StableId reason;};
// 必须先完整三源/人物/原预约；本读只处理本次真实面对面事件，不授远方当事人当前位置。
Outcome<VisitAppointmentMeetingQuote> preview_visit_appointment_meeting(const CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const PersonalAppointmentCatalog&,const VisitAppointmentState&,const VisitAppointmentMeetingState&,const StableId& actor,const MeetVisitAppointment&,const VisitAppointmentMeetingTime2Context* exact=nullptr);
Outcome<VisitAppointmentMeetingQuote> preview_visit_appointment_meeting_cancel(const CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const PersonalAppointmentCatalog&,const VisitAppointmentState&,const VisitAppointmentMeetingState&,const StableId& actor,const CancelVisitAppointment&,const VisitAppointmentMeetingTime2Context* exact=nullptr);
Outcome<StableId> start_visit_appointment_meeting_in_candidate(CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const PersonalAppointmentCatalog&,const VisitAppointmentState&,VisitAppointmentMeetingState&,const VisitAppointmentMeetingQuote&,const VisitAppointmentMeetingTime2Context* exact=nullptr,ActionElapsedStore* elapsed=nullptr);
Outcome<StableId> cancel_visit_appointment_meeting_in_candidate(CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const PersonalAppointmentCatalog&,const VisitAppointmentState&,VisitAppointmentMeetingState&,const VisitAppointmentMeetingQuote&,const VisitAppointmentMeetingTime2Context* exact=nullptr);
// 原phase先减remaining到0；真实完成后仍由统一release释放双方和Common Action。
Outcome<VisitAppointmentMeetingCompletion> finish_visit_appointment_meeting_in_candidate(CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const PersonalAppointmentCatalog&,const VisitAppointmentState&,VisitAppointmentMeetingState&,const ActionCoreView&,const DynamicStepInput&);
Status validate_visit_appointment_meetings(const CommonStore&,const InteractionState&,const InteractionCatalog&,const PersonalAppointmentCatalog&,const VisitAppointmentState&,const VisitAppointmentMeetingState&,const DynamicStepInput* phase=nullptr,const VisitAppointmentMeetingTime2Context* exact=nullptr);
const VisitAppointmentMeetingStarted* visit_appointment_meeting_start(const VisitAppointmentMeetingState&,const StableId& invitation);
const VisitAppointmentMeetingFinished* visit_appointment_meeting_finish(const VisitAppointmentMeetingState&,const StableId& invitation);
const VisitAppointmentMeetingCancelled* visit_appointment_meeting_cancellation(const VisitAppointmentMeetingState&,const StableId& invitation);
}
