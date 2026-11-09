// Original owning field callbacks shared by codec and raw retention.
#pragma once
#include "facility_condition_contract.hpp"
namespace hanmo::resident::facility_condition_fields {
template<class A>void fields(A& a,FacilityBaseSlotCapacity& v){a("slot_kind",v.slot_kind);a("capacity_unit_id",v.capacity_unit_id);a("units",v.units);}
template<class A>void fields(A& a,ProductionFacilityCondition& v){a("facility_ref",v.facility_ref);a("condition_revision",v.condition_revision);a("integrity",v.integrity);a("max_integrity",v.max_integrity);a("base_capacity_by_slot",v.base_capacity_by_slot);a("base_asset_capacity_units",v.base_asset_capacity_units);a("damage_policy_ref",v.damage_policy_ref);a("repair_policy_ref",v.repair_policy_ref);a("last_change_ref",v.last_change_ref);}
template<class A>void fields(A& a,ProductionFacilityConditionSeed& v){a("condition_ref",v.condition_ref);a("original_asset_ref",v.original_asset_ref);a("kind",v.kind);a("initial_integrity",v.initial_integrity);a("max_integrity",v.max_integrity);a("base_asset_capacity_units",v.base_asset_capacity_units);a("base_capacity_by_slot",v.base_capacity_by_slot);a("damage_policy_ref",v.damage_policy_ref);a("repair_policy_ref",v.repair_policy_ref);a("original_installation_seed_ref",v.original_installation_seed_ref);}
template<class A>void fields(A& a,FacilityAttackResultRef& v){a("source",v.source);a("source_schema",v.source_schema);}
template<class A>void fields(A& a,CompletedFacilityRepairRef& v){a("source",v.source);a("source_schema",v.source_schema);}
template<class A>void fields(A& a,FacilitySlotCapacityChange& v){a("slot_kind",v.slot_kind);a("before",v.before);a("after",v.after);}
template<class A>void fields(A& a,FacilityConditionChange& v){a("change_id",v.change_id);a("facility_ref",v.facility_ref);a("source",v.source);a("occurred_at",v.occurred_at);a("old_integrity",v.old_integrity);a("new_integrity",v.new_integrity);a("old_revision",v.old_revision);a("new_revision",v.new_revision);a("capacity_changes",v.capacity_changes);a("interrupted_activity_results",v.interrupted_activity_results);a("claim_release_results",v.claim_release_results);}
}
