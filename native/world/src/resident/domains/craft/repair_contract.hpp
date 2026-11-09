// Original professional repair. S9 owns actual acceptance; S7 owns fee/custody;
// condition is S9, material and money are Common, elapsed is solely S0.
#pragma once
#include "repair_policy.hpp"
#include "craft_facility_boundary_model.hpp"
#include "item_condition_head.hpp"
#include "resident/kernel/fixed_service_runtime.hpp"
namespace hanmo::resident {
struct FacilityClaimUntilRequest;struct LocalEconomyState;
struct CraftFacilityOccupancyContext;struct CraftJobsState;
struct RepairCharacterSourceContext;struct RepairSpatialSourceContext;struct RepairServiceReadContext;
struct RepairActualServiceCoverage {
 MomentSpan actual_span;std::vector<MomentOwnerSource> actual_sources;
 std::vector<StableId> actual_claim_refs;
};
class RepairOriginalRead {
public:virtual ~RepairOriginalRead()=default;
 virtual bool bound_to(const CommonStore&,const ItemInstancesState&)const noexcept=0;
 // Must read real professional acceptance, delivered custody, exclusive item
 // use, historical selected material lineage/properties, original worker
 // skill/head, workstation rights and frozen fee/return. Source-local readers
 // must not re-enter full Repair, item-condition, or S0 validation.
 // Source-local original acceptance: tangible contact, lawful customer/agent,
 // same instance and S7 fee/return/custody contract. Never demands a future Job,
 // material reservation, worker ConditionHead or service elapsed.
 virtual Status validate_repair_acceptance(const RepairAcceptedTerms&,const WorldMoment&,const RepairAcceptedSource* actual_acceptance)const=0;
 virtual Status validate_repair_unstarted_interruption(const RepairAcceptedSource&,const RepairUnstartedInterruptionSource&)const=0;
 // A true internal stop alone does not release customer custody or outstanding
 // service obligations. Reacceptance must read their original S7 disposition
 // before this real new acceptance cut (or the actual current quote).
 virtual Status validate_repair_reacceptance_after_unstarted_terminal(const RepairAcceptedSource&,const RepairUnstartedInterruptionSource&,const RepairAcceptedTerms&,const WorldMoment&,const RepairAcceptedSource* actual_new_acceptance)const=0;
 virtual Status validate_repair_terms(const RepairTerms&,const RepairJob* actual_job)const=0;
 virtual Outcome<std::vector<FixedServiceInputRef>> read_repair_start_inputs(const RepairJob&)const=0;
 virtual Outcome<RepairActualServiceCoverage> read_repair_service_coverage(const RepairJob&,const std::vector<FixedServiceInputRef>&,const MomentSpan&)const=0;
 virtual Outcome<std::vector<FixedServiceInputRef>> read_repair_control_inputs(const RepairJob&,RepairControlKind,const MomentOwnerSource&,const WorldMoment&)const=0;
 virtual Status validate_repair_control(const RepairJob&,const RepairControlSource&)const=0;
 virtual Status validate_repair_completion_fees(const RepairJob&,const RepairCompletedSource&)const=0;
 virtual Status validate_repair_interruption(const RepairJob&,const RepairInterruptedSource&)const=0;
 virtual Outcome<CraftFacilityEligibility> read_current_repair_facility_eligibility(const RepairJob&)const=0;
 virtual Outcome<CraftFacilityEligibility> read_repair_facility_eligibility_at_boundary(const RepairJob&,const CraftFacilityBoundarySource&)const=0;
 virtual Status validate_repair_return(const RepairJob&)const=0;
 // Original accepted custody/safety policy and actual same-inventory placement
 // back into lawful custody. No customer return, ownership change, or fake flow.
 virtual Status validate_repair_completed_workstation_clearance(const RepairJob&,const RepairCompletedSource&)const=0;
};
class RepairEffectsWrite {
public:virtual ~RepairEffectsWrite()=default;
 virtual bool bound_to(const CommonStore&,const ItemInstancesState&)const noexcept=0;
 virtual Outcome<std::vector<StableId>> adopt_repair_claims_in_candidate(const RepairJob&)=0;
 virtual Outcome<std::vector<StableId>> apply_repair_control_claims_in_candidate(const RepairJob&,const RepairControlSource&)=0;
 // Original service custody remains until its actual return/settlement owner.
 // Close only after actual Cleared; interrupted objects retain finite physical
 // responsibility under the original placement, even after the Action ends.
 virtual Status apply_repair_terminal_facility_disposition_in_candidate(const RepairJob&,const SourceRef&,const WorldMoment&)=0;
};
struct RepairReadContext {
 const CommonStore& common;const CommonCatalog& common_catalog;
 const ItemInstancesState& items;const LotPropertiesState& properties;const ItemServiceCatalog& item_catalog;
 const RepairPolicyCatalog& policies;const ActionElapsedStore& elapsed;
 const RepairOriginalRead& original;const ItemConditionReadContext& conditions;
 const CurrentMomentRead& clock;const TimeScaleRef& scale;ExactArithmeticBudget arithmetic_budget{};
 const CraftFacilityOccupancyContext* occupancy=nullptr;
 const RepairCharacterSourceContext* character_sources=nullptr;
 const RepairSpatialSourceContext* spatial_sources=nullptr;
 const RepairServiceReadContext* commercial_sources=nullptr;
};
struct RepairWriteContext {
 const RepairReadContext& read;RepairEffectsWrite& effects;
 const CurrentReadStampRead& stamps;ReadStampAuthority& stamp_authority;const DomainReadVersionSource& versions;
 CraftJobsState* facility_occupancy_owner=nullptr;
 LocalEconomyState* original_economy=nullptr;
};
struct RepairStartQuote {RepairTerms terms;WorldIdentity identity;std::vector<ReadVersion> actual_inventory_versions;};
SourceRef repair_job_source(const StableId&);SourceRef repair_completed_source(const StableId&);
SourceRef repair_control_source(const StableId&);SourceRef repair_interrupted_source(const StableId&);
ActionTimeRegistration repair_action_time_registration();
Outcome<RepairStartQuote> preview_repair_start(const RepairReadContext&,const RepairTerms&);
Outcome<StableId> start_repair_in_candidate(CommonStore&,ItemInstancesState&,ActionElapsedStore&,const RepairWriteContext&,const RepairStartQuote&);
Status provide_repair_service_interval_in_candidate(CommonStore&,ItemInstancesState&,ActionElapsedStore&,const RepairWriteContext&,const StableId& job_id,const MomentSpan&);
Outcome<StableId> complete_repair_in_candidate(CommonStore&,ItemInstancesState&,ActionElapsedStore&,const RepairWriteContext&,const StableId& job_id);
Outcome<StableId> apply_repair_control_in_candidate(CommonStore&,ItemInstancesState&,ActionElapsedStore&,const RepairWriteContext&,const StableId& job_id,RepairControlKind,const MomentOwnerSource& actual_cause);
// 实际到期与普通控制共用原暂停 writer；typed 原 episode 仅此内部入口赋值。
Outcome<StableId> apply_repair_facility_until_pause_in_candidate(CommonStore&,ItemInstancesState&,ActionElapsedStore&,const RepairWriteContext&,const StableId&,const FacilityClaimUntilRequest&);
Status validate_original_repair_control(const RepairReadContext&,const RepairJob&,const RepairControlSource&);
Outcome<StableId> interrupt_repair_in_candidate(CommonStore&,ItemInstancesState&,ActionElapsedStore&,const RepairWriteContext&,const StableId& job_id,const MomentOwnerSource& actual_cause);
Status adopt_repair_return_in_candidate(CommonStore&,ItemInstancesState&,ActionElapsedStore&,const RepairWriteContext&,const StableId& job_id,const SourceRef& actual_return_source);
Outcome<const RepairJob*> read_original_repair_job(const RepairReadContext&,const StableId&);
// Source-local completion validates original adopted condition prefix, actual
// service, material and fee effects. It never validates its own condition result.
Outcome<ActualItemRepairCompletion> read_actual_repair_completed(const RepairReadContext&,const SourceRef&,const StableId& instance_id);
Status validate_repair_jobs(const RepairReadContext&);
class NativeRepairActionTimeRead final:public ActionTimeOriginalSourceRead {
 const RepairReadContext& x_;
public:explicit NativeRepairActionTimeRead(const RepairReadContext&x):x_(x){}
 Status enumerate_action_time_owners(const ActionTimeRegistration&,ActionTimeOwnerIdSink&)const override;
 Outcome<ActionTimeOwnerProof> read_action_time_owner(const ActionTimeRegistration&,const StableId&)const override;
 Outcome<WorldMoment> read_action_time_source(const ElapsedTimeSource&)const override;
 Status validate_fixed_action_sources(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const ActionElapsedState&,const std::vector<FixedServiceTransition>&)const override;
 Status validate_shared_action_sources(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const SharedWorkElapsedState&)const override;
 Outcome<RunStop> read_action_continuation_stop(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const WorldMoment&)const override;
 Outcome<OriginalActionRemaining> read_original_action_remaining(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const WorldMoment&)const override;
 Outcome<StableId> read_gate_result_id(const SharedWorkGateResult&)const override;
 Outcome<SharedWorkGateResult> read_original_gate_result(const StableId&)const override;
 Status validate_original_gate_result(const SharedWorkGateResult&)const override;
};
}
