#pragma once
#include "treaty_aid_occurrence.hpp"
#include "treaty_aid_schedule_codec_fields.hpp"
#include <type_traits>
namespace hanmo::resident::treaty_aid_occurrence_codec_fields {
// Adapt the original S5 obligation field table; do not duplicate its schema.
template<class C>struct ObligationFields {
 C& archive;void operator()(const char* name,StableId& value){archive.field(name,value);}
};
template<class C>Status fields(C& c,TreatyAidOccurrenceBinding& r){
 using treaty_terms_codec_fields::source;
 c.field("id",r.id);c.object("schedule_ref",[&](auto& v){return source(v,r.schedule_ref);});
 c.field("occurrence_ordinal",r.occurrence_ordinal);
 c.object("establishment_result_ref",[&](auto& v){return source(v,r.establishment_result_ref);});
 c.object("original_obligation_ref",[&](auto& v){ObligationFields<std::decay_t<decltype(v)>> fields{v};obligation_ref_fields(fields,r.original_obligation_ref);return v.status();});
 c.optional("shipment_ref",r.shipment_ref,[](auto& v,auto& x){return source(v,x);});
 treaty_terms_codec_fields::sources(c,"receipt_result_refs",r.receipt_result_refs);
 c.optional("deadline_result_ref",r.deadline_result_ref,[](auto& v,auto& x){return source(v,x);});
 c.optional("termination_disposition_ref",r.termination_disposition_ref,[](auto& v,auto& x){return source(v,x);});
 if(c.status().ok){auto s=validate_treaty_aid_occurrence_shape(r);if(!s.ok)return c.reject(s.error.original.code);}
 return c.status();
}
}
