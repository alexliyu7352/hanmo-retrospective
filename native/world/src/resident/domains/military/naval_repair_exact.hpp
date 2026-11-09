#pragma once
#include "training_exact_model.hpp"
#include "naval_repair.hpp"
#include "resident/kernel/action_time_validation.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
enum class ExactNavalRepairInterruptionKind { contact,crew_changed,service_changed };
struct ExactNavalRepairInterruption {
 ExactNavalRepairInterruptionKind kind=ExactNavalRepairInterruptionKind::contact;
 MomentOwnerSource actual_cause;WorldMoment occurred_at;
};
// Original permissions, immutable whole-span participation, and ship damage/
// repair predecessors stay with their real owners. No read may substitute the
// current roster or location for a historical service window.
class ExactNavalRepairOriginalRead {
public:
 virtual ~ExactNavalRepairOriginalRead()=default;
 virtual bool bound_to(const CommonStore&,const MilitaryState&,const NavalState&,const MilitaryActionTimeState&)const noexcept=0;
 virtual Outcome<NavalRepairQuote> read_naval_repair_current_offer(const StableId& actor,const NavalRepairStart&)const=0;
 virtual Status validate_naval_repair_acceptance(const NewNavalRepairTimeSource2&)const=0;
 virtual Status validate_naval_repair_actual_service(const NewNavalRepairTimeSource2&,const MomentSpan&)const=0;
 virtual Status validate_naval_repair_current_completion(const NewNavalRepairTimeSource2&)const=0;
 // Only already-applied causes at the trusted current M. Prediction must not
 // fabricate a contact, ownership change, or casualty from a future estimate.
 virtual Outcome<std::optional<ExactNavalRepairInterruption>> read_naval_repair_actual_interruption(const NewNavalRepairTimeSource2&)const=0;
 virtual Status validate_naval_repair_completed_result(const NewNavalRepairTimeSource2&)const=0;
};
struct ExactNavalRepairReadContext {
 const MilitaryCatalog& military_catalog;const MilitaryAccess& access;
 const NavalCatalog& naval_catalog;const TimeScaleRef& scale;
 const ExactNavalRepairOriginalRead& original;const CurrentMomentRead& clock;
 ExactArithmeticBudget budget{};
};
struct ExactNavalRepairWriteContext {
 ExactNavalRepairReadContext read;
 // Real world occupation release, including S3 original consequences.
 std::function<Status(const StableId&)> release_action;
};
ActionTimeRegistration exact_naval_repair_action_registration();
class NativeNavalRepairActionTimeRead final:public ActionTimeOriginalSourceRead {
 const CommonStore& common_;const MilitaryState& military_;const NavalState& naval_;
 const MilitaryActionTimeState& state_;const ActionElapsedStore& elapsed_;
 ExactNavalRepairReadContext context_;
public:
 NativeNavalRepairActionTimeRead(const CommonStore& c,const MilitaryState& m,const NavalState& n,const MilitaryActionTimeState& s,const ActionElapsedStore& e,ExactNavalRepairReadContext x):common_(c),military_(m),naval_(n),state_(s),elapsed_(e),context_(x){}
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
Outcome<ExactNavalRepairReceipt> start_exact_naval_repair_in_candidate(CommonStore&,const MilitaryState&,NavalState&,MilitaryActionTimeState&,ActionElapsedStore&,const ExactNavalRepairWriteContext&,const StableId& actor,const NavalRepairStart&);
Status advance_exact_naval_repair_in_candidate(CommonStore&,const MilitaryState&,const NavalState&,MilitaryActionTimeState&,ActionElapsedStore&,const ExactNavalRepairWriteContext&,const StableId& action,const MomentSpan&);
Outcome<ExactNavalRepairReceipt> complete_exact_naval_repair_in_candidate(CommonStore&,const MilitaryState&,NavalState&,MilitaryActionTimeState&,ActionElapsedStore&,const ExactNavalRepairWriteContext&,const StableId& action);
Outcome<ExactNavalRepairReceipt> cancel_exact_naval_repair_in_candidate(CommonStore&,const MilitaryState&,NavalState&,MilitaryActionTimeState&,ActionElapsedStore&,const ExactNavalRepairWriteContext&,const StableId& actor,const NavalRepairCancel&);
// Call in the same candidate immediately after the genuine original cause.
Outcome<std::optional<ExactNavalRepairReceipt>> interrupt_exact_naval_repair_in_candidate(CommonStore&,const MilitaryState&,NavalState&,MilitaryActionTimeState&,ActionElapsedStore&,const ExactNavalRepairWriteContext&,const StableId& action);
Status validate_exact_naval_repair_sources(const CommonStore&,const MilitaryState&,const NavalState&,const MilitaryActionTimeState&,const ActionElapsedStore&,const ExactNavalRepairReadContext&,bool allow_current_completion_candidate=false);
struct ExactNavalRepairSourceView {
 const NewNavalRepairTimeSource2* original=nullptr;WorldMoment occurred_at;
};
// Charge addresses the original accepted reservation terms at start. It does
// not itself assert payment; only the genuine successful result proves that leg.
Outcome<ExactNavalRepairSourceView> read_verified_exact_naval_repair_source(const CommonStore&,const MilitaryState&,const NavalState&,const MilitaryActionTimeState&,const ActionElapsedStore&,const ExactNavalRepairReadContext&,const SourceRef&,Integer schema);
Outcome<const NewNavalRepairTimeSource2*> read_verified_exact_naval_repair_result(const CommonStore&,const MilitaryState&,const NavalState&,const MilitaryActionTimeState&,const ActionElapsedStore&,const ExactNavalRepairReadContext&,const SourceRef&,Integer schema);
}
