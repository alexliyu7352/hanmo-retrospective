// 原邀请/实领/接受保持原账；真正到场后的开工/取消/终态另有独立实际来源。
#pragma once
#include "visit_appointment_state.hpp"
#include "resident/domains/interaction/appointments/personal_appointment_state.hpp"
namespace hanmo::resident {
inline constexpr const char* visit_appointment_meeting_module="visit_appointment_meetings";
inline constexpr const char* visit_appointment_meeting_started_kind="visit_appointment_meeting_started";
inline constexpr const char* visit_appointment_meeting_cancelled_kind="visit_appointment_meeting_cancelled";
inline constexpr const char* visit_appointment_meeting_finished_kind="visit_appointment_meeting_finished";
inline constexpr const char* visit_appointment_meeting_action_kind="visit_appointment_meet";
struct VisitAppointmentMeetingStarted{StableId fact_id,invitation_fact_id,acceptance_fact_id,action_id;PersonalAppointmentContact contact;std::optional<VisitAppointmentPlayerReceipt> player_receipt;};
struct VisitAppointmentMeetingCancelled{StableId fact_id,invitation_fact_id,acceptance_fact_id;PersonalAppointmentContact contact;std::optional<VisitAppointmentPlayerReceipt> player_receipt;};
struct VisitAppointmentMeetingFinished{StableId fact_id,invitation_fact_id,start_fact_id,action_id;Integer tick=0;bool success=false;StableId reason;std::optional<VisitAppointmentPlayerReceipt> player_receipt;};
struct VisitAppointmentMeetingLifeSource{DeathRef death_ref;StableId invitation_fact_id,acceptance_fact_id;VisitAppointmentLifeRole role=VisitAppointmentLifeRole::inviter;S3LeafLifeEffect effect;};
struct VisitAppointmentMeetingState{Integer version=1;OrderedTable<VisitAppointmentMeetingStarted> started;OrderedTable<VisitAppointmentMeetingCancelled> cancelled;OrderedTable<VisitAppointmentMeetingFinished> finished;OrderedTable<VisitAppointmentMeetingLifeSource> life_sources;OrderedTable<S3EventTime> temporal_events;};
Status visit_visit_appointment_meetings_checkpoint(CodecVisitor&,VisitAppointmentMeetingState&);
BoundaryValue encode_visit_appointment_meetings(const VisitAppointmentMeetingState&);
Outcome<VisitAppointmentMeetingState> decode_visit_appointment_meetings(const BoundaryValue&);
Outcome<BoundaryValue> encode_visit_appointment_meeting_fact(const VisitAppointmentMeetingState&,const StableId&);
BoundaryValue encode_visit_appointment_meeting_action(const VisitAppointmentMeetingStarted&);
}
