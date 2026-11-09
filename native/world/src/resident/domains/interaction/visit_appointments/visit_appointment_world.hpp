// 所有临时服务仅借同候选；两独立段仍在原完整Interaction保存集之内。
#pragma once
#include "visit_appointment_boundary.hpp"
#include "visit_appointment_director.hpp"
#include "visit_appointment_meeting_sources.hpp"
#include "visit_appointment_meeting_progress.hpp"
namespace hanmo::resident {
struct WorldBundle;struct CompleteExactWorldReadBindings;
struct VisitAppointmentWorldResult{bool action_started=false;StableId source_id;};
Status validate_visit_appointment_world(const WorldBundle&);
Outcome<VisitAppointmentQuote> prepare_visit_appointment_world(const WorldBundle&,const StableId&,const VisitAppointmentCommand&);
Outcome<InteractionEffect> commit_visit_appointment_world(WorldBundle&,const StableId&,const VisitAppointmentCommand&,const VisitAppointmentQuote&);
Outcome<AuthoredVisitAppointmentQuote> prepare_authored_visit_appointment_world(const WorldBundle&,const StableId&,const AuthoredVisitAppointment&,const VisitAppointmentMeetingTime2Context* actual=nullptr,const CompleteExactWorldReadBindings* actual_world=nullptr);
Outcome<InteractionEffect> commit_authored_visit_appointment_world(WorldBundle&,const StableId&,const AuthoredVisitAppointment&,const AuthoredVisitAppointmentQuote&,const VisitAppointmentMeetingTime2Context* actual=nullptr,const CompleteExactWorldReadBindings* actual_world=nullptr);
Outcome<VisitAppointmentMeetingQuote> prepare_visit_appointment_meeting_world(const WorldBundle&,const StableId&,const VisitAppointmentMeetingCommand&);
Outcome<VisitAppointmentWorldResult> commit_visit_appointment_meeting_world(WorldBundle&,const StableId&,const VisitAppointmentMeetingCommand&,const VisitAppointmentMeetingQuote&);
Outcome<VisitAppointmentMeetingCompletion> step_visit_appointment_meeting_world(WorldBundle&,const ActionCoreView&,const DynamicStepInput&);
// 原当面预约的preview/commit两口都须加本门；仅查两人已有原邀约承诺。
Status check_visit_appointment_personal_accept_world(const WorldBundle&,const PersonalAppointmentQuote&);
Outcome<BoundaryValue> query_visit_appointment_world(const WorldBundle&,Integer cursor=0,Integer offer_cursor=0);
Outcome<StableId> visit_appointment_new_result_world(const WorldBundle&,const WorldBundle&,const StableId&,const StableId&,const ReceiptDetails&);
Outcome<std::vector<StableId>> visit_appointment_meeting_new_result_world(const WorldBundle&,const WorldBundle&,const StableId&,const StableId&,const ReceiptDetails&);
Status bind_visit_appointment_result_world(WorldBundle&,const StableId&,const std::vector<StableId>&,const StableId&,Integer,const CommitResult&,const VisitAppointmentMeetingTime2Context* actual=nullptr);
Outcome<std::optional<VisitAppointmentMeetingActionSource>> query_visit_appointment_meeting_action_world(const WorldBundle&,const StableId& actor,const StableId& action);
Outcome<std::optional<VisitAppointmentMeetingActionSource>> query_visit_appointment_meeting_completion_world(const WorldBundle&,const StableId& actor,const StableId& action,const ReceiptDetails&,Integer completed_tick,const StableId& actual_result_id);

Outcome<MomentOwnerSource> read_visit_appointment_director_command_effect_world(const WorldBundle& before,const WorldBundle& after,const StableId& actor,const ReceiptDetails&,const VisitAppointmentMeetingTime2Context* actual=nullptr);
Status validate_visit_appointment_director_actor_effect_world(const WorldBundle&,const StableId& actor,const SourceRef&,const VisitAppointmentMeetingTime2Context* actual=nullptr);
Status finalize_visit_appointment_director_actor_result_world(const WorldBundle&,const DirectorAdoptionResult2&,const VisitAppointmentMeetingTime2Context* actual=nullptr);
}
