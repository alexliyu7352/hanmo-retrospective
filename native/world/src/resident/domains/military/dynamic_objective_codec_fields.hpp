#pragma once
#include "dynamic_objective_model.hpp"
#include "dynamic_session_codec_fields.hpp"
namespace hanmo::resident::dynamic_objective_codec {
// The completed evidence's original outcome has exactly the original two
// semantic fields. Legacy presentation FieldOrder is not a source authority.
template<class C>Status outcome(C& c,MilitaryBattleResult& v){
 c.field("winner_side",v.winner_side);c.field("reason",v.reason);
 if(c.reading()&&c.status().ok)v.field_order.clear();return c.status();
}
template<class C>Status value(C& c,DynamicMilitaryObjectiveEvidence2& v){
 using dynamic_session_codec::field;
 field(c,"id",v.id);field(c,"session_ref",v.session_ref);field(c,"generation",v.generation);
 field(c,"effective_at",v.effective_at);field(c,"objective_definition_ref",v.objective_definition_ref);
 field(c,"session_initialization_source_ref",v.session_initialization_source_ref);
 field(c,"objective_transition_source_ref",v.objective_transition_source_ref);
 field(c,"cause_kind",v.cause_kind);field(c,"cause_source_refs",v.cause_source_refs);
 field(c,"actual_judgment_source_refs",v.actual_judgment_source_refs);
 field(c,"actual_exit_source_refs",v.actual_exit_source_refs);
 c.object("outcome",[&](auto& x){return outcome(x,v.outcome);});return c.status();
}
}
