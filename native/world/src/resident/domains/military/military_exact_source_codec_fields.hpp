// Fixed original source fields, shared by binary, JSON and borrowed projection.
#pragma once
#include "military_exact_sources.hpp"
#include "battle_codec.hpp"
#include "resident/kernel/exact_struct_codec.hpp"
#include <type_traits>
namespace hanmo::resident::military_exact_source_fields {
template<class C>Status value(C&,DynamicMilitaryBattleFact2&);
template<class C>Status value(C&,DynamicMilitaryCasualtyFact2&);
template<class C>Status value(C&,MilitaryMemberEffectLink&);
template<class C>Status value(C&,MilitaryBattleResult&);
template<class C>Status value(C&,MilitaryRosterReceipt&);
template<class C>Status value(C&,MilitaryActorChange&);
template<class C>Status value(C& c,StableId& v){return c.field("value",v);}
template<class C>Status value(C& c,Integer& v){return c.field("value",v);}
template<class C>Status value(C& c,WorldMoment& v){return exact_fields::moment(c,v);}
template<class C>Status value(C& c,SourceRef& v){return exact_fields::source(c,v);}
template<class C,class T>Status field(C& c,const char* name,T& v){return c.object(name,[&](auto& x){return value(x,v);});}
template<class C>Status field(C& c,const char* name,StableId& v){return c.field(name,v);}
template<class C>Status field(C& c,const char* name,Integer& v){return c.field(name,v);}
template<class C>Status field(C& c,const char* name,bool& v){return c.field(name,v);}
template<class C,class T,class A>Status field(C& c,const char* name,std::vector<T,A>& v){return c.sequence(name,v,[](auto& x,auto& row){return value(x,row);});}
template<class C,class T>Status field(C& c,const char* name,std::optional<T>& v){return c.optional(name,v,[](auto& x,auto& row){return value(x,row);});}

// These are the existing original terms/results, including their preserved
// FieldOrder and legal optional fields. Do not duplicate or flatten that table.
template<class C,class T>Status original_battle_record(C& c,T& v){
 if constexpr(std::is_same_v<std::decay_t<C>,CodecVisitor>)return battle_codec::visit(c,v);
 else return c.foreign_record(v,[](const BoundaryValue& raw)->Outcome<T>{
  try{T row;battle_codec::decode(raw,row,"dynamic_military/original_battle_record");return {true,std::move(row),{}};}
  catch(const boundary::DecodeFailure& e){return {false,{},e.error};}
 },[](ProjectionWriter& sink,const T& row){return battle_codec::emit_fields(sink,row);});
}
template<class C>Status value(C& c,MilitaryBattleResult& v){return original_battle_record(c,v);}
template<class C>Status value(C& c,MilitaryRosterReceipt& v){return original_battle_record(c,v);}
template<class C>Status value(C& c,MilitaryActorChange& v){return original_battle_record(c,v);}
template<class C>Status value(C& c,MilitaryMemberEffectLink& v){
 field(c,"actor_id",v.actor_id);field(c,"effect_index",v.effect_index);
 field(c,"purpose",v.purpose);field(c,"effect_source",v.effect_source);return c.status();
}
template<class C>Status value(C& c,DynamicMilitaryBattleFact2& v){
 field(c,"id",v.id);field(c,"session_ref",v.session_ref);
 field(c,"effective_at",v.effective_at);field(c,"started_at",v.started_at);
 field(c,"effect_group_ref",v.effect_group_ref);field(c,"army_ids",v.army_ids);
 field(c,"unit_ids",v.unit_ids);field(c,"result",v.result);
 field(c,"source_ref",v.source_ref);field(c,"contact_source_ref",v.contact_source_ref);
 field(c,"winner_side",v.winner_side);field(c,"final",v.final);return c.status();
}
template<class C>Status value(C& c,DynamicMilitaryCasualtyFact2& v){
 field(c,"id",v.id);field(c,"effective_at",v.effective_at);
 field(c,"session_ref",v.session_ref);field(c,"effect_group_ref",v.effect_group_ref);
 field(c,"original_casualty_rule_ref",v.original_casualty_rule_ref);
 field(c,"roster_receipts",v.roster_receipts);field(c,"actor_changes",v.actor_changes);
 field(c,"member_effect_links",v.member_effect_links);return c.status();
}
}
