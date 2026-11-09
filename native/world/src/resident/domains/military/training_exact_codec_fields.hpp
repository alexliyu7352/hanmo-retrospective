// S2 fixed §5.2/5.3 original training rows. This is not the complete annex.
#pragma once
#include "training_exact_model.hpp"
#include "training_codec.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/kernel/work_period_codec_fields.hpp"
#include <type_traits>
namespace hanmo::resident::training_exact_fields {
template<class C>Status value(C&,DynamicTrainingDrillInstance2&);
template<class C>Status value(C&,OriginalClosingSupplyGate2&);
template<class C>Status value(C&,TrainingParticipationSpan2&);
template<class C>Status value(C&,TrainingPeriodWitness2&);
template<class C>Status value(C&,DynamicMilitaryTrainingLesson2&);
template<class C>Status value(C&,DynamicMilitaryTrainingFact2&);
template<class C>Status value(C&,TrainingDrillFreezeSource2&);
template<class C>Status value(C&,MilitaryTrainingTerms&);
template<class C>Status value(C&,DrillState&);
template<class C>Status value(C& c,StableId& v){return c.field("value",v);}
template<class C>Status value(C& c,Integer& v){return c.field("value",v);}
template<class C>Status value(C& c,WorldMoment& v){return exact_fields::moment(c,v);}
template<class C>Status value(C& c,WorkUnits& v){return exact_fields::work_units(c,v);}
template<class C>Status value(C& c,SourceRef& v){return exact_fields::source(c,v);}
template<class C>Status value(C& c,ActionElapsedEntryRef& v){return exact_fields::required_ref(c,"action_ref",v.action_ref);}
template<class C>Status value(C& c,ActorActionWorkBinding& v){return work_fields::actor_action_binding(c,v);}
template<class C,class E>Status enumeration(C& c,E& v,Integer maximum){Integer tag=Integer(v);exact_fields::finite(c,"tag",tag,maximum);if(c.status().ok&&c.reading())v=E(tag);return c.status();}
template<class C>Status value(C& c,DynamicMilitaryTrainingPhase& v){return enumeration(c,v,1);}
template<class C>Status value(C& c,DynamicTrainingDrillPhase& v){return enumeration(c,v,2);}
template<class C,class T>Status field(C& c,const char* name,T& v){return c.object(name,[&](auto& x){return value(x,v);});}
template<class C>Status field(C& c,const char* name,StableId& v){return c.field(name,v);}
template<class C>Status field(C& c,const char* name,Integer& v){return c.field(name,v);}
template<class C>Status field(C& c,const char* name,bool& v){return c.field(name,v);}
template<class C,class T,class A>Status field(C& c,const char* name,std::vector<T,A>& v){return c.sequence(name,v,[](auto& x,auto& row){return value(x,row);});}
template<class C,class T>Status field(C& c,const char* name,std::optional<T>& v){return c.optional(name,v,[](auto& x,auto& row){return value(x,row);});}
// The accepted legacy terms and actual DrillState keep their one original
// field table and FieldOrder. Projection borrows the opened object directly.
template<class C,class T>Status original_training_record(C& c,T& v){
 if constexpr(std::is_same_v<std::decay_t<C>,CodecVisitor>)return training_codec::visit(c,v);
 else return c.foreign_record(v,[](const BoundaryValue& raw,ScopedCheckpointJsonDecode& budget)->Outcome<T>{
  if constexpr(std::is_same_v<T,MilitaryTrainingTerms>)return training_codec::decode_training_terms(raw,budget);
  else {static_assert(std::is_same_v<T,DrillState>);return training_codec::decode_drill_state(raw,budget);}
 },[](ProjectionWriter& sink,const T& original){return training_codec::emit_fields(sink,original);});
}
template<class C>Status value(C& c,MilitaryTrainingTerms& v){return original_training_record(c,v);}
template<class C>Status value(C& c,DrillState& v){return original_training_record(c,v);}
template<class C>Status value(C& c,DynamicTrainingDrillInstance2& v){
 field(c,"origin_seed",v.origin_seed);field(c,"origin_state",v.origin_state);field(c,"origin_field_order",v.origin_field_order);
 field(c,"challenge",v.challenge);field(c,"phase",v.phase);field(c,"frozen_at",v.frozen_at);field(c,"performance_bp",v.performance_bp);
 return c.status();
}
template<class C>Status value(C& c,OriginalClosingSupplyGate2& v){
 field(c,"lesson_ordinal",v.lesson_ordinal);field(c,"gate_at",v.gate_at);field(c,"selected_period_ref",v.selected_period_ref);
 field(c,"period_from_tick",v.period_from_tick);field(c,"period_to_tick",v.period_to_tick);field(c,"opening_receipt_ref",v.opening_receipt_ref);
 field(c,"supply_fact_ref",v.supply_fact_ref);field(c,"checked_terms_digest",v.checked_terms_digest);return c.status();
}
template<class C>Status value(C& c,TrainingParticipationSpan2& v){
 field(c,"action_ref",v.action_ref);field(c,"terms_source_ref",v.terms_source_ref);field(c,"from",v.from);field(c,"to",v.to);
 field(c,"actual_participant_selection_ref",v.actual_participant_selection_ref);field(c,"unit_and_occupation_source_refs",v.unit_and_occupation_source_refs);
 field(c,"eligibility_change_refs",v.eligibility_change_refs);return c.status();
}
template<class C>Status value(C& c,TrainingPeriodWitness2& v){
 field(c,"action_ref",v.action_ref);field(c,"period_ref",v.period_ref);field(c,"intersection_from",v.intersection_from);field(c,"intersection_to",v.intersection_to);
 field(c,"opening_receipt_ref",v.opening_receipt_ref);field(c,"actual_participation_span_refs",v.actual_participation_span_refs);field(c,"observed_supply_ref",v.observed_supply_ref);return c.status();
}
template<class C>Status value(C& c,DynamicMilitaryTrainingLesson2& v){
 field(c,"ordinal",v.ordinal);field(c,"service_from",v.service_from);field(c,"closed_at",v.closed_at);field(c,"gate_work_units",v.gate_work_units);
 field(c,"participation_span_refs",v.participation_span_refs);field(c,"period_witness_refs",v.period_witness_refs);field(c,"closing_supply_gate",v.closing_supply_gate);
 field(c,"fee_paid",v.fee_paid);field(c,"fee_effect_ref",v.fee_effect_ref);field(c,"training_before",v.training_before);field(c,"training_after",v.training_after);
 field(c,"fatigue_before",v.fatigue_before);field(c,"fatigue_after",v.fatigue_after);field(c,"drill_frozen_source_ref",v.drill_frozen_source_ref);field(c,"learning_result_ref",v.learning_result_ref);return c.status();
}
template<class C>Status value(C& c,DynamicMilitaryTrainingFact2& v){
 field(c,"id",v.id);field(c,"actor_id",v.actor_id);field(c,"phase",v.phase);field(c,"started_at",v.started_at);field(c,"completed_at",v.completed_at);
 field(c,"success",v.success);field(c,"terms",v.terms);field(c,"elapsed_source_ref",v.elapsed_source_ref);field(c,"work_binding_ref",v.work_binding_ref);
 field(c,"lessons",v.lessons);field(c,"terminal_code",v.terminal_code);field(c,"terminal_source_ref",v.terminal_source_ref);field(c,"drill",v.drill);field(c,"rule_version",v.rule_version);
 if(c.status().ok&&v.rule_version!=2)return c.reject("UnsupportedExactMilitaryTrainingRuleVersion",v.id);return c.status();
}
template<class C>Status value(C& c,TrainingDrillFreezeSource2& v){
 field(c,"action_ref",v.action_ref);field(c,"frozen_at",v.frozen_at);field(c,"first_lesson_ref",v.first_lesson_ref);field(c,"performance_bp",v.performance_bp);return c.status();
}
}
