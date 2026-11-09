// M-PASSAGE exact successor row bodies. No separate safe-exit World root.
#pragma once
#include "safe_exit_entry.hpp"
#include "treaty_terms_codec_fields.hpp"
#include "resident/domains/travel/spatial_carrier_ref_codec_fields.hpp"
namespace hanmo::resident::safe_exit_codec_fields {
using treaty_terms_codec_fields::source;
using treaty_terms_codec_fields::sources;
using treaty_terms_codec_fields::strings;
using treaty_terms_codec_fields::enum_field;
template<class C>Status scope(C& c,SafeExitSubjectScope& r){static constexpr const char* tags[]={"named_subjects","civilian_selection_scope"};std::size_t index=0;auto valid=treaty_terms_codec_fields::variant_tag(c,r,tags,index);if(!valid.ok)return valid;if(index==0){if(c.reading())r=SafeExitNamedSubjects{};return strings(c,"actor_or_member_refs",std::get<SafeExitNamedSubjects>(r).actor_or_member_refs);}if(c.reading())r=SafeExitCivilianSelectionScope{};auto& v=std::get<SafeExitCivilianSelectionScope>(r);c.object("population_selection_or_entry_ref",[&](auto& x){return source(x,v.population_selection_or_entry_ref);});return sources(c,"current_successor_refs",v.current_successor_refs);}
template<class C>Status cargo(C& c,SafeExitCargoScopeRef& r){c.object("entry_manifest_ref",[&](auto& v){return source(v,r.entry_manifest_ref);});return c.field("inventory_id",r.inventory_id);}
template<class C>Status binding(C& c,SafeExitBinding& r){
 c.field("id",r.id);c.field("treaty_id",r.treaty_id);c.field("terms_revision",r.terms_revision);c.field("term_id",r.term_id);
 c.object("entry_receipt_ref",[&](auto& v){return source(v,r.entry_receipt_ref);});c.object("entry_carrier",[&](auto& v){return spatial_carrier_ref_codec_fields::fields(v,r.entry_carrier);});c.object("entered_manifest_basis",[&](auto& v){return source(v,r.entered_manifest_basis);});
 c.sequence("entered_subject_refs",r.entered_subject_refs,[](auto& v,auto& x){return treaty_terms_codec_fields::subject(v,x);});c.object("exit_policy_ref",[&](auto& v){return source(v,r.exit_policy_ref);});
 static constexpr const char* states[]={"inside","exiting","outside","expired"};enum_field(c,"state",r.state,states);
 c.optional("termination_source",r.termination_source,[](auto& v,auto& x){return source(v,x);});c.optional("termination_at",r.termination_at,[](auto& v,auto& x){return exact_fields::moment(v,x);});c.optional("exit_from",r.exit_from,[](auto& v,auto& x){return exact_fields::moment(v,x);});c.optional("exit_until",r.exit_until,[](auto& v,auto& x){return exact_fields::moment(v,x);});
 strings(c,"exit_line_refs",r.exit_line_refs);sources(c,"manifest_successor_refs",r.manifest_successor_refs);c.optional("last_result_ref",r.last_result_ref,[](auto& v,auto& x){return source(v,x);});if(c.status().ok){auto valid=validate_safe_exit_binding_shape(r);if(!valid.ok)return c.reject(valid.error.original.code);}return c.status();
}
template<class C>Status line(C& c,SafeExitLine& r){
 c.field("line_id",r.line_id);c.field("binding_id",r.binding_id);c.object("entitlement_source_ref",[&](auto& v){return source(v,r.entitlement_source_ref);});c.object("subject_scope",[&](auto& v){return scope(v,r.subject_scope);});c.sequence("cargo_scope_refs",r.cargo_scope_refs,[](auto& v,auto& x){return cargo(v,x);});c.optional("chosen_path_id",r.chosen_path_id,[](auto& v,auto& x){return v.field("value",x);});c.field("last_exit_boundary_ordinal",r.last_exit_boundary_ordinal);
 static constexpr const char* states[]={"inside","exiting","outside","expired","superseded"};enum_field(c,"state",r.state,states);sources(c,"actual_crossing_refs",r.actual_crossing_refs);if(c.status().ok){auto valid=validate_safe_exit_line_shape(r);if(!valid.ok)return c.reject(valid.error.original.code);}return c.status();
}
}
