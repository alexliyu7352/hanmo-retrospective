#pragma once
#include "obligation_sources_model.hpp"
#include "resident/domains/local_economy/local_economy_model.hpp"
namespace hanmo::resident {
// Transient projections from the original source owner. None is persisted as
// another contract, price, cash balance, or derived outstanding amount.
struct OriginalEconomicPayable {
 ObligationRef obligation_ref;SourceRef arrear_ref;MomentOwnerSource canonical_charge_ref;
 WorldMoment established_at,due_at;Integer original_due=0;
};
struct OriginalEconomicAdjustment {
 MomentOwnerSource original_adjustment_ref;ObligationRef obligation_ref;SourceRef arrear_ref;
 EconomicPayableAdjustmentKind kind=EconomicPayableAdjustmentKind::positive_increment;
 Integer amount=0;WorldMoment occurred_at;
};
struct OriginalEconomicCashPayment {
 const PaymentReceipt* payment=nullptr;WorldMoment actual_at;
 MomentOwnerSource original_payment_trigger_ref;
};
class EconomicObligationOriginalRead {public:virtual ~EconomicObligationOriginalRead()=default;
 virtual bool bound_to(const CommonStore&,const LocalEconomyState&)const noexcept=0;
 // Source-local cuts must validate original acceptance, earned/terminal cause,
 // canonical identity and amounts, without calling this P11 debt reader back.
 virtual Outcome<OriginalEconomicPayable> read_original_payable(const EconomicObligationOriginalSource&)const=0;
 virtual Outcome<OriginalEconomicAdjustment> read_original_adjustment(const MomentOwnerSource&)const=0;
 // Source-local cash identity/cause; it must not call payment permission or
 // budget totals back. Full P11 validation invokes the separate authority leg.
 virtual Outcome<OriginalEconomicCashPayment> read_original_cash_payment(const SourceRef&)const=0;
 virtual Status validate_original_cash_payment_authority(const SourceRef&)const=0;
 virtual Outcome<ObligationResponsibilityEpoch> read_original_responsibility_genesis(const EconomicObligationOriginalSource&)const=0;
 virtual Status validate_original_responsibility_epoch(const ObligationResponsibilityEpoch&)const=0;
};
struct EconomicObligationReadContext {
 const CommonStore&common;const LocalEconomyState&economy;const EconomicObligationSourcesState&sources;
 const EconomicObligationOriginalRead&original;const CurrentMomentRead&clock;
};
struct EconomicObligationWriteContext {
 CommonStore&common;LocalEconomyState&economy;EconomicObligationSourcesState&sources;
 const EconomicObligationOriginalRead&original;const CurrentMomentRead&clock;
 EconomicObligationReadContext read()const{return {common,economy,sources,original,clock};}
};
struct EconomicPayableView {
 const RecurringObligation*obligation=nullptr;const ArrearRecord*arrear=nullptr;
 const EconomicPayableSourceBinding*source=nullptr;
 const ObligationResponsibilityEpoch*current_responsibility=nullptr;
 Integer positive_increments=0,cash_paid=0,unpaid_void=0,outstanding=0;
 ObligationExecutionEventRef last_execution_event;
};
Outcome<EconomicPayableView> read_economic_payable(const EconomicObligationReadContext&,const SourceRef&original_arrear);
// Dependency cut used only by the original budget-responsibility projection.
// Replays the identical charge/cash/adjustment/epoch graph, but does not call
// the payment authority whose budget input this projection is constructing.
// Full publication still requires read_economic_payable and the whole validator.
Outcome<EconomicPayableView> read_economic_payable_for_budget_evidence(const EconomicObligationReadContext&,const SourceRef&original_arrear);
Outcome<const ObligationResponsibilityEpoch*> read_current_obligation_responsibility(const EconomicObligationReadContext&,const ObligationRef&);
Outcome<const ObligationResponsibilityEpoch*> resolve_obligation_responsibility_at_payment(const EconomicObligationReadContext&,const SourceRef&original_payment);
Status validate_economic_obligation_sources(const EconomicObligationReadContext&);
// These only describe source identity and typed event equality. Source validity
// always comes from the original owner and the complete same-candidate reader.
Status validate_original_obligation_genesis(const ObligationResponsibilityEpoch& stored,
 const ObligationResponsibilityEpoch& verified_original);
bool same_economic_obligation_original_source(const EconomicObligationOriginalSource&,const EconomicObligationOriginalSource&);
bool same_obligation_execution_event(const ObligationExecutionEventRef&,const ObligationExecutionEventRef&);
}
