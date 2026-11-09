// S2 dynamic_military/2 actions: one original tactical service owner. These are
// not Common Actor Actions and do not create a second S0 ActionElapsed record.
#pragma once
#include "dynamic_session_model.hpp"
namespace hanmo::resident {
enum class MilitaryTacticalWorkStage { prepare,recovery };
struct MilitaryTacticalWorkBinding {
    StableId session_ref;Integer generation=0;StableId action_ref;
    MilitaryTacticalWorkStage stage=MilitaryTacticalWorkStage::prepare;
};
enum class DynamicMilitaryActionKind { attack,defend,wait,tactic,gunnery,board,ram };
enum class DynamicMilitaryCommitKind { at_effect,projectile_release };
enum class DynamicMilitaryDamageLeaf { legacy_factors,commanded_factors };
struct MilitaryActionStatusDurations2 {
    std::optional<HalfDayDuration> inspired,suppressed,hindered,exposed;
};
// Field order is fixed S2 section 2.1. No duration defaults or round conversion.
struct MilitaryActionTimingDefinition2 {
    StableId id;Integer version=0;DynamicMilitaryActionKind action_kind=DynamicMilitaryActionKind::attack;
    HalfDayDuration prepare_duration;std::optional<HalfDayDuration> recovery_duration;
    DynamicMilitaryCommitKind commit_kind=DynamicMilitaryCommitKind::at_effect;
    std::optional<HalfDayDuration> flight_duration,posture_duration;
    MilitaryActionStatusDurations2 status_durations;
    StableId original_troop_or_weapon_ref;std::optional<StableId> original_tactic_ref;
    DynamicMilitaryDamageLeaf original_damage_leaf=DynamicMilitaryDamageLeaf::legacy_factors;
    StableId geometry_ref,resource_basis_ref,objective_definition_ref;
};
struct DynamicMilitaryActionCatalog2 {OrderedTable<MilitaryActionTimingDefinition2> timings;};
// These actual original qualification sources cover the entire positive span.
// They do not contain caller-supplied credit, rates or elapsed success flags.
struct MilitaryTacticalServiceSpan2 {
    MomentSpan span;std::vector<MomentOwnerSource> participation_sources;
};
struct MilitaryTacticalService2 {
    MilitaryTacticalWorkBinding work_binding_ref;
    WorldMoment started_at,last_settled_at;
    WorkUnits required_units,credited_units;
    std::vector<MilitaryTacticalServiceSpan2> actual_service;
    std::optional<SourceRef> closed_by_source;
};
struct DynamicTacticalPreparing {};
struct DynamicTacticalUncommitted {};
struct DynamicTacticalReleased {SourceRef release_source_ref;WorldMoment released_at,impact_due_at;std::optional<SourceRef> impact_source_ref;};
struct DynamicTacticalApplied {SourceRef effect_source_ref;WorldMoment effective_at;};
using DynamicTacticalEffectState=std::variant<DynamicTacticalUncommitted,DynamicTacticalReleased,DynamicTacticalApplied>;
struct DynamicTacticalRecovering {SourceRef effect_source_ref;};
struct DynamicTacticalCompleted {SourceRef result_source_ref;WorldMoment completed_at;};
struct DynamicTacticalInterrupted {MomentOwnerSource actual_cause;WorldMoment interrupted_at;};
using DynamicTacticalActionPhase=std::variant<DynamicTacticalPreparing,DynamicTacticalRecovering,DynamicTacticalCompleted,DynamicTacticalInterrupted>;
struct DynamicMilitaryAction2 {
    StableId id,session_ref;Integer generation=0;StableId unit_ref;
    SourceRef accepted_intent_source_ref;WorldMoment accepted_at;
    StableId timing_definition_ref;Integer timing_definition_version=0,action_instance_ordinal=0;
    MilitaryOrder original_order;
    // At most one prepare and one recovery, each retains its own actual service.
    // No elapsed/countdown is duplicated in phase or session current.
    std::vector<MilitaryTacticalService2> service_stages;
    DynamicTacticalActionPhase phase;
    DynamicTacticalEffectState effect_state;
};
} // Actual source and section selection remain under the original S2 owner.
