// 仅本领域复用的规范实例号和真实会面等式；没有世界写入或自动回答。
#pragma once
#include "contact_inquiry.hpp"
#include <charconv>
namespace hanmo::resident::contact_inquiry_detail {
inline std::optional<Integer> serial(const StableId& id,const StableId& prefix,Integer next){const auto head=prefix+":";if(id.rfind(head,0)!=0)return {};const auto text=id.substr(head.size());Integer n=0;const auto parsed=std::from_chars(text.data(),text.data()+text.size(),n);if(text.empty()||parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size()||n<1||n>=next||std::to_string(n)!=text)return {};return n;}
inline ContactInquiryContact contact(const AuthoredProviderSourceProof& proof){const auto& p=proof.meeting;return {p.actor_id,p.provider_actor_id,p.session_id,p.context.place_id,p.context.role_id,p.dialogue_id,p.node_id,p.source_identity.tick};}
inline bool same_session(const InteractionSession& s,const ContactInquiryContact& c){return s.id==c.session_id&&s.actor_id==c.actor_id&&s.dialogue_id==c.dialogue_id&&s.node_id==c.node_id&&s.phase=="say"&&s.pending_action_id.empty()&&s.context.provider_actor_id==c.provider_actor_id&&s.context.role_id==c.role_id&&s.context.place_id==c.place_id&&!s.context.kind&&!s.context.encounter_id&&!s.context.input_error;}
inline const ContactInquiryFinished* finished(const ContactInquiryState& state,const StableId& start){for(const auto& row:state.finished.rows())if(row.second.start_fact_id==start)return &row.second;return nullptr;}
inline bool same_answer(const KnownContactLocation& a,const KnownContactLocation& b){return a.subject_actor_id==b.subject_actor_id&&a.place_id==b.place_id&&a.source_fact_id==b.source_fact_id&&a.source_kind==b.source_kind&&a.observed_tick==b.observed_tick&&a.learned_tick==b.learned_tick;}
}
