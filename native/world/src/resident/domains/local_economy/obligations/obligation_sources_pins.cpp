#include "obligation_sources_pins.hpp"
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};void take(Status s){if(!s.ok)throw Failure{std::move(s.error)};}
void original(EconomicObligationSourcePinSink&s,const EconomicObligationOriginalSource&source){std::visit([&](const auto&v){using T=std::decay_t<decltype(v)>;
 if constexpr(std::is_same_v<T,OriginalRecurringObligationSource>)take(s.original_source(v.original_source));
 else if constexpr(std::is_same_v<T,OriginalGovernanceWorkCharge>||std::is_same_v<T,OriginalOrderedEarnedCharge>||std::is_same_v<T,OriginalShopCancellationFee>||std::is_same_v<T,OriginalRepairEarnedCharge>)take(s.original_source(v.original_charge));
 else if constexpr(std::is_same_v<T,OriginalServiceTerminationPayable>)take(s.original_source(v.original_allocation_or_charge));
 else if constexpr(std::is_same_v<T,OriginalShopOrderRefund>)take(s.original_source(v.original_refund));
 else if constexpr(std::is_same_v<T,OriginalCrewShareAccrual>)take(s.original_source(v.original_accrual));
 else if constexpr(std::is_same_v<T,OriginalCrewShareCorrection>)take(s.original_source(v.original_correction));
 else if constexpr(std::is_same_v<T,OriginalMaterialAidCompensationPayable>)take(s.original_source(v.original_late_compensation));
 else if constexpr(std::is_same_v<T,OriginalMaterialAidPayable>)take(s.original_source(v.original_establishment));
 else if constexpr(std::is_same_v<T,OriginalTransportExpensePayable>)take(s.original_source(v.original_expense));
 else if constexpr(std::is_same_v<T,OriginalInvestmentPrincipal>){take(s.investment_contract(v.contract_ref));take(s.original_source(v.disbursement_ref));}
 else if constexpr(std::is_same_v<T,OriginalJointFundingReturn>){take(s.investment_contract(v.contract_ref));for(const auto&r:v.original_disbursement_refs)take(s.original_source(r));}
 else if constexpr(std::is_same_v<T,OriginalJointDistribution>){take(s.investment_contract(v.contract_ref));take(s.original_source(v.distribution_ref));}
},source);}
void event(EconomicObligationSourcePinSink&s,const ObligationExecutionEventRef&r){if(const auto*p=std::get_if<ObligationResponsibilityEpochRef>(&r))take(s.responsibility_epoch(*p));else take(s.original_cash_payment(std::get<SourceRef>(r)));}
}
Status visit_economic_obligation_source_pins(const EconomicObligationReadContext&x,EconomicObligationSourcePinSink&s){try{
 take(validate_economic_obligation_sources(x));
 for(const auto&v:x.sources.obligation_sources){take(s.obligation(v.obligation_ref));original(s,v.original_source);}
 for(const auto&v:x.sources.payable_sources){take(s.original_arrear(v.original_arrear_ref));take(s.obligation(v.obligation_ref));original(s,v.original_source);take(s.original_source(v.canonical_charge_ref));}
 for(const auto&v:x.sources.adjustments){take(s.original_source(v.original_adjustment_ref));take(s.obligation(v.obligation_ref));take(s.original_arrear(v.original_arrear_ref));}
 for(const auto&v:x.sources.recovery_bindings){take(s.original_source(v.original_correction_ref));take(s.obligation(v.original_obligation_ref));take(s.obligation(v.recovery_obligation_ref));take(s.original_arrear(v.original_arrear_ref));take(s.original_arrear(v.recovery_arrear_ref));}
 for(const auto&v:x.sources.responsibility_epochs){take(s.obligation(v.obligation_ref));if(v.previous_epoch_ref)take(s.responsibility_epoch(*v.previous_epoch_ref));take(s.original_source(v.original_source_ref));std::visit([&](const auto&r){take(s.original_source(r.source));},v.change_source);for(const auto&r:v.accepted_arrear_refs)take(s.original_arrear(r));if(v.current_source_amendment_ref)take(s.original_source(*v.current_source_amendment_ref));take(s.original_source(v.executor_authorization_ref));take(s.original_source(v.receiver_source_ref));if(v.previous_execution_event_ref)event(s,*v.previous_execution_event_ref);for(const auto&r:v.consent_refs)take(s.original_source(r));for(const auto&r:v.source_versions)take(s.domain_read_version(r));}
 for(const auto&v:x.sources.payment_responsibility_bindings){take(s.original_cash_payment(v.original_payment_ref));take(s.obligation(v.obligation_ref));take(s.original_arrear(v.arrear_ref));take(s.responsibility_epoch(v.responsibility_epoch_ref));take(s.original_source(v.original_payment_trigger_ref));event(s,v.previous_execution_event_ref);}
 for(const auto&v:x.sources.legacy_responsibility_prefixes){take(s.obligation(v.obligation_ref));take(s.original_source(v.original_responsibility_source));for(const auto&r:v.verified_payment_refs)take(s.original_cash_payment(r));take(s.original_source(v.actual_cutover_source));}
 return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}
}
