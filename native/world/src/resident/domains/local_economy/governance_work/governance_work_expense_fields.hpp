// Sole original Work-fee owning member description. Versioned wire omission
// belongs to the existing codec adapter; raw ownership includes actual fields.
#pragma once
#include "governance_work_fee_policy.hpp"
namespace hanmo::resident::governance_work_expense_fields {
template<class A>void fields(A&a,WorkActualStartRef&v){a("work_id",v.work_id);a("start_fact_id",v.start_fact_id);a("source_schema",v.source_schema);}
template<class A>void fields(A&a,WorkMeasuredReceiptRef&v){a("work_period_receipt_id",v.work_period_receipt_id);}
template<class A>void fields(A&a,WorkEconomicMeasurementRef&v){a("work_ref",v.work_ref);a("window",v.window);a("actual_period_sources",v.actual_period_sources);}
template<class A>void fields(A&a,WorkCompletedRef&v){a("result_ref",v.result_ref);a("source_schema",v.source_schema);}
template<class A>void fields(A&a,PatrolSecurityAppliedRef&v){a("outcome_ref",v.outcome_ref);a("source_schema",v.source_schema);}
template<class A>void fields(A&a,ProductionImprovementAppliedRef&v){a("receipt_ref",v.receipt_ref);a("source_schema",v.source_schema);}
template<class A>void fields(A&a,WorkFacilityActivatedRef&v){a("activation_ref",v.activation_ref);a("source_schema",v.source_schema);}
template<class A>void fields(A&a,WorkReportReceivedRef&v){a("reporting_result_ref",v.reporting_result_ref);a("knowledge_receipt_ref",v.knowledge_receipt_ref);a("reporting_schema",v.reporting_schema);a("knowledge_schema",v.knowledge_schema);}
template<class A>void fields(A&a,WorkFeeLine&v){a("line_id",v.line_id);a("kind",v.kind);a("trigger",v.trigger);a("unit",v.unit);a("price_qian",v.price_qian);a("pricing_quantity",v.pricing_quantity);a("maximum_quantity",v.maximum_quantity);a("maximum_qian",v.maximum_qian);a("rounding",v.rounding);a("payment_gate",v.payment_gate);a("accepted_target_ref",v.accepted_target_ref);}
template<class A>void fields(A&a,WorkCostCoverage&v){a("original_cost_ref",v.original_cost_ref);a("source_schema",v.source_schema);a("disposition",v.disposition);a("covered_line_id",v.covered_line_id);}
template<class A>void fields(A&a,WorkAdvanceArrangement&v){a("advance_qian",v.advance_qian);a("eligible_fee_line_ids",v.eligible_fee_line_ids);}
template<class A>void fields(A&a,GovernanceWorkExpenseApproval&v){a("approval_ref",v.approval_ref);a("source_schema",v.source_schema);a("authorization",v.authorization);a("offered_task_ref",v.offered_task_ref);a("expense_policy_ref",v.expense_policy_ref);a("budget_execution",v.budget_execution);a("budget_period",v.budget_period);a("payer_wallet_id",v.payer_wallet_id);a("contract_debtor",v.contract_debtor);a("permitted_work_kinds",v.permitted_work_kinds);a("valid_from",v.valid_from);a("valid_until",v.valid_until);}
template<class A>void fields(A&a,ProposedWorkExpenseTerms&v){a("approval",v.approval);a("accepted_terms_identity",v.accepted_terms_identity);a("canonical_scope_id",v.canonical_scope_id);a("work_kind",v.work_kind);a("fee_lines",v.fee_lines);a("cost_coverage",v.cost_coverage);a("advance",v.advance);a("contract_creditor",v.contract_creditor);a("payee_wallet_id",v.payee_wallet_id);a("profile_id",v.profile_id);a("rule_version",v.rule_version);}
template<class A>void fields(A&a,GovernanceWorkExpenseTerms&v){a("expense_terms_id",v.expense_terms_id);a("frozen",v.frozen);a("authorization",v.authorization);a("work_id",v.work_id);a("accepted_task_id",v.accepted_task_id);a("task_acceptance_source",v.task_acceptance_source);a("work_acceptance_source",v.work_acceptance_source);a("accepted_at",v.accepted_at);}
template<class A>void fields(A&a,WorkChargeKey&v){a("canonical_scope_id",v.canonical_scope_id);a("line_id",v.line_id);a("actual_occurrence",v.actual_occurrence);}
template<class A>void fields(A&a,GovernanceWorkChargeSource&v){a("id",v.id);a("charge_key",v.charge_key);a("expense_terms_ref",v.expense_terms_ref);a("work_id",v.work_id);a("trigger_evidence",v.trigger_evidence);a("earned_at",v.earned_at);a("measured_quantity",v.measured_quantity);a("earned_qian",v.earned_qian);a("original_obligation_id",v.original_obligation_id);a("original_arrear_id",v.original_arrear_id);a("original_payment_ids",v.original_payment_ids);}
template<class A>void fields(A&a,GovernanceWorkPaymentSource&v){a("payment_id",v.payment_id);a("charge_id",v.charge_id);a("paid_at",v.paid_at);}
template<class A>void fields(A&a,GovernanceWorkTerminationSettlement&v){a("id",v.id);a("expense_terms_ref",v.expense_terms_ref);a("termination_ref",v.termination_ref);a("settled_at",v.settled_at);a("preserved_charge_refs",v.preserved_charge_refs);}
template<class A>void fields(A&a,GovernanceWorkExpenseState&v){a("version",v.version);a("terms",v.terms);a("charges",v.charges);a("payments",v.payments);a("termination_settlements",v.termination_settlements);}
template<class A>void fields(A&a,GovernanceWorkFeePolicy&v){a("id",v.id);a("rule_version",v.rule_version);a("work_kind",v.work_kind);a("fee_lines",v.fee_lines);a("cost_coverage",v.cost_coverage);a("advance",v.advance);}
template<class A>void fields(A&a,GovernanceWorkFeeCatalog&v){a("version",v.version);a("identity",v.identity);a("policies",v.policies);}
}
