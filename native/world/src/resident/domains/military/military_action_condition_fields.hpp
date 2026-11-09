// One original head schema, embedded by the real accepting source only.
#pragma once
#include "military_action_condition_model.hpp"
#include "resident/domains/actor_foundation/condition_head_codec_fields.hpp"
#include "resident/kernel/common_fact_prefix_codec_fields.hpp"
#include "resident/kernel/exact_struct_codec.hpp"
namespace hanmo::resident::military_action_condition_fields {
template<class C>Status value(C& c,MilitaryActionAcceptanceConditions1& v){
 return c.sequence("participant_heads",v.participant_heads,[](auto& child,auto& head){return actor_condition_codec::head(child,head);});
}

template<class C>Status value(C& c,MilitaryActionEndpointObservation2& v){
 c.object("common_prefix",[&](auto& child){return common_fact_prefix_fields::fields(child,v.common_prefix,[](auto& source_fields,auto& source){return exact_fields::source(source_fields,source);});});
 c.object("condition_prefix",[&](auto& child){child.field("applied_receipt_count",v.condition_prefix.applied_receipt_count);child.optional("last_applied_receipt",v.condition_prefix.last_applied_receipt,[](auto& ref_fields,auto& ref){return ref_fields.field("result_id",ref.result_id);});if(child.status().ok&&(v.condition_prefix.applied_receipt_count<0||bool(v.condition_prefix.applied_receipt_count)!=bool(v.condition_prefix.last_applied_receipt)))return child.reject("MilitaryActionConditionAppliedPrefixInvalid");return child.status();});
 return c.sequence("participant_heads",v.participant_heads,[](auto& child,auto& head){return actor_condition_codec::head(child,head);});
}
}
