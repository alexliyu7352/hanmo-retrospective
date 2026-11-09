#include "repair_service_private_finance.hpp"
#include "repair_service_internal.hpp"
namespace hanmo::resident {using namespace repair_service_detail;
namespace {
void original_contract(const RepairPrivateFinancialContext&x,const RepairServiceContractSource&t){
 if(x.items.repair_service_contracts.contracts.find(t.id)!=&t)fail("RepairPrivateFinancialCandidateMismatch",t.id);
 header(x.common,repair_service_contract_source(t.id));
 if(compare(t.accepted_at,take(x.clock.read_current_moment()))>0)fail("RepairPrivateFutureAcceptance",t.id);
 if(t.terms.customer.extra||t.terms.provider.extra||t.terms.customer.kind!=Owner::Kind::actor||t.terms.provider.kind!=Owner::Kind::actor||t.terms.customer.id.empty()||t.terms.provider.id.empty()||!x.common.actor(t.terms.customer.id)||!x.common.actor(t.terms.provider.id))fail("RepairOriginalNonpersonalFinancialAuthorityRequired",t.id,ErrorKind::unsupported_capability);
}
void title(const RepairPrivateFinancialContext&x,const StableId&wallet,const Owner&expected,const SourceRef*cut){
 const auto actual=cut?take(x.titles.read_private_wallet_title_before_source(wallet,*cut)):take(x.titles.read_current_private_wallet_title(wallet));
 if(actual.wallet_id!=wallet||!owner(actual.legal_owner,expected))fail("RepairPrivateFinancialSuccessionContinuationRequired",wallet,ErrorKind::unsupported_capability);
}
void accepted_titles(const RepairPrivateFinancialContext&x,const RepairServiceContractSource&t){
 original_contract(x,t);const auto source=repair_service_contract_source(t.id);
 title(x,t.terms.payer_wallet_id,t.terms.customer,&source);title(x,t.terms.payee_wallet_id,t.terms.provider,&source);
}
void canonical_acceptance(const RepairPrivateFinancialContext&x,const RepairServiceContractSource&t){
 const RepairAcceptedSource*out=nullptr;const MomentOwnerSource contract{repair_service_contract_source(t.id),1};
 for(const auto&r:x.items.repair_jobs.acceptances.rows())if(same(r.second.terms.fee_terms_ref,contract)){
  const auto&a=r.second;if(out||r.first!=a.id||a.terms.instance_id!=t.terms.instance_id||a.terms.worker_id!=t.terms.worker_id||a.terms.policy_ref.policy_id!=t.terms.repair_policy_ref.policy_id||a.terms.policy_ref.version!=t.terms.repair_policy_ref.version||a.terms.terms_revision!=t.terms.terms_revision||!same(a.terms.actual_contact_ref,t.terms.actual_contact_source)||!same(a.terms.owner_authority_ref,t.terms.owner_authority_source)||!same(a.terms.return_terms_ref,contract)||!owner(a.terms.legal_owner,t.terms.customer)||compare(a.accepted_at,t.accepted_at)||header(x.common,repair_accepted_source(a.id))<=header(x.common,contract.source_ref))fail("RepairPrivateActualAcceptanceMismatch",t.id);out=&a;
 }
 if(!out)fail("RepairPrivateActualAcceptanceRequired",t.id);
}
const ArrearRecord&payable_identity(const RepairPrivateFinancialContext&x,const ObligationRef&ref,const StableId&arrear,const StableId&charge,Integer original){
 const auto*o=x.economy.obligations.find(ref.obligation_id);const auto*d=x.economy.arrears.find(arrear);const auto*remainder=o?x.economy.obligation_remainders.find(o->id):nullptr;
 // Original identity only. Today's paid/outstanding and current executor are
 // not evidence of an older payment; the shared P11 graph owns those checks.
 if(ref.owner_domain!="local_economy"||!o||!d||o->id!=ref.obligation_id||d->id!=arrear||d->obligation_id!=o->id||o->kind!=ObligationKind::contract_payment||o->active||o->period_ticks||o->next_due_tick||!o->rate_profile.empty()||!remainder||*remainder||d->original_due!=original||original<=0)fail("RepairPrivateOriginalPayableMismatch",charge);
 if(header(x.common,{"local_economy","contract_payment_due",d->id})<=header(x.common,repair_service_charge_source(charge)))fail("RepairPrivateOriginalPayableOrder",charge);return *d;
}
void original_payment(const RepairPrivateFinancialContext&x,const PaymentReceipt&p,const ObligationRef&obligation,const StableId&arrear,const StableId&authority,const StableId&from,const StableId&to,Integer amount,Integer original,Integer due_tick,const WorldMoment&at){
 const auto&d=payable_identity(x,obligation,arrear,authority,original);
 if(x.economy.payments.find(p.id)!=&p||p.due_tick!=due_tick||d.due_tick!=due_tick||p.obligation_id!=obligation.obligation_id||p.arrear_id!=arrear||p.authority_ref!=authority||p.from_wallet_id!=from||p.to_wallet_id!=to||p.amount!=amount||p.tick!=at.half_day_tick||amount<=0)fail("RepairPrivateOriginalPaymentMismatch",p.id);
 const SourceRef source{"local_economy","obligation_payment",p.id};if(header(x.common,source)<=header(x.common,{"local_economy","contract_payment_due",arrear}))fail("RepairPrivateOriginalPaymentOrder",p.id);
}
void current_personal_responsibility(const RepairServiceContractSource&t,const EconomicPayableView&due,const StableId&charge){
 const auto*epoch=due.current_responsibility;
 if(!epoch||epoch->debtor.kind!=SubjectKind::actor||epoch->debtor.id!=t.terms.customer.id||epoch->creditor.kind!=SubjectKind::actor||epoch->creditor.id!=t.terms.provider.id||epoch->source_wallet_ref!=t.terms.payer_wallet_id||epoch->destination_wallet_ref!=t.terms.payee_wallet_id||epoch->authority_ref!=charge||!same(epoch->executor_authorization_ref,t.terms.payer_authority_source)||!same(epoch->receiver_source_ref,t.terms.provider_acceptance_source))fail("RepairPrivateFinancialResponsibilityContinuationRequired",t.id,ErrorKind::unsupported_capability);
}

}
bool NativeRepairPrivateFinancialRead::bound_to(const CommonStore&c,const ItemInstancesState&i)const noexcept{return &c==&x_.common&&&i==&x_.items&&(!x_.obligations||(&x_.obligations->common==&c&&&x_.obligations->economy==&x_.economy&&&x_.obligations->clock==&x_.clock));}
Outcome<RepairServiceFinancialParties>NativeRepairPrivateFinancialRead::read_original_repair_service_parties(const RepairServiceContractSource&t)const{try{accepted_titles(x_,t);return {true,{{SubjectKind::actor,t.terms.customer.id},{SubjectKind::actor,t.terms.provider.id}},{}};}catch(Failure&e){return {false,{},std::move(e.error)};}}
Outcome<RepairServicePaymentPermission>NativeRepairPrivateFinancialRead::read_original_repair_payment_permission(const RepairServiceContractSource&t,Integer amount,const WorldMoment&at,RepairServicePaymentUse use,const MomentOwnerSource&responsibility)const{try{
 accepted_titles(x_,t);canonical_acceptance(x_,t);if(amount<=0||compare(at,take(x_.clock.read_current_moment())))fail("RepairPrivateActualPaymentBoundaryRequired",t.id);ref(responsibility);header(x_.common,responsibility.source_ref);const auto&s=x_.items.repair_service_contracts;const MomentOwnerSource contract{repair_service_contract_source(t.id),1};MomentOwnerSource authority;
 if(use==RepairServicePaymentUse::earned_charge){const auto*q=s.charges.find(responsibility.source_ref.id);if(!q||q->id!=responsibility.source_ref.id||!same(responsibility,{repair_service_charge_source(q->id),1})||!same(q->contract_ref,contract)||!q->original_obligation_ref||!q->original_arrear_id||q->earned_qian!=t.terms.fee_terms.amount_qian||compare(q->earned_at,at)>0)fail("RepairPrivateEarnedScopeMismatch",t.id);const auto&identity=payable_identity(x_,*q->original_obligation_ref,*q->original_arrear_id,q->id,q->earned_qian);const auto&read=*take(service_finance::original_obligation_read(x_.common,x_.economy,x_.obligations));if(&read.clock!=&x_.clock)fail("RepairPrivateFinancialClockMismatch",q->id);const auto due=take(read_economic_payable(read,{"local_economy","contract_payment_due",identity.id}));if(due.arrear!=&identity||due.obligation->id!=q->original_obligation_ref->obligation_id||!same(due.source->canonical_charge_ref,responsibility))fail("RepairPrivateOriginalPayableMismatch",q->id);current_personal_responsibility(t,due,q->id);if(amount>due.outstanding)fail("RepairPrivatePaymentExceedsOriginalDebt",q->id);authority=t.terms.payer_authority_source;}
 else fail("UnsupportedRepairPaymentUse",t.id);
 // A change of title requires the actual continuation/agency producer. It is
 // not converted into a fabricated temporary block at today's moment.
 title(x_,t.terms.payer_wallet_id,t.terms.customer,nullptr);title(x_,t.terms.payee_wallet_id,t.terms.provider,nullptr);ref(authority);
 return {true,RepairServicePaymentAllowed{authority},{}};
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Status NativeRepairPrivateFinancialRead::validate_original_repair_payment(const RepairServiceContractSource&t,const MomentOwnerSource&source,RepairServicePaymentUse use,Integer amount,const WorldMoment&at)const{try{
 accepted_titles(x_,t);canonical_acceptance(x_,t);if(amount<=0||compare(at,t.accepted_at)<0||compare(at,take(x_.clock.read_current_moment()))>0)fail("RepairPrivatePaymentMomentMismatch",t.id);const auto&s=x_.items.repair_service_contracts;const MomentOwnerSource contract{repair_service_contract_source(t.id),1};SourceRef money_source=source.source_ref;
 if(use==RepairServicePaymentUse::earned_charge){const auto*p=s.payments.find(source.source_ref.id);const auto*q=p?s.charges.find(p->charge_ref.source_ref.id):nullptr;const auto*actual=p?x_.economy.payments.find(p->actual_payment_ref.id):nullptr;if(!p||p->id!=source.source_ref.id||!same(source,{repair_service_paid_source(p->id),1})||!q||!same(p->charge_ref,{repair_service_charge_source(q->id),1})||!same(q->contract_ref,contract)||!q->original_obligation_ref||!q->original_arrear_id||!actual||!same_source_ref(p->actual_payment_ref,{"local_economy","obligation_payment",actual->id})||compare(p->paid_at,at))fail("RepairPrivateOriginalEarnedPaymentRequired",t.id);original_payment(x_,*actual,*q->original_obligation_ref,*q->original_arrear_id,q->id,t.terms.payer_wallet_id,t.terms.payee_wallet_id,amount,q->earned_qian,q->earned_at.half_day_tick,at);money_source=p->actual_payment_ref;}
 else fail("UnsupportedRepairPaymentUse",t.id);
 if(header(x_.common,money_source)<=header(x_.common,contract.source_ref)||header(x_.common,source.source_ref)<header(x_.common,money_source))fail("RepairPrivateOriginalPaymentOrder",source.source_ref.id);
 title(x_,t.terms.payer_wallet_id,t.terms.customer,&money_source);title(x_,t.terms.payee_wallet_id,t.terms.provider,&money_source);return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}
}
