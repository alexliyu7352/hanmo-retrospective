// 原零时打开实例与实际Common前界共用规范数字，不把字符串排序、今日位置当历史权源。
#pragma once
#include "born_personal_dialogue.hpp"
#include <charconv>
namespace hanmo::resident::born_personal_detail {
inline std::optional<Integer> serial(const CommonStore& c,const StableId& id,const StableId& prefix={}){const auto colon=id.find(':');if(colon==StableId::npos||(!prefix.empty()&&id.substr(0,colon)!=prefix))return {};const auto text=id.substr(colon+1);Integer n=0;auto p=std::from_chars(text.data(),text.data()+text.size(),n);if(p.ec!=std::errc{}||p.ptr!=text.data()+text.size()||n<1||n>=c.state().next_instance_id||std::to_string(n)!=text)return {};return n;}
inline bool identity(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
inline bool context(const InteractionContext& a,const InteractionContext& b){return a.place_id==b.place_id&&a.provider_actor_id==b.provider_actor_id&&a.role_id==b.role_id&&a.kind==b.kind&&a.encounter_id==b.encounter_id&&a.input_error==b.input_error;}
inline bool born_identity(const BornActorSource& a,const BornActorSource& b){return a.actor_id==b.actor_id&&a.birth_fact_id==b.birth_fact_id&&a.creation_fact_id==b.creation_fact_id&&a.prototype_id==b.prototype_id&&a.biological_parent_ids==b.biological_parent_ids&&a.birth_tick==b.birth_tick;}
inline bool is_parent(const BornActorSource& b,const StableId& actor){return std::find(b.biological_parent_ids.begin(),b.biological_parent_ids.end(),actor)!=b.biological_parent_ids.end();}
// 原interaction_occupied只查发起者；本模板须同时保实际listener/provider的已有会话。
inline bool involved(const InteractionSession& s,const StableId& actor){return s.actor_id==actor||s.context.provider_actor_id==actor;}
inline bool occupied(const InteractionState& state,const StableId& actor){for(const auto& s:state.interactions)if(involved(s,actor))return true;return false;}
inline bool narrative(const InteractionContext& c){return c.kind&&*c.kind=="narrative"&&c.role_id.empty()&&!c.encounter_id&&!c.input_error&&!c.place_id.empty()&&!c.provider_actor_id.empty();}
inline const BornPersonalDialogueOpening* opening(const BornPersonalDialogueState& state,const StableId& session){const BornPersonalDialogueOpening* result=nullptr;for(const auto& row:state.openings.rows())if(row.second.session_id==session){if(result)return nullptr;result=&row.second;}return result;}
inline const DialogueDefinition* dialogue(const InteractionCatalog& c,const StableId& id){for(const auto& d:c.dialogues)if(d.id==id)return &d;return nullptr;}
inline const DialogueNode* node(const DialogueDefinition& d,const StableId& id){for(const auto& n:d.nodes)if(n.id==id)return &n;return nullptr;}
inline bool opening_born(const BornPersonalDialogueOpening& o,const BornActorSource& b){return o.provider_actor_id==b.actor_id&&o.birth_fact_id==b.birth_fact_id&&o.creation_fact_id==b.creation_fact_id&&o.prototype_id==b.prototype_id&&b.adult&&b.alive&&is_parent(b,o.listener_actor_id);}
// 只用于实际同次打开与后继零时业务头；调用者不得把未完成Action ID当这个边界。
inline Status before(const CommonStore& c,const StableId& first,Integer first_tick,const StableId& last,Integer last_tick){const auto a=serial(c,first),b=serial(c,last);const auto& order=c.state().facts.order();auto p=std::find(order.begin(),order.end(),first),q=std::find(order.begin(),order.end(),last);if(!a||!b||*a>=*b||p==order.end()||q==order.end()||p>=q||first_tick<0||first_tick>last_tick||last_tick>c.tick())return common_failure("BornPersonalDialogueSourceOrderMismatch",first);return common_success();}
}
