#pragma once
#include "military_action_condition_model.hpp"
#include "resident/domains/actor_foundation/character_condition_read.hpp"
namespace hanmo::resident {
struct MilitaryAccess;struct MilitaryActionTimeState;
// Borrowed only during the actual central same-candidate service lifetime.
struct MilitaryActionConditionContext {
 const CommonStore& common;const MilitaryActionTimeState& actions;
 const AuthoredActorFoundationState& foundation;const CharacterConditionSources& sources;
 const CurrentMomentRead& clock;
};
Status validate_military_action_condition_prefix(const MilitaryActionConditionContext&,
 const MilitaryActionAcceptanceConditions1&,const WorldMoment& accepted_at);
Outcome<std::optional<MilitaryActionAcceptanceConditions1>> capture_military_action_acceptance_conditions(
 const CommonStore&,const MilitaryActionTimeState&,const MilitaryAccess&,const ActionCoreView&);
// Source-verified saved body plus actual availability at the accepting cut,
// using the original live ready-body predicates. This does not replace action
// occupation, battle, interaction, place, roster or whole-publication validation.
Status validate_military_action_acceptance_conditions(const CommonStore&,
 const MilitaryActionTimeState&,const MilitaryAccess&,const StableId& action_id,
 const WorldMoment& accepted_at,const std::vector<StableId>& actual_participants,
 const std::optional<MilitaryActionAcceptanceConditions1>&);
// Borrowed original participant output uses the very same acceptance reducer.
Status validate_military_action_acceptance_conditions(const CommonStore&,
 const MilitaryActionTimeState&,const MilitaryAccess&,const StableId& action_id,
 const WorldMoment& accepted_at,const CheckpointVector<const StableId*>& actual_participants,
 const std::optional<MilitaryActionAcceptanceConditions1>&);


// These are actual row slots, not caller-supplied historical timestamps.
enum class MilitaryActionConditionEndpoint2 { acceptance,completion };
Outcome<std::optional<MilitaryActionEndpointObservation2>> capture_military_action_endpoint_observation(
 const CommonStore&,const MilitaryActionTimeState&,const MilitaryAccess&,
 const ActionCoreView&,MilitaryActionConditionEndpoint2);
// Called at the last pure-read gate, before any original business side effect.
Status validate_military_action_endpoint_observation_unchanged(
 const CommonStore&,const MilitaryActionTimeState&,const MilitaryAccess&,
 const ActionCoreView&,MilitaryActionConditionEndpoint2,
 const MilitaryActionEndpointObservation2&);
// Strict source-local slot/prefix/body proof only. It does not replace the
// original endpoint's non-body predicate, nor turn a false predicate into error.
Status validate_military_action_endpoint_observation_source(
 const MilitaryActionConditionContext&,const StableId& action_id,
 MilitaryActionConditionEndpoint2,Integer selected_version);
Outcome<bool> read_military_action_endpoint_body_ready(
 const MilitaryActionConditionContext&,const StableId& action_id,
 MilitaryActionConditionEndpoint2,Integer selected_version);
// The same slot/source/body kernel for one actual participant. Whole endpoint
// coverage is still checked; only this participant's original role-specific
// predicate is returned. Battle/Action/place remain the original caller's gates.
Outcome<bool> read_military_action_endpoint_participant_body_ready(
 const MilitaryActionConditionContext&,const StableId& action_id,
 MilitaryActionConditionEndpoint2,const StableId& actual_participant,Integer selected_version);
class MilitaryActionCompletionConditionGuard {
 const CommonStore& common_;const MilitaryActionTimeState& actions_;
 const MilitaryAccess& access_;const ActionCoreView& action_;
 const MilitaryActionEndpointObservation2& observation_;
public:
 MilitaryActionCompletionConditionGuard(const CommonStore& c,const MilitaryActionTimeState& s,
  const MilitaryAccess& a,const ActionCoreView& action,const MilitaryActionEndpointObservation2& observation)
  :common_(c),actions_(s),access_(a),action_(action),observation_(observation){}
 MilitaryActionCompletionConditionGuard(const MilitaryActionCompletionConditionGuard&)=delete;
 MilitaryActionCompletionConditionGuard& operator=(const MilitaryActionCompletionConditionGuard&)=delete;
 Status validate_before_effects(const CommonWrite&,const MilitaryAccess&,const ActionCoreView&)const;
};
}
