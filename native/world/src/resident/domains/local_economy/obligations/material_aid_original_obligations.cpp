#include "material_aid_original_obligations.hpp"
#include "../material_aid/material_aid_internal.hpp"
namespace hanmo::resident {using namespace aid_detail;
namespace {
const MaterialAidObligation&from_source(const MaterialAidReadContext&x,const MomentOwnerSource&source){
 if(source.source_schema!=1||!same_source_ref(source.source_ref,material_aid_establishment_source(source.source_ref.id)))fail("OriginalMaterialAidEstablishmentRequired",source.source_ref.id);
 const auto&e=establishment(x,source.source_ref);const auto&o=obligation(x,e.original_obligation_ref);validate_established_terms(x,o);
 if(!o.frozen_money||!o.money_route||!o.original_money_obligation_id||!o.original_money_arrear_id)fail("OriginalMaterialAidMoneyDebtRequired",o.id);return o;
}
const MaterialAidObligation*by_money(const MaterialAidReadContext&x,const StableId&id){
 const MaterialAidObligation*out=nullptr;for(const auto&r:x.state.obligations.rows())if(r.second.original_money_obligation_id&&*r.second.original_money_obligation_id==id){if(out)fail("AmbiguousMaterialAidOriginalMoneyOwner",id);out=&r.second;}return out;
}
MomentOwnerSource established_source(const MaterialAidReadContext&x,const MaterialAidObligation&o){
 validate_established_terms(x,o);const auto&c=commitment(x,o.schedule_commitment_id);return {c.established_occurrence_refs[static_cast<std::size_t>(o.occurrence.ordinal)],1};
}
OriginalEconomicPayable original_payable(const MaterialAidReadContext&x,const MomentOwnerSource&source){
 const auto&o=from_source(x,source);return {{"local_economy",*o.original_money_obligation_id},{"local_economy","contract_payment_due",*o.original_money_arrear_id},source,o.established_at,WorldMoment{o.due_tick,{}},o.frozen_money->amount};
}
struct CashSource {OriginalEconomicCashPayment cash;AidSourceLegRef receiving;};
CashSource cash_source(const MaterialAidReadContext&x,const MaterialAidObligation&o,const SourceRef&ref){
 if(ref.domain!="local_economy"||ref.kind!="obligation_payment")fail("MaterialAidActualCashSourceRequired",ref.id);
 std::optional<CashSource>found;
 // The original receiving producer already exists before its S5 adoption.
 // This narrow read intentionally does not demand that future adoption row.
 for(const auto&source:take(x.resources.enumerate_actual_aid_receipts(o.occurrence))){const auto actual=original_receipt(x,o,source);if(!std::holds_alternative<BudgetMoneyQian>(actual.unit))continue;
  const auto cash=take(read_material_aid_actual_cash_payment(x,{"local_economy",o.id},source));if(cash.payment->id!=ref.id)continue;
  if(found)fail("MaterialAidCashReceiptReused",ref.id);found=CashSource{cash,source};
 }
 if(!found)fail("MaterialAidOriginalCashReceivingSourceRequired",ref.id);return std::move(*found);
}
}
Outcome<OriginalEconomicCashPayment>read_material_aid_actual_cash_payment(const MaterialAidReadContext&x,const ObligationRef&ref,const AidSourceLegRef&source){try{
 const auto&o=obligation(x,ref);const auto established=established_source(x,o);const auto due=original_payable(x,established);const auto actual=original_receipt(x,o,source);
 if(!std::holds_alternative<BudgetMoneyQian>(actual.unit))fail("MaterialAidActualMoneyReceiptRequired",source.source.source_ref.id);
 const auto cash=take(x.resources.read_actual_aid_money_payment(source));const auto*p=cash.payment;
 if(!p||x.economy.payments.find(p->id)!=p||p->id.empty()||p->obligation_id!=due.obligation_ref.obligation_id||p->arrear_id!=due.arrear_ref.id||p->due_tick!=o.due_tick||p->amount!=actual.quantity||p->amount<=0||p->from_wallet_id!=o.money_route->source_wallet_id||p->to_wallet_id!=o.money_route->beneficiary_wallet_id||p->to_wallet_id!=actual.beneficiary_container_id||p->authority_ref!=established.source_ref.id||p->tick!=cash.actual_at.half_day_tick||compare(cash.actual_at,actual.occurred_at)||compare(cash.actual_at,due.due_at)<0||!same(cash.original_payment_trigger_ref,established))fail("MaterialAidOriginalCashReceiptMismatch",source.source.source_ref.id);
 const SourceRef payment{"local_economy","obligation_payment",p->id};
 before(x.common,established.source_ref,due.arrear_ref);before(x.common,due.arrear_ref,payment);
 if(header(x.common,payment)>header(x.common,actual.actual_source.source.source_ref))fail("MaterialAidReceiptBeforeOriginalCash",p->id);
 return {true,cash,{}};
}catch(Failure&e){return {false,{},std::move(e.error)};}}
bool NativeMaterialAidObligationOriginalRead::bound_to(const CommonStore&c,const LocalEconomyState&e)const noexcept{return &x_.common==&c&&&x_.economy==&e&&x_.resources.bound_to(c)&&remainder_.bound_to(c,e);}
Outcome<OriginalEconomicPayable>NativeMaterialAidObligationOriginalRead::read_original_payable(const EconomicObligationOriginalSource&s)const{try{
 const auto*p=std::get_if<OriginalMaterialAidPayable>(&s);if(!p)return remainder_.read_original_payable(s);return {true,original_payable(x_,p->original_establishment),{}};
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Outcome<OriginalEconomicAdjustment>NativeMaterialAidObligationOriginalRead::read_original_adjustment(const MomentOwnerSource&s)const{return remainder_.read_original_adjustment(s);}
Outcome<OriginalEconomicCashPayment>NativeMaterialAidObligationOriginalRead::read_original_cash_payment(const SourceRef&s)const{try{
 const auto*p=x_.economy.payments.find(s.id);const auto*o=p?by_money(x_,p->obligation_id):nullptr;if(!o)return remainder_.read_original_cash_payment(s);return {true,cash_source(x_,*o,s).cash,{}};
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Status NativeMaterialAidObligationOriginalRead::validate_original_cash_payment_authority(const SourceRef&s)const{try{
 const auto*p=x_.economy.payments.find(s.id);const auto*o=p?by_money(x_,p->obligation_id):nullptr;if(!o)return remainder_.validate_original_cash_payment_authority(s);
 const auto original=cash_source(x_,*o,s);return x_.resources.validate_actual_aid_money_payment_authority(original.receiving);
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Outcome<ObligationResponsibilityEpoch>NativeMaterialAidObligationOriginalRead::read_original_responsibility_genesis(const EconomicObligationOriginalSource&s)const{try{
 const auto*p=std::get_if<OriginalMaterialAidPayable>(&s);if(!p)return remainder_.read_original_responsibility_genesis(s);const auto&o=from_source(x_,p->original_establishment);const auto due=original_payable(x_,p->original_establishment);
 const auto&c=commitment(x_,o.schedule_commitment_id);const auto resources=accepted_resources(x_,c);
 ObligationResponsibilityEpoch e;e.obligation_ref=due.obligation_ref;e.original_source_ref=due.canonical_charge_ref;e.change_source=OriginalResponsibilitySource{due.canonical_charge_ref};e.accepted_arrear_refs={due.arrear_ref};e.debtor={SubjectKind::organization,resources.source_party};e.creditor=resources.beneficiary;e.executor_authorization_ref=resources.actual_acceptance;e.source_wallet_ref=resources.money_route->source_wallet_id;e.destination_wallet_ref=resources.money_route->beneficiary_wallet_id;e.receiver_source_ref=resources.actual_acceptance;e.authority_ref=due.canonical_charge_ref.source_ref.id;e.effective_at=due.established_at;e.consent_refs={resources.actual_acceptance};
 return {true,std::move(e),{}};
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Status NativeMaterialAidObligationOriginalRead::validate_original_responsibility_epoch(const ObligationResponsibilityEpoch&e)const{try{
 const auto*o=by_money(x_,e.obligation_ref.obligation_id);if(!o)return remainder_.validate_original_responsibility_epoch(e);
 if(e.obligation_ref.owner_domain!="local_economy")fail("MaterialAidOriginalMoneyOwnerMismatch",e.obligation_ref.obligation_id);
 if(e.epoch||e.previous_epoch_ref||e.previous_execution_event_ref)fail("MaterialAidOriginalResponsibilityChangeRequired",e.obligation_ref.obligation_id,ErrorKind::unsupported_capability);
 const auto actual=take(read_original_responsibility_genesis(OriginalMaterialAidPayable{established_source(x_,*o)}));return validate_original_obligation_genesis(e,actual);
}catch(Failure&f){return {false,{},std::move(f.error)};}}
}
