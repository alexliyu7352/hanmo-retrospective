// Original D §§6–8 formation/mission owners. Explicit dynamic_military/3 only.
// Neither these records nor their refs are a second Army, body or S0 control.
#pragma once
#include "military_tactical_intent_model.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
namespace hanmo::resident {
struct MilitaryBattlefieldDestination3 {
 StableId arena_ref,geometry_profile_ref;HexPoint position;Integer heading=0;
};
struct MilitaryWorldDestination3 {StableId registered_s1_destination_ref;};
using MilitaryCommandDestination3=std::variant<MilitaryBattlefieldDestination3,MilitaryWorldDestination3>;
struct MilitaryMoveTo3 {MilitaryCommandDestination3 destination;};
struct MilitaryHoldPosition3 {MilitaryCommandDestination3 destination;};
struct MilitaryEngageKnownForce3 {StableId known_target_ref;};
struct MilitarySupportFormation3 {StableId allied_formation_ref;};
struct MilitaryWithdrawToward3 {StableId approved_exit_ref;};
struct MilitaryRescueEmergency3 {StableId known_emergency_ref,rescue_plan_ref;};
using MilitaryTacticalMission3=std::variant<MilitaryMoveTo3,MilitaryHoldPosition3,
 MilitaryEngageKnownForce3,MilitarySupportFormation3,MilitaryWithdrawToward3,
 MilitaryRescueEmergency3>;
struct MilitaryFormationWorld3 {StableId s1_force_location_ref;Integer binding_revision=0;};
struct MilitaryFormationInBattle3 {
 StableId session_id;Integer session_generation=0;SourceRef entry_source_ref;Integer binding_revision=0;
};
using MilitaryFormationSpatial3=std::variant<MilitaryFormationWorld3,MilitaryFormationInBattle3>;
struct MilitaryFormationPlayer3 {StableId controller_actor_id;Integer control_epoch=0;};
struct MilitaryFormationPrebattle3 {MomentOwnerSource assignment_ref;};
struct MilitaryFormationDispatched3 {MomentOwnerSource dispatch_fact_ref;};
using MilitaryFormationAutonomousOrigin3=std::variant<MilitaryFormationPrebattle3,MilitaryFormationDispatched3>;
struct MilitaryFormationAutonomous3 {MilitaryFormationAutonomousOrigin3 origin;StableId active_mission_ref;};
using MilitaryFormationControl3=std::variant<MilitaryFormationPlayer3,MilitaryFormationAutonomous3>;
// Current revisions are separate kinds, following C §24.5. Birth is an actual
// original admission, never inferred from a list of units or a UI stamp.
struct MilitaryTacticalFormationBirth3 {
 MomentOwnerSource original_admission_source;WorldMoment created_at;
 CommonActualFactPrefix actual_before_common_prefix;
 std::vector<MomentOwnerSource> original_source_refs;
};
struct MilitaryTacticalFormation3 {
 StableId formation_id,source_army_ref;Integer roster_revision=0;
 std::optional<StableId> naval_carrier_ref;MilitaryFormationSpatial3 spatial_binding;
 std::vector<StableId> member_unit_refs;std::optional<StableId> officer_assignment_ref;
 std::optional<StableId> superior_formation_ref;StableId mission_terms_ref;
 MilitaryFormationControl3 control;std::optional<StableId> active_intent_ref;
 Integer last_applied_order_ordinal=0,control_revision=0,command_binding_revision=0,intent_revision=0;
 MilitaryTacticalFormationBirth3 birth;
};
struct MilitaryForceWorldContext3 {};
struct MilitaryForceBattleContext3 {StableId session_ref;Integer generation=0;};
using MilitaryForceCommandContext3=std::variant<MilitaryForceWorldContext3,MilitaryForceBattleContext3>;
struct MilitaryForceCommandStamp3 {
 WorldIdentity world;WorldMoment moment;StableId formation_id;
 Integer control_revision=0,spatial_binding_revision=0,player_control_epoch=0;
 MilitaryForceCommandContext3 context;
};
struct DirectForceIntent3 {
 MilitaryForceCommandStamp3 stamp;StableId formation_id;
 Integer expected_intent_revision=0;MilitaryTacticalMission3 intent;
};
struct DirectForceIntentReceipt3 {
 StableId formation_id,mission_ref;Integer previous_intent_revision=0,intent_revision=0;
 WorldMoment accepted_at;std::vector<SourceRef> accepted_intent_refs;
};
// A non-tactical-action member is owned by an actual original execution. The
// mission only retains its source; it cannot hold elapsed/progress/location.
struct MilitaryMissionOriginalExecution3 {StableId unit_ref;MomentOwnerSource execution_source;};
struct MilitaryTacticalMissionRecord3 {
 StableId id,formation_ref;Integer intent_revision=0;WorldMoment adopted_at;
 std::optional<StableId> previous_intent_ref;
 DirectForceIntent3 original_direct_request;
 CommonActualFactPrefix actual_before_common_prefix;
 MomentOwnerSource control_binding_source;
 StableId original_solver_definition_ref;Integer original_solver_definition_version=0;
 std::vector<MomentOwnerSource> original_scope_sources;
 std::vector<MilitaryMissionOriginalExecution3> original_executions;
 // This is the sole original accepted-intent storage. No parallel table.
 std::vector<MilitaryAcceptedIntent3> accepted_intents;
 // Bound only after Common inserted the immutable successful result/history.
 // An absent value is an unpublished same-candidate intermediate, never cold-valid.
 std::optional<MomentOwnerSource> actual_command_source;
};
}
