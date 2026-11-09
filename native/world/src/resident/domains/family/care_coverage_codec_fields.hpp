#pragma once
#include "care_coverage.hpp"
#include "care_requirement_codec_fields.hpp"
namespace hanmo::resident::care_coverage_codec {
using care_requirement_codec::text;
using care_requirement_codec::source;
using care_requirement_codec::moment_source;
template<class C>Status profile(C& c,Integer version){return version<3||version>6?c.reject("CareExactCoverageProfileRequired"):c.status();}
template<class C>Status binding(C& c,CareBinding& v,Integer version){
 profile(c,version);if(!c.status().ok)return c.status();
 text(c,"binding_id",v.binding_id);text(c,"child_actor_id",v.child_actor_id);
 text(c,"carer_actor_id",v.carer_actor_id);text(c,"terms_ref",v.terms_ref);
 text(c,"accepted_ref",v.accepted_ref);c.field("started_tick",v.started_tick);
 c.optional("until_tick",v.until_tick,[](auto& a,auto& n){a.field("value",n);return a.status();});
 c.object("care_activity_ref",[&](auto& a){return moment_source(a,v.care_activity_ref);});
 c.object("placement_ref",[&](auto& a){return moment_source(a,v.placement_ref);});
 Integer tag=static_cast<Integer>(v.phase);c.field("phase",tag);
 if(!c.status().ok)return c.status();if(tag<0||tag>2)return c.reject("InvalidCareBindingPhase");
 if(c.reading())v.phase=static_cast<CareBindingPhase>(tag);
 auto valid=validate_care_binding_shape(v);return valid.ok?c.status():c.reject(valid.error.original.code);
}
template<class C>Status satisfied(C& c,CareCoverageSatisfied& v,Integer version){
 profile(c,version);if(!c.status().ok)return c.status();
 text(c,"fact_id",v.fact_id);text(c,"requirement_id",v.requirement_id);
 c.field("source_episode",v.source_episode);text(c,"binding_ref",v.binding_ref);
 c.object("handover_result_ref",[&](auto& a){return source(a,v.handover_result_ref);});
 c.field("effective_tick",v.effective_tick);if(!c.status().ok)return c.status();
 auto valid=validate_care_coverage_satisfied_shape(v);return valid.ok?c.status():c.reject(valid.error.original.code);
}
template<class C>Status lost(C& c,CareCoverageLost& v,Integer version){
 profile(c,version);if(!c.status().ok)return c.status();
 text(c,"fact_id",v.fact_id);text(c,"requirement_id",v.requirement_id);
 text(c,"old_binding_ref",v.old_binding_ref);
 c.object("cause_ref",[&](auto& a){return source(a,v.cause_ref);});
 c.field("effective_tick",v.effective_tick);if(!c.status().ok)return c.status();
 auto valid=validate_care_coverage_lost_shape(v);return valid.ok?c.status():c.reject(valid.error.original.code);
}
}
