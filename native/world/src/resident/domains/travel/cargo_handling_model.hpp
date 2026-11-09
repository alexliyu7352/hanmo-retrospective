// 固定cargo-handling §2：原S1四表，不新增库存、人口或独立时钟。
#pragma once
#include "spatial_carrier_model.hpp"
#include "resident/kernel/world_timeline.hpp"
namespace hanmo::resident {
enum class CargoHandlingOperation { load,unload };
enum class CargoHandlingLaborMode { named_workers,cohort_workers };
struct CargoHandlingRule {
    StableId id; Integer version=1; std::vector<StableId> allowed_carrier_kinds;
    std::vector<CargoHandlingOperation> allowed_operations;
    std::vector<StableId> allowed_business_kinds,allowed_cargo_classes;
    Integer handling_half_days=0,required_workers=0; std::vector<CargoHandlingLaborMode> labor_modes;
    std::optional<StableId> required_capability_purpose_ref,facility_requirement_ref;
    SourceRef cost_rule_ref,interruption_rule_ref;
};
struct NamedCargoHandlingWorkers { StableId lead_actor_id;std::vector<StableId> participant_actor_ids; };
struct CohortCargoHandlingWorkers { std::vector<SourceRef> allocation_request_refs; };
using CargoHandlingLaborSelection=std::variant<NamedCargoHandlingWorkers,CohortCargoHandlingWorkers>;
struct CargoHandlingTerms {
    StableId terms_id; SourceRef business_source; Integer batch_ordinal=0;
    CargoHandlingOperation operation=CargoHandlingOperation::load; SpatialCarrierRef carrier;
    StableId source_inventory_id,destination_inventory_id; std::vector<CargoSelection> cargo;
    std::vector<Owner> matching_owners; ReservationRef source_reservation;std::optional<Owner> destination_owner;
    StableId handling_rule_ref;Integer accepted_rule_version=0;
    SourceRef actual_site_or_anchor_ref,carrier_composition_source_ref,source_custody_ref,receiver_acceptance_source_ref;
    SourceRef expense_authority_ref,cost_terms_ref; CargoHandlingLaborSelection labor_selection;
    std::optional<SourceRef> facility_claim_ref; WorldMoment accepted_at;
    // Actual original start callback output. Only references live here; the
    // selected physical Terms row owns each corresponding receipt once.
    std::vector<SourceRef> actual_start_cost_sources;
};
enum class CargoHandlingPhase { working,suspended,ready_to_transfer,completed,interrupted,cancelled };
struct NamedCargoHandlingTiming { StableId action_ref;ActionElapsedEntryRef action_elapsed_source_ref; };
enum class CargoCohortServiceState { running,suspended,service_completed };
struct CohortCargoHandlingTiming {
    TimeScaleRef scale_ref;WorkUnits required_units,credited_units;WorldMoment last_settled_at;
    std::vector<SourceRef> participation_source_refs;CargoCohortServiceState state=CargoCohortServiceState::running;
};
struct CargoHandlingExecution {
    StableId execution_id,terms_ref;SourceRef business_source;Integer batch_ordinal=0;
    CargoHandlingPhase phase=CargoHandlingPhase::working;WorldMoment started_at;
    std::variant<NamedCargoHandlingTiming,CohortCargoHandlingTiming> timing;
    std::vector<SourceRef> labor_binding_refs;std::optional<SourceRef> facility_claim_ref;
    Integer progress_source_revision=0;std::optional<StableId> work_result_ref,terminal_result_ref;
    std::optional<SourceRef> waiting_cause_source_ref;
};
struct CargoHandlingWorkResult {
    StableId result_id,execution_ref,terms_ref;WorldMoment service_started_at,service_completed_at;
    std::variant<ActionElapsedEntryRef,SourceRef> timing_source_ref;std::vector<SourceRef> labor_coverage_source_refs,facility_use_source_refs,cost_source_refs;
    StableId rule_ref;Integer rule_version=0;
};
enum class CargoHandlingResultStatus { completed,interrupted,cancelled };
struct CargoHandlingResult {
    StableId result_id,execution_ref,terms_ref;SourceRef business_source;Integer batch_ordinal=0;
    CargoHandlingResultStatus status=CargoHandlingResultStatus::completed;WorldMoment occurred_at;
    std::optional<StableId> work_result_ref;
    std::vector<SourceRef> actual_transfer_source_refs,actual_resource_effect_refs,actual_lot_flow_refs;
    std::vector<SourceRef> carrier_manifest_change_refs,original_business_result_refs;
    std::vector<SourceRef> released_labor_source_refs,released_claim_source_refs;
    std::optional<SourceRef> reason_source_ref;
};
struct CargoHandlingState {
    OrderedTable<CargoHandlingTerms> terms;OrderedTable<CargoHandlingExecution> executions;
    OrderedTable<CargoHandlingWorkResult> work_results;OrderedTable<CargoHandlingResult> results;
};
}
