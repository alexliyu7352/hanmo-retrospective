// Sole ordinary field sequence for the original Repair condition read output.
// This ephemeral output is not saved, and has no durable-row raw exemption.
#pragma once
#include "item_condition_contract.hpp"
namespace hanmo::resident::item_condition_output_fields {
template<class C,class V>void repair_completion_fields(C&c,V&v){
 c("source_fact_ref",v.source_fact_ref);c("source_activity_ref",v.source_activity_ref);
 c("instance_id",v.instance_id);c("repair_policy_id",v.repair_policy_id);
 c("version_before",v.version_before);c("durability_after",v.durability_after);
 c("max_durability_after",v.max_durability_after);c("occurred_at",v.occurred_at);
 c("resource_effect_refs",v.resource_effect_refs);
}
template<class C>void fields(C&c,const ActualItemRepairCompletionBuildView&v){repair_completion_fields(c,v);}
template<class C>void fields(C&c,const ActualItemRepairCompletion&v){repair_completion_fields(c,v);}
}
