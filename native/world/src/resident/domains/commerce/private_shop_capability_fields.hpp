// Sole fixed P29 field sequence. Same genuine labor source plus actual_span
// identifies each original Condition subspan, never a synthetic proof fact.
#pragma once
#include "private_shop_capability_model.hpp"
#include "private_shop_operation_sources_fields.hpp"
#include "resident/kernel/world_timeline_codec_fields.hpp"
namespace hanmo::resident::private_shop_capability_fields {
using namespace exact_fields;
template<class C>Status original(C&c,MomentOwnerSource&v){c.object("source_ref",[&](auto&x){return source(x,v.source_ref);});return schema(c,"source_schema",v.source_schema);}
template<class C>Status originals(C&c,const char*name,std::vector<MomentOwnerSource>&v){
 std::size_t index=0;return c.sequence(name,v,[&](auto&x,auto&row){original(x,row);if(x.status().ok)for(std::size_t n=0;n<index;++n)if(row.source_schema==v[n].source_schema&&same_source_ref(row.source_ref,v[n].source_ref))return x.reject("DuplicatePrivateShopConditionSource");++index;return x.status();});
}
template<class C>Status purpose(C&c,ActorFoundationRuleRef&v){
 private_shop_operation_source_fields::rule(c,v);
 if(c.status().ok&&(v.id!="private_shop_operation"||v.version!=1))return c.reject("PrivateShopOriginalPurposeRequired");
 return c.status();
}
template<class C>Status profile(C&c,PrivateShopOperationProfile&v){
 required_ref(c,"operation_ref",v.operation_ref);Integer tag=static_cast<Integer>(v.operation_profile);
 finite(c,"operation_profile",tag,1);if(c.reading()&&c.status().ok)v.operation_profile=static_cast<PrivateShopOperationProfileKind>(tag);
 c.optional("migration_source",v.migration_source,[&](auto&x,auto&r){return original(x,r);});
 c.optional("accepted_purpose_ref",v.accepted_purpose_ref,[&](auto&x,auto&r){return purpose(x,r);});
 originals(c,"acceptance_condition_source_refs",v.acceptance_condition_source_refs);
 if(!c.status().ok)return c.status();
 if((tag==0&&(v.accepted_purpose_ref||!v.acceptance_condition_source_refs.empty()))||
  (tag==1&&(!v.accepted_purpose_ref||v.acceptance_condition_source_refs.empty())))return c.reject("PrivateShopOperationProfileProofMismatch",v.operation_ref);
 return c.status();
}
template<class C>Status actual_labor(C&c,PrivateShopActualLaborSourceRef&v){
 c.object("source_ref",[&](auto&x){return source(x,v.source_ref);});
 if(c.status().ok&&(v.source_ref.domain!="private_shops"||v.source_ref.kind!="private_shop_labor_applied"))return c.reject("PrivateShopActualLaborAddressRequired",v.source_ref.id);
 return c.status();
}
template<class C>Status segment(C&c,PrivateShopCapabilitySegmentSource&v){
 Integer tag=static_cast<Integer>(v.index());finite(c,"tag",tag,1);
 if(!c.status().ok)return c.status();if(c.reading()){if(tag==0)v=PrivateShopActualLaborSourceRef{};else v=FixedServiceTransitionRef{};}
 if(tag==0)return c.object("actual_labor_source_ref",[&](auto&x){return actual_labor(x,std::get<PrivateShopActualLaborSourceRef>(v));});
 return c.object("fixed_service_transition_ref",[&](auto&x){return time_fields::transition_ref(x,std::get<FixedServiceTransitionRef>(v));});
}
template<class C>Status evidence(C&c,PrivateShopActivityEvidence&v){
 Integer tag=static_cast<Integer>(v.index());finite(c,"tag",tag,1);
 if(!c.status().ok)return c.status();if(c.reading()){if(tag==0)v=PrivateShopServiceApplied{};else v=PrivateShopParticipationStopped{};}
 if(tag==0){auto&r=std::get<PrivateShopServiceApplied>(v);c.object("work_period_ref",[&](auto&x){return fixed_period_fields::ref(x,r.work_period_ref);});return c.object("original_labor_receipt_ref",[&](auto&x){return source(x,r.original_labor_receipt_ref);});}
 return c.object("original_operation_pause_or_ineligibility_result_ref",[&](auto&x){return time_fields::transition_ref(x,std::get<PrivateShopParticipationStopped>(v).original_operation_pause_or_ineligibility_result_ref);});
}
template<class C>Status binding(C&c,ShopOperationCapabilityBinding&v){
 required_ref(c,"operation_ref",v.operation_ref);
 c.object("segment_source_ref",[&](auto&x){return segment(x,v.segment_source_ref);});
 required_ref(c,"actor_ref",v.actor_ref);Integer tag=static_cast<Integer>(v.role);finite(c,"role",tag,1);
 if(c.reading()&&c.status().ok)v.role=static_cast<PrivateShopOperatorRole>(tag);
 c.optional("staff_contract_ref",v.staff_contract_ref,[&](auto&x,auto&r){return source(x,r);});
 c.object("purpose_ref",[&](auto&x){return purpose(x,v.purpose_ref);});
 c.object("actual_span",[&](auto&x){x.object("from",[&](auto&y){return moment(y,v.actual_span.from);});return x.object("to",[&](auto&y){return moment(y,v.actual_span.to);});});
 originals(c,"condition_source_refs",v.condition_source_refs);
 required_ref(c,"capability_rule_id",v.capability_rule_id);schema(c,"capability_rule_version",v.capability_rule_version);
 c.field("allowed",v.allowed);c.field("efficiency_bp",v.efficiency_bp);
 c.object("activity_evidence",[&](auto&x){return evidence(x,v.activity_evidence);});
 if(!c.status().ok)return c.status();
 if((tag==0&&v.staff_contract_ref)||(tag==1&&!v.staff_contract_ref)||v.condition_source_refs.empty()||
  v.efficiency_bp<0||v.efficiency_bp>10000||take(compare_world_moments(v.actual_span.from,v.actual_span.to))>=0||
  (std::holds_alternative<PrivateShopServiceApplied>(v.activity_evidence)&&(!v.allowed||v.efficiency_bp<=0)))
  return c.reject("InvalidPrivateShopCapabilityBinding",v.operation_ref);
 if(const auto*applied=std::get_if<PrivateShopServiceApplied>(&v.activity_evidence)){
  const auto*address=std::get_if<PrivateShopActualLaborSourceRef>(&v.segment_source_ref);
  if(!address||!same_source_ref(address->source_ref,applied->original_labor_receipt_ref))return c.reject("PrivateShopAppliedSegmentAddressMismatch",v.operation_ref);
 }else{
  const auto*address=std::get_if<FixedServiceTransitionRef>(&v.segment_source_ref);
  const auto&stopped=std::get<PrivateShopParticipationStopped>(v.activity_evidence).original_operation_pause_or_ineligibility_result_ref;
  if(!address||address->transition_id!=stopped.transition_id||address->source_schema!=stopped.source_schema||v.allowed||v.efficiency_bp!=0)return c.reject("PrivateShopStoppedSegmentAddressMismatch",v.operation_ref);
 }
 return c.status();
}
template<class C>Status state(C&c,PrivateShopOperationCapabilitiesState&v){
 c.field("schema_version",v.schema_version);if(c.status().ok&&v.schema_version!=1)return c.reject("UnsupportedPrivateShopCapabilitiesVersion");
 std::size_t index=0;
 c.sequence("operation_profiles",v.operation_profiles,[&](auto&x,auto&row){profile(x,row);if(x.status().ok)for(std::size_t n=0;n<index;++n)if(row.operation_ref==v.operation_profiles[n].operation_ref)return x.reject("DuplicatePrivateShopOperationProfile",row.operation_ref);++index;return x.status();});
 index=0;
 c.sequence("period_capability_bindings",v.period_capability_bindings,[&](auto&x,auto&row){binding(x,row);
  if(x.status().ok)for(std::size_t n=0;n<index;++n){const auto&old=v.period_capability_bindings[n];if(old.operation_ref==row.operation_ref&&take(compare_world_moments(old.actual_span.from,row.actual_span.to))<0&&take(compare_world_moments(row.actual_span.from,old.actual_span.to))<0)return x.reject("OverlappingPrivateShopCapabilityBindings",row.operation_ref);}
  ++index;return x.status();});
 return c.status();
}
}
