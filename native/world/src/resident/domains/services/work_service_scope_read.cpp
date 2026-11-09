#include "work_service_scope_read.hpp"
#include "resident/domains/tasks/work_execution/work_execution_detail.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
namespace hanmo::resident {namespace {
using namespace work_detail;


struct Counts {std::size_t charges=0,payables=0;};
class FinancialProjection final:public GovernanceWorkFinancialSourceVisitor {
 const CommonStore& common_;const S3WorkCommission& scope_;const WorkCommissionTerminal& terminal_;
 ServiceActualTerminationResult* output_;Counts counts_;std::size_t settlements_=0;
 Status add(const char*kind,const StableId&id,bool charge){
  const auto*h=common_.state().facts.find(id);if(!h||h->id!=id||h->domain!="local_economy"||h->kind!=kind)return common_failure("WorkServiceFinancialHeaderMismatch",id,ErrorKind::internal);
  auto&count=charge?counts_.charges:counts_.payables;if(count==SIZE_MAX)throw std::bad_alloc();++count;
  if(output_){const auto limit=charge?output_->earned_charge_refs.capacity():output_->existing_payable_refs.capacity();if(count>limit)return common_failure("WorkServiceResultChangedDuringRead",id,ErrorKind::internal);SourceRef ref{h->domain,h->kind,h->id};if(charge)output_->earned_charge_refs.emplace_back(std::move(ref));else output_->existing_payable_refs.push_back(std::move(ref));}
  return common_success();
 }
 bool same_terminal(const WorkCommissionTerminal&t)const{return t.task_id==terminal_.task_id&&t.work_id==terminal_.work_id&&t.task_terminal_fact_id==terminal_.task_terminal_fact_id&&t.source_schema==terminal_.source_schema&&same(t.accepted_task_ref,terminal_.accepted_task_ref);}
public:
 FinancialProjection(const CommonStore&c,const S3WorkCommission&s,const WorkCommissionTerminal&t,ServiceActualTerminationResult*out):common_(c),scope_(s),terminal_(t),output_(out){}
 const Counts&counts()const noexcept{return counts_;}
 Status finish()const{return settlements_==1?common_success():common_failure("WorkServiceOriginalSettlementRequired",terminal_.task_terminal_fact_id);}
 Status accepted_terms(const GovernanceWorkExpenseTerms&t)override{
  return t.work_id==scope_.work_id&&t.accepted_task_id==terminal_.task_id&&scope_.expense_terms_ref.domain=="local_economy"&&scope_.expense_terms_ref.kind=="governance_work_expense_terms"&&scope_.expense_terms_ref.id==t.expense_terms_id&&same(t.task_acceptance_source,scope_.accepted_task_ref)&&same(t.work_acceptance_source,scope_.work_acceptance_source_ref)?common_success():common_failure("WorkServiceAcceptedFeeScopeMismatch",scope_.work_id);
 }
 Status earned_charge(const GovernanceWorkChargeSource&c,const RecurringObligation*,const ArrearRecord*d)override{auto s=add("governance_work_charge",c.id,true);if(!s.ok||!d)return s;return add("contract_payment_due",d->id,false);}
 // Payment records were checked by the original financial visitor. Their due
 // source already resolves these cash legs; no second balance/payment list.
 Status charge_payment(const GovernanceWorkChargeSource&,const PaymentReceipt&,const GovernanceWorkPaymentSource&)override{return common_success();}
 Status termination_settlement(const GovernanceWorkTerminationSettlement&s)override{if(!same_terminal(s.termination_ref))return common_failure("WorkServiceSettlementTerminalMismatch",s.id);if(++settlements_!=1)return common_failure("DuplicateWorkServiceSettlement",s.id);return common_success();}
};
template<class T>void check_rows(std::size_t count){if(count>SIZE_MAX/sizeof(T))throw std::bad_alloc();}
}
Outcome<OwnedServiceActualTerminationResult> read_work_service_actual_termination(
 const WorkCommissionTerminalContext&x,const GovernanceWorkExpenseReadContext&money,
 const S3WorkCommission&scope,const WorkCommissionTerminal&terminal,const ReadStamp&stamp){
 if(&x.common!=&money.common||!x.work.bound_to(x.common,x.tasks))return {false,{},common_failure("WorkServiceResultWorldMismatch",scope.work_id,ErrorKind::internal).error};
 auto now=x.clock.read_current_moment();if(!now.ok)return {false,{},now.error};auto fee_now=money.clock.read_current_moment();if(!fee_now.ok)return {false,{},fee_now.error};auto equal=compare_world_moments(now.value,fee_now.value);if(!equal.ok)return {false,{},equal.error};if(equal.value)return {false,{},common_failure("WorkServiceResultClockMismatch",scope.work_id,ErrorKind::internal).error};
 // Use the named financial dependency cut, then validate all selected original
 // financial sources below. Ordinary Task/Work full validators remain intact.
 auto original=read_work_commission_termination_for_financial_evidence(x,scope,terminal,stamp);if(!original.ok)return {false,{},original.error};const auto&t=*original.value;
 OwnedServiceActualTerminationResult output;
 check_rows<SourceRef>(t.remaining_financial_or_return_refs.size());
 FinancialProjection sizing(x.common,scope,terminal,nullptr);auto valid=visit_governance_work_financial_sources(money,scope.expense_terms_ref,sizing);if(valid.ok)valid=sizing.finish();if(!valid.ok)return {false,{},valid.error};const auto counts=sizing.counts();check_rows<ServiceEarnedChargeRef>(counts.charges);check_rows<SourceRef>(counts.payables);(output.memory).grow(0);
 auto&r=output.value;r.termination_id=t.task_terminal_fact_id;r.scope=scope;r.original_terminal_source=terminal;r.accepted_fee_terms_ref=scope.expense_terms_ref;r.reason=ServiceOwnerConfirmedUnavailability{terminal};r.effective_game_time.kind=ActorFoundationAt::Kind::exact_moment;r.effective_game_time.exact_moment=t.effective_game_time;
 r.custody_or_continuity_refs=t.remaining_financial_or_return_refs;r.earned_charge_refs.reserve(counts.charges);r.existing_payable_refs.reserve(counts.payables);
 FinancialProjection filling(x.common,scope,terminal,&r);valid=visit_governance_work_financial_sources(money,scope.expense_terms_ref,filling);if(valid.ok)valid=filling.finish();if(!valid.ok)return {false,{},valid.error};const auto&actual=filling.counts();if(actual.charges!=counts.charges||actual.payables!=counts.payables)return {false,{},common_failure("WorkServiceResultChangedDuringRead",scope.work_id,ErrorKind::internal).error};
 valid=validate_service_actual_termination_result_shape(r,service_actual_termination_result_schema);if(!valid.ok)return {false,{},valid.error};return {true,std::move(output),{}};
}
}
