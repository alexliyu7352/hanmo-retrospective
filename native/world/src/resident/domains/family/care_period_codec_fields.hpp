// P24.1 base seven fields and v1c 3.2 source seven fields, one paired table.
#pragma once
#include "care_period.hpp"
#include "care_requirement_codec_fields.hpp"
namespace hanmo::resident::care_period_codec {
using care_requirement_codec::text;
using care_requirement_codec::moment_source;
using care_requirement_codec::rule;
template<class C>Status profile(C& c,Integer version){return version<3||version>6?c.reject("CareExactPeriodProfileRequired"):c.status();}
template<class C>Status window(C& c,CareCalendarWindow& v){
 c.field("from_tick",v.from_tick);c.field("to_tick",v.to_tick);
 return c.status().ok&&(v.from_tick<0||v.to_tick<1||v.to_tick-1!=v.from_tick)?c.reject("InvalidCareCalendarWindow"):c.status();
}
template<class C>Status period(C& c,CarePeriod& v,Integer version){
 profile(c,version);if(!c.status().ok)return c.status();
 text(c,"child",v.child);text(c,"caregiver",v.caregiver);
 c.object("place",[&](auto& a){return moment_source(a,v.place);});
 c.object("period",[&](auto& a){return window(a,v.period);});
 c.object("activity_ref",[&](auto& a){return moment_source(a,v.activity_ref);});
 c.object("food_support_ref",[&](auto& a){return moment_source(a,v.food_support_ref);});
 Integer tag=static_cast<Integer>(v.outcome);c.field("outcome",tag);
 if(!c.status().ok)return c.status();if(tag!=0)return c.reject("InvalidCarePeriodOutcome");
 if(c.reading())v.outcome=CarePeriodOutcome::completed;
 auto valid=validate_care_period_shape(v);return valid.ok?c.status():c.reject(valid.error.original.code);
}
template<class C>Status participation(C& c,CarePeriodParticipation& v){
 Integer tag=static_cast<Integer>(v.index());c.field("tag",tag);
 if(!c.status().ok)return c.status();if(tag<0||tag>1)return c.reject("InvalidCarePeriodParticipation");
 if(c.reading())v=tag==0?CarePeriodParticipation{OriginalCareActionParticipation{}}:CarePeriodParticipation{CompatibleFamilyActivityParticipation{}};
 if(tag==0){auto& p=std::get<OriginalCareActionParticipation>(v);
  c.object("original_action_ref",[&](auto& a){return moment_source(a,p.original_action_ref);});
  return c.object("provided_period_ref",[&](auto& a){return moment_source(a,p.provided_period_ref);});
 }
 auto& p=std::get<CompatibleFamilyActivityParticipation>(v);
 c.object("family_activity_ref",[&](auto& a){return moment_source(a,p.family_activity_ref);});
 c.object("compatibility_rule_ref",[&](auto& a){return rule(a,p.compatibility_rule_ref);});
 return c.object("actual_participation_ref",[&](auto& a){return moment_source(a,p.actual_participation_ref);});
}
template<class C>Status original_source(C& c,CarePeriodSource& v,Integer version){
 profile(c,version);if(!c.status().ok)return c.status();
 text(c,"care_period_ref",v.care_period_ref);text(c,"binding_ref",v.binding_ref);
 c.object("calendar_period_ref",[&](auto& a){return moment_source(a,v.calendar_period_ref);});
 c.object("participation",[&](auto& a){return participation(a,v.participation);});
 c.object("actual_complete_source_ref",[&](auto& a){return moment_source(a,v.actual_complete_source_ref);});
 c.sequence("provision_result_refs",v.provision_result_refs,[](auto& a,auto& r){return moment_source(a,r);});
 c.sequence("condition_result_refs",v.condition_result_refs,[](auto& a,auto& r){return text(a,"result_id",r.result_id);});
 if(!c.status().ok)return c.status();auto valid=validate_care_period_source_shape(v);return valid.ok?c.status():c.reject(valid.error.original.code);
}
}
