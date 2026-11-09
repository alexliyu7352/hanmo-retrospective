#include "repair_service_internal.hpp"
#include "repair_service_physical.hpp"
namespace hanmo::resident {using namespace repair_service_detail;
namespace {
void charge_money_coverage(const RepairServiceReadContext&x,const RepairServiceChargeSource&q){
 Integer paid=0;StableId last=q.original_arrear_id?*q.original_arrear_id:StableId{};std::size_t last_order=0;
 for(std::size_t n=0;n<q.payment_refs.size();++n){const auto&r=q.payment_refs[n];for(std::size_t k=0;k<n;++k)if(same_source_ref(q.payment_refs[k],r))fail("DuplicateRepairServicePaymentReference",q.id);const auto&p=payment(x,{r,1});if(!same(p.charge_ref,{repair_service_charge_source(q.id),1}))fail("RepairServiceForeignPaymentReference",q.id);const auto*actual=x.economy.payments.find(p.actual_payment_ref.id);paid=add(paid,actual->amount);const auto order=header(x.common,p.actual_payment_ref);if(last_order&&order<=last_order)fail("RepairServicePaymentOrderMismatch",q.id);last_order=order;last=actual->id;}
 for(const auto&row:state(x).payments.rows())if(same(row.second.charge_ref,{repair_service_charge_source(q.id),1})&&std::count_if(q.payment_refs.begin(),q.payment_refs.end(),[&](const auto&r){return same_source_ref(r,repair_service_paid_source(row.first));})!=1)fail("RepairServicePaymentReverseCoverageMismatch",row.first);
 if(q.earned_qian){const auto*d=x.economy.arrears.find(*q.original_arrear_id);const auto*o=x.economy.obligations.find(q.original_obligation_ref->obligation_id);if(paid!=d->paid_amount||o->last_receipt_id!=last)fail("RepairServiceOriginalPaymentConservationMismatch",q.id);}
 else if(paid)fail("ZeroRepairServiceChargeHasMoneyLeg",q.id);
}
void original_money_reverse(const RepairServiceReadContext&x){

 for(const auto&row:x.economy.obligations.rows()){const auto&o=row.second;const auto*h=x.common.state().facts.find(o.authority_ref);if(!h||h->domain!="commerce"||h->kind!="repair_service_charge")continue;const auto&q=charge(x,{repair_service_charge_source(h->id),1});if(!q.original_obligation_ref||q.original_obligation_ref->obligation_id!=row.first)fail("RepairServiceOrphanOriginalObligation",row.first);}
 for(const auto&row:x.economy.arrears.rows()){const auto*d=&row.second;const auto*o=x.economy.obligations.find(d->obligation_id);const auto*h=o?x.common.state().facts.find(o->authority_ref):nullptr;if(!h||h->domain!="commerce"||h->kind!="repair_service_charge")continue;const auto&q=charge(x,{repair_service_charge_source(h->id),1});if(q.original_arrear_id!=std::optional<StableId>{row.first})fail("RepairServiceOrphanOriginalArrear",row.first);}
 for(const auto&row:x.economy.payments.rows()){const auto&p=row.second;const auto*o=x.economy.obligations.find(p.obligation_id);const auto*h=o?x.common.state().facts.find(o->authority_ref):nullptr;if(!h||h->domain!="commerce"||h->kind!="repair_service_charge")continue;Integer count=0;for(const auto&r:state(x).payments.rows())if(same_source_ref(r.second.actual_payment_ref,{"local_economy","obligation_payment",row.first})){payment(x,{repair_service_paid_source(r.first),1});++count;}if(count!=1)fail("RepairServiceOrphanOriginalPayment",row.first);}
}
}
Status validate_repair_service_contracts(const RepairServiceReadContext&x){try{
 context(x);take(x.original.validate_original_repair_agreement_coverage());const auto&s=state(x);take(validate_repair_service_physical_sources(x));
 for(const auto&row:s.contracts.rows()){if(row.first!=row.second.id)fail("RepairServiceContractKeyMismatch",row.first);const auto&t=contract(x,{repair_service_contract_source(row.first),1});const auto&a=accepted_scope(x,t);
  std::optional<MomentOwnerSource>terminal;
  for(const auto&v:x.items.repair_jobs.unstarted_interruptions.rows())if(same(v.second.accepted_service_ref,{repair_accepted_source(a.id),1})){if(terminal)fail("DuplicateRepairOriginalTerminal",t.id);terminal=MomentOwnerSource{repair_unstarted_interrupted_source(v.first),1};}
  for(const auto&v:x.items.repair_jobs.jobs.rows())if(same(v.second.terms.accepted_service_ref,{repair_accepted_source(a.id),1})&&v.second.interrupted_ref){if(terminal)fail("DuplicateRepairOriginalTerminal",t.id);terminal=MomentOwnerSource{repair_interrupted_source(*v.second.interrupted_ref),1};}
  Integer settlements=0;for(const auto&v:s.termination_settlements.rows())if(same(v.second.contract_ref,{repair_service_contract_source(t.id),1})){if(!terminal||!same(v.second.actual_terminal_ref,*terminal))fail("RepairSettlementTerminalMismatch",v.first);settlement(x,{repair_service_termination_settled_source(v.first),1});++settlements;}
  if(settlements!=Integer(bool(terminal)))fail("RepairInternalTerminalFinancialCoverageMismatch",t.id);
  if(t.terms.custody_terms.uncollected_fee_qian)fail("RepairActualCustodyFeeOccurrenceWriterRequired",t.id,ErrorKind::unsupported_capability);
  for(const auto&other:s.contracts.rows())if(other.first!=t.id&&(same(other.second.terms.customer_acceptance_source,t.terms.customer_acceptance_source)||same(other.second.terms.provider_acceptance_source,t.terms.provider_acceptance_source)))fail("DuplicateRepairServiceOriginalAcceptance",t.id);
  // Whole publication requires every genuine start/completion trigger once.
  // This is outside the source-local cut used while writing that same charge.
  for(const auto&job:x.items.repair_jobs.jobs.rows())if(same(job.second.terms.accepted_service_ref,{repair_accepted_source(a.id),1})){
   std::optional<MomentOwnerSource>trigger;if(t.terms.fee_terms.trigger==RepairServiceFeeTrigger::actual_start)trigger=MomentOwnerSource{repair_job_source(job.first),1};else if(job.second.completed_ref)trigger=MomentOwnerSource{repair_completed_source(*job.second.completed_ref),1};
   if(trigger){Integer count=0;for(const auto&q:s.charges.rows())if(same(q.second.contract_ref,{repair_service_contract_source(t.id),1})&&same(q.second.actual_trigger_source,*trigger)){charge(x,{repair_service_charge_source(q.first),1});++count;}if(count!=1)fail("RepairServiceActualTriggerCoverageMismatch",t.id);}
  }
 }
 for(const auto&row:x.items.repair_jobs.acceptances.rows()){const auto&t=contract(x,row.second.terms.fee_terms_ref);if(accepted_scope(x,t).id!=row.first)fail("RepairServiceAcceptanceReverseCoverageMismatch",row.first);}
 for(const auto&row:s.charges.rows()){if(row.first!=row.second.id)fail("RepairServiceChargeKeyMismatch",row.first);const auto&q=charge(x,{repair_service_charge_source(row.first),1});charge_money_coverage(x,q);}
 for(const auto&row:s.payments.rows()){if(row.first!=row.second.id)fail("RepairServicePaymentKeyMismatch",row.first);payment(x,{repair_service_paid_source(row.first),1});}
 validate_repair_settlement_sources(x);original_money_reverse(x);
 for(const auto&row:x.common.state().facts.rows()){const auto&h=row.second;if(h.domain!="commerce")continue;if(h.kind=="repair_service_advance_paid"||h.kind=="repair_service_advance_applied"||h.kind=="repair_service_refund_assessed"||h.kind=="repair_service_refund_paid")fail("RetiredServicePrepaymentSource",row.first);bool missing=false;
 #define REPAIR_COVER(label,table) if(h.kind==label)missing=!s.table.find(row.first);
 REPAIR_COVER("repair_service_contract_accepted",contracts) REPAIR_COVER("repair_custody_received",custody_receipts) REPAIR_COVER("repair_service_charge",charges) REPAIR_COVER("repair_service_paid",payments) REPAIR_COVER("repair_service_termination_settled",termination_settlements) REPAIR_COVER("repair_item_returned",returns)
 #undef REPAIR_COVER
 if(missing)fail("RepairServiceOriginalHeaderCoverageMismatch",row.first);
 }
 return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Outcome<WorldMoment>read_repair_service_source_moment(const RepairServiceReadContext&x,const MomentOwnerSource&r){try{
 if(r.source_schema!=1||r.source_ref.domain!="commerce")fail("UnsupportedRepairServiceSource",r.source_ref.id,ErrorKind::unsupported_capability);
 if(r.source_ref.kind=="repair_service_contract_accepted")return {true,contract(x,r).accepted_at,{}};
 if(r.source_ref.kind=="repair_custody_received"||r.source_ref.kind=="repair_custody_item_transfer")return {true,take(read_repair_custody_received(x,{repair_custody_received_source(r.source_ref.id),1}))->occurred_at,{}};
 if(r.source_ref.kind=="repair_item_returned"||r.source_ref.kind=="repair_return_item_transfer")return {true,take(read_repair_item_returned(x,{repair_item_returned_source(r.source_ref.id),1}))->occurred_at,{}};
 if(r.source_ref.kind=="repair_service_charge")return {true,charge(x,r).earned_at,{}};
 if(r.source_ref.kind=="repair_service_paid")return {true,payment(x,r).paid_at,{}};
 if(r.source_ref.kind=="repair_service_termination_settled")return {true,settlement(x,r).settled_at,{}};
 fail("RepairOriginalServiceEffectReaderRequired",r.source_ref.id,ErrorKind::unsupported_capability);
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Outcome<std::optional<WorldMoment>>read_repair_service_original_money_moment(const RepairServiceReadContext&x,const SourceRef&r){try{
 if(r.domain!="local_economy"||(r.kind!="contract_payment_due"&&r.kind!="obligation_payment"))return {true,std::nullopt,{}};
 const RecurringObligation*o=nullptr;if(r.kind=="contract_payment_due"){const auto*d=x.economy.arrears.find(r.id);if(d)o=x.economy.obligations.find(d->obligation_id);}else{const auto*p=x.economy.payments.find(r.id);if(p)o=x.economy.obligations.find(p->obligation_id);}const auto*h=o?x.common.state().facts.find(o->authority_ref):nullptr;

 if(!h||h->domain!="commerce"||h->kind!="repair_service_charge")return {true,std::nullopt,{}};
 const auto&q=charge(x,{repair_service_charge_source(h->id),1});charge_money_coverage(x,q);header(x.common,r);
 if(r.kind=="contract_payment_due"){if(q.original_arrear_id!=std::optional<StableId>{r.id})fail("RepairServiceOriginalDueBindingMismatch",r.id);return {true,q.earned_at,{}};}
 const RepairServicePaymentSource*out=nullptr;for(const auto&row:state(x).payments.rows())if(same_source_ref(row.second.actual_payment_ref,r)){if(out)fail("DuplicateRepairOriginalPaymentAdoption",r.id);out=&payment(x,{repair_service_paid_source(row.first),1});}if(!out)fail("RepairServiceOriginalPaymentPayloadRequired",r.id);return {true,out->paid_at,{}};
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Status visit_repair_service_source_pins(const RepairServiceReadContext&x,const MomentOwnerSource&r,RepairServiceSourcePinSink&sink){try{
 take(read_repair_service_source_moment(x,r));
 if(r.source_ref.kind=="repair_custody_received"||r.source_ref.kind=="repair_item_returned"||r.source_ref.kind=="repair_custody_item_transfer"||r.source_ref.kind=="repair_return_item_transfer")fail("RepairPhysicalTypedPinContextRequired",r.source_ref.id,ErrorKind::unsupported_capability);
 if(r.source_ref.kind=="repair_service_contract_accepted"){const auto&t=contract(x,r).terms;for(const auto*s:{&t.fee_policy_source,&t.fee_terms.original_rule_ref,&t.return_terms.receiver_authority,&t.return_terms.original_rule_ref,&t.custody_terms.authority_ref,&t.custody_terms.original_rule_ref,&t.actual_contact_source,&t.customer_acceptance_source,&t.provider_acceptance_source,&t.owner_authority_source,&t.payer_authority_source})take(sink.source(*s));}
 else if(r.source_ref.kind=="repair_service_charge"){const auto&q=charge(x,r);take(sink.source(q.contract_ref));take(sink.source(q.actual_trigger_source));if(q.original_obligation_ref)take(sink.obligation(*q.original_obligation_ref));}
 else if(r.source_ref.kind=="repair_service_paid"){const auto&p=payment(x,r);take(sink.source(p.charge_ref));take(sink.source({p.actual_payment_ref,1}));}
 else if(r.source_ref.kind=="repair_service_termination_settled"){const auto&s=settlement(x,r);take(sink.source(s.contract_ref));take(sink.source(s.actual_terminal_ref));for(const auto&ref:s.preserved_charge_refs)take(sink.source(ref));for(const auto&ref:s.custody_or_continuity_refs)take(sink.source(ref));}
 return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}
}
