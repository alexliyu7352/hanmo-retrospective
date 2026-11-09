// v1c §3.2 + FH §5.3B. Only the original Care owner holds these rows.
#pragma once
#include "care_arrangement.hpp"
#include "resident/domains/growth/growth_fixed_service_model.hpp"
#include "resident/kernel/fixed_service_work_period.hpp"
namespace hanmo::resident {
struct CareHandoverActorCommand {
 StableId actor_id;LifeActualCommandBinding command_binding;LifeActorRoute route_basis;
 StableId accepted_terms_ref;std::vector<MomentOwnerSource> authority_source_refs;
};
struct CareHandoverAcceptedObligation {
 SourceRef original_obligation_ref;Integer original_obligation_source_schema=0;
 ActorFoundationRuleRef accepted_rule_ref;
};
struct CareHandoverConditionChanged {ConditionReceiptRefV2 condition_receipt_ref;};
struct CareHandoverDeathChanged {DeathRef death_ref;};
struct CareHandoverPositionChanged {SourceRef source_ref;Integer source_schema=0;};
struct CareHandoverOccupationChanged {SourceRef source_ref;Integer source_schema=0;};
struct CareHandoverFacilityChanged {SourceRef source_ref;Integer source_schema=0;};
struct CareHandoverAuthorityOrTermsChanged {SourceRef source_ref;Integer source_schema=0;};
using CareHandoverDependencyChange=std::variant<CareHandoverConditionChanged,CareHandoverDeathChanged,
 CareHandoverPositionChanged,CareHandoverOccupationChanged,CareHandoverFacilityChanged,CareHandoverAuthorityOrTermsChanged>;
struct CareHandoverDependencyChanged {CareHandoverDependencyChange change;ActorFoundationRuleRef continuation_rule_ref;};
struct CareHandoverOriginalOutcome {SourceRef outcome_source_ref;Integer outcome_source_schema=0;};
using CareHandoverBoundaryCause=std::variant<CareHandoverActorCommand,CareHandoverAcceptedObligation,
 CareHandoverDependencyChanged,CareHandoverOriginalOutcome>;
enum class CareHandoverBoundaryKind {started,suspended,resumed,input_rebound,completed,stopped};
struct CareHandoverExecutionBoundary1 {
 StableId boundary_id;Integer source_schema=1;StableId execution_ref;Integer boundary_ordinal=0;
 std::optional<StableId> previous_boundary_ref;CareHandoverBoundaryKind kind=CareHandoverBoundaryKind::started;
 WorldMoment occurred_at;ActorFoundationRuleRef transition_rule_ref;CareHandoverBoundaryCause cause;
 std::vector<FrozenFacilityCapabilityInputs> worker_capability_inputs;
 std::vector<MomentOwnerSource> actual_participation_source_refs,facility_use_binding_refs;
 FieldOrder field_order;
};
struct CareHandoverWorkPeriod2 {
 StableId source_id;Integer source_schema=2;StableId execution_ref;WorkPeriodRef2 work_period_ref;
 WorldMoment provided_at;std::vector<FrozenFacilityCapabilityInputs> worker_capability_inputs;
 std::vector<ConditionReceiptRefV2> worker_condition_receipt_refs;FieldOrder field_order;
};
struct CareHandoverExecution {
 StableId handover_id,requirement_id,accepted_care_terms_ref,original_action_ref;
 WorkExecutionBinding execution_binding;ActorFoundationRuleRef handover_rule_ref;
 std::vector<MomentOwnerSource> actual_placement_result_refs;
 std::optional<SourceRef> completion_event_ref;std::optional<MomentOwnerSource> terminal_source_ref;
 // Only selected care/5 owns this tail. No clock, progress, or phase mirror.
 std::vector<CareHandoverWorkPeriod2> work_period_sources;
 std::vector<CareHandoverExecutionBoundary1> handover_execution_boundaries;
};
SourceRef care_handover_execution_source(const StableId&);
SourceRef care_handover_boundary_source(const StableId&);
SourceRef care_handover_period_source(const StableId&);
Outcome<Integer> care_handover_source_schema(const SourceRef&);
Status validate_care_handover_boundary_shape(const CareHandoverExecutionBoundary1&);
Status validate_care_handover_period_shape(const CareHandoverWorkPeriod2&);
Status validate_care_handover_execution_shape(const CareHandoverExecution&,Integer selected_version);
struct CareHandoverSourceContext {
 const CareAcceptanceContext& acceptance;const OrderedTable<CareHandoverExecution>& executions;
 const ActionElapsedStore& elapsed;const TimeScaleRef& time_scale;
};
// Mandatory at install, restore and publication. Every original boundary must
// have its exact S0 adoption; no completion-due allowance exists at this gate.
Status validate_care_handover_time_source_closure(const CareHandoverSourceContext&);
// Source-local reads establish original identity, chronology, accepted H and
// sole FSL credit. They intentionally do not recurse through their own body,
// facility effects or later CoverageSatisfied. Full owner validation must also
// check independent authority, participation, input changes and actual effects.
// Only an already-written actual Completed at the exact full-credit due may
// precede its S0 Terminal inside a private candidate. Stable closure above is
// mandatory; no caller-selectable skip, success cache or alternate source.
Outcome<const CareHandoverExecutionBoundary1*> read_care_handover_boundary_for_original_effect(
 const CareHandoverSourceContext&,const SourceRef&,Integer source_schema);
Outcome<const CareHandoverWorkPeriod2*> read_care_handover_period_for_original_effect(
 const CareHandoverSourceContext&,const SourceRef&,Integer source_schema);
// S1's original placement writer reads this before Care authors Completed.
// Historical S1 validation reads the identical last actual period afterward.
// This establishes the original accepted identities and full earned H only;
// S1 must independently apply its actual lawful placement/dependency result.
// It intentionally does not call CareCoverageOriginalRead, S1 or body effects.
struct CareHandoverPlacementInput {
 const CareHandoverExecution* execution=nullptr;
 const AcceptedCaregiver* accepted=nullptr;
 const CareHandoverWorkPeriod2* last_actual_period=nullptr;
 const CareTemporalEvent* actual_completion=nullptr;
};
Outcome<CareHandoverPlacementInput> read_care_handover_placement_input(
 const CareHandoverSourceContext&,const SourceRef& execution,Integer source_schema);
// Source-local causal cut for S1's actual binding_change_results/2 result.
// The period remains internal: compare its verified original allocation to the
// real S1 writer's allocation, then Common append order to actual Completed.
Status validate_care_handover_placement_adoption_cut(const CareHandoverSourceContext&,
 const SourceRef& execution,Integer execution_schema,
 const MomentOwnerSource& actual_placement_result,const WorldMoment& adopted_at);
}
