// P22/P23: original Repair rows embedded only in item_instances/3.
#pragma once
#include "item_condition_model.hpp"
#include "craft_facility_boundary_model.hpp"
#include "resident/model/common_model.hpp"
#include "resident/kernel/world_timeline.hpp"
#include "resident/facility_claims/facility_claims.hpp"
#include "resident/domains/actor_foundation/actor_foundation_refs.hpp"
namespace hanmo::resident {
struct RepairPolicyRef {StableId policy_id;Integer version=0;};
struct RepairMaterialRequirement {StableId definition_id;Integer quantity=0;};
// The ordinary explicitly selected rule restores the original existing maximum.
// Other maximum-changing rules require their own original compiled handler.
enum class RepairConditionRule {restore_existing_maximum};
struct RepairPolicy {
 StableId policy_id;Integer version=0;std::vector<StableId> applicable_definitions;
 StableId required_skill_id;Integer minimum_skill_level=0,minimum_equipment_level=0;
 StableId workstation_slot_kind;std::vector<RepairMaterialRequirement> materials;
 Integer duration_half_days=0,fee_qian=0;RepairConditionRule condition_rule=RepairConditionRule::restore_existing_maximum;
 StableId fee_policy_ref,custody_policy_ref;
};
struct RepairPolicyCatalog {OrderedTable<RepairPolicy> policies;};
struct RepairMaterialSelection {Integer material_index=0;CargoSelection selected;Owner legal_owner;};
struct RepairReservedMaterial {Integer selection_ordinal=0;CargoSelection original,held;StableId definition_id;Owner legal_owner;std::optional<LotPropertiesRef> properties_ref;};
// Actual accepted service scope, before any Action or work input is adopted.
// Fee/return sources are the original S7 accepted contract for this instance,
// never a shared author policy or a future S9 execution identity.
struct RepairAcceptedTerms {
 RepairPolicyRef policy_ref;StableId instance_id,worker_id;
 CargoSelection original_item;Owner legal_owner;ItemConditionHead condition_head;
 FacilitySlotRef workstation;MomentOwnerSource actual_contact_ref,owner_authority_ref;
 MomentOwnerSource fee_terms_ref,return_terms_ref;
 std::optional<MomentOwnerSource> delivered_custody_source_ref;
 Integer terms_revision=0;
};
struct RepairAcceptedSource {StableId id;RepairAcceptedTerms terms;WorldMoment accepted_at;};
struct RepairUnstartedInterruptionSource {
 StableId id;MomentOwnerSource accepted_service_ref;WorldMoment occurred_at;MomentOwnerSource actual_cause;
};
struct RepairTerms {
 RepairPolicyRef policy_ref;StableId instance_id,worker_id;
 MomentOwnerSource accepted_service_ref,custody_source_ref;
 CargoSelection original_item;Owner legal_owner;ReservationRef service_holder;
 ItemConditionHead condition_head;ActorConditionHead worker_condition_head;
 std::vector<RepairMaterialSelection> materials;FacilitySlotRef workstation;
 MomentOwnerSource fee_terms_ref,return_terms_ref;
 WorldMoment accepted_at;Integer terms_revision=0;
};
enum class RepairJobPhase {reserved,working,suspended,completed,interrupted};
struct RepairJob {
 StableId id,action_id;RepairTerms terms;WorldMoment started_at;
 std::vector<RepairReservedMaterial> material_reservations;
 std::vector<StableId> claim_refs,control_refs;
 std::vector<StableId> facility_occupancy_event_refs;
 std::optional<StableId> completed_ref,interrupted_ref,returned_ref;
 RepairJobPhase phase=RepairJobPhase::reserved;
};
enum class RepairControlKind {suspended,resumed,input_rebound};
struct RepairControlSource {
 StableId id,job_id;RepairControlKind kind=RepairControlKind::suspended;WorldMoment occurred_at;
 MomentOwnerSource actual_cause;std::vector<FixedServiceInputRef> adopted_inputs;
 std::optional<CraftFacilityClaimUntil> facility_until;
};
struct RepairCompletedSource {
 StableId id,job_id;WorldMoment occurred_at;
 std::vector<CargoSelection> consumed_materials;
 std::vector<MomentOwnerSource> actual_fee_sources;
 std::optional<StableId> condition_result_ref;
};
struct RepairInterruptedSource {StableId id,job_id;WorldMoment occurred_at;MomentOwnerSource actual_cause;};
struct RepairJobsState {
 OrderedTable<RepairAcceptedSource> acceptances;
 OrderedTable<RepairJob> jobs;
 OrderedTable<RepairControlSource> controls;OrderedTable<RepairCompletedSource> completions;
 OrderedTable<RepairInterruptedSource> interruptions;
 OrderedTable<RepairUnstartedInterruptionSource> unstarted_interruptions;
};
}
