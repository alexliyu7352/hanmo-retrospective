#pragma once
#include "treaty_incident.hpp"
#include "treaty_terms_codec_fields.hpp"
namespace hanmo::resident::treaty_incident_codec_fields {
template<class C>Status obligation(C& c,TreatyObligationRef& r){
 c.field("owner_domain",r.owner_domain);c.field("contract_id",r.contract_id);c.field("term_id",r.term_id);return c.field("terms_revision",r.terms_revision);
}
template<class C>Status incident(C& c,TreatyIncident& r){
 c.field("id",r.id);c.field("treaty_id",r.treaty_id);c.field("terms_revision",r.terms_revision);c.field("term_id",r.term_id);
 c.object("source_fact_ref",[&](auto& v){return treaty_terms_codec_fields::source(v,r.source_fact_ref);});
 c.field("occurrence_tick",r.occurrence_tick);
 static constexpr const char* kinds[]={"unnotified_incident","unable_to_perform","deliberate_breach"};
 treaty_terms_codec_fields::enum_field(c,"incident_kind",r.incident_kind,kinds);c.field("liable_party",r.liable_party);
 c.sequence("remaining_obligation_refs",r.remaining_obligation_refs,[](auto& v,auto& x){return obligation(v,x);});
 if(c.status().ok){auto s=validate_treaty_incident_shape(r);if(!s.ok)return c.reject(s.error.original.code);}return c.status();
}
}
