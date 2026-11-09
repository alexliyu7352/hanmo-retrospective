// S2 §6.2: the two fixed thirteen-field rows and original Return/2 payload.
#pragma once
#include "military_action_time_records.hpp"
#include "military_codec.hpp"
#include "lifecycle_codec.hpp"
#include "battle_codec.hpp"
#include "resident/domains/interaction/interaction_codec.hpp"
#include "resident/domains/interaction/interaction_projection.hpp"
#include "resident/kernel/exact_struct_codec.hpp"
#include <type_traits>
namespace hanmo::resident::military_action_time_fields {
template<class C>Status value(C&,DynamicMilitaryActionFact2&);
template<class C>Status value(C&,DynamicMilitaryLogisticsFact2&);
template<class C>Status value(C&,DynamicMilitaryReturnFact2&);
template<class C>Status value(C&,MilitaryTerms&);
template<class C>Status value(C&,MilitaryActionResult&);
template<class C>Status value(C&,MilitaryLogisticsTerms&);
template<class C>Status value(C&,MilitaryLogisticsResult&);
template<class C>Status value(C&,MilitaryReturnAuthority&);
template<class C>Status value(C&,MilitaryActorChange&);
template<class C>Status value(C&,InteractionContext&);
template<class C>Status value(C& c,StableId& v){return c.field("value",v);}
template<class C>Status value(C& c,Integer& v){return c.field("value",v);}
template<class C>Status value(C& c,WorldMoment& v){return exact_fields::moment(c,v);}
template<class C>Status value(C& c,SourceRef& v){return exact_fields::source(c,v);}
template<class C>Status value(C& c,ActionElapsedEntryRef& v){return exact_fields::required_ref(c,"action_ref",v.action_ref);}
template<class C,class T>Status field(C& c,const char* name,T& v){return c.object(name,[&](auto& x){return value(x,v);});}
template<class C>Status field(C& c,const char* name,StableId& v){return c.field(name,v);}
template<class C>Status field(C& c,const char* name,Integer& v){return c.field(name,v);}
template<class C>Status field(C& c,const char* name,bool& v){return c.field(name,v);}
template<class C,class T,class A>Status field(C& c,const char* name,std::vector<T,A>& v){return c.sequence(name,v,[](auto& x,auto& row){return value(x,row);});}
template<class C,class T>Status field(C& c,const char* name,std::optional<T>& v){return c.optional(name,v,[](auto& x,auto& row){return value(x,row);});}
template<class C>Status value(C& c,MilitaryTerms& v){
 if constexpr(std::is_same_v<std::decay_t<C>,CodecVisitor>)return military_original_action_codec::visit_terms(c,v);
 else return c.foreign_record(v,[](const BoundaryValue& raw,ScopedCheckpointJsonDecode& budget){return military_original_action_codec::decode_terms(raw,budget);},military_original_action_codec::emit_terms_fields);
}
template<class C>Status value(C& c,MilitaryActionResult& v){
 if constexpr(std::is_same_v<std::decay_t<C>,CodecVisitor>)return military_original_action_codec::visit_result(c,v);
 else return c.foreign_record(v,[](const BoundaryValue& raw,ScopedCheckpointJsonDecode& budget){return military_original_action_codec::decode_result(raw,budget);},military_original_action_codec::emit_result_fields);
}
template<class C,class T>Status original_logistics_record(C& c,T& v){
 if constexpr(std::is_same_v<std::decay_t<C>,CodecVisitor>)return lifecycle_codec::visit(c,v);
 else return c.foreign_record(v,[](const BoundaryValue& raw,ScopedCheckpointJsonDecode& budget)->Outcome<T>{
  if constexpr(std::is_same_v<T,MilitaryLogisticsTerms>)return lifecycle_codec::decode_logistics_terms(raw,budget);
  else if constexpr(std::is_same_v<T,MilitaryLogisticsResult>)return lifecycle_codec::decode_logistics_result(raw,budget);
  else {static_assert(std::is_same_v<T,MilitaryReturnAuthority>);return lifecycle_codec::decode_return_authority(raw,budget);}
 },[](ProjectionWriter& w,const T& row){return lifecycle_codec::emit_fields(w,row);});
}
template<class C>Status value(C& c,MilitaryLogisticsTerms& v){return original_logistics_record(c,v);}
template<class C>Status value(C& c,MilitaryLogisticsResult& v){return original_logistics_record(c,v);}
template<class C>Status value(C& c,MilitaryReturnAuthority& v){return original_logistics_record(c,v);}
template<class C>Status value(C& c,MilitaryActorChange& v){
 if constexpr(std::is_same_v<std::decay_t<C>,CodecVisitor>)return battle_codec::visit(c,v);
 else return c.foreign_record(v,[](const BoundaryValue& raw,ScopedCheckpointJsonDecode& budget){return battle_codec::decode_actor_change(raw,budget);},[](ProjectionWriter& w,const MilitaryActorChange& row){return battle_codec::emit_fields(w,row);});
}
template<class C>Status value(C& c,InteractionContext& v){
 if constexpr(std::is_same_v<std::decay_t<C>,CodecVisitor>)return interaction_codec::visit_context(c,v);
 else return c.foreign_record(v,[](const BoundaryValue& raw,ScopedCheckpointJsonDecode& budget){return interaction_codec::decode_context(raw,budget,"exact_military_action/original_context");},emit_interaction_context_fields);
}
template<class C,class T>Status action_row(C& c,T& v){
 field(c,"id",v.id);field(c,"command",v.command);field(c,"actor_id",v.actor_id);field(c,"started_at",v.started_at);field(c,"completed_at",v.completed_at);
 field(c,"phase",v.phase);field(c,"success",v.success);field(c,"original_terms",v.original_terms);field(c,"original_context",v.original_context);
 field(c,"elapsed_source_ref",v.elapsed_source_ref);field(c,"result",v.result);field(c,"terminal_source_ref",v.terminal_source_ref);field(c,"rule_version",v.rule_version);
 if(!c.status().ok)return c.status();
 if(v.rule_version!=2)return c.reject("UnsupportedExactMilitaryActionRuleVersion",v.id);
 if(v.phase!="started"&&v.phase!="completed")return c.reject("InvalidExactMilitaryActionPhase",v.id);
 return c.status();
}
template<class C>Status value(C& c,DynamicMilitaryActionFact2& v){
 action_row(c,v);if(!c.status().ok)return c.status();
 if((v.command!="office_accept"&&v.command!="army_form")||(v.command=="office_accept"&&!std::holds_alternative<OfficeTerms>(v.original_terms))||(v.command=="army_form"&&!std::holds_alternative<ArmyFormationTerms>(v.original_terms)))return c.reject("ExactMilitaryActionTermsKindMismatch",v.id);
 return c.status();
}
template<class C>Status value(C& c,DynamicMilitaryLogisticsFact2& v){
 action_row(c,v);if(!c.status().ok)return c.status();
 if(v.command!="army_return"&&v.command!="army_resupply")return c.reject("InvalidExactMilitaryLogisticsCommand",v.id);
 if(v.original_context)return c.reject("ExactMilitaryLogisticsContextMustBeAbsent",v.id);
 return c.status();
}
template<class C>Status value(C& c,DynamicMilitaryReturnFact2& v){
 field(c,"id",v.id);field(c,"army_id",v.army_id);field(c,"order_id",v.order_id);field(c,"garrison_id",v.garrison_id);field(c,"source_garrison_id",v.source_garrison_id);field(c,"ownership_id",v.ownership_id);field(c,"actor_id",v.actor_id);
 field(c,"authority",v.authority);field(c,"completed_at",v.completed_at);
 c.object("goods",[&](auto& x){if constexpr(std::is_same_v<std::decay_t<decltype(x)>,CodecVisitor>)return lifecycle_codec::visit_return_goods(x,v.goods);else return x.foreign_record(v.goods,[](const BoundaryValue& raw,ScopedCheckpointJsonDecode& budget){return lifecycle_codec::decode_return_goods(raw,budget);},lifecycle_codec::emit_return_goods_fields);});
 field(c,"money",v.money);field(c,"unit_ids",v.unit_ids);field(c,"returned_retainers",v.returned_retainers);field(c,"lost_retainers",v.lost_retainers);field(c,"prisoner_ids",v.prisoner_ids);field(c,"actor_changes",v.actor_changes);
 field(c,"logistics_action_ref",v.logistics_action_ref);field(c,"completion_source_ref",v.completion_source_ref);return c.status();
}
}
