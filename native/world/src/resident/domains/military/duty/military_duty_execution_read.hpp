#pragma once
#include "military_duty_contract.hpp"
#include "resident/domains/travel/movement_action_time.hpp"
namespace hanmo::resident {
struct MilitaryDutyExecutionTerminalRead {MomentOwnerSource actual_source;WorldMoment ended_at;};
struct MilitaryDutyOriginalExecutionRead {
    DutyExecutionBinding binding;MomentOwnerSource original_source;WorldMoment started_at;
    std::vector<StableId> named_participants;
    std::optional<MilitaryDutyExecutionTerminalRead> terminal;
    SpatialCarrierRef actor_or_group_ref;
};
// Borrowed same-world original S1 leaves. RegisteredMilitaryActivity is finite:
// schema 1 names the existing military Army/Fleet group journey, not a free ID.
class MilitaryDutyMovementSources {
    const CommonStore& common_;const TravelState& travel_;const MovementExactState& movement_;
    const SpatialCarrierState& carriers_;const GroupMovementExecutionState& groups_;
    const TravelCatalog& catalog_;const TravelServices& services_;ExactArithmeticBudget budget_;
public:
    MilitaryDutyMovementSources(const CommonStore& c,const TravelState& t,const MovementExactState& m,
        const SpatialCarrierState& carriers,const GroupMovementExecutionState& g,
        const TravelCatalog& catalog,const TravelServices& services,ExactArithmeticBudget b={})
        :common_(c),travel_(t),movement_(m),carriers_(carriers),groups_(g),catalog_(catalog),services_(services),budget_(b){}
    Outcome<MilitaryDutyOriginalExecutionRead> read(const DutyExecutionBinding&,const WorldMoment& current)const;
};
struct MilitaryDutyExecutionRead {
    StableId duty_action_id,order_or_assignment_ref,leader_actor_id;
    MomentOwnerSource accepted_terms_source,original_assignment_source;
    DutyExecutionBinding binding;MomentOwnerSource actual_binding_source,original_execution_source;
    WorldMoment bound_at,execution_started_at,finite_until;
    std::optional<MilitaryDutyExecutionTerminalRead> terminal;
    MilitaryDutyPhase phase=MilitaryDutyPhase::assembling;
    SpatialCarrierRef actor_or_group_ref;
};
// Assembling is an actual absent binding, never an invented accepted_at start.
Outcome<std::optional<MilitaryDutyExecutionRead>> read_military_duty_execution(
    const CommonStore&,const MilitaryDutyState&,const MilitaryDutyContext&,const StableId& duty);
// Only binds a real already-started personal/group movement inside its existing
// candidate; Station still uses the original Common Action writer.
Outcome<SourceRef> activate_military_movement_duty_in_candidate(CommonStore&,MilitaryDutyState&,
    const MilitaryDutyContext&,const StableId& duty,const DutyExecutionBinding&,const WorldMoment&);
// Stops only future Duty service. It never completes the Task or teleports,
// cancels, releases or restarts the underlying S1 carrier execution.
Outcome<SourceRef> expire_military_movement_duty_in_candidate(CommonStore&,MilitaryDutyState&,
    const MilitaryDutyContext&,const StableId& duty,const WorldMoment&);
Outcome<std::vector<SourceRef>> consume_military_duty_movement_terminal_in_candidate(
    CommonStore&,MilitaryDutyState&,const MilitaryDutyContext&,const MomentOwnerSource& actual_terminal,const WorldMoment&);
// Called only from the actual original begin/end writers, never from a period
// consumer to backfill a missing start. Direct S3 Work assignments require the
// real S3 consumer even if the caller did not supply an optional adapter.
Status consume_actual_military_duty_execution_begin(const CommonStore&,const MilitaryDutyState&,const MilitaryDutyContext&,const StableId& duty);
Status consume_actual_military_duty_execution_end(const CommonStore&,const MilitaryDutyState&,const MilitaryDutyContext&,const StableId& duty);
Status validate_military_duty_execution_bindings(const CommonStore&,const MilitaryDutyState&,
    const MilitaryDutyContext&,const WorldMoment&);
}
