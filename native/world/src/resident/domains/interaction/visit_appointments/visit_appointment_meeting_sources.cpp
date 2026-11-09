// 新邀请三个本人动作均须当时真实控制来源；同tick仍以实际头双序截面，不借今日metadata。
#include "visit_appointment_meeting_sources.hpp"
#include "visit_appointment_detail.hpp"
#include "resident/domains/family/historical_player.hpp"
#include "resident/domains/family/family_legacy.hpp"
#include "resident/domains/family/voluntary_succession.hpp"
namespace hanmo::resident {namespace {
// 完整原域负责事实授权；这里只检查两个真实已分配实例与Common顺序的同一截面。
std::optional<Integer> instance(const CommonStore& c,const StableId& id){const auto colon=id.find(':');if(colon==StableId::npos)return {};return visit_appointment_detail::serial(id,id.substr(0,colon),c.state().next_instance_id);}
}
Outcome<StableId> read_visit_appointment_meeting_player_before_fact(const CommonStore& c,const FamilyState& family,const FamilyLegacyState* legacy,Integer tick,const StableId& fact){
    const auto* head=c.state().facts.find(fact);const auto boundary=head?visit_appointment_detail::serial(fact,head->kind,c.state().next_instance_id):std::optional<Integer>{};const auto& order=c.state().facts.order();const auto at=std::find(order.begin(),order.end(),fact);
    if(!head||head->id!=fact||head->domain!=visit_appointment_meeting_module||(head->kind!=visit_appointment_meeting_started_kind&&head->kind!=visit_appointment_meeting_cancelled_kind&&head->kind!=visit_appointment_meeting_finished_kind)||!boundary||at==order.end()||tick<0||tick>c.tick())return {false,{},common_failure("VisitAppointmentMeetingControlBoundaryRequired",fact).error};
    auto same_side=[&](const StableId& id,Integer changed_tick)->Status{const auto n=instance(c,id);const auto where=std::find(order.begin(),order.end(),id);if(!n||where==order.end()||*n==*boundary||((*n<*boundary)!=(where<at))||(where<at?changed_tick>tick:changed_tick<tick))return common_failure("VisitAppointmentMeetingControlOrderMismatch",id);return common_success();};
    for(const auto& id:family.successions){auto step=family_succession_step(family,id);if(!step.ok)return {false,{},step.error};auto valid=same_side(id,step.value.tick);if(!valid.ok)return {false,{},valid.error};}
    if(legacy)for(const auto& row:legacy->activations.rows()){auto valid=same_side(row.first,row.second.tick);if(!valid.ok)return {false,{},valid.error};}
    return legacy?family_player_before_fact_with_legacy(c,family,*legacy,tick,fact):family_player_before_fact(c,family,tick,fact);
}

Status validate_visit_appointment_meeting_sources(const CommonStore& c,const CommonCatalog& catalog,const VisitAppointmentMeetingState& state,const FamilyDeathOriginRead& deaths,const VisitAppointmentPlayerHistoryRead& player){
 if(!player&&(!state.started.empty()||!state.cancelled.empty()||!state.finished.empty()))return common_failure("VisitAppointmentMeetingPlayerHistoryRequired");
 auto alive=[&](const StableId& actor,Integer tick,const StableId& fact)->Status{auto source=read_actor_presence_before_fact(c,catalog,deaths,actor,tick,fact);if(!source.ok)return {false,{},source.error};return source.value.alive?common_success():common_failure("VisitAppointmentMeetingHistoricalActorUnavailable",fact);};
 auto contact=[&](const PersonalAppointmentContact& contact,const StableId& fact)->Status{for(const auto& actor:{contact.actor_id,contact.provider_actor_id}){auto valid=alive(actor,contact.tick,fact);if(!valid.ok)return valid;}auto original=player(contact.tick,fact);if(!original.ok)return {false,{},original.error};return original.value==contact.actor_id?common_success():common_failure("VisitAppointmentMeetingHistoricalPlayerMismatch",fact);};
 for(const auto& row:state.started.rows()){auto valid=contact(row.second.contact,row.first);if(!valid.ok)return valid;}for(const auto& row:state.cancelled.rows()){auto valid=contact(row.second.contact,row.first);if(!valid.ok)return valid;}
 for(const auto& row:state.finished.rows()){const auto& f=row.second;const auto* start=state.started.find(f.start_fact_id);if(!start||!f.player_receipt)return common_failure("VisitAppointmentMeetingPermanentResultRequired",row.first);auto original=player(f.tick,row.first);if(!original.ok)return {false,{},original.error};if(original.value!=f.player_receipt->request_actor_id)return common_failure("VisitAppointmentMeetingHistoricalPlayerMismatch",row.first);if(f.success)for(const auto& actor:{start->contact.actor_id,start->contact.provider_actor_id}){auto valid=alive(actor,f.tick,row.first);if(!valid.ok)return valid;}}
 return common_success();
}
}
