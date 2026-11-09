#include "obligation_sources_read.hpp"
#include <limits>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char*code,const StableId&id={},ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,id,kind).error};}
void take(Status s){if(!s.ok)throw Failure{std::move(s.error)};}
template<class T>T take(Outcome<T>s){if(!s.ok)throw Failure{std::move(s.error)};return std::move(s.value);}
Integer add(Integer a,Integer b){if(a<0||b<0||a>std::numeric_limits<Integer>::max()-b)fail("EconomicPayableOverflow");return a+b;}
int cmp(const WorldMoment&a,const WorldMoment&b){return take(compare_world_moments(a,b));}
bool same(const SourceRef&a,const SourceRef&b){return same_source_ref(a,b);}
bool same(const MomentOwnerSource&a,const MomentOwnerSource&b){return a.source_schema==b.source_schema&&same(a.source_ref,b.source_ref);}
bool same(const ObligationRef&a,const ObligationRef&b){return same_obligation_ref(a,b);}
bool same(const ObligationResponsibilityEpochRef&a,const ObligationResponsibilityEpochRef&b){return same(a.obligation_ref,b.obligation_ref)&&a.epoch==b.epoch;}
bool subject(const SubjectRef&a,const SubjectRef&b){return a.kind==b.kind&&a.id==b.id;}
std::size_t header(const CommonStore&c,const SourceRef&r){
 if(!valid_source_ref(r))fail("EconomicOriginalHeaderRequired",r.id);
 const auto*p=c.state().facts.find(r.id);if(!p||p->id!=r.id||p->domain!=r.domain||p->kind!=r.kind)fail("EconomicOriginalHeaderRequired",r.id);
 std::size_t n=0;for(const auto&v:c.state().facts.rows()){if(&v.second==p)return n;++n;}fail("EconomicOriginalHeaderOrderMissing",r.id);
}
void moment(const WorldMoment&m,const WorldMoment&now,const StableId&id){if(m.half_day_tick<0||cmp(m,now)>0)fail("EconomicOriginalMomentMismatch",id);}
WorldMoment context(const EconomicObligationReadContext&x){if(!economic_obligation_sources_format_valid(x.sources)||!x.original.bound_to(x.common,x.economy))fail("EconomicObligationContextMismatch");auto now=take(x.clock.read_current_moment());if(now.half_day_tick<x.common.tick())fail("EconomicObligationClockMismatch");return now;}
const EconomicObligationSourceBinding&binding(const EconomicObligationReadContext&x,const ObligationRef&r){
 take(validate_obligation_ref_shape(r));if(r.owner_domain!="local_economy")fail("EconomicObligationOwnerMismatch",r.obligation_id);
 const EconomicObligationSourceBinding*out=nullptr;for(const auto&v:x.sources.obligation_sources)if(same(v.obligation_ref,r)){if(out)fail("DuplicateEconomicObligationSource",r.obligation_id);out=&v;}if(!out)fail("EconomicObligationSourceRequired",r.obligation_id);return *out;
}
const EconomicPayableSourceBinding&payable_binding(const EconomicObligationReadContext&x,const SourceRef&r){
 if(r.domain!="local_economy"||r.kind!="contract_payment_due")fail("EconomicPayableSourceKindRequired",r.id);
 const EconomicPayableSourceBinding*out=nullptr;for(const auto&v:x.sources.payable_sources)if(same(v.original_arrear_ref,r)){if(out)fail("DuplicateEconomicPayableSource",r.id);out=&v;}if(!out)fail("EconomicPayableSourceRequired",r.id);return *out;
}
const PaymentResponsibilityBinding&payment_binding(const EconomicObligationReadContext&x,const SourceRef&r){
 if(r.domain!="local_economy"||r.kind!="obligation_payment")fail("EconomicCashPaymentSourceRequired",r.id);
 const PaymentResponsibilityBinding*out=nullptr;for(const auto&v:x.sources.payment_responsibility_bindings)if(same(v.original_payment_ref,r)){if(out)fail("DuplicateEconomicPaymentResponsibility",r.id);out=&v;}if(!out)fail("EconomicPaymentResponsibilityRequired",r.id);return *out;
}
// All accepted arrears are explicit original rows. Future coverage is only
// after the actual source cut, including exact Common order at the same M.
bool responsibility_covers(const EconomicObligationReadContext&x,const ObligationResponsibilityEpoch&e,const EconomicPayableSourceBinding&p){
 for(std::size_t i=0;i<e.accepted_arrear_refs.size();++i){
  const auto&ref=e.accepted_arrear_refs[i];const auto&accepted=payable_binding(x,ref);
  if(!same(accepted.obligation_ref,e.obligation_ref)||cmp(accepted.established_at,e.effective_at)>0)fail("EconomicResponsibilityAcceptedScopeMismatch",ref.id);
  for(std::size_t j=0;j<i;++j)if(same(ref,e.accepted_arrear_refs[j]))fail("DuplicateEconomicResponsibilityAcceptedArrear",ref.id);
 }
 for(const auto&ref:e.accepted_arrear_refs)if(same(ref,p.original_arrear_ref))return true;
 if(!e.include_future_occurrences)return false;
 const auto relation=cmp(p.established_at,e.effective_at);
 if(relation<0)return false;if(relation>0)return true;
 return header(x.common,p.canonical_charge_ref.source_ref)>header(x.common,e.original_source_ref.source_ref);
}
enum class CashReadPurpose { complete, original_budget_evidence };
struct Chain {const ObligationResponsibilityEpoch*current=nullptr;const ObligationResponsibilityEpoch*at_payment=nullptr;ObligationExecutionEventRef tail;};
Chain chain(const EconomicObligationReadContext&x,const ObligationRef&r,const SourceRef*requested=nullptr,CashReadPurpose purpose=CashReadPurpose::complete){
 const auto now=context(x);binding(x,r);const auto*o=x.economy.obligations.find(r.obligation_id);if(!o)fail("EconomicOriginalObligationMissing",r.obligation_id);
 for(const auto&v:x.sources.legacy_responsibility_prefixes)if(same(v.obligation_ref,r))fail("EconomicLegacyResponsibilityOriginalReaderRequired",r.obligation_id,ErrorKind::unsupported_capability);
 const ObligationResponsibilityEpoch*root=nullptr;std::size_t epochs=0,payments=0;
 for(const auto&v:x.sources.responsibility_epochs)if(same(v.obligation_ref,r)){++epochs;if(!v.previous_execution_event_ref){if(root||v.epoch!=0||v.previous_epoch_ref||!std::holds_alternative<OriginalResponsibilitySource>(v.change_source))fail("EconomicResponsibilityGenesisMismatch",r.obligation_id);root=&v;}}
 for(const auto&v:x.sources.payment_responsibility_bindings)if(same(v.obligation_ref,r))++payments;
 if(!root)fail("EconomicResponsibilityGenesisRequired",r.obligation_id);
 take(x.original.validate_original_responsibility_epoch(*root));moment(root->effective_at,now,r.obligation_id);
 const ObligationResponsibilityEpoch*current=root;WorldMoment at=root->effective_at;std::size_t at_order=header(x.common,root->original_source_ref.source_ref);
 ObligationExecutionEventRef event=ObligationResponsibilityEpochRef{r,0};std::size_t visited=1;Chain result;
 for(;;){const ObligationResponsibilityEpoch*next_epoch=nullptr;const PaymentResponsibilityBinding*next_payment=nullptr;
  for(const auto&v:x.sources.responsibility_epochs)if(same(v.obligation_ref,r)&&v.previous_execution_event_ref&&same_obligation_execution_event(*v.previous_execution_event_ref,event)){if(next_epoch||next_payment)fail("EconomicResponsibilityFork",r.obligation_id);next_epoch=&v;}
  for(const auto&v:x.sources.payment_responsibility_bindings)if(same(v.obligation_ref,r)&&same_obligation_execution_event(v.previous_execution_event_ref,event)){if(next_epoch||next_payment)fail("EconomicResponsibilityFork",r.obligation_id);next_payment=&v;}
  if(!next_epoch&&!next_payment)break;if(visited>=epochs+payments)fail("EconomicResponsibilityCycle",r.obligation_id);++visited;
  if(next_epoch){const auto&v=*next_epoch;if(current->epoch==std::numeric_limits<std::uint64_t>::max()||v.epoch!=current->epoch+1||!v.previous_epoch_ref||!same(*v.previous_epoch_ref,ObligationResponsibilityEpochRef{r,current->epoch})||std::holds_alternative<OriginalResponsibilitySource>(v.change_source))fail("EconomicResponsibilityEpochMismatch",r.obligation_id);take(x.original.validate_original_responsibility_epoch(v));moment(v.effective_at,now,r.obligation_id);if(cmp(v.effective_at,at)<0)fail("EconomicResponsibilityTimeReversal",r.obligation_id);const auto order=header(x.common,v.original_source_ref.source_ref);if(order<=at_order)fail("EconomicResponsibilitySourceOrderMismatch",r.obligation_id);at_order=order;at=v.effective_at;current=&v;event=ObligationResponsibilityEpochRef{r,v.epoch};
  }else{const auto&v=*next_payment;const auto&p=take(x.original.read_original_cash_payment(v.original_payment_ref));const auto&payable=payable_binding(x,v.arrear_ref);if(!same(payable.obligation_ref,r)||!responsibility_covers(x,*current,payable))fail("EconomicPaymentOutsideAcceptedResponsibility",v.original_payment_ref.id);if(!p.payment||x.economy.payments.find(v.original_payment_ref.id)!=p.payment||!same(v.responsibility_epoch_ref,ObligationResponsibilityEpochRef{r,current->epoch})||p.payment->obligation_id!=r.obligation_id||!same(v.arrear_ref,{"local_economy","contract_payment_due",p.payment->arrear_id})||p.payment->from_wallet_id!=current->source_wallet_ref||p.payment->to_wallet_id!=current->destination_wallet_ref||p.payment->authority_ref!=current->authority_ref||!same(v.original_payment_trigger_ref,p.original_payment_trigger_ref)||cmp(v.actual_at,p.actual_at)||p.payment->tick!=v.actual_at.half_day_tick||p.payment->amount<=0)fail("EconomicHistoricalPaymentResponsibilityMismatch",v.original_payment_ref.id);const auto order=header(x.common,v.original_payment_ref);if(order<=at_order)fail("EconomicPaymentResponsibilitySourceOrderMismatch",v.original_payment_ref.id);at_order=order;if(purpose==CashReadPurpose::complete)take(x.original.validate_original_cash_payment_authority(v.original_payment_ref));moment(v.actual_at,now,v.original_payment_ref.id);if(cmp(v.actual_at,at)<0)fail("EconomicPaymentTimeReversal",v.original_payment_ref.id);at=v.actual_at;event=v.original_payment_ref;if(requested&&same(v.original_payment_ref,*requested))result.at_payment=current;}
 }
 if(visited!=epochs+payments)fail("EconomicResponsibilityOrphan",r.obligation_id);
 if(!subject(o->principal,current->debtor)||!subject(o->beneficiary,current->creditor)||o->source_wallet_or_inventory!=current->source_wallet_ref||o->destination!=current->destination_wallet_ref||o->authority_ref!=current->authority_ref)fail("EconomicCurrentResponsibilityProjectionMismatch",r.obligation_id);
 if(requested&&!result.at_payment)fail("EconomicPaymentNotInResponsibilityChain",requested->id);result.current=current;result.tail=event;return result;
}
}
Status validate_original_obligation_genesis(const ObligationResponsibilityEpoch&e,const ObligationResponsibilityEpoch&actual){try{
 if(actual.accepted_arrear_refs.size()!=1||actual.include_future_occurrences||actual.current_source_amendment_ref||!actual.source_versions.empty())fail("OriginalResponsibilityGenesisMismatch",actual.obligation_ref.obligation_id);
 if(e.epoch||e.previous_epoch_ref||e.previous_execution_event_ref||actual.epoch||actual.previous_epoch_ref||actual.previous_execution_event_ref||!std::holds_alternative<OriginalResponsibilitySource>(e.change_source)||!std::holds_alternative<OriginalResponsibilitySource>(actual.change_source))fail("OriginalResponsibilityGenesisMismatch",e.obligation_ref.obligation_id);
 if(!same_obligation_ref(e.obligation_ref,actual.obligation_ref)||!same(e.original_source_ref,actual.original_source_ref)||!same(std::get<OriginalResponsibilitySource>(e.change_source).source,actual.original_source_ref)||e.include_future_occurrences||e.current_source_amendment_ref||e.accepted_arrear_refs.size()!=1||!same(e.accepted_arrear_refs.front(),actual.accepted_arrear_refs.front())||e.debtor.kind!=actual.debtor.kind||e.debtor.id!=actual.debtor.id||e.creditor.kind!=actual.creditor.kind||e.creditor.id!=actual.creditor.id||!same(e.executor_authorization_ref,actual.executor_authorization_ref)||e.source_wallet_ref!=actual.source_wallet_ref||e.destination_wallet_ref!=actual.destination_wallet_ref||!same(e.receiver_source_ref,actual.receiver_source_ref)||e.authority_ref!=actual.authority_ref||cmp(e.effective_at,actual.effective_at)||e.consent_refs.size()!=actual.consent_refs.size()||!e.source_versions.empty())fail("OriginalResponsibilityGenesisMismatch",e.original_source_ref.source_ref.id);
 for(std::size_t i=0;i<e.consent_refs.size();++i)if(!same(e.consent_refs[i],actual.consent_refs[i]))fail("OriginalResponsibilityConsentMismatch",e.original_source_ref.source_ref.id);
 return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}
bool same_economic_obligation_original_source(const EconomicObligationOriginalSource&a,const EconomicObligationOriginalSource&b){
 if(a.index()!=b.index())return false;
 return std::visit([&](const auto&v){using T=std::decay_t<decltype(v)>;const auto&w=std::get<T>(b);
  if constexpr(std::is_same_v<T,OriginalRecurringObligationSource>)return same(v.original_source,w.original_source);
  else if constexpr(std::is_same_v<T,OriginalGovernanceWorkCharge>||std::is_same_v<T,OriginalOrderedEarnedCharge>||std::is_same_v<T,OriginalShopCancellationFee>||std::is_same_v<T,OriginalRepairEarnedCharge>)return same(v.original_charge,w.original_charge);
  else if constexpr(std::is_same_v<T,OriginalServiceTerminationPayable>)return same(v.original_allocation_or_charge,w.original_allocation_or_charge);
  else if constexpr(std::is_same_v<T,OriginalShopOrderRefund>)return same(v.original_refund,w.original_refund);
  else if constexpr(std::is_same_v<T,OriginalCrewShareAccrual>)return same(v.original_accrual,w.original_accrual);
  else if constexpr(std::is_same_v<T,OriginalCrewShareCorrection>)return same(v.original_correction,w.original_correction);
  else if constexpr(std::is_same_v<T,OriginalMaterialAidCompensationPayable>)return same(v.original_late_compensation,w.original_late_compensation);
  else if constexpr(std::is_same_v<T,OriginalMaterialAidPayable>)return same(v.original_establishment,w.original_establishment);
  else if constexpr(std::is_same_v<T,OriginalTransportExpensePayable>)return same(v.original_expense,w.original_expense);
  else {if(v.contract_ref.contract_id!=w.contract_ref.contract_id||v.contract_ref.terms_revision!=w.contract_ref.terms_revision)return false;
   if constexpr(std::is_same_v<T,OriginalInvestmentPrincipal>)return same(v.disbursement_ref,w.disbursement_ref);
   else if constexpr(std::is_same_v<T,OriginalJointDistribution>)return v.party_ordinal==w.party_ordinal&&same(v.distribution_ref,w.distribution_ref);
   else {if(v.original_disbursement_refs.size()!=w.original_disbursement_refs.size())return false;for(std::size_t i=0;i<v.original_disbursement_refs.size();++i)if(!same(v.original_disbursement_refs[i],w.original_disbursement_refs[i]))return false;return true;}
  }
 },a);
}
bool same_obligation_execution_event(const ObligationExecutionEventRef&a,const ObligationExecutionEventRef&b){if(a.index()!=b.index())return false;if(const auto*p=std::get_if<ObligationResponsibilityEpochRef>(&a))return same(*p,std::get<ObligationResponsibilityEpochRef>(b));return same(std::get<SourceRef>(a),std::get<SourceRef>(b));}
Outcome<const ObligationResponsibilityEpoch*>read_current_obligation_responsibility(const EconomicObligationReadContext&x,const ObligationRef&r){try{return {true,chain(x,r).current,{}};}catch(Failure&e){return {false,nullptr,std::move(e.error)};}}
Outcome<const ObligationResponsibilityEpoch*>resolve_obligation_responsibility_at_payment(const EconomicObligationReadContext&x,const SourceRef&r){try{const auto&b=payment_binding(x,r);return {true,chain(x,b.obligation_ref,&r).at_payment,{}};}catch(Failure&e){return {false,nullptr,std::move(e.error)};}}
namespace {
Outcome<EconomicPayableView>read_payable(const EconomicObligationReadContext&x,const SourceRef&r,CashReadPurpose purpose){try{
 const auto now=context(x);const auto&b=payable_binding(x,r);const auto&origin=binding(x,b.obligation_ref);
 if(origin.scheduler!=EconomicObligationScheduler::source_only||std::holds_alternative<OriginalRecurringObligationSource>(origin.original_source))fail("EconomicSourceOnlyReaderRequired",r.id,ErrorKind::unsupported_capability);
 if(!same_economic_obligation_original_source(origin.original_source,b.original_source))fail("EconomicPayableOriginMismatch",r.id);
 const auto original=take(x.original.read_original_payable(b.original_source));if(!same(original.obligation_ref,b.obligation_ref)||!same(original.arrear_ref,r)||!same(original.canonical_charge_ref,b.canonical_charge_ref)||cmp(original.established_at,b.established_at)||cmp(original.due_at,b.due_at)||original.original_due<=0||cmp(b.due_at,b.established_at)<0)fail("EconomicOriginalPayableMismatch",r.id);
 moment(b.established_at,now,r.id);const auto*d=x.economy.arrears.find(r.id);const auto*o=x.economy.obligations.find(b.obligation_ref.obligation_id);const auto*remainder=o?x.economy.obligation_remainders.find(o->id):nullptr;
 if(!o||!d||!remainder||*remainder||o->active||o->period_ticks||o->next_due_tick||!o->rate_profile.empty()||o->kind!=ObligationKind::contract_payment||d->obligation_id!=o->id||d->original_due!=original.original_due||d->due_tick!=b.due_at.half_day_tick||d->paid_amount<0)fail("EconomicSourceOnlyShapeMismatch",r.id);
 const auto due_order=header(x.common,r);if(header(x.common,b.canonical_charge_ref.source_ref)>=due_order)fail("EconomicPayableBeforeOriginalCharge",r.id);
 const auto responsibility=chain(x,b.obligation_ref,nullptr,purpose);EconomicPayableView out{o,d,&b,responsibility.current,0,0,0,d->original_due,responsibility.tail};
 WorldMoment last_financial_at=b.established_at;
 std::size_t seen_adjustment=0,seen_cash=0,position=0;
 for(const auto&row:x.common.state().facts.rows()){const SourceRef event{row.second.domain,row.second.kind,row.second.id};bool event_seen=false;
  if(const auto*p=x.economy.payments.find(row.first);p&&p->arrear_id==d->id){if(event.domain!="local_economy"||event.kind!="obligation_payment"||p->id!=event.id||p->obligation_id!=o->id||p->due_tick!=d->due_tick||p->amount<=0||position<=due_order||p->amount>out.outstanding)fail("EconomicOriginalCashHistoryMismatch",p->id);const auto&pay=payment_binding(x,event);const auto actual=take(x.original.read_original_cash_payment(event));if(cmp(actual.actual_at,b.due_at)<0||cmp(actual.actual_at,last_financial_at)<0)fail("EconomicPaymentBeforeActualDue",p->id);last_financial_at=actual.actual_at;if(!same(pay.obligation_ref,b.obligation_ref)||!same(pay.arrear_ref,r))fail("EconomicCashWrongPayable",p->id);out.cash_paid=add(out.cash_paid,p->amount);out.outstanding-=p->amount;++seen_cash;event_seen=true;}
  for(const auto&a:x.sources.adjustments)if(same(a.original_arrear_ref,r)&&same(a.original_adjustment_ref.source_ref,event)){if(event_seen||position<=due_order||!same(a.obligation_ref,b.obligation_ref))fail("EconomicAdjustmentBindingMismatch",event.id);const auto actual=take(x.original.read_original_adjustment(a.original_adjustment_ref));if(!same(actual.original_adjustment_ref,a.original_adjustment_ref)||!same(actual.obligation_ref,b.obligation_ref)||!same(actual.arrear_ref,r)||actual.kind!=a.kind||actual.amount<=0||cmp(actual.occurred_at,b.established_at)<0)fail("EconomicOriginalAdjustmentMismatch",event.id);moment(actual.occurred_at,now,event.id);if(cmp(actual.occurred_at,last_financial_at)<0)fail("EconomicAdjustmentTimeReversal",event.id);last_financial_at=actual.occurred_at;if(a.kind==EconomicPayableAdjustmentKind::positive_increment){out.positive_increments=add(out.positive_increments,actual.amount);out.outstanding=add(out.outstanding,actual.amount);}else if(a.kind==EconomicPayableAdjustmentKind::unpaid_void){if(actual.amount>out.outstanding)fail("EconomicVoidExceedsActualUnpaid",event.id);out.unpaid_void=add(out.unpaid_void,actual.amount);out.outstanding-=actual.amount;}else fail("UnsupportedEconomicAdjustmentKind",event.id);++seen_adjustment;event_seen=true;}
  ++position;
 }
 std::size_t cash_count=0,adjust_count=0;for(const auto&p:x.economy.payments.rows())if(p.second.arrear_id==d->id)++cash_count;for(const auto&a:x.sources.adjustments)if(same(a.original_arrear_ref,r))++adjust_count;
 if(cash_count!=seen_cash||adjust_count!=seen_adjustment||out.cash_paid!=d->paid_amount)fail("EconomicPayableCashAndSourceCoverageMismatch",r.id);if(out.outstanding&&!responsibility_covers(x,*out.current_responsibility,b))fail("EconomicOpenPayableOutsideAcceptedResponsibility",r.id);return {true,out,{}};
}catch(Failure&e){return {false,{},std::move(e.error)};}}
} // one balance/history algorithm, two named dependency layers
Outcome<EconomicPayableView>read_economic_payable(const EconomicObligationReadContext&x,const SourceRef&r){return read_payable(x,r,CashReadPurpose::complete);}
Outcome<EconomicPayableView>read_economic_payable_for_budget_evidence(const EconomicObligationReadContext&x,const SourceRef&r){return read_payable(x,r,CashReadPurpose::original_budget_evidence);}
Status validate_economic_obligation_sources(const EconomicObligationReadContext&x){try{
 context(x);for(const auto&b:x.sources.obligation_sources){binding(x,b.obligation_ref);if(b.scheduler==EconomicObligationScheduler::recurring)fail("EconomicRecurringSourceAdapterRequired",b.obligation_ref.obligation_id,ErrorKind::unsupported_capability);std::size_t n=0;for(const auto&p:x.sources.payable_sources)if(same(p.obligation_ref,b.obligation_ref)){take(read_economic_payable(x,p.original_arrear_ref));++n;}if(!n)fail("EconomicObligationWithoutOriginalPayable",b.obligation_ref.obligation_id);
  const auto*o=x.economy.obligations.find(b.obligation_ref.obligation_id);StableId last;
  for(const auto&row:x.common.state().facts.rows()){
   if(const auto*d=x.economy.arrears.find(row.first);d&&d->obligation_id==o->id){payable_binding(x,{"local_economy","contract_payment_due",row.first});last=row.first;}
   if(const auto*p=x.economy.payments.find(row.first);p&&p->obligation_id==o->id){payment_binding(x,{"local_economy","obligation_payment",row.first});last=row.first;}
  }
  if(last.empty()||o->last_receipt_id!=last)fail("EconomicOriginalLastReceiptMismatch",b.obligation_ref.obligation_id);
 }
 for(const auto&p:x.sources.payable_sources)take(read_economic_payable(x,p.original_arrear_ref));
 for(const auto&a:x.sources.adjustments)payable_binding(x,a.original_arrear_ref);
 for(const auto&e:x.sources.responsibility_epochs)binding(x,e.obligation_ref);
 for(const auto&p:x.sources.payment_responsibility_bindings){binding(x,p.obligation_ref);payable_binding(x,p.arrear_ref);payment_binding(x,p.original_payment_ref);}
 for(const auto&p:x.sources.legacy_responsibility_prefixes){binding(x,p.obligation_ref);fail("EconomicLegacyResponsibilityOriginalReaderRequired",p.obligation_ref.obligation_id,ErrorKind::unsupported_capability);}
 for(const auto&r:x.sources.recovery_bindings){binding(x,r.original_obligation_ref);binding(x,r.recovery_obligation_ref);fail("EconomicRecoveryOriginalClosureReaderRequired",r.original_correction_ref.source_ref.id,ErrorKind::unsupported_capability);}
 return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}
}
