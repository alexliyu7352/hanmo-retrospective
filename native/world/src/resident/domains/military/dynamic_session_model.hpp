// S2 dynamic_military/2 session current and its finite actual transition sources.
// Roster, named-body condition, vessel integrity and spatial cursor stay at their owners.
#pragma once
#include "military_local_motion.hpp"
namespace hanmo::resident {
struct DynamicMilitaryStreamIdentity {StableId session_ref;Integer generation=0;SourceRef initialization_source_ref;};
enum class DynamicMilitaryRandomStage {strike,commander_exposure,original_rout_choice,original_ai_decision};
struct DynamicMilitaryRandomSlot {
    SourceRef effect_ref;DynamicMilitaryRandomStage stage=DynamicMilitaryRandomStage::strike;
    DynamicMilitaryStreamIdentity stream_identity;StableId algorithm_version;
    Integer requested_min=0,requested_max=0;StableId seed,before_state,after_state;
    std::uint64_t before_draw_ordinal=0,after_draw_ordinal=0;Integer draw_result=0;
};
struct DynamicMilitarySessionRandom {
    DynamicMilitaryStreamIdentity stream_identity;StableId algorithm_version,seed,current_state;
    std::uint64_t draw_ordinal=0;SourceRef last_commit_source_ref;
};
enum class DynamicMilitaryStatusKind {inspired,suppressed,hindered,exposed};
struct DynamicMilitaryStatus {DynamicMilitaryStatusKind kind=DynamicMilitaryStatusKind::inspired;SourceRef retained_activation_ref;WorldMoment valid_from,expires_at;};
struct DynamicNoPosture {};
struct DynamicDefendPosture {SourceRef activation_ref;WorldMoment valid_from,expires_at;Integer guard_value=0;std::vector<SourceRef> qualification_source_refs;};
struct DynamicLearnedGuardPosture {SourceRef activation_ref;WorldMoment valid_from,expires_at;SourceRef learned_tactic_entry_ref;std::vector<SourceRef> qualification_source_refs;};
struct DynamicFeintPosture {SourceRef activation_ref;WorldMoment valid_from,expires_at;std::vector<SourceRef> qualification_source_refs;};
struct DynamicSupportToken {SourceRef activation_ref;StableId supporter_unit_ref,target_unit_ref;WorldMoment valid_from,expires_at;std::vector<SourceRef> qualification_source_refs;std::optional<SourceRef> consumed_by_effect_ref;};
struct DynamicSupportPosture {DynamicSupportToken token;};
using DynamicMilitaryPosture=std::variant<DynamicNoPosture,DynamicDefendPosture,DynamicLearnedGuardPosture,DynamicFeintPosture,DynamicSupportPosture>;
struct DynamicMilitaryBodyTacticalState {MilitaryLocalBodyRef local_body_ref;SourceRef spatial_binding_ref;Integer heading=0;SourceRef heading_source_ref;};
struct DynamicMilitaryUnitTacticalState {StableId unit_ref;MilitaryLocalBodyRef body_ref;SourceRef entry_snapshot_ref;Integer tactic_points=0;SourceRef tactic_points_source_ref;std::vector<DynamicMilitaryStatus> statuses;DynamicMilitaryPosture posture;std::vector<SourceRef> qualification_source_refs;};
struct DynamicNotHold {};
struct DynamicHoldUnqualified {SourceRef last_condition_source_ref;};
struct DynamicHolding {WorldMoment held_since;SourceRef qualification_transition_ref;};
using DynamicMilitaryHoldState=std::variant<DynamicNotHold,DynamicHoldUnqualified,DynamicHolding>;
struct DynamicMilitaryEscapedUnit {StableId unit_ref;SourceRef actual_exit_source_ref;};
struct DynamicMilitaryObjectiveCurrent {StableId objective_definition_ref;WorldMoment started_at,deadline_at;DynamicMilitaryHoldState hold_state;std::vector<DynamicMilitaryEscapedUnit> escaped_units;SourceRef last_transition_source_ref;std::optional<SourceRef> outcome_source_ref;};
struct DynamicDecisionAwaitingTrigger {std::optional<SourceRef> last_closed_decision_ref;};
struct DynamicDecisionPending {SourceRef trigger_source_ref;};
struct DynamicDecisionCommitted {SourceRef decision_source_ref;std::vector<SourceRef> adopted_intent_refs;};
using DynamicMilitaryDecisionPhase=std::variant<DynamicDecisionAwaitingTrigger,DynamicDecisionPending,DynamicDecisionCommitted>;
struct DynamicMilitaryDecisionCurrent {StableId controller_ref;Integer control_binding_generation=0;DynamicMilitaryDecisionPhase phase;};
struct DynamicMilitaryActive {};
struct DynamicMilitaryTerminal {SourceRef outcome_source_ref;WorldMoment closed_at;std::vector<SourceRef> unresolved_committed_effect_refs;};
using DynamicMilitarySessionLifecycle=std::variant<DynamicMilitaryActive,DynamicMilitaryTerminal>;
struct DynamicMilitarySessionCurrent2 {
    StableId session_ref;Integer generation=0;StableId definition_lock_ref;SourceRef initialization_source_ref;
    WorldMoment opened_at;DynamicMilitarySessionLifecycle lifecycle;
    std::vector<DynamicMilitaryBodyTacticalState> body_tactical_states;
    std::vector<DynamicMilitaryUnitTacticalState> unit_tactical_states;
    DynamicMilitaryObjectiveCurrent objective_current;std::vector<DynamicMilitaryDecisionCurrent> decision_current;
    DynamicMilitarySessionRandom session_rng;SourceRef last_session_transition_ref;
};
struct DynamicMilitaryUnitEntry {StableId unit_ref;MilitaryLocalBodyRef body_ref;SourceRef entry_snapshot_ref;Integer initial_tactic_points=0;SourceRef original_point_initialization_source_ref;};
struct DynamicMilitaryBodyEntry {MilitaryLocalBodyRef body_ref;Integer initial_heading=0;SourceRef actual_heading_source_ref;};
struct DynamicSessionInitialized {
    SourceRef entry_or_contact_source_ref;StableId definition_lock_ref;
    std::vector<DynamicMilitaryUnitEntry> unit_entry_rows;std::vector<DynamicMilitaryBodyEntry> body_entry_rows;
    StableId objective_definition_ref;SourceRef initial_objective_condition_ref,initial_rng_source_ref;
    StableId initial_rng_seed,initial_rng_state;std::uint64_t initial_draw_ordinal=0;
};
enum class DynamicObjectiveQualification {unqualified,holding};
enum class DynamicObjectiveCause {actual_entry,motion_boundary,deployment,contest_changed,unit_qualification_changed,actual_exit,objective_gate};
struct DynamicObjectiveQualificationTransition {
    StableId objective_definition_ref;DynamicObjectiveQualification from_state=DynamicObjectiveQualification::unqualified,to_state=DynamicObjectiveQualification::unqualified;
    std::optional<SourceRef> previous_holding_source_ref;DynamicObjectiveCause cause_kind=DynamicObjectiveCause::actual_entry;
    std::vector<SourceRef> cause_source_refs;std::vector<StableId> actual_qualified_unit_refs,actual_contesting_unit_refs;
};
enum class DynamicPostureCloseReason {expired,eligibility_lost,actual_handoff,replaced,session_terminal};
struct DynamicPostureClosed {StableId unit_ref;SourceRef activation_ref;DynamicPostureCloseReason reason=DynamicPostureCloseReason::expired;SourceRef expiry_or_cause_ref;std::optional<SourceRef> replacement_activation_ref;};
struct DynamicStatusExpired {StableId unit_ref;DynamicMilitaryStatusKind status_kind=DynamicMilitaryStatusKind::inspired;SourceRef retained_activation_ref;WorldMoment scheduled_expires_at;};
struct DynamicSessionDecisionCommitted {StableId controller_ref;Integer control_binding_generation=0;SourceRef trigger_source_ref,knowledge_snapshot_ref;StableId policy_definition_ref;std::vector<SourceRef> selected_intent_refs;std::vector<DynamicMilitaryRandomSlot> rng_slots;};
struct DynamicSessionTerminated {SourceRef objective_outcome_source_ref;std::vector<SourceRef> actual_exit_or_stay_source_refs,closed_posture_refs,retained_committed_effect_refs;};
using DynamicSessionTransitionPayload=std::variant<DynamicSessionInitialized,DynamicObjectiveQualificationTransition,DynamicPostureClosed,DynamicStatusExpired,DynamicSessionDecisionCommitted,DynamicSessionTerminated>;
struct DynamicSessionStateSource2 {StableId id,session_ref;Integer generation=0;WorldMoment effective_at;DynamicSessionTransitionPayload payload;};
} // These values are emitted in the fixed session_current_states/session_state_sources fields.
