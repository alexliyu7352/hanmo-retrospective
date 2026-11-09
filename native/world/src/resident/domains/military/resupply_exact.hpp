#pragma once
#include "training_exact_model.hpp"
#include "resident/kernel/action_time_validation.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
// Accepted Army/Office/ownership history, actual whole-span participation and
// original resource results. Current army members or Office holders cannot prove
// their historical values. These source-local reads do not re-enter S0.
class ExactResupplyOriginalRead {
public:
 virtual ~ExactResupplyOriginalRead()=default;
 virtual bool bound_to(const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&)const noexcept=0;
 virtual Status validate_resupply_acceptance(const DynamicMilitaryLogisticsFact2&)const=0;
 virtual Outcome<std::vector<StableId>> read_resupply_original_participants(const DynamicMilitaryLogisticsFact2&)const=0;
 virtual Status validate_resupply_actual_service(const DynamicMilitaryLogisticsFact2&,const MomentSpan&)const=0;
 virtual Status validate_resupply_completed_result(const DynamicMilitaryLogisticsFact2&)const=0;
};
struct ExactResupplyReadContext {
 const MilitaryCatalog& catalog;const MilitaryAccess& access;const TimeScaleRef& scale;
 const ExactResupplyOriginalRead& original;const CurrentMomentRead& clock;
 ExactArithmeticBudget budget{};
};
struct ExactResupplyWriteContext {ExactResupplyReadContext read;std::function<Status(const StableId&)> release_action;};
ActionTimeRegistration exact_resupply_action_registration();
class NativeResupplyActionTimeRead final:public ActionTimeOriginalSourceRead {
 const CommonStore& common_;const MilitaryState& military_;const MilitaryActionTimeState& state_;
 const ActionElapsedStore& elapsed_;ExactResupplyReadContext context_;
public:
 NativeResupplyActionTimeRead(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& s,const ActionElapsedStore& e,ExactResupplyReadContext x):common_(c),military_(m),state_(s),elapsed_(e),context_(x){}
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
Outcome<StableId> start_exact_resupply_action_in_candidate(CommonStore&,MilitaryState&,MilitaryActionTimeState&,ActionElapsedStore&,const ExactResupplyWriteContext&,const ActionCoreView&,const ArmyLogistics&,const MilitaryQuote&);
Status advance_exact_resupply_action_in_candidate(CommonStore&,MilitaryState&,MilitaryActionTimeState&,ActionElapsedStore&,const ExactResupplyWriteContext&,const StableId&,const MomentSpan&);
Outcome<MilitaryEffect> complete_exact_resupply_action_in_candidate(CommonStore&,MilitaryState&,MilitaryActionTimeState&,ActionElapsedStore&,const ExactResupplyWriteContext&,const StableId&);
Status validate_exact_resupply_sources(const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,const ActionElapsedStore&,const ExactResupplyReadContext&,bool allow_current_completion_candidate=false);
Outcome<const DynamicMilitaryLogisticsFact2*> read_verified_exact_resupply_result(const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,const ActionElapsedStore&,const ExactResupplyReadContext&,const SourceRef&,Integer schema);
}
