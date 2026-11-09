#pragma once
#include "training_exact_model.hpp"
#include "training_original_rules.hpp"
#include "resident/kernel/action_time_validation.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
struct ExactTrainingPeriodView {
 StableId period_ref;Integer from_tick=0,to_tick=0;WorldMoment opening_at;
 MomentOwnerSource original_opening;
};
// Borrows the existing calendar owner's actual historical opening. Closing is a
// scheduler predecessor, not a second period record created by training.
class ExactTrainingPeriodRead {
public:virtual ~ExactTrainingPeriodRead()=default;
 virtual Outcome<ExactTrainingPeriodView> read_training_period(Integer actual_from_tick)const=0;
};
struct ExactTrainingTerminationCause {MomentOwnerSource actual_source;WorldMoment occurred_at;StableId original_code;};
class ExactTrainingOriginalRead {
public:virtual ~ExactTrainingOriginalRead()=default;
 virtual bool bound_to(const CommonStore&,const MilitaryState&)const noexcept=0;
 // Reads historical Office/roster/skill/place/payer/condition at the actual start
 // Common cut; today's same terms do not prove a historical acceptance.
 virtual Status validate_training_acceptance(const DynamicMilitaryTrainingFact2&)const=0;
 virtual Outcome<TrainingParticipationSpan2> read_actual_training_participation(const DynamicMilitaryTrainingFact2&,const MomentSpan&)const=0;
 virtual Status validate_actual_training_participation(const DynamicMilitaryTrainingFact2&,const TrainingParticipationSpan2&)const=0;
 virtual Outcome<std::optional<ExactTrainingTerminationCause>> read_training_eligibility_change(const DynamicMilitaryTrainingFact2&,const WorldMoment&)const=0;
 virtual Status validate_training_terminal_cause(const DynamicMilitaryTrainingFact2&,const SourceRef&,const WorldMoment&,const StableId&)const=0;
 // Empty only when the original accepted learning rule has no learning effect.
 virtual Status validate_training_learning_result(const DynamicMilitaryTrainingFact2&,const DynamicMilitaryTrainingLesson2&)const=0;
};
class ExactTrainingLearningWrite {
public:virtual ~ExactTrainingLearningWrite()=default;
 virtual Outcome<std::optional<SourceRef>> apply_original_training_learning_in_candidate(CommonStore&,const DynamicMilitaryTrainingFact2&,const DynamicMilitaryTrainingLesson2&)=0;
};
struct ExactTrainingReadContext {
 const MilitaryCatalog& catalog;const MilitaryAccess& access;const TimeScaleRef& scale;
 const ExactTrainingPeriodRead& periods;const ExactTrainingOriginalRead& original;
 const CurrentMomentRead& clock;ExactArithmeticBudget budget{};
};
struct ExactTrainingWriteContext {
 ExactTrainingReadContext read;ExactTrainingLearningWrite& learning;
 std::function<Status(const StableId&)> release_action;
};
ActionTimeRegistration exact_military_training_registration();
class NativeMilitaryTrainingActionTimeRead final:public ActionTimeOriginalSourceRead {
 const CommonStore& common_;const MilitaryState& military_;const MilitaryActionTimeState& state_;
 const ActionElapsedStore& elapsed_;ExactTrainingReadContext context_;
public:
 NativeMilitaryTrainingActionTimeRead(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& s,const ActionElapsedStore& e,ExactTrainingReadContext x):common_(c),military_(m),state_(s),elapsed_(e),context_(x){}
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
Outcome<OriginalClosingSupplyGate2> read_original_training_closing_supply_gate(const CommonStore&,const MilitaryState&,const ExactTrainingReadContext&,const DynamicMilitaryTrainingFact2&,Integer ordinal);
Status validate_exact_military_training_sources(const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,const ActionElapsedStore&,const ExactTrainingReadContext&,bool allow_unconsumed_current_gate=false);
Outcome<StableId> start_exact_military_training_in_candidate(CommonStore&,MilitaryState&,MilitaryActionTimeState&,ActionElapsedStore&,const ExactTrainingWriteContext&,const ActionCoreView& actual_action,const MilitaryTrain&,const MilitaryQuote&);
Status advance_exact_military_training_in_candidate(CommonStore&,MilitaryState&,MilitaryActionTimeState&,ActionElapsedStore&,const ExactTrainingWriteContext&,const StableId& action,const MomentSpan& actual_span);
Outcome<std::optional<WorldMoment>> read_next_exact_training_gate(const MilitaryActionTimeState&,const StableId& action,ExactArithmeticBudget={});
Outcome<MilitaryEffect> complete_exact_military_training_gate_in_candidate(CommonStore&,MilitaryState&,MilitaryActionTimeState&,ActionElapsedStore&,const ExactTrainingWriteContext&,const StableId& action);
Outcome<MilitaryEffect> cancel_exact_military_training_in_candidate(CommonStore&,MilitaryState&,MilitaryActionTimeState&,ActionElapsedStore&,const ExactTrainingWriteContext&,const StableId& actor,const StableId& action);
// Called immediately after a real eligibility producer in the same candidate;
// re-reads its actual cause and never substitutes a new pause/retry policy.
Outcome<MilitaryEffect> interrupt_exact_military_training_in_candidate(CommonStore&,MilitaryState&,MilitaryActionTimeState&,ActionElapsedStore&,const ExactTrainingWriteContext&,const StableId& action,const MomentOwnerSource& actual_cause);
Outcome<MilitaryEffect> apply_exact_training_drill_command_in_candidate(CommonStore&,MilitaryState&,MilitaryActionTimeState&,const ExactTrainingReadContext&,const StableId& actor,const MilitaryDrillCommand&);
Outcome<std::vector<StableId>> exact_military_training_mouths(const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,const ExactTrainingReadContext&,const MilitaryUnitRecord&,const StableId& garrison);
}
