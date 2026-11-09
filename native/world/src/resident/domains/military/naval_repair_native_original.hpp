#pragma once
#include "naval_repair_participation.hpp"
#include "naval_repair_legacy_battle.hpp"
namespace hanmo::resident {
// The original profession requires all participants to have only this Action
// and no incompatible battle occupation. The complete World must supply actual
// source coverage for every selected F/W/V/M/Legacy owner. This is not proved by
// this repair's elapsed service, present Action, or two empty endpoint views.
class ExactNavalRepairOccupationRead {
public:
 virtual ~ExactNavalRepairOccupationRead()=default;
 virtual bool bound_to(const CommonStore&,const MilitaryState&,const NavalState&,
  const MilitaryExactState&,const MilitaryActionTimeState&)const noexcept=0;
 virtual Status validate_repair_current_occupation(const NavalRepairTerms&,
  const StableId& actual_action)const=0;
 virtual Status validate_repair_acceptance_occupation(const NewNavalRepairTimeSource2&)const=0;
 virtual Status validate_repair_service_occupation(const NewNavalRepairTimeSource2&,
  const MomentSpan&)const=0;
 // At the real result cut. An interruption permits only its already-verified
 // actual cause at that endpoint; it never retroactively excuses earlier work.
 virtual Status validate_repair_result_occupation(const NewNavalRepairTimeSource2&)const=0;
 // The original Naval2..6 contact stream is read by the concrete below. This
 // dependency must cover all OTHER selected real contact/entry owners, return
 // their true Common cause and exact M, and reject a missing selected provider.
 // Null means a completely verified empty relevant source set, not no adapter.
 virtual Outcome<std::optional<ExactNavalRepairInterruption>>
  read_first_nonlegacy_repair_contact(const NewNavalRepairTimeSource2&)const=0;
};
struct NativeExactNavalRepairOriginalContext {
 const ExactNavalRepairParticipationContext& participation;
 const NavalHistoryAccess& naval_history;
 const ExactNavalRepairOccupationRead& occupations;
};
// Borrowed same-candidate composition. The owning World retains every original
// dependency and constructs a fresh body/spatial lease at the actual phase M.
// No persistent table, resource balance history or fallback provider is added.
class NativeExactNavalRepairOriginalRead final:public ExactNavalRepairOriginalRead {
 NativeExactNavalRepairOriginalContext context_;
public:
 explicit NativeExactNavalRepairOriginalRead(NativeExactNavalRepairOriginalContext x):context_(x){}
 bool bound_to(const CommonStore&,const MilitaryState&,const NavalState&,
  const MilitaryActionTimeState&)const noexcept override;
 Outcome<NavalRepairQuote> read_naval_repair_current_offer(const StableId&,
  const NavalRepairStart&)const override;
 Status validate_naval_repair_acceptance(const NewNavalRepairTimeSource2&)const override;
 Status validate_naval_repair_actual_service(const NewNavalRepairTimeSource2&,
  const MomentSpan&)const override;
 Status validate_naval_repair_current_completion(const NewNavalRepairTimeSource2&)const override;
 Outcome<std::optional<ExactNavalRepairInterruption>>
  read_naval_repair_actual_interruption(const NewNavalRepairTimeSource2&)const override;
 Status validate_naval_repair_completed_result(const NewNavalRepairTimeSource2&)const override;
};
}
