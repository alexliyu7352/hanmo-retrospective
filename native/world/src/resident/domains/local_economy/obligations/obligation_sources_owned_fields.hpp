// Original P11-owned members, shared by codec and raw/census ownership.
// No validation, archive tag, temporary DTO or second field sequence.
#pragma once
#include "obligation_sources_model.hpp"
namespace hanmo::resident::obligation_source_owned_fields {
template<class A>void fields(A&a,OriginalRecurringObligationSource&v){a("original_source",v.original_source);}
template<class A>void fields(A&a,OriginalGovernanceWorkCharge&v){a("original_charge",v.original_charge);}
template<class A>void fields(A&a,OriginalServiceTerminationPayable&v){a("original_allocation_or_charge",v.original_allocation_or_charge);}
template<class A>void fields(A&a,OriginalOrderedEarnedCharge&v){a("original_charge",v.original_charge);}
template<class A>void fields(A&a,OriginalShopOrderRefund&v){a("original_refund",v.original_refund);}
template<class A>void fields(A&a,OriginalShopCancellationFee&v){a("original_charge",v.original_charge);}
template<class A>void fields(A&a,OriginalCrewShareAccrual&v){a("original_accrual",v.original_accrual);}
template<class A>void fields(A&a,OriginalCrewShareCorrection&v){a("original_correction",v.original_correction);}
template<class A>void fields(A&a,OriginalMaterialAidCompensationPayable&v){a("original_late_compensation",v.original_late_compensation);}
template<class A>void fields(A&a,OriginalMaterialAidPayable&v){a("original_establishment",v.original_establishment);}
template<class A>void fields(A&a,OriginalTransportExpensePayable&v){a("original_expense",v.original_expense);}
template<class A>void fields(A&a,OriginalRepairEarnedCharge&v){a("original_charge",v.original_charge);}
template<class A>void fields(A&a,OriginalInvestmentPrincipal&v){a("contract_ref",v.contract_ref);a("disbursement_ref",v.disbursement_ref);}
template<class A>void fields(A&a,OriginalJointFundingReturn&v){a("contract_ref",v.contract_ref);a("original_disbursement_refs",v.original_disbursement_refs);}
template<class A>void fields(A&a,OriginalJointDistribution&v){a("contract_ref",v.contract_ref);a("distribution_ref",v.distribution_ref);a("party_ordinal",v.party_ordinal);}
template<class A>void fields(A&a,EconomicObligationSourceBinding&v){a("obligation_ref",v.obligation_ref);a("scheduler",v.scheduler);a("original_source",v.original_source);}
template<class A>void fields(A&a,EconomicPayableSourceBinding&v){a("original_arrear_ref",v.original_arrear_ref);a("obligation_ref",v.obligation_ref);a("original_source",v.original_source);a("canonical_charge_ref",v.canonical_charge_ref);a("established_at",v.established_at);a("due_at",v.due_at);}
template<class A>void fields(A&a,EconomicPayableAdjustmentBinding&v){a("original_adjustment_ref",v.original_adjustment_ref);a("obligation_ref",v.obligation_ref);a("original_arrear_ref",v.original_arrear_ref);a("kind",v.kind);}
template<class A>void fields(A&a,EconomicRecoveryObligationBinding&v){a("original_correction_ref",v.original_correction_ref);a("original_obligation_ref",v.original_obligation_ref);a("recovery_obligation_ref",v.recovery_obligation_ref);a("original_arrear_ref",v.original_arrear_ref);a("recovery_arrear_ref",v.recovery_arrear_ref);}
template<class A>void fields(A&a,ObligationResponsibilityEpochRef&v){a("obligation_ref",v.obligation_ref);a("epoch",v.epoch);}
template<class A>void fields(A&a,OriginalResponsibilitySource&v){a("source",v.source);}
template<class A>void fields(A&a,ConsentedObligationNovation&v){a("source",v.source);}
template<class A>void fields(A&a,ObligationExecutionWalletChange&v){a("source",v.source);}
template<class A>void fields(A&a,ObligationReceiverContinuity&v){a("source",v.source);}
template<class A>void fields(A&a,ObligationOriginalPoliticalTakeover&v){a("source",v.source);}
template<class A>void fields(A&a,ObligationResponsibilityEpoch&v){a("obligation_ref",v.obligation_ref);a("epoch",v.epoch);a("previous_epoch_ref",v.previous_epoch_ref);a("original_source_ref",v.original_source_ref);a("change_source",v.change_source);a("accepted_arrear_refs",v.accepted_arrear_refs);a("include_future_occurrences",v.include_future_occurrences);a("current_source_amendment_ref",v.current_source_amendment_ref);a("debtor",v.debtor);a("creditor",v.creditor);a("executor_authorization_ref",v.executor_authorization_ref);a("source_wallet_ref",v.source_wallet_ref);a("destination_wallet_ref",v.destination_wallet_ref);a("receiver_source_ref",v.receiver_source_ref);a("authority_ref",v.authority_ref);a("effective_at",v.effective_at);a("previous_execution_event_ref",v.previous_execution_event_ref);a("consent_refs",v.consent_refs);a("source_versions",v.source_versions);}
template<class A>void fields(A&a,PaymentResponsibilityBinding&v){a("original_payment_ref",v.original_payment_ref);a("obligation_ref",v.obligation_ref);a("arrear_ref",v.arrear_ref);a("responsibility_epoch_ref",v.responsibility_epoch_ref);a("actual_at",v.actual_at);a("original_payment_trigger_ref",v.original_payment_trigger_ref);a("previous_execution_event_ref",v.previous_execution_event_ref);}
template<class A>void fields(A&a,LegacyResponsibilityPrefix&v){a("original_schema",v.original_schema);a("obligation_ref",v.obligation_ref);a("original_responsibility_source",v.original_responsibility_source);a("verified_payment_refs",v.verified_payment_refs);a("actual_cutover_source",v.actual_cutover_source);}
template<class A>void fields(A&a,EconomicObligationSourcesState&v){a("schema_version",v.schema_version);a("obligation_sources",v.obligation_sources);a("payable_sources",v.payable_sources);a("adjustments",v.adjustments);a("recovery_bindings",v.recovery_bindings);a("responsibility_epochs",v.responsibility_epochs);a("payment_responsibility_bindings",v.payment_responsibility_bindings);a("legacy_responsibility_prefixes",v.legacy_responsibility_prefixes);}
}
