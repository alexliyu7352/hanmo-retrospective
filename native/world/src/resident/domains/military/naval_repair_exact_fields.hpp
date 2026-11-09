// Canonical C21 timing fields first; original source payload tails only once.
#pragma once
#include "naval_repair_exact_model.hpp"
#include "naval_repair_codec.hpp"
#include "naval_repair_charges_codec.hpp"
#include "resident/kernel/exact_struct_codec.hpp"
#include <type_traits>
namespace hanmo::resident::naval_repair_exact_fields {
template<class C>Status value(C&,ExactNavalRepairResult2&);
template<class C>Status value(C& c,StableId& v){return c.field("value",v);}
template<class C>Status value(C& c,SourceRef& v){return exact_fields::source(c,v);}
template<class C>Status value(C& c,WorldMoment& v){return exact_fields::moment(c,v);}
template<class C>Status value(C& c,ActionElapsedEntryRef& v){return exact_fields::required_ref(c,"action_ref",v.action_ref);}
template<class C>Status value(C& c,MomentOwnerSource& v){c.object("source_ref",[&](auto& x){return value(x,v.source_ref);});return c.field("source_schema",v.source_schema);}
template<class C>Status value(C& c,NavalRepairTerms& v){if constexpr(std::is_same_v<std::decay_t<C>,CodecVisitor>)return naval_repair_codec::visit_terms(c,v);else return c.foreign_record(v,[](const BoundaryValue& raw,ScopedCheckpointJsonDecode& budget){return naval_repair_codec::decode_terms(raw,budget);},naval_repair_codec::emit_terms_fields);}
template<class C>Status value(C& c,NavalRepairChargeFact& v){if constexpr(std::is_same_v<std::decay_t<C>,CodecVisitor>)return naval_repair_charges_codec::visit_fact(c,v);else return c.foreign_record(v,[](const BoundaryValue& raw,ScopedCheckpointJsonDecode& budget){return naval_repair_charges_codec::decode_fact(raw,budget);},naval_repair_charges_codec::emit_fact_fields);}
template<class C,class T>Status field(C& c,const char* name,T& v){return c.object(name,[&](auto& x){return value(x,v);});}
template<class C>Status field(C& c,const char* name,StableId& v){return c.field(name,v);}
template<class C>Status field(C& c,const char* name,Integer& v){return c.field(name,v);}
template<class C>Status field(C& c,const char* name,bool& v){return c.field(name,v);}
template<class C,class T>Status field(C& c,const char* name,std::optional<T>& v){return c.optional(name,v,[](auto& x,auto& row){return value(x,row);});}
template<class C,class T,class A>Status field(C& c,const char* name,std::vector<T,A>& v){return c.sequence(name,v,[](auto& x,auto& row){return value(x,row);});}
template<class C>Status value(C& c,ExactNavalRepairResult2& v){
 field(c,"id",v.id);field(c,"action_id",v.action_id);field(c,"actor_id",v.actor_id);field(c,"request_key",v.request_key);field(c,"reason",v.reason);field(c,"interruption_source_id",v.interruption_source_id);field(c,"success",v.success);field(c,"actual_interruption_source",v.actual_interruption_source);return c.status();
}
template<class C>Status value(C& c,NewNavalRepairTimeSource2& v){
 auto& t=v.timing;
 field(c,"start_fact_ref",t.start_fact_ref);field(c,"action_ref",t.action_ref);field(c,"started_at",t.started_at);field(c,"completion_or_interruption_at",t.completion_or_interruption_at);field(c,"elapsed_ref",t.elapsed_ref);field(c,"result_ref",t.result_ref);field(c,"charge_source_refs",t.charge_source_refs);
 field(c,"request_key",v.request_key);field(c,"original_terms",v.original_terms);field(c,"original_result",v.original_result);field(c,"original_charge",v.original_charge);field(c,"rule_version",v.rule_version);
 if(!c.status().ok)return c.status();if(v.rule_version!=2)return c.reject("UnsupportedExactNavalRepairRuleVersion",t.action_ref);return c.status();
}
}
