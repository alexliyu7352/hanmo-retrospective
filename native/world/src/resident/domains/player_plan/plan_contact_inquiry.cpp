#include "resident/domains/interaction/appointments/personal_appointment_world.hpp"
#include "resident/domains/interaction/contact_inquiries/contact_inquiry_world.hpp"
#include "resident/domains/interaction/visit_appointments/visit_appointment_world.hpp"
// 只借原询问永久账与实际提交结果；不重新询问、不提前透露知情，也不递归验证World。
#include "plan_binding.hpp"
#include "resident/kernel/world_bundle.hpp"
namespace hanmo::resident {
bool player_plan_contact_inquiry_continuation(const CommonStore& common,const ContactInquiryState& state,const ActionCoreView& action){
    if(action.kind!=contact_inquiry_action_kind)return false;
    auto source=query_contact_inquiry_action(common,state,action.actor_id,action.id);
    return source.ok&&source.value&&source.value->status=="active"&&source.value->actor_id==action.actor_id&&source.value->action_id==action.id&&source.value->start_tick==action.start_tick&&source.value->remaining_ticks==action.remaining_ticks&&action.remaining_ticks==1;
}
bool player_plan_contact_inquiry_continuation(const WorldBundle& w,const ActionCoreView& action){
    return w.catalog->contact_inquiries&&w.interaction.contact_inquiries&&player_plan_contact_inquiry_continuation(w.common,*w.interaction.contact_inquiries,action);
}
Outcome<bool> read_player_plan_contact_inquiry_continuation(const WorldBundle&w,const ActionCoreView&action){
 if(!w.catalog->contact_inquiries||action.kind!=contact_inquiry_action_kind)return {true,false,{}};
 if(!w.interaction.contact_inquiries)return {false,{},common_failure("PlayerPlanOriginalInteractionStateRequired",action.id,ErrorKind::internal).error};
 auto source=query_contact_inquiry_action_world(w,action.actor_id,action.id);if(!source.ok)return {false,{},std::move(source.error)};
 return {true,source.value&&source.value->status=="active"&&source.value->actor_id==action.actor_id&&source.value->action_id==action.id&&source.value->start_tick==action.start_tick&&source.value->remaining_ticks==action.remaining_ticks,{}};
}
bool player_plan_contact_inquiry_meeting_matches(const ContactInquiryActionSource& source,const InteractionSession& session){
    if(source.status!="active"||source.actor_id!=source.contact.actor_id)return false;const auto& c=source.contact;
    return session.id==c.session_id&&session.actor_id==c.actor_id&&session.dialogue_id==c.dialogue_id&&session.node_id==c.node_id&&session.phase=="say"&&session.pending_action_id.empty()&&!session.pending_command&&session.context.provider_actor_id==c.provider_actor_id&&session.context.role_id==c.role_id&&session.context.place_id==c.place_id&&!session.context.kind&&!session.context.encounter_id&&!session.context.input_error;
}
Outcome<std::optional<ContactInquiryActionSource>> player_plan_contact_inquiry_completion(const WorldBundle& w,const StableId& actor,const StableId& action,const ReceiptDetails& receipt,Integer tick,const StableId& result_id){
    if(!w.catalog->contact_inquiries)return {true,std::nullopt,{}};
    if(!w.interaction.contact_inquiries)return {false,{},common_failure("ContactInquiryProfileRequired").error};
    return query_contact_inquiry_completion_world(w,actor,action,receipt,tick,result_id);
}
}
