#include "politics_result_projection2.hpp"
namespace hanmo::resident {
bool native_politics_time2_result_kind(const FactRecord&h){if(h.domain!="politics")return false;for(const auto*k:{"political_appointment_offered","political_appointment_accepted","political_appointment_declined","political_appointment_withdrawn","political_appointment_expired","political_appointment_arrived","political_appointment_invalidated","political_office_dismissed"})if(h.kind==k)return true;return false;}
Outcome<WorldMoment> NativePoliticsTime2ResultRead::read_selected_source_moment(const MomentOwnerSource&r)const{return read_politics_source_moment(access_.common,access_.politics,access_.organization_runtime,r.source_ref,r.source_schema,access_.clock);}
Outcome<ExactOwnerFactRead> NativePoliticsTime2ResultRead::read_selected_fact(const FactRecord&h)const{
 if(!native_politics_time2_result_kind(h))return {false,{},common_failure("UnsupportedPoliticsTime2Result",h.id,ErrorKind::unsupported_capability).error};auto at=read_selected_source_moment({{h.domain,h.kind,h.id},2});if(!at.ok)return {false,{},at.error};auto original=politics_dynamic_fact(access_.politics,h.id);if(!original||original->id!=h.id||original->kind!=h.kind||original->tick!=at.value.half_day_tick)return {false,{},common_failure("PoliticsTime2ProjectionMismatch",h.id).error};const auto&v=*original;
 // Preserve the original finite projection and publication policy. Appointment
 // history alone does not issue knowledge, a Report, or an Action result.
 ExactOwnerFactRead out;out.fact=DynamicFactViewV2{v.id,v.kind,v.rule_version,at.value,OwnerFactMoment{{h.domain,h.kind,h.id},2},v.cause_fact_ids,v.affair_ids,v.participants,v.supply,v.obligation,v.subject_id,v.outcome,v.publicly_visible};return {true,std::move(out),{}};
}
}
