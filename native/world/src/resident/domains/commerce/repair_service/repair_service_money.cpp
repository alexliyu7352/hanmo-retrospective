#include "resident/domains/local_economy/obligations/original_service_payable.hpp"
#include "repair_service_internal.hpp"
namespace hanmo::resident::repair_service_detail {
const RepairServiceChargeSource&charge_original_source(const RepairServiceReadContext&x,const MomentOwnerSource&r){
 context(x);if(r.source_schema!=1||!same_source_ref(r.source_ref,repair_service_charge_source(r.source_ref.id)))fail("UnsupportedRepairServiceCharge",r.source_ref.id,ErrorKind::unsupported_capability);
 const auto*q=state(x).charges.find(r.source_ref.id);if(!q||q->id!=r.source_ref.id)fail("RepairServiceChargeSourceRequired",r.source_ref.id);const auto&t=contract(x,q->contract_ref);const auto at=actual_trigger(x,t,q->actual_trigger_source);
 if(q->fee_line_id!=t.terms.fee_terms.line_id||q->actual_quantity!=1||q->earned_qian!=t.terms.fee_terms.amount_qian||compare(at,q->earned_at)||header(x.common,r.source_ref)<=header(x.common,q->actual_trigger_source.source_ref))fail("RepairServiceChargeOriginalMismatch",q->id);
 for(const auto&other:state(x).charges.rows())if(other.first!=q->id&&same(other.second.contract_ref,q->contract_ref)&&other.second.fee_line_id==q->fee_line_id)fail("DuplicateRepairServiceChargeOccurrence",q->id);
 if(!q->earned_qian){if(q->original_obligation_ref||q->original_arrear_id||!q->payment_refs.empty())fail("ZeroRepairServiceChargeHasMoneyLeg",q->id);return *q;}
 return *q;
}
const RepairServiceChargeSource&charge(const RepairServiceReadContext&x,const MomentOwnerSource&r){
 const auto&qref=charge_original_source(x,r);const auto*q=&qref;if(!q->earned_qian)return *q;
 const auto&t=contract(x,q->contract_ref);const auto at=q->earned_at;
 if(!q->original_obligation_ref||!q->original_arrear_id||q->original_obligation_ref->owner_domain!="local_economy")fail("RepairServiceOriginalPayableRequired",q->id);
 const auto&read=*take(service_finance::original_obligation_read(x.common,x.economy,x.obligations));const auto actual=take(read_economic_payable(read,{"local_economy","contract_payment_due",*q->original_arrear_id}));
 if(actual.obligation->id!=q->original_obligation_ref->obligation_id||actual.arrear->original_due!=q->earned_qian||header(x.common,actual.source->original_arrear_ref)<=header(x.common,r.source_ref))fail("RepairServiceOriginalPayableMismatch",q->id);
 return *q;
}
const RepairServicePaymentSource&payment(const RepairServiceReadContext&x,const MomentOwnerSource&r){
 if(r.source_schema!=1||!same_source_ref(r.source_ref,repair_service_paid_source(r.source_ref.id)))fail("UnsupportedRepairServicePayment",r.source_ref.id,ErrorKind::unsupported_capability);
 const auto*p=state(x).payments.find(r.source_ref.id);if(!p||p->id!=r.source_ref.id)fail("RepairServicePaymentSourceRequired",r.source_ref.id);const auto&q=charge(x,p->charge_ref);const auto&t=contract(x,q.contract_ref);const auto*paid=x.economy.payments.find(p->actual_payment_ref.id);
 if(!q.original_obligation_ref||!q.original_arrear_id||!same_source_ref(p->actual_payment_ref,{"local_economy","obligation_payment",p->actual_payment_ref.id})||!paid||paid->id!=p->actual_payment_ref.id||paid->obligation_id!=q.original_obligation_ref->obligation_id||paid->arrear_id!=*q.original_arrear_id||paid->amount<=0||paid->due_tick!=q.earned_at.half_day_tick||paid->tick!=p->paid_at.half_day_tick||compare(p->paid_at,q.earned_at)<0||compare(p->paid_at,now(x))>0||header(x.common,p->actual_payment_ref)<=header(x.common,p->charge_ref.source_ref)||header(x.common,r.source_ref)<=header(x.common,p->actual_payment_ref))fail("RepairServiceOriginalPaymentMismatch",p->id);
 for(const auto&other:state(x).payments.rows())if(other.first!=p->id&&same_source_ref(other.second.actual_payment_ref,p->actual_payment_ref))fail("DuplicateRepairOriginalPaymentAdoption",p->id);
 if(std::count_if(q.payment_refs.begin(),q.payment_refs.end(),[&](const auto&v){return same_source_ref(v,r.source_ref);})!=1)fail("RepairServicePaymentReverseBindingMismatch",p->id);
 const auto&read=*take(service_finance::original_obligation_read(x.common,x.economy,x.obligations));take(resolve_obligation_responsibility_at_payment(read,p->actual_payment_ref));return *p;
}
}
namespace hanmo::resident {using namespace repair_service_detail;
Outcome<const RepairServiceChargeSource*>read_repair_service_charge(const RepairServiceReadContext&x,const MomentOwnerSource&r){try{return {true,&charge(x,r),{}};}catch(Failure&e){return {false,nullptr,std::move(e.error)};}}
Outcome<SourceRef>charge_actual_repair_service_in_candidate(CommonStore&c,ItemInstancesState&i,LocalEconomyState&e,const RepairServiceReadContext&x,const MomentOwnerSource&accepted,const MomentOwnerSource&trigger){try{
 writable(c,i,e,x);const auto&t=contract(x,accepted);const auto at=actual_trigger(x,t,trigger);auto&s=i.repair_service_contracts;
 for(const auto&row:s.charges.rows())if(same(row.second.contract_ref,accepted)&&row.second.fee_line_id==t.terms.fee_terms.line_id){const auto&prior=charge(x,{repair_service_charge_source(row.first),1});if(!same(prior.actual_trigger_source,trigger))fail("ConflictingRepairServiceChargeReplay",row.first);return {true,repair_service_charge_source(row.first),{}};}
 if(compare(at,now(x)))fail("RepairServiceActualTriggerBoundaryRequired",trigger.source_ref.id);
 for(const auto&row:s.termination_settlements.rows())if(same(row.second.contract_ref,accepted))fail("RepairServiceChargeAfterSettlement",t.id);
 RepairServiceChargeSource q;q.id=take(c.next_id("repair_service_charge"));q.contract_ref=accepted;q.fee_line_id=t.terms.fee_terms.line_id;q.actual_trigger_source=trigger;q.earned_at=at;q.actual_quantity=1;q.earned_qian=t.terms.fee_terms.amount_qian;
 if(q.earned_qian){q.original_obligation_ref=ObligationRef{"local_economy",take(c.next_id("repair_service_obligation"))};q.original_arrear_id=take(c.next_id("contract_payment_due"));}
 const auto result=repair_service_charge_source(q.id);append(c,result);if(!s.charges.insert(result.id,std::move(q)))fail("DuplicateRepairServiceCharge",result.id);if(s.charges.find(result.id)->earned_qian)take(service_finance::post_payable_in_candidate(*take(service_finance::original_obligation_write(c,e,x.obligations,x.obligation_writes)),OriginalRepairEarnedCharge{{result,1}}));charge(x,{result,1});return {true,result,{}};
}catch(Failure&error){return {false,{},std::move(error.error)};}}
Outcome<std::optional<SourceRef>>pay_actual_repair_service_charge_in_candidate(CommonStore&c,ItemInstancesState&i,LocalEconomyState&e,const RepairServiceReadContext&x,const MomentOwnerSource&r){try{
 writable(c,i,e,x);const auto&q=charge(x,r);if(!q.earned_qian)return {true,std::nullopt,{}};const auto&t=contract(x,q.contract_ref);
 auto*d=e.arrears.find(*q.original_arrear_id);auto*o=e.obligations.find(q.original_obligation_ref->obligation_id);const auto&obligation_read=*take(service_finance::original_obligation_read(c,e,x.obligations));const Integer unpaid=take(read_economic_payable(obligation_read,{"local_economy","contract_payment_due",d->id})).outstanding;if(!unpaid)return {true,std::nullopt,{}};const auto at=now(x);const Integer amount=std::min(unpaid,take(c.spendable(t.terms.payer_wallet_id)));if(!amount)return {true,std::nullopt,{}};
 const auto permission=take(x.original.read_original_repair_payment_permission(t,amount,at,RepairServicePaymentUse::earned_charge,r));if(const auto*blocked=std::get_if<RepairServicePaymentBlocked>(&permission)){ref(blocked->actual_cause);if(compare(blocked->occurred_at,at)>0)fail("RepairServiceFuturePaymentRestriction",q.id);return {true,std::nullopt,{}};}ref(std::get<RepairServicePaymentAllowed>(permission).original_authority);
 const auto*payee=c.wallet(t.terms.payee_wallet_id);if(!payee||payee->balance>std::numeric_limits<Integer>::max()-amount)fail("RepairServicePayeeBalanceOverflow",t.terms.payee_wallet_id);
 auto actual_money=apply_original_repair_money(c,t.terms.payer_wallet_id,t.terms.payee_wallet_id,amount);
 const auto paid_id=take(c.next_id("obligation_payment"));PaymentReceipt actual{paid_id,d->id,o->id,t.terms.payer_wallet_id,t.terms.payee_wallet_id,at.half_day_tick,d->due_tick,amount,q.id};take(service_finance::record_payment_in_candidate(*take(service_finance::original_obligation_write(c,e,x.obligations,x.obligation_writes)),actual,std::move(actual_money)));
 RepairServicePaymentSource source{take(c.next_id("repair_service_paid")),r,{"local_economy","obligation_payment",paid_id},at};const auto result=repair_service_paid_source(source.id);append(c,result);if(!i.repair_service_contracts.payments.insert(result.id,std::move(source)))fail("DuplicateRepairServicePayment",result.id);i.repair_service_contracts.charges.find(q.id)->payment_refs.push_back(result);payment(x,{result,1});take(read_economic_payable(obligation_read,{"local_economy","contract_payment_due",d->id}));return {true,result,{}};
}catch(Failure&error){return {false,{},std::move(error.error)};}}
}

namespace hanmo::resident {
Outcome<OriginalEconomicPayable>read_original_repair_charge_for_obligation(const RepairServiceReadContext&x,const MomentOwnerSource&r){try{
 const auto&q=repair_service_detail::charge_original_source(x,r);
 if(!q.earned_qian||!q.original_obligation_ref||q.original_obligation_ref->owner_domain!="local_economy"||!q.original_arrear_id)repair_service_detail::fail("OriginalRepairPayableChargeRequired",r.source_ref.id);
 for(const auto&v:x.items.repair_service_contracts.charges.rows())if(v.first!=q.id&&((v.second.original_obligation_ref&&same_obligation_ref(*v.second.original_obligation_ref,*q.original_obligation_ref))||v.second.original_arrear_id==q.original_arrear_id))repair_service_detail::fail("DuplicateOriginalRepairPayableSource",q.id);
 return {true,{*q.original_obligation_ref,{"local_economy","contract_payment_due",*q.original_arrear_id},r,q.earned_at,q.earned_at,q.earned_qian},{}};
}catch(repair_service_detail::Failure&e){return {false,{},std::move(e.error)};}}
}
