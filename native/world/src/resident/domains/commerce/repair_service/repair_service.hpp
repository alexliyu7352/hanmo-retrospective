// Actual S7 Repair contract/fee owner. Caller values never substitute for S9 work.
#pragma once
#include "repair_service_model.hpp"
#include "resident/domains/craft/item_service.hpp"
#include "resident/domains/local_economy/local_economy_model.hpp"
#include "resident/kernel/read_stamp.hpp"
namespace hanmo::resident {
struct RepairReadContext;
struct RepairServiceFinancialParties {SubjectRef debtor,creditor;};
struct RepairServicePaymentAllowed {MomentOwnerSource original_authority;};
struct RepairServicePaymentBlocked {MomentOwnerSource actual_cause;WorldMoment occurred_at;};
using RepairServicePaymentPermission=std::variant<RepairServicePaymentAllowed,RepairServicePaymentBlocked>;
enum class RepairServicePaymentUse {earned_charge=1};
class RepairServiceOriginalRead {public:virtual ~RepairServiceOriginalRead()=default;
 virtual bool bound_to(const CommonStore&,const ItemInstancesState&)const noexcept=0;
 // Publication gate includes unaccepted offers, sealed commands and their
 // unique payloads; a successful source-local contract read is insufficient.
 virtual Status validate_original_repair_agreement_coverage()const=0;
 // Read the actual locked native rule and complete agreed values. An arbitrary
 // policy ID, today's wallet owner or mere physical contact is not agreement.
 virtual Status validate_original_repair_service_rules(const RepairServiceContractTerms&,const RepairPolicy&)const=0;
 virtual Status validate_original_repair_service_acceptance(const RepairServiceContractTerms&,const WorldMoment&,const RepairServiceContractSource* actual_contract)const=0;
 // Explicit original liability mapping, particularly for household/agent cases.
 virtual Outcome<RepairServiceFinancialParties> read_original_repair_service_parties(const RepairServiceContractSource&)const=0;
 virtual Outcome<RepairServicePaymentPermission> read_original_repair_payment_permission(const RepairServiceContractSource&,Integer,const WorldMoment&,RepairServicePaymentUse,const MomentOwnerSource& actual_responsibility)const=0;
 // Re-read actual authority and the two original Common wallet legs of this
 // precise immutable source. This cannot invoke the whole S7/Repair validator.
 virtual Status validate_original_repair_payment(const RepairServiceContractSource&,const MomentOwnerSource& actual_payment,RepairServicePaymentUse,Integer,const WorldMoment&)const=0;
 virtual Status validate_original_repair_custody(const RepairServiceContractSource&,const RepairCustodyReceivedSource&)const=0;
 virtual Outcome<std::vector<MomentOwnerSource>> read_original_repair_terminal_continuity(const RepairServiceContractSource&,const MomentOwnerSource& actual_terminal)const=0;
 virtual Status validate_original_repair_return(const RepairServiceContractSource&,const RepairItemReturnedSource&)const=0;
};
struct EconomicObligationReadContext;struct EconomicObligationWriteContext;
struct RepairServiceReadContext {
 const CommonStore&common;const ItemInstancesState&items;const LocalEconomyState&economy;
 const RepairPolicyCatalog&policies;const RepairServiceOriginalRead&original;const CurrentMomentRead&clock;
 const RepairReadContext*repair=nullptr;
 const RepairServiceRuleCatalog* rules=nullptr;
 const EconomicObligationReadContext* obligations=nullptr;
 EconomicObligationWriteContext* obligation_writes=nullptr;
};
SourceRef repair_service_offer_source(const StableId&);
SourceRef repair_service_assent_source(const StableId&);
SourceRef repair_service_contract_source(const StableId&);
SourceRef repair_custody_received_source(const StableId&);
SourceRef repair_service_charge_source(const StableId&);
SourceRef repair_service_paid_source(const StableId&);
SourceRef repair_service_termination_settled_source(const StableId&);
SourceRef repair_item_returned_source(const StableId&);
// The source-local contract reader does not require the later S9 acceptance.
// It is the only admissible read within that same acceptance candidate.
Outcome<const RepairServiceContractSource*> read_repair_service_contract(const RepairServiceReadContext&,const MomentOwnerSource&);
Status validate_repair_service_contract_proposal(const RepairServiceReadContext&,const RepairServiceContractTerms&);
// This internal writer must be followed by the original S9 acceptance and the
// full validator before publication. No future Job/Action identity is issued.
Outcome<SourceRef> accept_repair_service_contract_in_candidate(CommonStore&,ItemInstancesState&,const RepairServiceReadContext&,const RepairServiceContractTerms&);
Outcome<const RepairServiceChargeSource*> read_repair_service_charge(const RepairServiceReadContext&,const MomentOwnerSource&);
Outcome<SourceRef> charge_actual_repair_service_in_candidate(CommonStore&,ItemInstancesState&,LocalEconomyState&,const RepairServiceReadContext&,const MomentOwnerSource&contract,const MomentOwnerSource&actual_trigger);
// Absent money or a real temporary payment restriction leaves the original
// earned debt. A technical/invalid-source error is never changed into absence.
Outcome<std::optional<SourceRef>> pay_actual_repair_service_charge_in_candidate(CommonStore&,ItemInstancesState&,LocalEconomyState&,const RepairServiceReadContext&,const MomentOwnerSource&charge);
Status validate_repair_service_contracts(const RepairServiceReadContext&);
Outcome<WorldMoment> read_repair_service_source_moment(const RepairServiceReadContext&,const MomentOwnerSource&);
Outcome<std::optional<WorldMoment>> read_repair_service_original_money_moment(const RepairServiceReadContext&,const SourceRef&);
class RepairServiceSourcePinSink {public:virtual ~RepairServiceSourcePinSink()=default;virtual Status source(const MomentOwnerSource&)=0;virtual Status item_condition(const ItemConditionHead&)=0;virtual Status obligation(const ObligationRef&)=0;};
Status visit_repair_service_source_pins(const RepairServiceReadContext&,const MomentOwnerSource&,RepairServiceSourcePinSink&);
}
namespace hanmo::resident {
Status validate_repair_commercial_accepted_terms(const RepairServiceReadContext&,const RepairAcceptedTerms&,const WorldMoment&,const RepairAcceptedSource*);
Status consume_original_repair_start_fee_in_candidate(CommonStore&,ItemInstancesState&,LocalEconomyState&,const RepairServiceReadContext&,const RepairJob&);
Outcome<std::vector<MomentOwnerSource>> consume_original_repair_completion_fee_in_candidate(CommonStore&,ItemInstancesState&,LocalEconomyState&,const RepairServiceReadContext&,const RepairJob&,const RepairCompletedSource&);
Status validate_original_repair_completion_fee(const RepairServiceReadContext&,const RepairJob&,const RepairCompletedSource&);
}
namespace hanmo::resident {
Outcome<SourceRef> settle_original_repair_service_termination_in_candidate(CommonStore&,ItemInstancesState&,LocalEconomyState&,const RepairServiceReadContext&,const MomentOwnerSource&contract,const MomentOwnerSource&actual_terminal);
// Called only after the real S9 acceptance is in this same candidate.
Status finalize_repair_service_acceptance_in_candidate(CommonStore&,ItemInstancesState&,const RepairServiceReadContext&,const MomentOwnerSource&contract);
}

namespace hanmo::resident {
// Actual inspected inputs only. The S7/S9 source refs are assigned by the two
// original writers in one candidate, never supplied as future identities.
Outcome<RepairServiceAcceptedResult> accept_original_repair_service_in_candidate(
 CommonStore&,ItemInstancesState&,const RepairServiceReadContext&,const RepairReadContext&,
 const RepairServiceContractTerms&,const RepairServiceAcceptanceInspection&);
// Both original terminal paths must consume this financial consequence before
// whole publication. Neither the original earned debt nor custody is erased.
Status consume_original_repair_termination_finance_in_candidate(CommonStore&,ItemInstancesState&,
 LocalEconomyState&,const RepairServiceReadContext&,const MomentOwnerSource&contract,const MomentOwnerSource&terminal);
Outcome<SourceRef> interrupt_unstarted_repair_service_in_candidate(CommonStore&,ItemInstancesState&,
 LocalEconomyState&,const RepairServiceReadContext&,const RepairReadContext&,const MomentOwnerSource&accepted,const MomentOwnerSource&cause);
}
