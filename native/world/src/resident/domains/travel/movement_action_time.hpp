// M类仅借原空间执行，不保存ActionElapsed、不发许可或预计世界到时。
#pragma once
#include "movement_remaining.hpp"
#include "group_movement.hpp"
#include "../../kernel/action_time_validation.hpp"
#include <functional>
namespace hanmo::resident {
ActionTimeRegistration movement_action_time_registration(Integer selected_travel_version=4);
// 首次真实执行采用时冻结的路线终点；不随后续改道/今日位置变化。
struct OriginalPersonalMovementDestination {StableId action_id,actor_id;MomentOwnerSource execution_source;WorldMoment started_at;TravelDestination destination;MovementAnchor actual_destination_anchor;};
class NativeMovementActionTimeRead final:public ActionTimeOriginalSourceRead {
    const CommonRead& common_;const TravelState& travel_;const MovementExactState& movement_;const TravelCatalog& catalog_;const TravelServices& services_;ExactArithmeticBudget budget_;
public:
    NativeMovementActionTimeRead(const CommonRead& c,const TravelState& t,const MovementExactState& m,const TravelCatalog& catalog,const TravelServices& services,ExactArithmeticBudget b={}):common_(c),travel_(t),movement_(m),catalog_(catalog),services_(services),budget_(b){}
    bool bound_to(const CommonRead& common,const MovementExactState& movement)const noexcept{return &common_==&common&&&movement_==&movement;}
    Outcome<OriginalPersonalMovementDestination> read_original_execution_destination(const StableId& action_id)const;
    Outcome<std::optional<MomentOwnerSource>> read_original_execution_funding_plan(const StableId& action_id)const;
    // Strict actual Common append order, including same-M acceptance/dispatch.
    Status validate_original_execution_common_order(const StableId& action_id,const SourceRef& actual_acceptance,const SourceRef& actual_dispatch)const;
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
// 调用原组合根release_action；真释放成功才保终局。调用者仍在唯一候选内。
Outcome<SourceRef> finish_exact_personal_movement_in_candidate(CommonWrite&,MovementExactState&,const StableId& prefix_id,const std::function<Status(const StableId&)>& original_release_action,ExactArithmeticBudget budget={});
Status sync_exact_personal_action_time_projection(CommonWrite&,const TravelState&,const MovementExactState&,const TravelCatalog&,const TravelServices&,const StableId& action_id,ExactArithmeticBudget budget={});
Outcome<bool> read_actual_personal_contact_presence(const CommonRead&,const MovementExactState&,const StableId& action_id,const StableId& contact_id,ExactArithmeticBudget budget={});
Outcome<std::optional<PersonalMovementTerminalSource>> read_contact_completed_personal_action(const CommonRead&,const MovementExactState&,const StableId& action_id,const StableId& contact_id,ExactArithmeticBudget budget={});
Status validate_movement_action_time_sources(const CommonRead&,const MovementExactState&,ExactArithmeticBudget budget={});
// 群体不占Common Actor Action；真实GroupJourney仍为唯一执行与停点。
struct GroupMovementTimeProof {GroupMovementExecution execution;SourceRef opening_source;WorldMoment started_at;std::optional<SourceRef> actual_terminal_source;std::optional<WorldMoment> ended_at;RunStop stop;MovementConditionalRemaining remaining;};
Outcome<GroupMovementTimeProof> read_group_movement_time(const CommonStore&,const SpatialCarrierState&,const MovementExactState&,const GroupMovementExecutionState&,const TravelCatalog&,const TravelServices&,const StableId& journey_id,const WorldMoment&,ExactArithmeticBudget budget={});
}
