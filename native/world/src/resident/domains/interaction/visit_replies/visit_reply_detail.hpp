// 只读原消息/回复身份和规范ID；不会读取未送达者的当前坐标或替人作决定。
#pragma once
#include "visit_reply.hpp"
#include <charconv>
namespace hanmo::resident::visit_reply_detail {
inline const VisitReplyPolicy* policy(const VisitReplyCatalog& c,const StableId& id){for(const auto& p:c.policies)if(p.id==id)return &p;return nullptr;}
inline const VisitMessagePolicy* original(const VisitMessageCatalog& c,const StableId& id){for(const auto& p:c.policies)if(p.id==id)return &p;return nullptr;}
inline const VisitMessageTerminal* received(const VisitMessageState& s,const StableId& id){const VisitMessageTerminal* found=nullptr;for(const auto& row:s.received.rows())if(row.second.message_fact_id==id){if(found)return nullptr;found=&row.second;}return found;}
inline const VisitReplyPosted* replied(const VisitReplyState& s,const StableId& id){for(const auto& row:s.posted.rows())if(row.second.message_fact_id==id)return &row.second;return nullptr;}
inline const VisitReplyCollected* collected(const VisitReplyState& s,const StableId& id){for(const auto& row:s.collected.rows())if(row.second.reply_fact_id==id)return &row.second;return nullptr;}
inline VisitMessageContact contact(const AuthoredProviderSourceProof& p,Integer tick){const auto& m=p.meeting;return {m.actor_id,p.speaker_actor_id,m.context.place_id,m.context.role_id,m.session_id,m.dialogue_id,m.node_id,tick};}
inline std::optional<Integer> serial(const StableId& id,const StableId& prefix,Integer next){const auto head=prefix+":";if(id.rfind(head,0)!=0)return {};const auto text=id.substr(head.size());Integer number=0;auto parsed=std::from_chars(text.data(),text.data()+text.size(),number);if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size()||number<1||number>=next||std::to_string(number)!=text)return {};return number;}
}
