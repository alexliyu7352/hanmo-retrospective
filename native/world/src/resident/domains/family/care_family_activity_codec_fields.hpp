// One field sequence, shared by binary, JSON and borrowed projection.
#pragma once
#include "care_family_activity.hpp"
#include "care_arrangement_codec_fields.hpp"
#include "care_handover_execution_codec_fields.hpp"
namespace hanmo::resident::care_family_activity_codec {
template<class C>Status end_cause(C& c,CareFamilyActivityEndCause& v){
 Integer tag=static_cast<Integer>(v.index());c.field("tag",tag);if(!c.status().ok)return c.status();
 if(tag<0||tag>2)return c.reject("InvalidCareFamilyActivityEndCause");
 if(c.reading()){if(tag==0)v=CareLossCauseV2{CareHolderDeath{}};else if(tag==1)v=CareSubjectDeath{};else v=CarePersonalStop{};}
 if(tag==0)return c.object("loss_cause",[&](auto& x){return care_requirement_codec::loss(x,std::get<CareLossCauseV2>(v));});
 if(tag==1)return c.object("death_ref",[&](auto& x){return care_requirement_codec::death(x,std::get<CareSubjectDeath>(v).death_ref);});
 auto& stop=std::get<CarePersonalStop>(v);
 c.object("actual_stop_result",[&](auto& x){return care_requirement_codec::moment_source(x,stop.actual_stop_result);});
 return care_handover_codec::actor_command(c,stop.actual_command);
}
template<class C>Status end(C& c,CareFamilyActivityEnd& v){
 care_requirement_codec::text(c,"end_id",v.end_id);
 c.object("ended_at",[&](auto& x){return family_life4_codec::moment(x,v.ended_at);});
 return c.object("cause",[&](auto& x){return end_cause(x,v.cause);});
}
template<class C>Status activity(C& c,CareFamilyActivity& v,Integer version){
 if(version!=6)return c.reject("CareFamilyActivityVersion6Required");
 care_requirement_codec::text(c,"activity_id",v.activity_id);
 care_requirement_codec::text(c,"accepted_care_terms_ref",v.accepted_care_terms_ref);
 care_requirement_codec::text(c,"child_actor_id",v.child_actor_id);
 care_requirement_codec::text(c,"carer_actor_id",v.carer_actor_id);
 c.object("actual_handover_execution_ref",[&](auto& x){return care_requirement_codec::moment_source(x,v.actual_handover_execution_ref);});
 c.object("actual_start_work_period_ref",[&](auto& x){return care_requirement_codec::moment_source(x,v.actual_start_work_period_ref);});
 c.object("actual_placement_result_ref",[&](auto& x){return care_requirement_codec::moment_source(x,v.actual_placement_result_ref);});
 c.object("started_at",[&](auto& x){return family_life4_codec::moment(x,v.started_at);});
 c.object("compatibility_rule_ref",[&](auto& x){return care_requirement_codec::rule(x,v.compatibility_rule_ref);});
 care_arrangement_codec::moment_refs(c,"adopted_participation_refs",v.adopted_participation_refs);
 c.optional("ended_by",v.ended_by,[](auto& x,auto& e){return end(x,e);});
 if(!c.status().ok)return c.status();auto valid=validate_care_family_activity_shape(v,version);return valid.ok?c.status():c.reject(valid.error.original.code);
}
}
