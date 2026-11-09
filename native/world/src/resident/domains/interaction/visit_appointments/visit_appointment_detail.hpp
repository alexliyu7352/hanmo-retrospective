#pragma once
#include "visit_appointment.hpp"
#include "visit_appointment_commitments.hpp"
#include "resident/domains/interaction/visit_replies/visit_reply_detail.hpp"
#include <limits>
namespace hanmo::resident::visit_appointment_detail {
// 域内共同业务腿；只能由下面两种明确producer的包装器调用，不是公开World命令。
Outcome<VisitAppointmentQuote> preview_business(const CommonStore&,const VisitMessageState&,const VisitMessageCatalog&,const PersonalAppointmentCatalog&,const PersonalAppointmentState&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const VisitAppointmentCatalog&,const VisitAppointmentState&,const StableId&,const VisitAppointmentCommand&,const VisitReplyCatalog*,const VisitAppointmentMeetingState*,const VisitAppointmentLifeContext* life=nullptr);
Outcome<StableId> apply_business(CommonStore&,const VisitMessageState&,const VisitMessageCatalog&,const PersonalAppointmentCatalog&,const PersonalAppointmentState&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const VisitAppointmentCatalog&,VisitAppointmentState&,const StableId&,const VisitAppointmentCommand&,const VisitAppointmentQuote&,const VisitReplyCatalog*,const VisitAppointmentMeetingState*,const VisitAppointmentLifeContext* life=nullptr);
using visit_reply_detail::serial;
using visit_reply_detail::original;
using visit_reply_detail::received;
using visit_reply_detail::contact;
inline const VisitAppointmentOffered* offered(const VisitAppointmentState& s,const StableId& message){const VisitAppointmentOffered* result=nullptr;for(const auto& row:s.offered.rows())if(row.second.message_fact_id==message){if(result)return nullptr;result=&row.second;}return result;}
inline const VisitAppointmentCollected* collected(const VisitAppointmentState& s,const StableId& invitation){const VisitAppointmentCollected* result=nullptr;for(const auto& row:s.collected.rows())if(row.second.invitation_fact_id==invitation){if(result)return nullptr;result=&row.second;}return result;}
inline const VisitAppointmentAccepted* accepted(const VisitAppointmentState& s,const StableId& invitation){const VisitAppointmentAccepted* result=nullptr;for(const auto& row:s.accepted.rows())if(row.second.invitation_fact_id==invitation){if(result)return nullptr;result=&row.second;}return result;}
inline bool same_terms(const VisitAppointmentTerms& a,const VisitAppointmentTerms& b){return a.appointment_policy_id==b.appointment_policy_id&&a.inviter_actor_id==b.inviter_actor_id&&a.invitee_actor_id==b.invitee_actor_id&&a.place_id==b.place_id&&a.role_id==b.role_id&&a.offered_tick==b.offered_tick&&a.accept_before_tick==b.accept_before_tick&&a.earliest_tick==b.earliest_tick&&a.latest_tick==b.latest_tick&&a.duration_ticks==b.duration_ticks;}
inline Outcome<VisitAppointmentTerms> terms(const VisitAppointmentPolicy& p,const VisitMessagePolicy& m,const PersonalAppointmentPolicy& a,const InteractionCatalog& definitions,Integer tick){VisitAppointmentTerms t;const auto* role=definitions.common?definitions.common->roles.find(a.role_id):nullptr;if(!role||tick<0||__builtin_add_overflow(tick,p.accept_before_offset,&t.accept_before_tick)||__builtin_add_overflow(tick,p.earliest_offset,&t.earliest_tick)||__builtin_add_overflow(tick,p.latest_offset,&t.latest_tick)||t.latest_tick>2147483646)return {false,{},common_failure("VisitAppointmentWindowOverflow",p.id).error};t.appointment_policy_id=a.id;t.inviter_actor_id=m.recipient_actor_id;t.invitee_actor_id=m.sender_actor_id;t.place_id=role->place_id;t.role_id=a.role_id;t.offered_tick=tick;t.duration_ticks=a.duration_ticks;return {true,std::move(t),{}};}
inline bool overlap(Integer a,Integer b,Integer c,Integer d){return a<=d&&c<=b;}
inline bool owns(const PersonalAppointmentAccepted& a,const StableId& actor){return a.contact.actor_id==actor||a.contact.provider_actor_id==actor;}
inline bool released(const PersonalAppointmentState& s,const StableId& id){for(const auto& row:s.cancelled.rows())if(row.second.appointment_id==id)return true;for(const auto& row:s.finished.rows())if(row.second.appointment_id==id)return true;return false;}
// 只查本人承诺；invitee尚未答受前不是其承诺，不因另一端未知状态释放inviter自己的原承诺。
inline Status conflict(const CommonStore& common,const PersonalAppointmentState& old,const VisitAppointmentState& state,const VisitAppointmentMeetingState* meetings,const StableId& actor,const VisitAppointmentTerms& t,const StableId& excluding={},const CurrentMomentRead* clock=nullptr){for(const auto& row:old.accepted.rows())if(owns(row.second,actor)&&!released(old,row.first)&&overlap(t.earliest_tick,t.latest_tick,row.second.earliest_tick,row.second.latest_tick))return common_failure("VisitAppointmentCommitmentConflict",row.first);return check_visit_appointment_current_commitment(common,state,meetings,actor,t.earliest_tick,t.latest_tick,excluding,clock);}
}
