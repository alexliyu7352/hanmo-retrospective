// 原 binding/WorldMoment/WorkUnits 共用原字段，不加隐式版本或时间值。
#pragma once
#include "fixed_service_work_period.hpp"
#include "work_period_codec_fields.hpp"
namespace hanmo::resident::fixed_period_fields {
template<class C>Status ref(C& c,WorkPeriodRef2& v){
 c.object("binding",[&](auto& x){return work_fields::binding(x,v.binding);});
 c.field("period_ordinal",v.period_ordinal);
 c.object("from",[&](auto& x){return exact_fields::moment(x,v.from);});
 c.object("until",[&](auto& x){return exact_fields::moment(x,v.until);});
 c.field("source_schema_version",v.source_schema_version);
 if(c.status().ok)exact_fields::take(validate_work_period_ref2(v));
 return c.status();
}
template<class C>Status view(C& c,WorkPeriodView2& v){
 c.object("ref",[&](auto& x){return ref(x,v.ref);});
 c.sequence("actual_actor_roles",v.actual_actor_roles,[](auto& x,auto& role){exact_fields::required_ref(x,"actor_id",role.actor_id);return exact_fields::required_ref(x,"role",role.role);});
 exact_fields::references(c,"actual_labor_allocations",v.actual_labor_allocations);
 exact_fields::references(c,"facility_claim_refs",v.facility_claim_refs);
 exact_fields::required_ref(c,"work_unit_id",v.work_unit_id);
 c.object("units_before",[&](auto& x){return exact_fields::work_units(x,v.units_before);});
 c.object("units_added",[&](auto& x){return exact_fields::work_units(x,v.units_added);});
 c.object("units_after",[&](auto& x){return exact_fields::work_units(x,v.units_after);});
 work_fields::source_list(c,"resource_effect_refs",v.resource_effect_refs);
 work_fields::source_list(c,"exertion_source_refs",v.exertion_source_refs);
 Integer tag=static_cast<Integer>(v.disposition);exact_fields::finite(c,"disposition",tag,2);
 if(c.status().ok&&c.reading())v.disposition=static_cast<WorkPeriodDisposition>(tag);
 c.optional("reason",v.reason,[](auto& x,auto& r){return exact_fields::source(x,r);});
 if(c.status().ok)exact_fields::take(validate_work_period_view2(v));
 return c.status();
}
}
