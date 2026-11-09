#include "resident/domains/interaction/appointments/personal_appointment_world.hpp"
#include "resident/domains/interaction/contact_inquiries/contact_inquiry_world.hpp"
#include "resident/domains/interaction/visit_appointments/visit_appointment_world.hpp"
// 只续本人已经开工的书面约会；精确原Action与结果来源不授发邀、接受或赴约权限。
#include "plan_binding.hpp"
#include "resident/kernel/world_bundle.hpp"
namespace hanmo::resident {
bool player_plan_visit_appointment_meeting_continuation(const CommonStore& common,const VisitAppointmentMeetingState& state,const ActionCoreView& action){
    if(action.kind!=visit_appointment_meeting_action_kind)return false;
    auto source=query_visit_appointment_meeting_action(common,state,action.actor_id,action.id);
    return source.ok&&source.value&&source.value->status=="active"&&source.value->actor_id==action.actor_id&&source.value->action_id==action.id&&source.value->start_tick==action.start_tick&&source.value->remaining_ticks==action.remaining_ticks&&action.remaining_ticks==1;
}
bool player_plan_visit_appointment_meeting_continuation(const WorldBundle& w,const ActionCoreView& action){
    return w.catalog->visit_appointment_invitations&&w.interaction.visit_appointment_meetings&&player_plan_visit_appointment_meeting_continuation(w.common,*w.interaction.visit_appointment_meetings,action);
}
Outcome<bool> read_player_plan_visit_appointment_meeting_continuation(const WorldBundle&w,const ActionCoreView&action){
 if(!w.catalog->visit_appointment_invitations||action.kind!=visit_appointment_meeting_action_kind)return {true,false,{}};
 if(!w.interaction.visit_appointment_meetings)return {false,{},common_failure("PlayerPlanOriginalInteractionStateRequired",action.id,ErrorKind::internal).error};
 auto source=query_visit_appointment_meeting_action_world(w,action.actor_id,action.id);if(!source.ok)return {false,{},std::move(source.error)};
 return {true,source.value&&source.value->status=="active"&&source.value->actor_id==action.actor_id&&source.value->action_id==action.id&&source.value->start_tick==action.start_tick&&source.value->remaining_ticks==action.remaining_ticks,{}};
}
bool player_plan_visit_appointment_meeting_matches(const VisitAppointmentMeetingActionSource& source,const InteractionSession& session){
    if(source.status!="active"||source.actor_id!=source.contact.actor_id)return false;const auto& c=source.contact;
    return session.id==c.session_id&&session.actor_id==c.actor_id&&session.dialogue_id==c.dialogue_id&&session.node_id==c.node_id&&session.phase=="say"&&session.pending_action_id.empty()&&!session.pending_command&&session.context.provider_actor_id==c.provider_actor_id&&session.context.role_id==c.role_id&&session.context.place_id==c.place_id&&!session.context.kind&&!session.context.encounter_id&&!session.context.input_error;
}
Outcome<std::optional<VisitAppointmentMeetingActionSource>> player_plan_visit_appointment_meeting_completion(const WorldBundle& w,const StableId& actor,const StableId& action,const ReceiptDetails& receipt,Integer tick,const StableId& result_id){
    if(!w.catalog->visit_appointment_invitations)return {true,std::nullopt,{}};
    if(!w.interaction.visit_appointment_meetings)return {false,{},common_failure("VisitAppointmentMeetingProfileRequired").error};
    return query_visit_appointment_meeting_completion_world(w,actor,action,receipt,tick,result_id);
}
}
