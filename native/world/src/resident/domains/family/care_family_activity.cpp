#include "care_family_activity.hpp"
#include "resident/domains/actor_foundation/authored_origin.hpp"
#include "care_personal_stop.hpp"
namespace hanmo::resident {namespace {
bool text(const StableId& v){return !v.empty()&&v.size()<=256;}
bool source(const MomentOwnerSource& v){return text(v.source_ref.domain)&&text(v.source_ref.kind)&&text(v.source_ref.id)&&v.source_schema>0;}
bool same(const SourceRef& a,const SourceRef& b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
Status bad(const StableId& id){return common_failure("InvalidCareFamilyActivity",id,ErrorKind::internal);}
}
SourceRef care_family_activity_started_source(const StableId& id){return {"care_requirements","care_family_activity_started",id};}
SourceRef care_family_activity_ended_source(const StableId& id){return {"care_requirements","care_family_activity_ended",id};}
Outcome<Integer> care_family_activity_source_schema(const SourceRef& ref){if(text(ref.id)&&ref.domain=="care_requirements"&&(ref.kind=="care_family_activity_started"||ref.kind=="care_family_activity_ended"))return {true,1,{}};return {false,{},common_failure("UnknownCareFamilyActivitySource",ref.id,ErrorKind::unsupported_capability).error};}
Outcome<ActorFoundationRuleRef> original_exclusive_family_care_rule(const CatalogIdentity& catalog){
 bool selected=false;for(const auto& module:catalog.module_versions)if(module.first=="care_requirements"){if(selected||module.second!="6")return {false,{},common_failure("CareFamilyActivityVersion6Required",{},ErrorKind::unsupported_capability).error};selected=true;}
 if(!selected||catalog.content_digest.empty())return {false,{},common_failure("CareFamilyActivityVersion6Required",{},ErrorKind::unsupported_capability).error};
 return {true,{"original_exclusive_family_care",1,catalog.content_digest},{}};
}
Status validate_care_family_activity_shape(const CareFamilyActivity& v,Integer version){
 if(version!=6)return common_failure("CareFamilyActivityVersion6Required",v.activity_id,ErrorKind::unsupported_capability);
 if(!text(v.activity_id)||!text(v.accepted_care_terms_ref)||!text(v.child_actor_id)||!text(v.carer_actor_id)||v.child_actor_id==v.carer_actor_id||!source(v.actual_handover_execution_ref)||!source(v.actual_start_work_period_ref)||!source(v.actual_placement_result_ref)||v.compatibility_rule_ref.id!="original_exclusive_family_care"||v.compatibility_rule_ref.version!=1||!text(v.compatibility_rule_ref.catalog_digest)||v.adopted_participation_refs.empty()||v.adopted_participation_refs.size()>65536)return bad(v.activity_id);
 if(v.actual_handover_execution_ref.source_schema!=3||v.actual_handover_execution_ref.source_ref.domain!="care_requirements"||v.actual_handover_execution_ref.source_ref.kind!="care_handover_execution"||v.actual_start_work_period_ref.source_schema!=2||v.actual_start_work_period_ref.source_ref.domain!="care_requirements"||v.actual_start_work_period_ref.source_ref.kind!="care_handover_period"||v.actual_placement_result_ref.source_schema!=2||v.actual_placement_result_ref.source_ref.domain!="travel"||v.actual_placement_result_ref.source_ref.kind!="travel_dependent_binding_change")return bad(v.activity_id);
 auto at=compare_world_moments(v.started_at,v.started_at);if(!at.ok)return {false,{},at.error};
 for(std::size_t i=0;i<v.adopted_participation_refs.size();++i){if(!source(v.adopted_participation_refs[i]))return bad(v.activity_id);for(std::size_t j=0;j<i;++j)if(same(v.adopted_participation_refs[i].source_ref,v.adopted_participation_refs[j].source_ref))return bad(v.activity_id);}
 if(v.ended_by){if(!text(v.ended_by->end_id)||v.ended_by->end_id==v.activity_id)return bad(v.activity_id);auto order=compare_world_moments(v.ended_by->ended_at,v.started_at);if(!order.ok)return {false,{},order.error};if(order.value<0)return bad(v.activity_id);
  auto shape=std::visit([&](const auto& cause)->Status{using T=std::decay_t<decltype(cause)>;if constexpr(std::is_same_v<T,CareSubjectDeath>)return cause.death_ref.domain=="family_life"&&cause.death_ref.kind=="family_death"&&text(cause.death_ref.id)?common_success():bad(v.activity_id);else if constexpr(std::is_same_v<T,CarePersonalStop>)return validate_care_personal_stop_shape(v,cause);else return std::visit([&](const auto& c)->Status{using L=std::decay_t<decltype(c)>;if constexpr(std::is_same_v<L,CareHolderDeath>)return c.death_ref.domain=="family_life"&&c.death_ref.kind=="family_death"&&text(c.death_ref.id)?common_success():bad(v.activity_id);else if constexpr(std::is_same_v<L,CareAcceptedWindowEnded>)return c.accepted_care_terms_ref==v.accepted_care_terms_ref&&c.until_tick>=0?common_success():bad(v.activity_id);else if constexpr(std::is_same_v<L,CareRequiredCapacityLost>)return text(c.capacity_transition_ref.id)&&text(c.condition_receipt_ref.result_id)&&c.purpose_ref.id=="care_participant"&&c.purpose_ref.version==1&&c.purpose_ref.catalog_digest==v.compatibility_rule_ref.catalog_digest?common_success():bad(v.activity_id);else if constexpr(std::is_same_v<L,CareActualSeparation>)return source(c.actual_location_or_binding_transition)?common_success():bad(v.activity_id);else if constexpr(std::is_same_v<L,CareParticipationConflict>)return source(c.actual_occupation_change)&&actor_foundation_rule_equal(c.compatibility_rule_ref,v.compatibility_rule_ref)?common_success():bad(v.activity_id);else return source(c.actual_ended_result)?common_success():bad(v.activity_id);},cause);},v.ended_by->cause);if(!shape.ok)return shape;
 }
 return common_success();
}
bool care_family_activity_end_cause_equal(const CareFamilyActivityEndCause& a,const CareFamilyActivityEndCause& b){
 if(a.index()!=b.index())return false;
 auto owner=[](const MomentOwnerSource& x,const MomentOwnerSource& y){return x.source_schema==y.source_schema&&same(x.source_ref,y.source_ref);};
 auto death=[](const DeathRef& x,const DeathRef& y){return x.domain==y.domain&&x.kind==y.kind&&x.id==y.id;};
 return std::visit([&](const auto& x){using T=std::decay_t<decltype(x)>;const auto& y=std::get<T>(b);
  if constexpr(std::is_same_v<T,CareSubjectDeath>)return death(x.death_ref,y.death_ref);
  else if constexpr(std::is_same_v<T,CarePersonalStop>)return care_personal_stop_equal(x,y);
  else {if(x.index()!=y.index())return false;return std::visit([&](const auto& u){using L=std::decay_t<decltype(u)>;const auto& v=std::get<L>(y);
   if constexpr(std::is_same_v<L,CareHolderDeath>)return death(u.death_ref,v.death_ref);
   else if constexpr(std::is_same_v<L,CareRequiredCapacityLost>)return same(u.capacity_transition_ref,v.capacity_transition_ref)&&u.condition_receipt_ref.result_id==v.condition_receipt_ref.result_id&&actor_foundation_rule_equal(u.purpose_ref,v.purpose_ref);
   else if constexpr(std::is_same_v<L,CareActualSeparation>)return owner(u.actual_location_or_binding_transition,v.actual_location_or_binding_transition);
   else if constexpr(std::is_same_v<L,CareAcceptedWindowEnded>)return u.accepted_care_terms_ref==v.accepted_care_terms_ref&&u.until_tick==v.until_tick;
   else if constexpr(std::is_same_v<L,CareParticipationConflict>)return owner(u.actual_occupation_change,v.actual_occupation_change)&&actor_foundation_rule_equal(u.compatibility_rule_ref,v.compatibility_rule_ref);
   else return owner(u.actual_ended_result,v.actual_ended_result);
  },x);}
 },a);
}

}
