// Subrecord of treaty_performance/1, not a new persisted root.
#pragma once
#include "treaty_aid_schedule.hpp"
#include "treaty_terms_codec_fields.hpp"
namespace hanmo::resident::treaty_aid_schedule_codec_fields {
template<class C>Status fields(C& c,TreatyAidSchedule& r){
 c.field("id",r.id);c.field("treaty_id",r.treaty_id);c.field("terms_revision",r.terms_revision);c.field("term_id",r.term_id);
 c.object("activation_ref",[&](auto& v){return treaty_terms_codec_fields::source(v,r.activation_ref);});
 c.object("schedule_terms_ref",[&](auto& v){return treaty_terms_codec_fields::source(v,r.schedule_terms_ref);});
 c.field("next_occurrence_ordinal",r.next_occurrence_ordinal);
 c.optional("termination_ref",r.termination_ref,[](auto& v,auto& x){return treaty_terms_codec_fields::source(v,x);});
 if(c.status().ok&&(r.id.empty()||r.treaty_id.empty()||r.term_id.empty()||r.terms_revision<1||r.next_occurrence_ordinal<0))return c.reject("InvalidTreatyAidSchedule");
 return c.status();
}
}
