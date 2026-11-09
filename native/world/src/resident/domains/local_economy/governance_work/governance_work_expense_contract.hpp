// IC-V1a 7: one S5 fee owner. S3 owns work; S6 owns approval; Common owns money.
#pragma once
#include <string_view>
#include "../local_economy_model.hpp"
#include "../fiscal/budget_execution_contract.hpp"
#include "resident/domains/tasks/work_execution/work_execution_contract.hpp"
#include "resident/domains/tasks/work_execution/work_follow_through.hpp"
#include "resident/kernel/read_stamp.hpp"
namespace hanmo::resident {
enum class WorkFeeKind { startup,measured_work,completion };
enum class WorkFeeTrigger { actual_start,measured_work,work_completed,outcome_applied,report_received };
enum class WorkFeeUnit { occurrence,qualified_work_half_days,economic_worker_half_days };
enum class WorkFeeRounding { exact_integer,cumulative_floor };
enum class WorkFeePaymentGate { pay_on_charge,accepted_arrears };
enum class WorkCostCoverageKind { included,separately_payable,quotation_reference };
struct WorkActualStartRef {StableId work_id,start_fact_id;Integer source_schema=1;};
struct WorkMeasuredReceiptRef {StableId work_period_receipt_id;};
struct WorkEconomicMeasurementRef {WorkRef work_ref;MomentSpan window;std::vector<MomentOwnerSource> actual_period_sources;};
using WorkMeasuredRef=std::variant<WorkMeasuredReceiptRef,WorkEconomicMeasurementRef>;
struct WorkCompletedRef {SourceRef result_ref;Integer source_schema=1;};
struct PatrolSecurityAppliedRef {SourceRef outcome_ref;Integer source_schema=1;};
struct ProductionImprovementAppliedRef {SourceRef receipt_ref;Integer source_schema=1;};
struct WorkFacilityActivatedRef {SourceRef activation_ref;Integer source_schema=1;};
using WorkOutcomeAppliedRef=std::variant<PatrolSecurityAppliedRef,ProductionImprovementAppliedRef,WorkFacilityActivatedRef>;
struct WorkReportReceivedRef {SourceRef reporting_result_ref,knowledge_receipt_ref;Integer reporting_schema=1,knowledge_schema=2;};
using WorkChargeEvidence=std::variant<WorkActualStartRef,WorkMeasuredRef,WorkCompletedRef,WorkOutcomeAppliedRef,WorkReportReceivedRef>;
struct WorkFeeLine {
 StableId line_id;WorkFeeKind kind=WorkFeeKind::startup;WorkFeeTrigger trigger=WorkFeeTrigger::actual_start;
 WorkFeeUnit unit=WorkFeeUnit::occurrence;Integer price_qian=0;CanonicalFraction pricing_quantity;
 CanonicalFraction maximum_quantity;Integer maximum_qian=0;WorkFeeRounding rounding=WorkFeeRounding::exact_integer;WorkFeePaymentGate payment_gate=WorkFeePaymentGate::pay_on_charge;
 // The accepted target/goal/reporting obligation; absent for the work's own start/period/result.
 std::optional<SourceRef> accepted_target_ref;
};
struct WorkCostCoverage {SourceRef original_cost_ref;Integer source_schema=0;WorkCostCoverageKind disposition=WorkCostCoverageKind::quotation_reference;StableId covered_line_id;};
// Read/write compatibility for the original /1 fields only. Nonzero is always
// rejected; current /2 terms/catalog do not serialize this retired field.
struct WorkAdvanceArrangement {Integer advance_qian=0;std::vector<StableId> eligible_fee_line_ids;};
// S6 supplies this projection from its real immutable approval. No cap becomes a price.
struct GovernanceWorkExpenseApproval {
 SourceRef approval_ref;Integer source_schema=1;WorkProgramAuthorizationScope authorization;SourceRef offered_task_ref,expense_policy_ref;
 BudgetExecutionRef budget_execution;BudgetPeriod budget_period;StableId payer_wallet_id;
 SubjectRef contract_debtor;std::vector<WorkKind> permitted_work_kinds;WorldMoment valid_from,valid_until;
};
struct ProposedWorkExpenseTerms {
 GovernanceWorkExpenseApproval approval;TermsRevisionRef accepted_terms_identity;StableId canonical_scope_id;
 WorkKind work_kind=WorkKind::patrol;std::vector<WorkFeeLine> fee_lines;std::vector<WorkCostCoverage> cost_coverage;
 WorkAdvanceArrangement advance;SubjectRef contract_creditor;StableId payee_wallet_id,profile_id,rule_version;
};
struct GovernanceWorkExpenseTerms {
 StableId expense_terms_id;ProposedWorkExpenseTerms frozen;WorkAuthorizationRef authorization;StableId work_id,accepted_task_id;
 SourceRef task_acceptance_source,work_acceptance_source;WorldMoment accepted_at;
};
struct PrepareWorkFeeQuote {
 SourceRef governance_approval_ref,offered_task_ref,expense_policy_ref;WorkKind chosen_permitted_execution=WorkKind::patrol;
};
struct WorkFeeQuote {
 WorldIdentity identity;ProposedWorkExpenseTerms proposed;std::vector<ReadVersion> resource_versions;
 std::vector<DomainReadVersion> domain_versions;std::vector<SourceRef> actual_dependencies;
};
struct WorkExpenseAcceptance {
 StableId work_id,accepted_task_id;SourceRef task_acceptance_source,work_acceptance_source;
 WorkAuthorizationRef authorization;
};
struct WorkChargeMeasurement {
 StableId work_id,canonical_scope_id;WorkFeeTrigger trigger=WorkFeeTrigger::actual_start;WorkFeeUnit unit=WorkFeeUnit::occurrence;
 SourceRef canonical_occurrence;WorldMoment occurred_at;std::optional<MomentSpan> actual_window;
 CanonicalFraction actual_quantity;bool qualifies=false;
};
// Canonical identity is the accepted scope+line+real occurrence, never a retry/terminal ID.
struct WorkChargeKey {StableId canonical_scope_id,line_id;SourceRef actual_occurrence;};
struct GovernanceWorkChargeSource {
 StableId id;WorkChargeKey charge_key;SourceRef expense_terms_ref;StableId work_id;WorkChargeEvidence trigger_evidence;
 WorldMoment earned_at;CanonicalFraction measured_quantity;Integer earned_qian=0;
 StableId original_obligation_id,original_arrear_id;std::vector<StableId> original_payment_ids;
};
struct GovernanceWorkPaymentSource {StableId payment_id,charge_id;WorldMoment paid_at;};
// Internal call-site discriminator. The accompanying original responsibility is
// the actual earned charge. Acceptance creates no payment.
enum class WorkFeePaymentUse {earned_charge=1};
struct WorkFeePaymentAllowed {};
// A real temporary authority/continuity restriction preserves the already earned
// payable and its existing reservation. An invalid source remains a technical error.
struct WorkFeePaymentBlocked {SourceRef actual_cause;WorldMoment occurred_at;};
using WorkFeePaymentPermission=std::variant<WorkFeePaymentAllowed,WorkFeePaymentBlocked>;
struct GovernanceWorkTerminationSettlement {
 StableId id;SourceRef expense_terms_ref;WorkCommissionTerminal termination_ref;WorldMoment settled_at;
 std::vector<SourceRef> preserved_charge_refs;
};
class GovernanceWorkSettlementOriginalRead {
public:virtual ~GovernanceWorkSettlementOriginalRead()=default;
 // Must use S3's named financial-evidence dependency cut: all original Task,
 // Work and follow-through evidence is checked, without recursively checking
 // this same fee owner's sources through WorkResult.expense_receipt_refs.
 virtual Outcome<const WorkCommissionTaskTerminalSource*> read_work_commission_termination_for_financial_evidence(const S3WorkCommission&,const WorkCommissionTerminal&)const=0;
 // The actual terminal source preserves already-earned charges.
};
struct GovernanceWorkExpenseState {
 Integer version=1;OrderedTable<GovernanceWorkExpenseTerms> terms;OrderedTable<GovernanceWorkChargeSource> charges;
 OrderedTable<GovernanceWorkPaymentSource> payments;
 OrderedTable<GovernanceWorkTerminationSettlement> termination_settlements;
};
// Original concrete S6/S3/target owners implement these reads. A caller cannot submit
// a quantity, price, completion flag or approval in place of these original records.
class GovernanceWorkExpenseOriginalRead {
public:virtual ~GovernanceWorkExpenseOriginalRead()=default;
 virtual Outcome<ProposedWorkExpenseTerms> read_work_fee_offer(const PrepareWorkFeeQuote&,const ReadStamp&)const=0;
 // Also re-read the exact author expense policy; a rule ref alone is not proof.
 virtual Status validate_work_expense_acceptance(const ProposedWorkExpenseTerms&,const WorkExpenseAcceptance&,const WorldMoment&,bool existing_source)const=0;
 virtual Outcome<WorkChargeMeasurement> read_work_charge_evidence(const GovernanceWorkExpenseTerms&,const WorkFeeLine&,const WorkChargeEvidence&)const=0;
 virtual Outcome<WorkFeePaymentPermission> read_work_fee_payment_permission(const GovernanceWorkExpenseTerms&,Integer amount,const WorldMoment&,WorkFeePaymentUse,const SourceRef& original_responsibility)const=0;
 virtual Status validate_work_fee_payment(const GovernanceWorkExpenseTerms&,Integer amount,const WorldMoment&,const SourceRef& actual_payment_source,bool existing_source)const=0;
 virtual Outcome<ReservationOwnerPhase> read_work_commission_resource_phase(const GovernanceWorkExpenseTerms&)const=0;
 virtual Status validate_work_fee_cost_coverage(const GovernanceWorkExpenseTerms&)const=0;
};
struct EconomicObligationReadContext;struct EconomicObligationWriteContext;
struct GovernanceWorkExpenseReadContext {
 const CommonStore& common;const LocalEconomyState& economy;const GovernanceWorkExpenseState& state;
 const GovernanceWorkExpenseOriginalRead& original;const CurrentMomentRead& clock;const BudgetExecutionState* budget_execution=nullptr;const GovernanceWorkSettlementOriginalRead* settlement=nullptr;
 const EconomicObligationReadContext* obligations=nullptr;
 EconomicObligationWriteContext* obligation_writes=nullptr;
};
Outcome<WorkFeeQuote> prepare_work_fee_quote(const CommonStore&,const GovernanceWorkExpenseOriginalRead&,const PrepareWorkFeeQuote&,const ReadStamp&,const ReadStampAuthority&,const DomainReadVersionSource&);
// S3 supplies the allocated work identity and real Task acceptance; it then stores
// this returned ref in the same accepted Work payload before the candidate is validated.
Outcome<SourceRef> accept_governance_work_expense_in_candidate(CommonStore&,GovernanceWorkExpenseState&,const GovernanceWorkExpenseOriginalRead&,const WorkFeeQuote&,const WorkExpenseAcceptance&,const CurrentMomentRead&,const DomainReadVersionSource&);
Outcome<const GovernanceWorkExpenseTerms*> read_governance_work_expense_terms(const GovernanceWorkExpenseReadContext&,const SourceRef&,const StableId& work_id,const StableId& accepted_task_id);
Outcome<std::optional<SourceRef>> evaluate_governance_work_charge_in_candidate(CommonStore&,LocalEconomyState&,GovernanceWorkExpenseState&,const GovernanceWorkExpenseReadContext&,const SourceRef& terms,const StableId& line_id,const WorkChargeEvidence&);
Outcome<std::optional<SourceRef>> pay_governance_work_charge_in_candidate(CommonStore&,LocalEconomyState&,GovernanceWorkExpenseState&,const GovernanceWorkExpenseReadContext&,const SourceRef& charge);
// Synchronous borrowed projection of the original financial records. A visitor
// owns and budgets any output it chooses to copy. No callback creates a payable,
// allocation, payment or settlement, and no borrowed row outlives this call.
// The canonical Common source kinds are, respectively: governance_work_expense_terms,
// governance_work_charge, obligation_payment, and work_service_settled,
// all in local_economy, source schema 1. A nonzero charge's actual due
// source is contract_payment_due/{ArrearRecord.id}; a zero charge has no legs.
class GovernanceWorkFinancialSourceVisitor {
public:
 virtual ~GovernanceWorkFinancialSourceVisitor()=default;
 virtual Status accepted_terms(const GovernanceWorkExpenseTerms&)=0;
 virtual Status earned_charge(const GovernanceWorkChargeSource&,const RecurringObligation*,const ArrearRecord*)=0;
 virtual Status charge_payment(const GovernanceWorkChargeSource&,const PaymentReceipt&,const GovernanceWorkPaymentSource&)=0;
 virtual Status termination_settlement(const GovernanceWorkTerminationSettlement&)=0;
};
// The same complete original financial-source/amount/authority/once/coverage
// checks used by whole-world validation run before any callback. Current wallet
// reservation and current Work resource phase remain the separate whole-world
// gate; they are not historical evidence for this projection.
Status visit_governance_work_financial_sources(const GovernanceWorkExpenseReadContext&,const SourceRef& accepted_expense_terms,GovernanceWorkFinancialSourceVisitor&);
Status validate_governance_work_expenses(const GovernanceWorkExpenseReadContext&);
Outcome<std::optional<WorldMoment>> read_governance_work_fact_moment(const GovernanceWorkExpenseReadContext&,const SourceRef&);
// S3's actual period writer invokes this consumer after its original source exists.
class GovernanceWorkExpenseWriter final:public WorkExpenseConsumer,public DomainActivityReservationOwnerRead {
 CommonStore& common_;LocalEconomyState& economy_;GovernanceWorkExpenseState& state_;const GovernanceWorkExpenseReadContext& x_;
 Status consume(const StableId&,WorkFeeTrigger,const WorkChargeEvidence&);
public:
 GovernanceWorkExpenseWriter(CommonStore&,LocalEconomyState&,GovernanceWorkExpenseState&,const GovernanceWorkExpenseReadContext&);
 Outcome<ReservationOwnerView> read_domain_activity_reservation_owner(const ReservationRef&)const override;
 Status consume_actual_start(const WorkActualStartSource&)override;
 Status consume_measured_work(const WorkPeriodReceipt2&)override;
 Outcome<std::vector<SourceRef>> consume_work_result(const WorkResult&)override;
 Status consume_applied_outcome(const StableId& work_id,const WorkOutcomeAppliedRef&);
 Status consume_received_report(const StableId& work_id,const WorkReportReceivedRef&);
};
class GovernanceWorkBudgetResponsibilityRead final:public BudgetResponsibilityRead {
 const GovernanceWorkExpenseReadContext& x_;
public:explicit GovernanceWorkBudgetResponsibilityRead(const GovernanceWorkExpenseReadContext&x):x_(x){}
 Outcome<ActualBudgetResponsibility> read_budget_responsibility(const BudgetExecutionBinding&)const override;
};
// Called at the real Task/Work acceptance tail, after its S6 binding and original
// Work acceptance source exist in the same candidate; failure rolls all legs back.
Outcome<BudgetExecutionBinding> bind_governance_work_budget_in_candidate(CommonStore&,BudgetExecutionState&,const BudgetAuthorizationRead&,const BudgetResponsibilityRead& all_original_responsibilities,const GovernanceWorkExpenseReadContext&,const SourceRef& terms_ref);
// Pre-hydration source identity only; full financial/authority validation remains
// validate_governance_work_expenses. The paid-at child does not replace LE cash.
class GovernanceWorkOriginalFactHeaderSink {
public:virtual ~GovernanceWorkOriginalFactHeaderSink()=default;
 virtual Status append(const FactRecord& actual_header,Integer original_schema)=0;
};
bool is_governance_work_original_fact_kind(std::string_view);
Outcome<Integer> read_governance_work_original_fact_payload_schema(const CommonStore&,const GovernanceWorkExpenseState&,const FactRecord&);
Status visit_governance_work_original_fact_headers(const CommonStore&,const LocalEconomyState&,const GovernanceWorkExpenseState&,GovernanceWorkOriginalFactHeaderSink&);
Status emit_governance_work_original_fact_payload_fields(ProjectionWriter&,const CommonStore&,const GovernanceWorkExpenseState&,const FactRecord&);
Outcome<BoundaryValue> encode_governance_work_expenses(const GovernanceWorkExpenseState&);
Outcome<GovernanceWorkExpenseState> decode_governance_work_expenses(const BoundaryValue&);
Status visit_governance_work_expenses(CodecVisitor&,GovernanceWorkExpenseState&);
Status emit_governance_work_expenses(ProjectionWriter&,const GovernanceWorkExpenseState&);
}
