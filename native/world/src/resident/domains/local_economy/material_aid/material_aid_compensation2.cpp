#include "material_aid_compensation2.hpp"
#include "material_aid_compensation_basis.hpp"
#include "material_aid_internal.hpp"
#include "../service_finance_execution.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include <set>
namespace hanmo::resident {using namespace aid_detail;
SourceRef material_aid_compensation_scope_source2(const StableId&id){return {"local_economy","aid_compensation_accepted",id};}
SourceRef material_aid_compensation_disposition_source2(const StableId&id){return {"local_economy","aid_compensation_disposed",id};}
Status validate_material_aid_compensation_values2(const MaterialAidCompensationState2&s){try{
 std::set<StableId>ids,commitments;
 auto identity=[&](const StableId&key,const StableId&id){if(key.empty()||key!=id||!ids.insert(id).second)fail("AidCompensationRecordIdentityMismatch",key);};
 if(s.scopes.rows().size()>CodecVisitor::max_items||s.holds.rows().size()>CodecVisitor::max_items||s.dispositions.rows().size()>CodecVisitor::max_items||s.payments.rows().size()>CodecVisitor::max_items)fail("CheckpointCollectionTooLarge");
 for(const auto&r:s.scopes.rows()){const auto&v=r.second;identity(r.first,v.id);
  if(!valid_source_ref(v.original_schedule_commitment)||!same_source_ref(v.original_schedule_commitment,aid_schedule_commitment_source(v.original_schedule_commitment.id))||!commitments.insert(v.original_schedule_commitment.id).second||!valid_source_ref(v.original_resource_acceptance.source_ref)||v.original_resource_acceptance.source_schema<1||v.occurrence_hold_refs.empty()||v.occurrence_hold_refs.size()>CodecVisitor::max_items)fail("AidCompensationScopeShapeMismatch",v.id);
  take(encode_world_moment(v.accepted_at));std::set<StableId>owned;
  for(std::size_t n=0;n<v.occurrence_hold_refs.size();++n){const auto&id=v.occurrence_hold_refs[n];const auto*h=s.holds.find(id);if(!owned.insert(id).second||!h||h->scope_id!=v.id||h->occurrence_ordinal!=static_cast<Integer>(n))fail("AidCompensationHoldCoverageMismatch",id);}
 }
 for(const auto&r:s.holds.rows()){const auto&h=r.second;identity(r.first,h.id);const auto*scope=s.scopes.find(h.scope_id);
  if(!scope||h.occurrence_ordinal<0||static_cast<std::uint64_t>(h.occurrence_ordinal)>=scope->occurrence_hold_refs.size()||scope->occurrence_hold_refs[h.occurrence_ordinal]!=h.id)fail("AidCompensationOrphanHold",h.id);
  if(h.disposition_ref){const auto*d=s.dispositions.find(*h.disposition_ref);if(!d||d->hold_id!=h.id)fail("AidCompensationDispositionCoverageMismatch",h.id);}
 }
 for(const auto&r:s.dispositions.rows()){const auto&d=r.second;identity(r.first,d.id);const auto*h=s.holds.find(d.hold_id);
  if(!h||!h->disposition_ref||*h->disposition_ref!=d.id||!valid_source_ref(d.original_cause.source_ref)||d.original_cause.source_schema!=1||bool(d.original_obligation_id)!=bool(d.original_arrear_id)||(d.original_obligation_id&&(d.original_obligation_id->empty()||d.original_arrear_id->empty()||*d.original_obligation_id==*d.original_arrear_id)))fail("AidCompensationDispositionShapeMismatch",d.id);
  switch(d.kind){case AidCompensationDispositionKind2::received_in_time:case AidCompensationDispositionKind2::cancelled_future:if(d.original_obligation_id)fail("AidCompensationReleaseCannotCreateDebt",d.id);break;case AidCompensationDispositionKind2::late_due:break;default:fail("UnsupportedAidCompensationDisposition",d.id);}
  if(d.original_obligation_id&&(!ids.insert(*d.original_obligation_id).second||!ids.insert(*d.original_arrear_id).second))fail("AidCompensationDuplicatePayableIdentity",d.id);
  take(encode_world_moment(d.occurred_at));
 }
 for(const auto&r:s.payments.rows()){const auto&v=r.second;identity(r.first,v.id);const auto*d=s.dispositions.find(v.disposition_id);if(!d||!d->original_obligation_id||d->kind!=AidCompensationDispositionKind2::late_due)fail("AidCompensationPaymentOriginalDueRequired",v.id);take(encode_world_moment(v.paid_at));}
 return common_success();}catch(Failure&e){return {false,{},std::move(e.error)};}}
namespace {
struct ScopeRead{const AidScheduleCommitment*principal;MaterialAidAcceptedResources accepted;const TreatyBoundedCompensation*policy;};
ScopeRead scope_original(const MaterialAidCompensationReadContext2&x,const AidCompensationScope2&s){
 take(validate_material_aid_compensation_values2(x.state));const auto&p=x.principal;now(p);const auto&c=commitment(p,s.original_schedule_commitment.id);validate_commitment_identity(p,c);
 const auto t=terms(p,schedule(p,c.schedule_id));const auto*policy=std::get_if<TreatyBoundedCompensation>(&t.original_term->late_policy);
 if(!policy||policy->currency_id!="qian"||policy->maximum_amount<0||policy->obligation_owner!="local_economy"||static_cast<std::uint64_t>(t.original_term->occurrences)!=s.occurrence_hold_refs.size()||compare(c.accepted_at,s.accepted_at)||!same(c.resource_acceptance_ref,s.original_resource_acceptance))fail("AidCompensationOriginalScopeMismatch",s.id);
 before(p.common,s.original_schedule_commitment,material_aid_compensation_scope_source2(s.id));
 auto accepted=accepted_resources(p,c);if(!accepted.compensation)fail("AidCompensationExplicitOriginalSelectionRequired",s.id);
 const auto&a=*accepted.compensation;const auto&b=a.budget_adoption;
 if(a.money_route.source_wallet_id.empty()||a.money_route.beneficiary_wallet_id.empty()||a.money_route.source_wallet_id==a.money_route.beneficiary_wallet_id||!b.id.empty()||!b.responsibility_ref.domain.empty()||!b.responsibility_ref.kind.empty()||!b.responsibility_ref.id.empty()||b.responsibility_source_schema||b.execution_ref.budget_id.empty()||b.execution_ref.budget_revision<1||b.execution_ref.policy_revision<0||b.execution_ref.approval_fact_id.empty()||b.period.period_from_tick<0||b.period.period_until_tick<=b.period.period_from_tick||b.purpose!=BudgetExecutionPurpose::material_aid||!std::holds_alternative<BudgetMoneyQian>(b.unit)||b.payer_container_id!=a.money_route.source_wallet_id||b.execution_ref.budget_id!=t.original_term->budget_ref.authorization_id||b.execution_ref.budget_revision!=t.original_term->budget_ref.authorization_revision||b.execution_ref.approval_fact_id!=t.original_term->budget_ref.approval_source.id||b.period.period_from_tick>s.accepted_at.half_day_tick||compare(s.accepted_at,WorldMoment{b.period.period_until_tick,{}})>=0)fail("AidCompensationOriginalBudgetSelectionMismatch",s.id);
 return {&c,std::move(accepted),policy};
}
void original_binding(const MaterialAidCompensationReadContext2&x,const AidCompensationScope2&s,const ScopeRead&original,const BudgetExecutionBinding*proposed=nullptr){
 const auto&b=original.accepted.compensation->budget_adoption;const BudgetExecutionBinding*binding=proposed;
 if(!binding)for(const auto&v:x.principal.budget.bindings)if(same_source_ref(v.responsibility_ref,material_aid_compensation_scope_source2(s.id))){if(binding)fail("AidCompensationAmbiguousBudgetBinding",v.id);binding=&v;}
 if(!binding||!same_source_ref(binding->responsibility_ref,material_aid_compensation_scope_source2(s.id))||binding->responsibility_source_schema!=2||!same(binding->execution_ref,b.execution_ref)||!same(binding->period,b.period)||!same(binding->unit,b.unit)||binding->payer_container_id!=b.payer_container_id||binding->purpose!=b.purpose||compare(binding->accepted_at,s.accepted_at))fail("AidCompensationBudgetBindingMismatch",s.id);
}

}
Outcome<MaterialAidCompensationOriginal2>read_material_aid_compensation_original2(const MaterialAidCompensationReadContext2&x,const MomentOwnerSource&source){try{
 take(validate_material_aid_compensation_values2(x.state));const auto&p=x.principal;
 if(source.source_schema!=2||!same_source_ref(source.source_ref,material_aid_compensation_disposition_source2(source.source_ref.id)))fail("AidCompensationOriginalDispositionRequired",source.source_ref.id);
 const auto*d=x.state.dispositions.find(source.source_ref.id);if(!d)fail("AidCompensationOriginalDispositionRequired",source.source_ref.id);
 const auto*h=x.state.holds.find(d->hold_id);const auto*s=x.state.scopes.find(h->scope_id);const auto original=scope_original(x,*s);original_binding(x,*s,original);const auto&c=*original.principal;
 before(p.common,material_aid_compensation_scope_source2(s->id),source.source_ref);before(p.common,d->original_cause.source_ref,source.source_ref);
 if(compare(d->occurred_at,s->accepted_at)<0||compare(d->occurred_at,now(p))>0)fail("AidCompensationDispositionMomentMismatch",d->id);
 if(d->kind==AidCompensationDispositionKind2::cancelled_future){
  const auto*r=take(read_material_aid_schedule_release(p,d->original_cause.source_ref));if(r->schedule_commitment_id!=c.id||compare(r->occurred_at,d->occurred_at)||static_cast<std::uint64_t>(h->occurrence_ordinal)<c.established_occurrence_refs.size())fail("AidCompensationCannotReleaseEstablishedOccurrence",d->id);
 }else{
  if(static_cast<std::uint64_t>(h->occurrence_ordinal)>=c.established_occurrence_refs.size())fail("AidCompensationOriginalEstablishedOccurrenceRequired",d->id);
  const auto&e=establishment(p,c.established_occurrence_refs[h->occurrence_ordinal]);const auto&o=obligation(p,e.original_obligation_ref);validate_established_terms(p,o);
  if(d->kind==AidCompensationDispositionKind2::late_due){
   const auto basis=take(read_material_aid_compensation_basis(p,e.original_obligation_ref,d->original_cause.source_ref));
   if(compare(basis.actual_deadline,d->occurred_at)||basis.amount!=original.policy->maximum_amount||bool(d->original_obligation_id)!=(basis.amount>0))fail("AidCompensationOriginalLateAmountMismatch",d->id);
   const auto*late=p.state.deadline_results.find(d->original_cause.source_ref.id);
   if(basis.amount>0){if(!late->compensation_obligation_ref||late->compensation_obligation_ref->owner_domain!="local_economy"||late->compensation_obligation_ref->obligation_id!=*d->original_obligation_id||*d->original_obligation_id==o.id)fail("AidCompensationOriginalLatePayableMismatch",d->id);}
   else if(late->compensation_obligation_ref)fail("AidCompensationZeroMaximumCannotCreateDebt",d->id);
  }else{
   if(!same_source_ref(d->original_cause.source_ref,material_aid_receipt_source(d->original_cause.source_ref.id)))fail("AidCompensationOriginalReceivingRequired",d->id);
   const auto*r=p.state.receipts.find(d->original_cause.source_ref.id);if(!r||r->obligation_id!=o.id||compare(receipt(p,o,*r).occurred_at,d->occurred_at)||compare(d->occurred_at,WorldMoment{o.delivery_deadline_tick,{}})>0)fail("AidCompensationInTimeReceivingRequired",d->id);
   const auto cause_order=header(p.common,d->original_cause.source_ref),cut=header(p.common,source.source_ref);
   for(const auto&id:o.receipt_result_refs){const auto order=header(p.common,material_aid_receipt_source(id));if(order>cause_order&&order<cut)fail("AidCompensationReleaseMustNameCompletingReceipt",d->id);}
   for(const auto&m:remaining(p,o,&d->occurred_at,&source.source_ref))if(m.remaining)fail("AidCompensationUnfulfilledRelease",d->id);
  }
 }
 return {true,{s,h,d,std::move(original.accepted),original.policy},{}};
 }catch(Failure&e){return {false,{},std::move(e.error)};}}
Status validate_material_aid_compensation_original_coverage2(const MaterialAidCompensationReadContext2&x){try{
 take(validate_material_aid_compensation_values2(x.state));const auto&p=x.principal;
 for(const auto&r:x.state.scopes.rows()){const auto original=scope_original(x,r.second);original_binding(x,r.second,original);}
 for(const auto&r:x.state.dispositions.rows())take(read_material_aid_compensation_original2(x,{material_aid_compensation_disposition_source2(r.first),2}));
 for(const auto&r:p.state.schedule_commitments.rows()){const auto t=terms(p,schedule(p,r.second.schedule_id));std::size_t count=0;for(const auto&s:x.state.scopes.rows())if(s.second.original_schedule_commitment.id==r.first)++count;if(count!=std::size_t(std::holds_alternative<TreatyBoundedCompensation>(t.original_term->late_policy)))fail("AidCompensationSelectedScopeCoverageMismatch",r.first);}
 for(const auto&r:x.state.scopes.rows()){const auto&scope=r.second;const auto&com=commitment(p,scope.original_schedule_commitment.id);
  for(const auto&id:scope.occurrence_hold_refs){const auto&h=*x.state.holds.find(id);const auto*d=h.disposition_ref?x.state.dispositions.find(*h.disposition_ref):nullptr;
   if(static_cast<std::uint64_t>(h.occurrence_ordinal)<com.established_occurrence_refs.size()){
    const auto&e=establishment(p,com.established_occurrence_refs[h.occurrence_ordinal]);const auto&o=obligation(p,e.original_obligation_ref);
    if(o.deadline_result_ref&&(!d||d->kind!=AidCompensationDispositionKind2::late_due))fail("AidCompensationOriginalLateCoverageMismatch",o.id);
    const WorldMoment deadline{o.delivery_deadline_tick,{}};bool full=true;for(const auto&m:remaining(p,o,&deadline))full&=m.remaining==0;
    if(full&&!o.deadline_result_ref&&(!d||d->kind!=AidCompensationDispositionKind2::received_in_time))fail("AidCompensationInTimeReleaseCoverageMismatch",o.id);
   }else if(com.termination_source_ref&&(!d||d->kind!=AidCompensationDispositionKind2::cancelled_future))fail("AidCompensationFutureReleaseCoverageMismatch",h.id);
  }
 }
 for(const auto&r:x.state.dispositions.rows())if(r.second.original_obligation_id){
  const auto*context=p.obligations;if(!context||&context->common!=&p.common||&context->economy!=&p.economy||&context->clock!=&p.clock||context->sources.schema_version!=2)fail("AidCompensationSameWorldP11Required",r.first);
  const auto due=take(read_economic_payable(*context,{"local_economy","contract_payment_due",*r.second.original_arrear_id}));if(due.obligation->id!=*r.second.original_obligation_id||due.positive_increments||due.unpaid_void)fail("AidCompensationOriginalP11CoverageMismatch",r.first);
  for(const auto&cash:p.economy.payments.rows())if(cash.second.obligation_id==*r.second.original_obligation_id){const auto*row=x.state.payments.find(cash.first);if(!row||row->disposition_id!=r.first)fail("AidCompensationUnadoptedOriginalPayment",cash.first);}
 }
 for(const auto&r:x.state.payments.rows()){
  if(!p.obligations)fail("AidCompensationSameWorldP11Required",r.first);const auto cash=take(p.obligations->original.read_original_cash_payment({"local_economy","obligation_payment",r.first}));if(cash.payment!=p.economy.payments.find(r.first))fail("AidCompensationCashSameRootRequired",r.first);
 }
 for(const auto&r:p.common.state().facts.rows()){const auto&f=r.second;if(f.domain!="local_economy")continue;if(f.kind=="aid_compensation_accepted"&&!x.state.scopes.find(f.id))fail("AidCompensationOrphanCommonScope",f.id);if(f.kind=="aid_compensation_disposed"&&!x.state.dispositions.find(f.id))fail("AidCompensationOrphanCommonDisposition",f.id);}
 return common_success();}catch(Failure&e){return {false,{},std::move(e.error)};}}
Outcome<ActualBudgetResponsibility>NativeMaterialAidCompensationBudgetRead2::read_budget_responsibility(const BudgetExecutionBinding&binding)const{try{
 const auto&p=x_.principal;if(!same_source_ref(binding.responsibility_ref,material_aid_compensation_scope_source2(binding.responsibility_ref.id)))fail("AidCompensationBudgetResponsibilityRequired",binding.id);
 const auto*scope=x_.state.scopes.find(binding.responsibility_ref.id);if(!scope)fail("AidCompensationScopeRequired",binding.id);
 const auto original=scope_original(x_,*scope);original_binding(x_,*scope,original,&binding);
 ActualBudgetResponsibility out;out.responsibility_ref=binding.responsibility_ref;out.source_schema=2;out.unit=binding.unit;out.period=binding.period;
 for(const auto&id:scope->occurrence_hold_refs){const auto*h=x_.state.holds.find(id);if(!h||h->scope_id!=scope->id)fail("AidCompensationHoldRequired",id);const auto maximum=original.policy->maximum_amount;
  if(!h->disposition_ref){if(maximum>0)out.commitments.push_back({"local_economy",h->id,scope->id,maximum,binding.execution_ref});continue;}
  const auto source=take(read_material_aid_compensation_original2(x_,{material_aid_compensation_disposition_source2(*h->disposition_ref),2}));const auto&d=*source.disposition;
  if(d.kind!=AidCompensationDispositionKind2::late_due){if(maximum>0)out.releases.push_back({"local_economy",h->id,d.id,maximum,binding.execution_ref});continue;}
  if(!maximum)continue;
  const auto*ob=p.obligations;if(!ob||&ob->common!=&p.common||&ob->economy!=&p.economy||&ob->clock!=&p.clock||ob->sources.schema_version!=2)fail("AidCompensationSameWorldP11Required",d.id);
  const SourceRef arrear{"local_economy","contract_payment_due",*d.original_arrear_id};const auto due=take(read_economic_payable_for_budget_evidence(*ob,arrear));
  if(due.positive_increments||due.unpaid_void||due.obligation->id!=*d.original_obligation_id||due.arrear->original_due!=maximum||!same_source_ref(due.source->canonical_charge_ref.source_ref,material_aid_compensation_disposition_source2(d.id))||due.source->canonical_charge_ref.source_schema!=2)fail("AidCompensationOriginalP11Mismatch",d.id);
  if(due.outstanding>0)out.commitments.push_back({"local_economy",due.obligation->id,scope->id,due.outstanding,binding.execution_ref});
  for(const auto&payment:ob->sources.payment_responsibility_bindings)if(payment.obligation_ref.owner_domain=="local_economy"&&payment.obligation_ref.obligation_id==due.obligation->id){const auto*cash=p.economy.payments.find(payment.original_payment_ref.id);if(!cash||cash->amount<=0)fail("AidCompensationOriginalCashRequired",payment.original_payment_ref.id);out.charges.push_back({"local_economy",due.obligation->id,cash->id,cash->amount,binding.execution_ref});}
 }
 return {true,std::move(out),{}};
 }catch(Failure&e){return {false,{},std::move(e.error)};}}
Outcome<SourceRef>accept_material_aid_compensation_in_candidate2(CommonStore&common,MaterialAidCompensationState2&state,const MaterialAidCompensationReadContext2&x,const MaterialAidWriteContext&write,const SourceRef&commitment_ref){try{
 take(validate_material_aid_compensation_values2(state));const auto&p=x.principal;if(&common!=&p.common||&state!=&x.state||&write.read!=&p||&write.budget!=&p.budget||&write.economy!=&p.economy)fail("AidCompensationCandidateIdentityMismatch");
 if(!same_source_ref(commitment_ref,aid_schedule_commitment_source(commitment_ref.id)))fail("AidCompensationOriginalScheduleRequired",commitment_ref.id);
 const auto&c=commitment(p,commitment_ref.id);validate_commitment_identity(p,c);const auto t=terms(p,schedule(p,c.schedule_id));const auto*policy=std::get_if<TreatyBoundedCompensation>(&t.original_term->late_policy);
 if(!policy||policy->currency_id!="qian"||policy->maximum_amount<0||policy->obligation_owner!="local_economy")fail("AidCompensationOriginalPolicyRequired",c.id);
 const auto accepted=accepted_resources(p,c);if(!accepted.compensation)fail("AidCompensationExplicitOriginalSelectionRequired",c.id);
 for(const auto&r:state.scopes.rows())if(same_source_ref(r.second.original_schedule_commitment,commitment_ref)){const auto original=scope_original(x,r.second);original_binding(x,r.second,original);return {true,material_aid_compensation_scope_source2(r.first),{}};}
 if(compare(c.accepted_at,now(p))||t.original_term->occurrences<=0||static_cast<std::uint64_t>(t.original_term->occurrences)>CodecVisitor::max_items)fail("AidCompensationActualAcceptanceBoundaryRequired",c.id);
 AidCompensationScope2 scope;scope.id=take(common.next_id("aid_compensation_scope"));scope.original_schedule_commitment=commitment_ref;scope.original_resource_acceptance=c.resource_acceptance_ref;scope.accepted_at=c.accepted_at;
 for(Integer n=0;n<t.original_term->occurrences;++n){AidCompensationHold2 hold;hold.id=take(common.next_id("aid_compensation_hold"));hold.scope_id=scope.id;hold.occurrence_ordinal=n;scope.occurrence_hold_refs.push_back(hold.id);const auto id=hold.id;if(!state.holds.insert(id,std::move(hold)))fail("AidCompensationDuplicateHold",id);}
 const auto source=material_aid_compensation_scope_source2(scope.id);if(!state.scopes.insert(source.id,std::move(scope)))fail("AidCompensationDuplicateScope",source.id);take(common.append_fact({source.id,source.kind,source.domain}));
 auto binding=accepted.compensation->budget_adoption;if(!binding.id.empty()||!binding.responsibility_ref.domain.empty()||!binding.responsibility_ref.kind.empty()||!binding.responsibility_ref.id.empty()||binding.responsibility_source_schema)fail("AidCompensationBindingIdOwnerRequired",source.id);
 binding.responsibility_ref=source;binding.responsibility_source_schema=2;binding.accepted_at=c.accepted_at;
 take(bind_budget_execution_in_candidate(common,write.budget,write.budget_authorization,write.budget_responsibilities,binding,&p.clock));
 const auto&stored=*state.scopes.find(source.id);const auto original=scope_original(x,stored);original_binding(x,stored,original);return {true,source,{}};
 }catch(Failure&e){return {false,{},std::move(e.error)};}}

namespace {
AidCompensationHold2& owned_hold(MaterialAidCompensationState2&state,const StableId&commitment,Integer ordinal){
 AidCompensationScope2*scope=nullptr;for(const auto&r:state.scopes.rows())if(r.second.original_schedule_commitment.id==commitment){if(scope)fail("AidCompensationAmbiguousScope",commitment);scope=state.scopes.find(r.first);}
 if(!scope||ordinal<0||static_cast<std::uint64_t>(ordinal)>=scope->occurrence_hold_refs.size())fail("AidCompensationOriginalHoldRequired",commitment);
 auto*h=state.holds.find(scope->occurrence_hold_refs[ordinal]);if(!h||h->scope_id!=scope->id||h->occurrence_ordinal!=ordinal)fail("AidCompensationOriginalHoldRequired",commitment);return *h;
}
void compensation_candidate(CommonStore&c,MaterialAidCompensationState2&s,const MaterialAidCompensationReadContext2&x){if(&c!=&x.principal.common||&s!=&x.state)fail("AidCompensationCandidateIdentityMismatch");take(validate_material_aid_compensation_values2(s));now(x.principal);}
SourceRef append_disposition(CommonStore&c,MaterialAidCompensationState2&s,AidCompensationHold2&h,AidCompensationDisposition2 d){
 if(h.disposition_ref||d.hold_id!=h.id)fail("AidCompensationHoldAlreadyDisposed",h.id);const auto source=material_aid_compensation_disposition_source2(d.id);
 if(!s.dispositions.insert(source.id,std::move(d)))fail("AidCompensationDuplicateDisposition",source.id);take(c.append_fact({source.id,source.kind,source.domain}));h.disposition_ref=source.id;return source;
}
}
Outcome<std::optional<SourceRef>>release_material_aid_compensation_on_receipt_in_candidate2(CommonStore&c,MaterialAidCompensationState2&s,const MaterialAidCompensationReadContext2&x,const SourceRef&actual){try{
 compensation_candidate(c,s,x);const auto&p=x.principal;if(!same_source_ref(actual,material_aid_receipt_source(actual.id)))fail("AidCompensationOriginalReceiptRequired",actual.id);
 const auto*r=p.state.receipts.find(actual.id);if(!r)fail("AidCompensationOriginalReceiptRequired",actual.id);const auto&o=obligation(p,{"local_economy",r->obligation_id});const auto received=receipt(p,o,*r);
 auto&h=owned_hold(s,o.schedule_commitment_id,o.occurrence.ordinal);
 if(h.disposition_ref){const auto*d=s.dispositions.find(*h.disposition_ref);if(d->kind==AidCompensationDispositionKind2::received_in_time)return {true,material_aid_compensation_disposition_source2(d->id),{}};return {true,std::nullopt,{}};}
 if(compare(received.occurred_at,WorldMoment{o.delivery_deadline_tick,{}})>0)return {true,std::nullopt,{}};
 for(const auto&m:remaining(p,o))if(m.remaining)return {true,std::nullopt,{}};
 if(compare(received.occurred_at,now(p))||o.receipt_result_refs.empty()||o.receipt_result_refs.back()!=r->id)fail("AidCompensationActualCompletingReceiptRequired",actual.id);
 AidCompensationDisposition2 d;d.id=take(c.next_id("aid_compensation_disposition"));d.hold_id=h.id;d.kind=AidCompensationDispositionKind2::received_in_time;d.original_cause={actual,1};d.occurred_at=received.occurred_at;
 const auto result=append_disposition(c,s,h,std::move(d));take(read_material_aid_compensation_original2(x,{result,2}));return {true,result,{}};
 }catch(Failure&e){return {false,{},std::move(e.error)};}}
Outcome<std::vector<SourceRef>>release_material_aid_compensation_on_termination_in_candidate2(CommonStore&c,MaterialAidCompensationState2&s,const MaterialAidCompensationReadContext2&x,const SourceRef&actual){try{
 compensation_candidate(c,s,x);const auto&p=x.principal;const auto*r=take(read_material_aid_schedule_release(p,actual));const auto&com=commitment(p,r->schedule_commitment_id);
 if(compare(r->occurred_at,now(p)))fail("AidCompensationActualTerminationBoundaryRequired",actual.id);
 const auto t=terms(p,schedule(p,com.schedule_id));std::vector<SourceRef>out;
 for(Integer ordinal=static_cast<Integer>(com.established_occurrence_refs.size());ordinal<t.original_term->occurrences;++ordinal){auto&h=owned_hold(s,com.id,ordinal);
  if(h.disposition_ref){const auto*d=s.dispositions.find(*h.disposition_ref);if(d->kind!=AidCompensationDispositionKind2::cancelled_future||!same_source_ref(d->original_cause.source_ref,actual))fail("AidCompensationTerminationReplayMismatch",h.id);out.push_back(material_aid_compensation_disposition_source2(d->id));continue;}
  AidCompensationDisposition2 d;d.id=take(c.next_id("aid_compensation_disposition"));d.hold_id=h.id;d.kind=AidCompensationDispositionKind2::cancelled_future;d.original_cause={actual,1};d.occurred_at=r->occurred_at;const auto result=append_disposition(c,s,h,std::move(d));take(read_material_aid_compensation_original2(x,{result,2}));out.push_back(result);
 }
 return {true,std::move(out),{}};
 }catch(Failure&e){return {false,{},std::move(e.error)};}}
Outcome<std::optional<SourceRef>>close_material_aid_compensation_deadline_in_candidate2(CommonStore&c,TreatyMaterialAidState&principal,MaterialAidCompensationState2&s,const MaterialAidCompensationReadContext2&x,const MaterialAidWriteContext&write,const ObligationRef&ref){try{
 compensation_candidate(c,s,x);const auto&p=x.principal;if(&principal!=&p.state||&write.read!=&p||&write.economy!=&p.economy||&write.budget!=&p.budget)fail("AidCompensationCandidateIdentityMismatch");
 const auto&old=obligation(p,ref);auto&h=owned_hold(s,old.schedule_commitment_id,old.occurrence.ordinal);
 if(h.disposition_ref){const auto*d=s.dispositions.find(*h.disposition_ref);take(read_material_aid_compensation_original2(x,{material_aid_compensation_disposition_source2(d->id),2}));if(d->kind==AidCompensationDispositionKind2::late_due)return {true,d->original_cause.source_ref,{}};return {true,std::nullopt,{}};}
 if(old.deadline_result_ref)fail("AidCompensationOriginalLateDispositionMissing",old.id);
 if(compare(now(p),WorldMoment{old.delivery_deadline_tick,{}}))fail("AidCompensationActualDeadlineBoundaryRequired",old.id);
 bool unpaid=false;for(const auto&m:remaining(p,old))unpaid|=m.remaining>0;if(!unpaid)fail("AidCompensationCompletingReceiptDispositionMissing",old.id);
 const auto basis=take(read_material_aid_compensation_basis(p,ref));
 AidCompensationDisposition2 d;d.id=take(c.next_id("aid_compensation_disposition"));d.hold_id=h.id;d.kind=AidCompensationDispositionKind2::late_due;d.occurred_at=basis.actual_deadline;
 if(basis.amount>0){d.original_obligation_id=take(c.next_id("aid_compensation_obligation"));d.original_arrear_id=take(c.next_id("aid_compensation_arrear"));}
 MaterialAidDeadlineResult late;late.id=take(c.next_id("material_aid_deadline"));late.obligation_id=old.id;late.occurred_at=basis.actual_deadline;if(d.original_obligation_id)late.compensation_obligation_ref=ObligationRef{"local_economy",*d.original_obligation_id};
 const auto late_source=material_aid_deadline_source(late.id);d.original_cause={late_source,1};const auto obligation_id=old.id;
 if(!principal.deadline_results.insert(late_source.id,std::move(late)))fail("DuplicateMaterialAidDeadline",late_source.id);take(c.append_fact({late_source.id,late_source.kind,late_source.domain}));principal.obligations.find(obligation_id)->deadline_result_ref=late_source.id;
 const auto result=append_disposition(c,s,h,std::move(d));take(read_material_aid_compensation_original2(x,{result,2}));
 if(basis.amount>0){auto*w=take(service_finance::original_obligation_write(c,write.economy,p.obligations,write.obligation_writes));if(w->sources.schema_version!=2)fail("AidCompensationSelectedP11VersionRequired",result.id);take(service_finance::post_payable_in_candidate(*w,OriginalMaterialAidCompensationPayable{{result,2}}));}
 auto*updated=principal.obligations.find(obligation_id);updated->state=projected_state(*updated,remaining(p,*updated));take(validate_treaty_material_aid(p));take(validate_budget_execution(c,write.budget,write.budget_authorization,write.budget_responsibilities,&p.clock));return {true,late_source,{}};
 }catch(Failure&e){return {false,{},std::move(e.error)};}}
namespace {
Status compensation_payable(const MaterialAidCompensationReadContext2&x,const MaterialAidObligation&o,const TreatyBoundedCompensation&policy,const ObligationRef&ref,const WorldMoment&at,bool evidence){try{
 const AidCompensationDisposition2*d=nullptr;for(const auto&r:x.state.dispositions.rows())if(r.second.original_obligation_id&&*r.second.original_obligation_id==ref.obligation_id){if(d)fail("AidCompensationAmbiguousOriginalPayable",ref.obligation_id);d=&r.second;}
 if(!d||ref.owner_domain!="local_economy")fail("AidCompensationOriginalPayableRequired",ref.obligation_id);
 const auto v=take(read_material_aid_compensation_original2(x,{material_aid_compensation_disposition_source2(d->id),2}));
 if(!o.deadline_result_ref||d->original_cause.source_ref.id!=*o.deadline_result_ref||compare(d->occurred_at,at)||v.policy->currency_id!=policy.currency_id||v.policy->maximum_amount!=policy.maximum_amount||v.policy->obligation_owner!=policy.obligation_owner)fail("AidCompensationOriginalPayableMismatch",ref.obligation_id);
 const auto*p=x.principal.obligations;if(!p||&p->common!=&x.principal.common||&p->economy!=&x.principal.economy||&p->clock!=&x.principal.clock||p->sources.schema_version!=2)fail("AidCompensationSameWorldP11Required",ref.obligation_id);
 const SourceRef arrear{"local_economy","contract_payment_due",*d->original_arrear_id};const auto actual=take(evidence?read_economic_payable_for_budget_evidence(*p,arrear):read_economic_payable(*p,arrear));if(actual.obligation->id!=ref.obligation_id||actual.arrear->original_due!=policy.maximum_amount)fail("AidCompensationOriginalPayableMismatch",ref.obligation_id);return common_success();
 }catch(Failure&e){return {false,{},std::move(e.error)};}}

} // same original payable checks, distinct authority recursion cut
Status validate_material_aid_compensation_payable2(const MaterialAidCompensationReadContext2&x,const MaterialAidObligation&o,const TreatyBoundedCompensation&p,const ObligationRef&r,const WorldMoment&m){return compensation_payable(x,o,p,r,m,false);}
Status validate_material_aid_compensation_payable_for_budget_evidence2(const MaterialAidCompensationReadContext2&x,const MaterialAidObligation&o,const TreatyBoundedCompensation&p,const ObligationRef&r,const WorldMoment&m){return compensation_payable(x,o,p,r,m,true);}

Outcome<SourceRef>pay_material_aid_compensation_in_candidate2(CommonStore&c,MaterialAidCompensationState2&s,const MaterialAidCompensationReadContext2&x,const MaterialAidWriteContext&write,const MomentOwnerSource&actual_due,Integer amount){try{
 compensation_candidate(c,s,x);const auto&p=x.principal;if(&write.read!=&p||&write.economy!=&p.economy||&write.budget!=&p.budget)fail("AidCompensationCandidateIdentityMismatch");
 const auto v=take(read_material_aid_compensation_original2(x,actual_due));const auto&d=*v.disposition;if(d.kind!=AidCompensationDispositionKind2::late_due||!d.original_obligation_id||amount<=0)fail("AidCompensationPositiveOriginalDueRequired",d.id);
 auto*w=take(service_finance::original_obligation_write(c,write.economy,p.obligations,write.obligation_writes));if(w->sources.schema_version!=2)fail("AidCompensationSelectedP11VersionRequired",d.id);
 const auto before=take(read_economic_payable(w->read(),{"local_economy","contract_payment_due",*d.original_arrear_id}));const auto&epoch=*before.current_responsibility;
 if(before.obligation->id!=*d.original_obligation_id||before.positive_increments||before.unpaid_void||amount>before.outstanding||epoch.epoch||epoch.previous_epoch_ref||epoch.previous_execution_event_ref||epoch.source_wallet_ref!=v.accepted.compensation->money_route.source_wallet_id||epoch.destination_wallet_ref!=v.accepted.compensation->money_route.beneficiary_wallet_id||epoch.authority_ref!=d.id)fail("AidCompensationOriginalPaymentScopeMismatch",d.id);
 const auto&scope=*v.scope;const auto original=scope_original(x,scope);original_binding(x,scope,original);const BudgetExecutionBinding*binding=nullptr;
 for(const auto&b:p.budget.bindings)if(same_source_ref(b.responsibility_ref,material_aid_compensation_scope_source2(scope.id)))binding=&b;
 if(!binding)fail("AidCompensationOriginalBudgetBindingRequired",scope.id);take(write.budget_authorization.validate_budget_execution_adoption(*binding,BudgetExecutionCheck::existing_source));
 const auto at=now(p);PaymentReceipt cash;cash.id=take(c.next_id("aid_compensation_payment"));cash.obligation_id=*d.original_obligation_id;cash.arrear_id=*d.original_arrear_id;cash.due_tick=d.occurred_at.half_day_tick;cash.tick=at.half_day_tick;cash.amount=amount;cash.from_wallet_id=epoch.source_wallet_ref;cash.to_wallet_id=epoch.destination_wallet_ref;cash.authority_ref=d.id;
 auto transferred=take(service_finance::transfer_money_in_candidate(c,cash.from_wallet_id,cash.to_wallet_id,amount));take(service_finance::record_payment_in_candidate(*w,cash,std::move(transferred)));
 if(!s.payments.insert(cash.id,AidCompensationPayment2{cash.id,d.id,at}))fail("AidCompensationDuplicateCashAdoption",cash.id);
 take(read_economic_payable(w->read(),{"local_economy","contract_payment_due",*d.original_arrear_id}));take(validate_budget_execution(c,write.budget,write.budget_authorization,write.budget_responsibilities,&p.clock));return {true,SourceRef{"local_economy","obligation_payment",cash.id},{}};
 }catch(Failure&e){return {false,{},std::move(e.error)};}}

Outcome<WorldMoment>read_material_aid_compensation_source_moment2(const MaterialAidCompensationReadContext2&x,const SourceRef&source,Integer schema){try{
 if(schema!=2||source.domain!="local_economy")fail("UnsupportedAidCompensationSource",source.id);
 if(same_source_ref(source,material_aid_compensation_scope_source2(source.id))){const auto*s=x.state.scopes.find(source.id);if(!s)fail("AidCompensationOriginalScopeRequired",source.id);const auto original=scope_original(x,*s);original_binding(x,*s,original);return {true,s->accepted_at,{}};}
 const auto original=take(read_material_aid_compensation_original2(x,{source,schema}));return {true,original.disposition->occurred_at,{}};
 }catch(Failure&e){return {false,{},std::move(e.error)};}}

}
