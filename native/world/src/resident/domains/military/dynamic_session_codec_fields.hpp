// Fixed S2 section 2.5 fields. One finite field table, full uint64 ordinals.
#pragma once
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "dynamic_session.hpp"
#include "military_local_body_codec_fields.hpp"
#include "resident/kernel/exact_struct_codec.hpp"
#include "resident/storage/common_checkpoint.hpp"
namespace hanmo::resident::dynamic_session_codec {
template<class C>Status value(C&, DynamicMilitaryStreamIdentity&);
template<class C>Status value(C&, DynamicMilitaryRandomSlot&);
template<class C>Status value(C&, DynamicMilitarySessionRandom&);
template<class C>Status value(C&, DynamicMilitaryStatus&);
template<class C>Status value(C&, DynamicNoPosture&);
template<class C>Status value(C&, DynamicDefendPosture&);
template<class C>Status value(C&, DynamicLearnedGuardPosture&);
template<class C>Status value(C&, DynamicFeintPosture&);
template<class C>Status value(C&, DynamicSupportToken&);
template<class C>Status value(C&, DynamicSupportPosture&);
template<class C>Status value(C&, DynamicMilitaryBodyTacticalState&);
template<class C>Status value(C&, DynamicMilitaryUnitTacticalState&);
template<class C>Status value(C&, DynamicNotHold&);
template<class C>Status value(C&, DynamicHoldUnqualified&);
template<class C>Status value(C&, DynamicHolding&);
template<class C>Status value(C&, DynamicMilitaryEscapedUnit&);
template<class C>Status value(C&, DynamicMilitaryObjectiveCurrent&);
template<class C>Status value(C&, DynamicDecisionAwaitingTrigger&);
template<class C>Status value(C&, DynamicDecisionPending&);
template<class C>Status value(C&, DynamicDecisionCommitted&);
template<class C>Status value(C&, DynamicMilitaryDecisionCurrent&);
template<class C>Status value(C&, DynamicMilitaryActive&);
template<class C>Status value(C&, DynamicMilitaryTerminal&);
template<class C>Status value(C&, DynamicMilitarySessionCurrent2&);
template<class C>Status value(C&, DynamicMilitaryUnitEntry&);
template<class C>Status value(C&, DynamicMilitaryBodyEntry&);
template<class C>Status value(C&, DynamicSessionInitialized&);
template<class C>Status value(C&, DynamicObjectiveQualificationTransition&);
template<class C>Status value(C&, DynamicPostureClosed&);
template<class C>Status value(C&, DynamicStatusExpired&);
template<class C>Status value(C&, DynamicSessionDecisionCommitted&);
template<class C>Status value(C&, DynamicSessionTerminated&);
template<class C>Status value(C&, DynamicMilitaryRandomStage&);
template<class C>Status value(C&, DynamicMilitaryStatusKind&);
template<class C>Status value(C&, DynamicObjectiveQualification&);
template<class C>Status value(C&, DynamicObjectiveCause&);
template<class C>Status value(C&, DynamicPostureCloseReason&);
template<class C>Status value(C&, SourceRef&);
template<class C>Status value(C&, WorldMoment&);
template<class C>Status value(C&, MilitaryLocalBodyRef&);
template<class C>Status value(C&, DynamicSessionStateSource2&);
template<class C>Status value(C& c,StableId& v){return c.field("value",v);}
template<class C,class T>Status value(C&,std::vector<T>&);
template<class C,class T>Status value(C&,std::optional<T>&);
template<class C,class...T>Status value(C&,std::variant<T...>&);
template<class C,class T>Status field(C& c,const char* name,T& v){return c.object(name,[&](auto& x){return value(x,v);});}
template<class C>Status field(C& c,const char* name,StableId& v){return c.field(name,v);}
template<class C>Status field(C& c,const char* name,Integer& v){return c.field(name,v);}
template<class C>Status field(C& c,const char* name,std::uint64_t& v){return c.unsigned_field(name,v);}
template<class C,class T>Status field(C& c,const char* name,std::vector<T>& v){return c.sequence(name,v,[](auto& x,auto& item){return value(x,item);});}
template<class C,class T>Status field(C& c,const char* name,std::optional<T>& v){return c.optional(name,v,[](auto& x,auto& item){return value(x,item);});}
template<class C,class T>Status value(C& c,std::vector<T>& v){return field(c,"items",v);}
template<class C,class T>Status value(C& c,std::optional<T>& v){return field(c,"optional",v);}
template<std::size_t I=0,class C,class V>bool construct(C& c,V& v,Integer tag){if constexpr(I<std::variant_size_v<V>){if(tag==Integer(I)){if(!prepare_checkpoint_default_values<std::variant_alternative_t<I,V>>(c).ok)return true;v.template emplace<I>();return true;}return construct<I+1>(c,v,tag);}return false;}
template<class C,class...T>Status value(C& c,std::variant<T...>& v){Integer tag=Integer(v.index());c.field("tag",tag);if(c.reading()&&c.status().ok&&!construct(c,v,tag))return c.reject("UnknownDynamicSessionVariant");if(c.status().ok)c.object("value",[&](auto& x){return std::visit([&](auto& item){return value(x,item);},v);});return c.status();}
template<class C>Status value(C& c,WorldMoment& v){return exact_fields::moment(c,v);}
template<class C>Status value(C& c,SourceRef& v){field(c,"domain",v.domain);field(c,"kind",v.kind);return field(c,"id",v.id);}
template<class C>Status value(C& c,MilitaryLocalBodyRef& v){return military_local_body_codec::value(c,v);}
template<class C>Status value(C& c,DynamicMilitaryRandomStage& v){Integer tag=Integer(v);c.field("tag",tag);if(c.status().ok&&(tag<0||tag>3))return c.reject("UnknownDynamicSessionEnum");if(c.reading()&&c.status().ok)v=DynamicMilitaryRandomStage(tag);return c.status();}
template<class C>Status value(C& c,DynamicMilitaryStatusKind& v){Integer tag=Integer(v);c.field("tag",tag);if(c.status().ok&&(tag<0||tag>3))return c.reject("UnknownDynamicSessionEnum");if(c.reading()&&c.status().ok)v=DynamicMilitaryStatusKind(tag);return c.status();}
template<class C>Status value(C& c,DynamicObjectiveQualification& v){Integer tag=Integer(v);c.field("tag",tag);if(c.status().ok&&(tag<0||tag>1))return c.reject("UnknownDynamicSessionEnum");if(c.reading()&&c.status().ok)v=DynamicObjectiveQualification(tag);return c.status();}
template<class C>Status value(C& c,DynamicObjectiveCause& v){Integer tag=Integer(v);c.field("tag",tag);if(c.status().ok&&(tag<0||tag>6))return c.reject("UnknownDynamicSessionEnum");if(c.reading()&&c.status().ok)v=DynamicObjectiveCause(tag);return c.status();}
template<class C>Status value(C& c,DynamicPostureCloseReason& v){Integer tag=Integer(v);c.field("tag",tag);if(c.status().ok&&(tag<0||tag>4))return c.reject("UnknownDynamicSessionEnum");if(c.reading()&&c.status().ok)v=DynamicPostureCloseReason(tag);return c.status();}
template<class C>Status value(C& c,DynamicMilitaryStreamIdentity& v){field(c,"session_ref",v.session_ref);field(c,"generation",v.generation);field(c,"initialization_source_ref",v.initialization_source_ref);return c.status();}
template<class C>Status value(C& c,DynamicMilitaryRandomSlot& v){field(c,"effect_ref",v.effect_ref);field(c,"stage",v.stage);field(c,"stream_identity",v.stream_identity);field(c,"algorithm_version",v.algorithm_version);field(c,"requested_min",v.requested_min);field(c,"requested_max",v.requested_max);field(c,"seed",v.seed);field(c,"before_state",v.before_state);field(c,"after_state",v.after_state);field(c,"before_draw_ordinal",v.before_draw_ordinal);field(c,"after_draw_ordinal",v.after_draw_ordinal);field(c,"draw_result",v.draw_result);return c.status();}
template<class C>Status value(C& c,DynamicMilitarySessionRandom& v){field(c,"stream_identity",v.stream_identity);field(c,"algorithm_version",v.algorithm_version);field(c,"seed",v.seed);field(c,"current_state",v.current_state);field(c,"draw_ordinal",v.draw_ordinal);field(c,"last_commit_source_ref",v.last_commit_source_ref);return c.status();}
template<class C>Status value(C& c,DynamicMilitaryStatus& v){field(c,"kind",v.kind);field(c,"retained_activation_ref",v.retained_activation_ref);field(c,"valid_from",v.valid_from);field(c,"expires_at",v.expires_at);return c.status();}
template<class C>Status value(C& c,DynamicNoPosture& v){return c.status();}
template<class C>Status value(C& c,DynamicDefendPosture& v){field(c,"activation_ref",v.activation_ref);field(c,"valid_from",v.valid_from);field(c,"expires_at",v.expires_at);field(c,"guard_value",v.guard_value);field(c,"qualification_source_refs",v.qualification_source_refs);return c.status();}
template<class C>Status value(C& c,DynamicLearnedGuardPosture& v){field(c,"activation_ref",v.activation_ref);field(c,"valid_from",v.valid_from);field(c,"expires_at",v.expires_at);field(c,"learned_tactic_entry_ref",v.learned_tactic_entry_ref);field(c,"qualification_source_refs",v.qualification_source_refs);return c.status();}
template<class C>Status value(C& c,DynamicFeintPosture& v){field(c,"activation_ref",v.activation_ref);field(c,"valid_from",v.valid_from);field(c,"expires_at",v.expires_at);field(c,"qualification_source_refs",v.qualification_source_refs);return c.status();}
template<class C>Status value(C& c,DynamicSupportToken& v){field(c,"activation_ref",v.activation_ref);field(c,"supporter_unit_ref",v.supporter_unit_ref);field(c,"target_unit_ref",v.target_unit_ref);field(c,"valid_from",v.valid_from);field(c,"expires_at",v.expires_at);field(c,"qualification_source_refs",v.qualification_source_refs);field(c,"consumed_by_effect_ref",v.consumed_by_effect_ref);return c.status();}
template<class C>Status value(C& c,DynamicSupportPosture& v){field(c,"token",v.token);return c.status();}
template<class C>Status value(C& c,DynamicMilitaryBodyTacticalState& v){field(c,"local_body_ref",v.local_body_ref);field(c,"spatial_binding_ref",v.spatial_binding_ref);field(c,"heading",v.heading);field(c,"heading_source_ref",v.heading_source_ref);return c.status();}
template<class C>Status value(C& c,DynamicMilitaryUnitTacticalState& v){field(c,"unit_ref",v.unit_ref);field(c,"body_ref",v.body_ref);field(c,"entry_snapshot_ref",v.entry_snapshot_ref);field(c,"tactic_points",v.tactic_points);field(c,"tactic_points_source_ref",v.tactic_points_source_ref);field(c,"statuses",v.statuses);field(c,"posture",v.posture);field(c,"qualification_source_refs",v.qualification_source_refs);return c.status();}
template<class C>Status value(C& c,DynamicNotHold& v){return c.status();}
template<class C>Status value(C& c,DynamicHoldUnqualified& v){field(c,"last_condition_source_ref",v.last_condition_source_ref);return c.status();}
template<class C>Status value(C& c,DynamicHolding& v){field(c,"held_since",v.held_since);field(c,"qualification_transition_ref",v.qualification_transition_ref);return c.status();}
template<class C>Status value(C& c,DynamicMilitaryEscapedUnit& v){field(c,"unit_ref",v.unit_ref);field(c,"actual_exit_source_ref",v.actual_exit_source_ref);return c.status();}
template<class C>Status value(C& c,DynamicMilitaryObjectiveCurrent& v){field(c,"objective_definition_ref",v.objective_definition_ref);field(c,"started_at",v.started_at);field(c,"deadline_at",v.deadline_at);field(c,"hold_state",v.hold_state);field(c,"escaped_units",v.escaped_units);field(c,"last_transition_source_ref",v.last_transition_source_ref);field(c,"outcome_source_ref",v.outcome_source_ref);return c.status();}
template<class C>Status value(C& c,DynamicDecisionAwaitingTrigger& v){field(c,"last_closed_decision_ref",v.last_closed_decision_ref);return c.status();}
template<class C>Status value(C& c,DynamicDecisionPending& v){field(c,"trigger_source_ref",v.trigger_source_ref);return c.status();}
template<class C>Status value(C& c,DynamicDecisionCommitted& v){field(c,"decision_source_ref",v.decision_source_ref);field(c,"adopted_intent_refs",v.adopted_intent_refs);return c.status();}
template<class C>Status value(C& c,DynamicMilitaryDecisionCurrent& v){field(c,"controller_ref",v.controller_ref);field(c,"control_binding_generation",v.control_binding_generation);field(c,"phase",v.phase);return c.status();}
template<class C>Status value(C& c,DynamicMilitaryActive& v){return c.status();}
template<class C>Status value(C& c,DynamicMilitaryTerminal& v){field(c,"outcome_source_ref",v.outcome_source_ref);field(c,"closed_at",v.closed_at);field(c,"unresolved_committed_effect_refs",v.unresolved_committed_effect_refs);return c.status();}
template<class C>Status value(C& c,DynamicMilitarySessionCurrent2& v){field(c,"session_ref",v.session_ref);field(c,"generation",v.generation);field(c,"definition_lock_ref",v.definition_lock_ref);field(c,"initialization_source_ref",v.initialization_source_ref);field(c,"opened_at",v.opened_at);field(c,"lifecycle",v.lifecycle);field(c,"body_tactical_states",v.body_tactical_states);field(c,"unit_tactical_states",v.unit_tactical_states);field(c,"objective_current",v.objective_current);field(c,"decision_current",v.decision_current);field(c,"session_rng",v.session_rng);field(c,"last_session_transition_ref",v.last_session_transition_ref);return c.status();}
template<class C>Status value(C& c,DynamicMilitaryUnitEntry& v){field(c,"unit_ref",v.unit_ref);field(c,"body_ref",v.body_ref);field(c,"entry_snapshot_ref",v.entry_snapshot_ref);field(c,"initial_tactic_points",v.initial_tactic_points);field(c,"original_point_initialization_source_ref",v.original_point_initialization_source_ref);return c.status();}
template<class C>Status value(C& c,DynamicMilitaryBodyEntry& v){field(c,"body_ref",v.body_ref);field(c,"initial_heading",v.initial_heading);field(c,"actual_heading_source_ref",v.actual_heading_source_ref);return c.status();}
template<class C>Status value(C& c,DynamicSessionInitialized& v){field(c,"entry_or_contact_source_ref",v.entry_or_contact_source_ref);field(c,"definition_lock_ref",v.definition_lock_ref);field(c,"unit_entry_rows",v.unit_entry_rows);field(c,"body_entry_rows",v.body_entry_rows);field(c,"objective_definition_ref",v.objective_definition_ref);field(c,"initial_objective_condition_ref",v.initial_objective_condition_ref);field(c,"initial_rng_source_ref",v.initial_rng_source_ref);field(c,"initial_rng_seed",v.initial_rng_seed);field(c,"initial_rng_state",v.initial_rng_state);field(c,"initial_draw_ordinal",v.initial_draw_ordinal);return c.status();}
template<class C>Status value(C& c,DynamicObjectiveQualificationTransition& v){field(c,"objective_definition_ref",v.objective_definition_ref);field(c,"from_state",v.from_state);field(c,"to_state",v.to_state);field(c,"previous_holding_source_ref",v.previous_holding_source_ref);field(c,"cause_kind",v.cause_kind);field(c,"cause_source_refs",v.cause_source_refs);field(c,"actual_qualified_unit_refs",v.actual_qualified_unit_refs);field(c,"actual_contesting_unit_refs",v.actual_contesting_unit_refs);return c.status();}
template<class C>Status value(C& c,DynamicPostureClosed& v){field(c,"unit_ref",v.unit_ref);field(c,"activation_ref",v.activation_ref);field(c,"reason",v.reason);field(c,"expiry_or_cause_ref",v.expiry_or_cause_ref);field(c,"replacement_activation_ref",v.replacement_activation_ref);return c.status();}
template<class C>Status value(C& c,DynamicStatusExpired& v){field(c,"unit_ref",v.unit_ref);field(c,"status_kind",v.status_kind);field(c,"retained_activation_ref",v.retained_activation_ref);field(c,"scheduled_expires_at",v.scheduled_expires_at);return c.status();}
template<class C>Status value(C& c,DynamicSessionDecisionCommitted& v){field(c,"controller_ref",v.controller_ref);field(c,"control_binding_generation",v.control_binding_generation);field(c,"trigger_source_ref",v.trigger_source_ref);field(c,"knowledge_snapshot_ref",v.knowledge_snapshot_ref);field(c,"policy_definition_ref",v.policy_definition_ref);field(c,"selected_intent_refs",v.selected_intent_refs);field(c,"rng_slots",v.rng_slots);return c.status();}
template<class C>Status value(C& c,DynamicSessionTerminated& v){field(c,"objective_outcome_source_ref",v.objective_outcome_source_ref);field(c,"actual_exit_or_stay_source_refs",v.actual_exit_or_stay_source_refs);field(c,"closed_posture_refs",v.closed_posture_refs);field(c,"retained_committed_effect_refs",v.retained_committed_effect_refs);return c.status();}
template<class C>Status value(C& c,DynamicSessionStateSource2& v){field(c,"id",v.id);field(c,"session_ref",v.session_ref);field(c,"generation",v.generation);field(c,"effective_at",v.effective_at);Integer tag=Integer(v.payload.index());c.field("kind",tag);if(c.reading()&&c.status().ok&&!construct(c,v.payload,tag))return c.reject("UnknownDynamicSessionSourceKind");if(c.status().ok)c.object("payload",[&](auto& x){return std::visit([&](auto& payload){return value(x,payload);},v.payload);});return c.status();}
}
