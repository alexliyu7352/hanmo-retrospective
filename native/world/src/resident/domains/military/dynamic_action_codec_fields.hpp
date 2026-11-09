// One finite field table for original S2 actions and authored timings.
#pragma once
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "dynamic_action.hpp"
#include "resident/kernel/exact_struct_codec.hpp"
#include "resident/storage/common_checkpoint.hpp"
namespace hanmo::resident::dynamic_action_codec {
template<class C>Status value(C&, MilitaryTacticalWorkBinding&);
template<class C>Status value(C&, MilitaryActionStatusDurations2&);
template<class C>Status value(C&, MilitaryActionTimingDefinition2&);
template<class C>Status value(C&, MilitaryTacticalServiceSpan2&);
template<class C>Status value(C&, MilitaryTacticalService2&);
template<class C>Status value(C&, DynamicTacticalPreparing&);
template<class C>Status value(C&, DynamicTacticalRecovering&);
template<class C>Status value(C&, DynamicTacticalCompleted&);
template<class C>Status value(C&, DynamicTacticalInterrupted&);
template<class C>Status value(C&, DynamicTacticalUncommitted&);
template<class C>Status value(C&, DynamicTacticalReleased&);
template<class C>Status value(C&, DynamicTacticalApplied&);
template<class C>Status value(C&, DynamicMilitaryAction2&);
template<class C>Status value(C&, MilitaryOrder&);
template<class C>Status value(C&, HexPoint&);
template<class C>Status value(C&, SourceRef&);
template<class C>Status value(C&, MomentOwnerSource&);
template<class C>Status value(C&, MomentSpan&);
template<class C>Status value(C&, MilitaryTacticalWorkStage&);
template<class C>Status value(C&, DynamicMilitaryActionKind&);
template<class C>Status value(C&, DynamicMilitaryCommitKind&);
template<class C>Status value(C&, DynamicMilitaryDamageLeaf&);
template<class C>Status value(C&, WorldMoment&);
template<class C>Status value(C&, WorkUnits&);
template<class C>Status value(C&, HalfDayDuration&);
template<class C>Status value(C& c,StableId& v){return c.field("value",v);}
template<class C,class T>Status value(C&,std::optional<T>&);
template<class C,class...T>Status value(C&,std::variant<T...>&);
template<class C,class T>Status field(C& c,const char* name,T& v){return c.object(name,[&](auto& x){return value(x,v);});}
template<class C>Status field(C& c,const char* name,StableId& v){return c.field(name,v);}
template<class C>Status field(C& c,const char* name,Integer& v){return c.field(name,v);}
template<class C,class T>Status field(C& c,const char* name,std::vector<T>& v){return c.sequence(name,v,[](auto& x,auto& item){return value(x,item);});}
template<class C,class T>Status field(C& c,const char* name,std::optional<T>& v){return c.optional(name,v,[](auto& x,auto& item){return value(x,item);});}
template<class C,class T>Status value(C& c,std::optional<T>& v){return field(c,"optional",v);}
template<std::size_t I=0,class C,class V>bool construct(C& c,V& v,Integer tag){if constexpr(I<std::variant_size_v<V>){if(tag==Integer(I)){if(!prepare_checkpoint_default_values<std::variant_alternative_t<I,V>>(c).ok)return true;v.template emplace<I>();return true;}return construct<I+1>(c,v,tag);}return false;}
template<class C,class...T>Status value(C& c,std::variant<T...>& v){Integer tag=Integer(v.index());c.field("tag",tag);if(c.reading()&&c.status().ok&&!construct(c,v,tag))return c.reject("UnknownDynamicTacticalActionVariant");if(c.status().ok)c.object("value",[&](auto& x){return std::visit([&](auto& item){return value(x,item);},v);});return c.status();}
template<class C>Status value(C& c,WorldMoment& v){return exact_fields::moment(c,v);}
template<class C>Status value(C& c,WorkUnits& v){return exact_fields::work_units(c,v);}
template<class C>Status value(C& c,HalfDayDuration& v){return c.object("value",[&](auto& x){return exact_fields::fraction(x,v.value);});}
template<class C>Status value(C& c,MilitaryTacticalWorkStage& v){Integer tag=Integer(v);c.field("tag",tag);if(c.status().ok&&(tag<0||tag>1))return c.reject("UnknownDynamicTacticalActionEnum");if(c.reading()&&c.status().ok)v=MilitaryTacticalWorkStage(tag);return c.status();}
template<class C>Status value(C& c,DynamicMilitaryActionKind& v){Integer tag=Integer(v);c.field("tag",tag);if(c.status().ok&&(tag<0||tag>6))return c.reject("UnknownDynamicTacticalActionEnum");if(c.reading()&&c.status().ok)v=DynamicMilitaryActionKind(tag);return c.status();}
template<class C>Status value(C& c,DynamicMilitaryCommitKind& v){Integer tag=Integer(v);c.field("tag",tag);if(c.status().ok&&(tag<0||tag>1))return c.reject("UnknownDynamicTacticalActionEnum");if(c.reading()&&c.status().ok)v=DynamicMilitaryCommitKind(tag);return c.status();}
template<class C>Status value(C& c,DynamicMilitaryDamageLeaf& v){Integer tag=Integer(v);c.field("tag",tag);if(c.status().ok&&(tag<0||tag>1))return c.reject("UnknownDynamicTacticalActionEnum");if(c.reading()&&c.status().ok)v=DynamicMilitaryDamageLeaf(tag);return c.status();}
template<class C>Status value(C& c,MilitaryTacticalWorkBinding& v){field(c,"session_ref",v.session_ref);field(c,"generation",v.generation);field(c,"action_ref",v.action_ref);field(c,"stage",v.stage);return c.status();}
template<class C>Status value(C& c,MilitaryActionStatusDurations2& v){field(c,"inspired",v.inspired);field(c,"suppressed",v.suppressed);field(c,"hindered",v.hindered);field(c,"exposed",v.exposed);return c.status();}
template<class C>Status value(C& c,MilitaryActionTimingDefinition2& v){field(c,"id",v.id);field(c,"version",v.version);field(c,"action_kind",v.action_kind);field(c,"prepare_duration",v.prepare_duration);field(c,"recovery_duration",v.recovery_duration);field(c,"commit_kind",v.commit_kind);field(c,"flight_duration",v.flight_duration);field(c,"posture_duration",v.posture_duration);field(c,"status_durations",v.status_durations);field(c,"original_troop_or_weapon_ref",v.original_troop_or_weapon_ref);field(c,"original_tactic_ref",v.original_tactic_ref);field(c,"original_damage_leaf",v.original_damage_leaf);field(c,"geometry_ref",v.geometry_ref);field(c,"resource_basis_ref",v.resource_basis_ref);field(c,"objective_definition_ref",v.objective_definition_ref);return c.status();}
template<class C>Status value(C& c,MilitaryTacticalServiceSpan2& v){field(c,"span",v.span);field(c,"participation_sources",v.participation_sources);return c.status();}
template<class C>Status value(C& c,MilitaryTacticalService2& v){field(c,"work_binding_ref",v.work_binding_ref);field(c,"started_at",v.started_at);field(c,"last_settled_at",v.last_settled_at);field(c,"required_units",v.required_units);field(c,"credited_units",v.credited_units);field(c,"actual_service",v.actual_service);field(c,"closed_by_source",v.closed_by_source);return c.status();}
template<class C>Status value(C& c,DynamicTacticalPreparing& v){return c.status();}
template<class C>Status value(C& c,DynamicTacticalRecovering& v){field(c,"effect_source_ref",v.effect_source_ref);return c.status();}
template<class C>Status value(C& c,DynamicTacticalCompleted& v){field(c,"result_source_ref",v.result_source_ref);field(c,"completed_at",v.completed_at);return c.status();}
template<class C>Status value(C& c,DynamicTacticalInterrupted& v){field(c,"actual_cause",v.actual_cause);field(c,"interrupted_at",v.interrupted_at);return c.status();}
template<class C>Status value(C& c,DynamicTacticalUncommitted& v){return c.status();}
template<class C>Status value(C& c,DynamicTacticalReleased& v){field(c,"release_source_ref",v.release_source_ref);field(c,"released_at",v.released_at);field(c,"impact_due_at",v.impact_due_at);field(c,"impact_source_ref",v.impact_source_ref);return c.status();}
template<class C>Status value(C& c,DynamicTacticalApplied& v){field(c,"effect_source_ref",v.effect_source_ref);field(c,"effective_at",v.effective_at);return c.status();}
template<class C>Status value(C& c,DynamicMilitaryAction2& v){field(c,"id",v.id);field(c,"session_ref",v.session_ref);field(c,"generation",v.generation);field(c,"unit_ref",v.unit_ref);field(c,"accepted_intent_source_ref",v.accepted_intent_source_ref);field(c,"accepted_at",v.accepted_at);field(c,"timing_definition_ref",v.timing_definition_ref);field(c,"timing_definition_version",v.timing_definition_version);field(c,"action_instance_ordinal",v.action_instance_ordinal);field(c,"original_order",v.original_order);field(c,"service_stages",v.service_stages);field(c,"phase",v.phase);field(c,"effect_state",v.effect_state);return c.status();}
template<class C>Status value(C& c,MilitaryOrder& v){field(c,"action",v.action);field(c,"destination",v.destination);field(c,"heading",v.heading);field(c,"target_id",v.target_id);field(c,"tactic_id",v.tactic_id);return c.status();}
template<class C>Status value(C& c,HexPoint& v){field(c,"q",v.q);field(c,"r",v.r);return c.status();}
template<class C>Status value(C& c,SourceRef& v){field(c,"domain",v.domain);field(c,"kind",v.kind);field(c,"id",v.id);return c.status();}
template<class C>Status value(C& c,MomentOwnerSource& v){field(c,"source_ref",v.source_ref);field(c,"source_schema",v.source_schema);return c.status();}
template<class C>Status value(C& c,MomentSpan& v){field(c,"from",v.from);field(c,"to",v.to);return c.status();}
}
