// 领域内部纯读工具：规范实例号、原会面来源与约期交叠；没有世界写入或第二行程表。
#pragma once
#include "personal_appointment.hpp"
#include <charconv>
namespace hanmo::resident::appointment_detail {
inline std::optional<Integer> serial(const StableId& id,const StableId& prefix,Integer next){const auto head=prefix+":";if(id.rfind(head,0)!=0)return {};const auto text=id.substr(head.size());Integer n=0;const auto parsed=std::from_chars(text.data(),text.data()+text.size(),n);if(text.empty()||parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size()||n<1||n>=next||std::to_string(n)!=text)return {};return n;}
inline PersonalAppointmentContact contact(const AuthoredProviderSourceProof& proof){const auto& p=proof.meeting;return {p.actor_id,p.provider_actor_id,p.session_id,p.context.place_id,p.context.role_id,p.dialogue_id,p.node_id,p.source_identity.tick};}
inline bool pair(const PersonalAppointmentAccepted& a,const PersonalAppointmentAccepted& b){return a.contact.actor_id==b.contact.actor_id||a.contact.actor_id==b.contact.provider_actor_id||a.contact.provider_actor_id==b.contact.actor_id||a.contact.provider_actor_id==b.contact.provider_actor_id;}
inline bool overlap(const PersonalAppointmentAccepted& a,const PersonalAppointmentAccepted& b){return a.earliest_tick<b.latest_tick+b.duration_ticks&&b.earliest_tick<a.latest_tick+a.duration_ticks;}
inline const PersonalAppointmentStarted* start(const PersonalAppointmentState& s,const StableId& id){for(const auto& row:s.started.rows())if(row.second.appointment_id==id)return &row.second;return nullptr;}
inline const PersonalAppointmentFinished* finish(const PersonalAppointmentState& s,const StableId& id){for(const auto& row:s.finished.rows())if(row.second.appointment_id==id)return &row.second;return nullptr;}
inline bool cancelled(const PersonalAppointmentState& s,const StableId& id){for(const auto& row:s.cancelled.rows())if(row.second.appointment_id==id)return true;return false;}
inline StableId status(const PersonalAppointmentState& s,const PersonalAppointmentAccepted& a,Integer tick){if(cancelled(s,a.fact_id))return "cancelled";if(const auto* f=finish(s,a.fact_id))return f->success?"fulfilled":"interrupted";if(start(s,a.fact_id))return "meeting";return tick>a.latest_tick?"missed":"accepted";}
inline bool same_session(const InteractionSession& s,const PersonalAppointmentContact& c){return s.id==c.session_id&&s.actor_id==c.actor_id&&s.dialogue_id==c.dialogue_id&&s.node_id==c.node_id&&s.phase=="say"&&s.pending_action_id.empty()&&s.context.provider_actor_id==c.provider_actor_id&&s.context.role_id==c.role_id&&s.context.place_id==c.place_id&&!s.context.kind&&!s.context.encounter_id&&!s.context.input_error;}
}
