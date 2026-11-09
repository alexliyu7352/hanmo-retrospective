#include "material_aid_compensation_obligations2.hpp"
#include "material_aid_internal.hpp"
namespace hanmo::resident {using namespace aid_detail;
namespace {
const AidCompensationDisposition2*owned_due(const MaterialAidCompensationReadContext2&x,const StableId&id){const AidCompensationDisposition2*out=nullptr;for(const auto&r:x.state.dispositions.rows())if(r.second.original_obligation_id&&*r.second.original_obligation_id==id){if(out)fail("AidCompensationAmbiguousOriginalPayable",id);out=&r.second;}return out;}
MomentOwnerSource origin(const AidCompensationDisposition2&d){return {material_aid_compensation_disposition_source2(d.id),2};}
MaterialAidCompensationOriginal2 original(const MaterialAidCompensationReadContext2&x,const MomentOwnerSource&source){auto out=take(read_material_aid_compensation_original2(x,source));if(out.disposition->kind!=AidCompensationDispositionKind2::late_due||!out.disposition->original_obligation_id||out.policy->maximum_amount<=0)fail("AidCompensationPositiveOriginalPayableRequired",source.source_ref.id);return out;}
OriginalEconomicPayable payable(const MaterialAidCompensationOriginal2&v){const auto&d=*v.disposition;return {{"local_economy",*d.original_obligation_id},{"local_economy","contract_payment_due",*d.original_arrear_id},origin(d),d.occurred_at,d.occurred_at,v.policy->maximum_amount};}
}
bool NativeMaterialAidCompensationOriginalRead2::bound_to(const CommonStore&c,const LocalEconomyState&e)const noexcept{return &x_.principal.common==&c&&&x_.principal.economy==&e&&x_.principal.resources.bound_to(c)&&remainder_.bound_to(c,e);}
Outcome<OriginalEconomicPayable>NativeMaterialAidCompensationOriginalRead2::read_original_payable(const EconomicObligationOriginalSource&s)const{try{const auto*p=std::get_if<OriginalMaterialAidCompensationPayable>(&s);if(!p)return remainder_.read_original_payable(s);return {true,payable(original(x_,p->original_late_compensation)),{}};}catch(Failure&e){return {false,{},std::move(e.error)};}}
Outcome<OriginalEconomicAdjustment>NativeMaterialAidCompensationOriginalRead2::read_original_adjustment(const MomentOwnerSource&s)const{return remainder_.read_original_adjustment(s);}
Outcome<ObligationResponsibilityEpoch>NativeMaterialAidCompensationOriginalRead2::read_original_responsibility_genesis(const EconomicObligationOriginalSource&s)const{try{
 const auto*p=std::get_if<OriginalMaterialAidCompensationPayable>(&s);if(!p)return remainder_.read_original_responsibility_genesis(s);
 const auto v=original(x_,p->original_late_compensation);const auto due=payable(v);const auto&a=v.accepted;const auto&route=a.compensation->money_route;
 ObligationResponsibilityEpoch out;out.obligation_ref=due.obligation_ref;out.original_source_ref=due.canonical_charge_ref;out.change_source=OriginalResponsibilitySource{due.canonical_charge_ref};out.accepted_arrear_refs={due.arrear_ref};out.debtor={SubjectKind::organization,a.source_party};out.creditor=a.beneficiary;out.executor_authorization_ref=a.actual_acceptance;out.source_wallet_ref=route.source_wallet_id;out.destination_wallet_ref=route.beneficiary_wallet_id;out.receiver_source_ref=a.actual_acceptance;out.authority_ref=due.canonical_charge_ref.source_ref.id;out.effective_at=due.established_at;out.consent_refs={a.actual_acceptance};return {true,std::move(out),{}};
 }catch(Failure&e){return {false,{},std::move(e.error)};}}
Status NativeMaterialAidCompensationOriginalRead2::validate_original_responsibility_epoch(const ObligationResponsibilityEpoch&e)const{try{
 const auto*d=owned_due(x_,e.obligation_ref.obligation_id);if(!d)return remainder_.validate_original_responsibility_epoch(e);
 if(e.obligation_ref.owner_domain!="local_economy")fail("AidCompensationOriginalPayableOwnerMismatch",e.obligation_ref.obligation_id);
 const auto expected=take(read_original_responsibility_genesis(OriginalMaterialAidCompensationPayable{origin(*d)}));return validate_original_obligation_genesis(e,expected);
 }catch(Failure&f){return {false,{},std::move(f.error)};}}
Outcome<OriginalEconomicCashPayment>NativeMaterialAidCompensationOriginalRead2::read_original_cash_payment(const SourceRef&s)const{try{
 const auto&p=x_.principal;const auto*cash=p.economy.payments.find(s.id);const auto*row=x_.state.payments.find(s.id);const auto*d=cash?owned_due(x_,cash->obligation_id):nullptr;
 if(!row&&!d)return remainder_.read_original_cash_payment(s);
 if(!row||!d||row->id!=s.id||row->disposition_id!=d->id||!same_source_ref(s,{"local_economy","obligation_payment",s.id}))fail("AidCompensationOriginalCashAdoptionRequired",s.id);
 const auto v=original(x_,origin(*d));const auto due=payable(v);const auto&route=v.accepted.compensation->money_route;
 if(!cash||cash->id!=s.id||cash->arrear_id!=due.arrear_ref.id||cash->amount<=0||cash->obligation_id!=due.obligation_ref.obligation_id||cash->due_tick!=due.due_at.half_day_tick||cash->tick!=row->paid_at.half_day_tick||compare(row->paid_at,due.due_at)<0||compare(row->paid_at,now(p))>0||cash->from_wallet_id!=route.source_wallet_id||cash->to_wallet_id!=route.beneficiary_wallet_id||cash->authority_ref!=d->id)fail("AidCompensationOriginalCashMismatch",s.id);
 before(p.common,due.canonical_charge_ref.source_ref,due.arrear_ref);before(p.common,due.arrear_ref,s);return {true,{cash,row->paid_at,due.canonical_charge_ref},{}};
 }catch(Failure&e){return {false,{},std::move(e.error)};}}
Status NativeMaterialAidCompensationOriginalRead2::validate_original_cash_payment_authority(const SourceRef&s)const{try{
 const auto*cash=x_.principal.economy.payments.find(s.id);const auto*d=cash?owned_due(x_,cash->obligation_id):nullptr;if(!d){if(x_.state.payments.find(s.id))fail("AidCompensationOriginalCashOwnerMismatch",s.id);return remainder_.validate_original_cash_payment_authority(s);}
 take(read_original_cash_payment(s));const auto v=original(x_,origin(*d));const BudgetExecutionBinding*binding=nullptr;
 for(const auto&b:x_.principal.budget.bindings)if(same_source_ref(b.responsibility_ref,material_aid_compensation_scope_source2(v.scope->id))){if(binding)fail("AidCompensationAmbiguousBudgetBinding",s.id);binding=&b;}
 if(!binding)fail("AidCompensationOriginalBudgetBindingRequired",s.id);
 // Signed recipient/source choices were checked by original(), not by today's
 // owner. Existing responsibility is not re-adopted under a new current budget.
 return authorization_.validate_budget_execution_adoption(*binding,BudgetExecutionCheck::existing_source);
 }catch(Failure&e){return {false,{},std::move(e.error)};}}
}
