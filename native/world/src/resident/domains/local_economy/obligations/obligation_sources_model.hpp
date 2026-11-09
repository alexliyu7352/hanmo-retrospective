// P11 / V02 and 05 L2. Source bindings only: the existing LE tables own every
// obligation, arrear and cash payment. No current outstanding/paid copy exists.
#pragma once
#include "resident/domains/local_economy/obligation_ref.hpp"
#include "resident/model/dynamic_contract.hpp"
#include "resident/kernel/world_timeline.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/domains/commerce/business_financing/investment_contract_ref.hpp"
namespace hanmo::resident {
// Each wrapper names an original source owner. The corresponding native reader
// checks its exact source kind/schema and canonical identity; arbitrary refs are
// not interchangeable with these alternatives. No constructor creates a source.
struct OriginalRecurringObligationSource {MomentOwnerSource original_source;};
struct OriginalGovernanceWorkCharge {MomentOwnerSource original_charge;};
struct OriginalServiceTerminationPayable {MomentOwnerSource original_allocation_or_charge;};
struct OriginalOrderedEarnedCharge {MomentOwnerSource original_charge;};
struct OriginalShopOrderRefund {MomentOwnerSource original_refund;};
struct OriginalShopCancellationFee {MomentOwnerSource original_charge;};
struct OriginalCrewShareAccrual {MomentOwnerSource original_accrual;};
struct OriginalCrewShareCorrection {MomentOwnerSource original_correction;};
struct OriginalMaterialAidPayable {MomentOwnerSource original_establishment;};
struct OriginalTransportExpensePayable {MomentOwnerSource original_expense;};
struct OriginalRepairEarnedCharge {MomentOwnerSource original_charge;};
struct OriginalInvestmentPrincipal {InvestmentContractRef contract_ref;MomentOwnerSource disbursement_ref;};
struct OriginalJointFundingReturn {InvestmentContractRef contract_ref;std::vector<MomentOwnerSource> original_disbursement_refs;};
struct OriginalJointDistribution {InvestmentContractRef contract_ref;MomentOwnerSource distribution_ref;Integer party_ordinal=0;};
// /2 append-only tag 14. A late compensation is not an Aid establishment.
struct OriginalMaterialAidCompensationPayable {MomentOwnerSource original_late_compensation;};
using EconomicObligationOriginalSource=std::variant<OriginalRecurringObligationSource,
 OriginalGovernanceWorkCharge,OriginalServiceTerminationPayable,OriginalOrderedEarnedCharge,
 OriginalShopOrderRefund,OriginalShopCancellationFee,OriginalCrewShareAccrual,
 OriginalCrewShareCorrection,OriginalMaterialAidPayable,OriginalTransportExpensePayable,
 OriginalRepairEarnedCharge,OriginalInvestmentPrincipal,OriginalJointFundingReturn,
 OriginalJointDistribution,OriginalMaterialAidCompensationPayable>;
inline bool economic_obligation_original_source_supported(Integer schema,const EconomicObligationOriginalSource& source)noexcept{
 return (schema==1&&source.index()<14)||(schema==2&&source.index()<15);
}
enum class EconomicObligationScheduler {recurring,source_only};
struct EconomicObligationSourceBinding {
 ObligationRef obligation_ref;EconomicObligationScheduler scheduler=EconomicObligationScheduler::source_only;
 EconomicObligationOriginalSource original_source;
};
// Canonical identity is an original owner's real charge/accrual ref. Its native
// reader checks cross-wrapper scope/line/occurrence identity before adoption.
struct EconomicPayableSourceBinding {
 SourceRef original_arrear_ref;ObligationRef obligation_ref;
 EconomicObligationOriginalSource original_source;MomentOwnerSource canonical_charge_ref;
 WorldMoment established_at,due_at;
};
enum class EconomicPayableAdjustmentKind {positive_increment,unpaid_void};
struct EconomicPayableAdjustmentBinding {
 MomentOwnerSource original_adjustment_ref;ObligationRef obligation_ref;
 SourceRef original_arrear_ref;EconomicPayableAdjustmentKind kind=EconomicPayableAdjustmentKind::positive_increment;
};
struct EconomicRecoveryObligationBinding {
 MomentOwnerSource original_correction_ref;ObligationRef original_obligation_ref,recovery_obligation_ref;
 SourceRef original_arrear_ref,recovery_arrear_ref;
};
struct ObligationResponsibilityEpochRef {ObligationRef obligation_ref;std::uint64_t epoch=0;};
using ObligationExecutionEventRef=std::variant<ObligationResponsibilityEpochRef,SourceRef>;
struct OriginalResponsibilitySource {MomentOwnerSource source;};
struct ConsentedObligationNovation {MomentOwnerSource source;};
struct ObligationExecutionWalletChange {MomentOwnerSource source;};
struct ObligationReceiverContinuity {MomentOwnerSource source;};
struct ObligationOriginalPoliticalTakeover {MomentOwnerSource source;};
using ObligationResponsibilityChangeSource=std::variant<OriginalResponsibilitySource,
 ConsentedObligationNovation,ObligationExecutionWalletChange,ObligationReceiverContinuity,
 ObligationOriginalPoliticalTakeover>;
struct ObligationResponsibilityEpoch {
 ObligationRef obligation_ref;std::uint64_t epoch=0;
 std::optional<ObligationResponsibilityEpochRef> previous_epoch_ref;
 MomentOwnerSource original_source_ref;ObligationResponsibilityChangeSource change_source;
 std::vector<SourceRef> accepted_arrear_refs;bool include_future_occurrences=false;
 std::optional<MomentOwnerSource> current_source_amendment_ref;
 SubjectRef debtor,creditor;MomentOwnerSource executor_authorization_ref;
 StableId source_wallet_ref,destination_wallet_ref;MomentOwnerSource receiver_source_ref;
 StableId authority_ref;WorldMoment effective_at;
 std::optional<ObligationExecutionEventRef> previous_execution_event_ref;
 std::vector<MomentOwnerSource> consent_refs;std::vector<DomainReadVersion> source_versions;
};
struct PaymentResponsibilityBinding {
 SourceRef original_payment_ref;ObligationRef obligation_ref;SourceRef arrear_ref;
 ObligationResponsibilityEpochRef responsibility_epoch_ref;WorldMoment actual_at;
 MomentOwnerSource original_payment_trigger_ref;ObligationExecutionEventRef previous_execution_event_ref;
};
struct LegacyResponsibilityPrefix {
 Integer original_schema=0;ObligationRef obligation_ref;MomentOwnerSource original_responsibility_source;
 std::vector<SourceRef> verified_payment_refs;MomentOwnerSource actual_cutover_source;
};
struct EconomicObligationSourcesState {
 Integer schema_version=1;
 std::vector<EconomicObligationSourceBinding> obligation_sources;
 std::vector<EconomicPayableSourceBinding> payable_sources;
 std::vector<EconomicPayableAdjustmentBinding> adjustments;
 std::vector<EconomicRecoveryObligationBinding> recovery_bindings;
 // L2/P1 tail order is fixed by the original contract.
 std::vector<ObligationResponsibilityEpoch> responsibility_epochs;
 std::vector<PaymentResponsibilityBinding> payment_responsibility_bindings;
 std::vector<LegacyResponsibilityPrefix> legacy_responsibility_prefixes;
};
// One selected format gate is used by cold, codec and original hot adoption.
// /1 cannot contain the new alternative even when all its scalar fields parse.
inline bool economic_obligation_sources_format_valid(const EconomicObligationSourcesState&s)noexcept{
 if(s.schema_version!=1&&s.schema_version!=2)return false;
 for(const auto&r:s.obligation_sources)if(!economic_obligation_original_source_supported(s.schema_version,r.original_source))return false;
 for(const auto&r:s.payable_sources)if(!economic_obligation_original_source_supported(s.schema_version,r.original_source))return false;
 return true;
}

}
