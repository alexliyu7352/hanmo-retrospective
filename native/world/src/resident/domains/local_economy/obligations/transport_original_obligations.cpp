#include "transport_original_finance_detail.hpp"
#include "resident/domains/travel/transport_authority_codec.hpp"
namespace hanmo::resident::transport_finance_detail {
std::size_t header(const CommonStore&c,const SourceRef&r){
 const auto*h=c.state().facts.find(r.id);if(!valid_source_ref(r)||!h||h->id!=r.id||h->domain!=r.domain||h->kind!=r.kind)fail("OriginalTransportHeaderRequired",r.id);
 std::size_t n=0;for(const auto&v:c.state().facts.rows()){if(&v.second==h)return n;++n;}fail("OriginalTransportHeaderOrderRequired",r.id);
}
bool same_cost(const ActualTransportExpense&a,const ActualTransportExpense&b){return same_source_ref(a.source_ref,b.source_ref)&&a.source_schema==b.source_schema&&a.journey_id==b.journey_id&&a.segment_or_handling_id==b.segment_or_handling_id&&a.cost_rule_id==b.cost_rule_id&&a.payee_wallet_id==b.payee_wallet_id&&a.destination_id==b.destination_id&&a.purpose==b.purpose&&a.amount==b.amount&&!compare(a.occurred_at,b.occurred_at);}
OriginalTransportFinance original(const CommonStore&c,const TransportPaymentReceipt&r,
 const ActualTransportExpense&cost,const TransportExpenseResponsibility&parties){
 if(r.id.empty()||r.original_obligation_id.empty()||r.original_arrear_id.empty()||r.original_payment_receipt_id.empty()||r.original_obligation_id==r.original_arrear_id||r.original_arrear_id==r.original_payment_receipt_id||r.id==r.original_payment_receipt_id||r.expense.amount<=0||!same_cost(r.expense,cost)||!same_source_ref(r.request.actual_movement_or_handling_source,cost.source_ref)||r.request.source_schema!=cost.source_schema||r.payer_wallet_id!=parties.payer_wallet_id||parties.payer_wallet_id.empty())fail("OriginalTransportPayableScopeMismatch",r.id);
 header(c,cost.source_ref);
 if(parties.debtor.id.empty()||parties.creditor.id.empty()||!valid_source_ref(parties.executor_authorization.source_ref)||parties.executor_authorization.source_schema<1||!valid_source_ref(parties.receiver_source.source_ref)||parties.receiver_source.source_schema<1||parties.consent_refs.empty())fail("OriginalTransportResponsibilityMismatch",r.id);
 for(std::size_t i=0;i<parties.consent_refs.size();++i){const auto&s=parties.consent_refs[i];if(!valid_source_ref(s.source_ref)||s.source_schema<1)fail("OriginalTransportConsentSourceRequired",r.id);for(std::size_t j=0;j<i;++j)if(source(s,parties.consent_refs[j]))fail("DuplicateOriginalTransportConsent",r.id);}
 OriginalTransportFinance out;out.payable={{"local_economy",r.original_obligation_id},arrear(r),expense(r),cost.occurred_at,cost.occurred_at,cost.amount};
 auto&e=out.genesis;e.obligation_ref=out.payable.obligation_ref;e.original_source_ref=expense(r);e.change_source=OriginalResponsibilitySource{expense(r)};e.accepted_arrear_refs={arrear(r)};e.debtor=parties.debtor;e.creditor=parties.creditor;e.executor_authorization_ref=parties.executor_authorization;e.source_wallet_ref=r.payer_wallet_id;e.destination_wallet_ref=cost.payee_wallet_id;e.receiver_source_ref=parties.receiver_source;e.authority_ref=r.id;e.effective_at=cost.occurred_at;e.consent_refs=parties.consent_refs;
 return out;
}
OriginalTransportFinance historical(const CommonStore&c,const TransportPaymentReceipt&r,const EconomicTransportPaymentContext&x){
 const auto actual=take(x.actual_sources.read_actual_transport_expense(r.request.actual_movement_or_handling_source,r.request.source_schema));take(x.actual_sources.validate_transport_expense_authority(actual.source_ref,actual.source_schema,r.request.authority));
 const auto parties=take(x.authorities.read_original_transport_responsibility(r.request.authority,actual,TransportExpenseAuthorityCheck::existing_charge));
 auto out=original(c,r,actual,parties);if(header(c,adoption(r))<=header(c,actual.source_ref)||compare(actual.occurred_at,take(x.actual_sources.current_transport_expense_moment()))>0)fail("OriginalTransportAdoptionOrderMismatch",r.id);return out;
}
OriginalEconomicCashPayment cash(const CommonStore&c,const LocalEconomyState&e,const TransportPaymentReceipt&r){
 const auto*p=e.payments.find(r.original_payment_receipt_id);const auto*d=e.arrears.find(r.original_arrear_id);const auto*o=e.obligations.find(r.original_obligation_id);
 if(!p||!d||!o||p->id!=r.original_payment_receipt_id||p->obligation_id!=r.original_obligation_id||p->arrear_id!=r.original_arrear_id||d->obligation_id!=o->id||p->amount!=r.expense.amount||p->amount<=0||p->tick!=r.expense.occurred_at.half_day_tick||p->due_tick!=r.expense.occurred_at.half_day_tick||p->from_wallet_id!=r.payer_wallet_id||p->to_wallet_id!=r.expense.payee_wallet_id||p->authority_ref!=r.id)fail("OriginalTransportCashBindingMismatch",r.id);
 const auto charged=header(c,r.expense.source_ref),due=header(c,arrear(r)),paid=header(c,payment(r)),adopted=header(c,adoption(r));if(!(charged<due&&due<paid&&paid<adopted))fail("OriginalTransportCashOrderMismatch",r.id);
 return {p,r.expense.occurred_at,expense(r)};
}
const TransportPaymentReceipt*by_payment(const EconomicTransportExpenseState&s,const SourceRef&r){
 if(r.domain!="local_economy"||r.kind!="obligation_payment")return nullptr;const TransportPaymentReceipt*out=nullptr;for(const auto&v:s.payments)if(v.original_payment_receipt_id==r.id){if(out)fail("DuplicateTransportCashAdoption",r.id);out=&v;}return out;
}
}
namespace hanmo::resident {using namespace transport_finance_detail;
bool NativeTransportObligationOriginalRead::bound_to(const CommonStore&c,const LocalEconomyState&e)const noexcept{return &c==&common_&&&e==&economy_&&remainder_.bound_to(c,e);}
Outcome<OriginalEconomicPayable>NativeTransportObligationOriginalRead::read_original_payable(const EconomicObligationOriginalSource&s)const{try{
 const auto*p=std::get_if<OriginalTransportExpensePayable>(&s);if(!p)return remainder_.read_original_payable(s);const auto*r=take(read_transport_payment(transport_,p->original_expense.source_ref,p->original_expense.source_schema));return {true,historical(common_,*r,context_).payable,{}};
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Outcome<OriginalEconomicAdjustment>NativeTransportObligationOriginalRead::read_original_adjustment(const MomentOwnerSource&s)const{return remainder_.read_original_adjustment(s);}
Outcome<OriginalEconomicCashPayment>NativeTransportObligationOriginalRead::read_original_cash_payment(const SourceRef&s)const{try{const auto*r=by_payment(transport_,s);if(!r)return remainder_.read_original_cash_payment(s);return {true,cash(common_,economy_,*r),{}};}catch(Failure&e){return {false,{},std::move(e.error)};}}
Status NativeTransportObligationOriginalRead::validate_original_cash_payment_authority(const SourceRef&s)const{try{const auto*r=by_payment(transport_,s);if(!r)return remainder_.validate_original_cash_payment_authority(s);cash(common_,economy_,*r);const auto original=historical(common_,*r,context_);const auto permission=take(context_.authorities.read_transport_expense_allowance(r->request.authority,r->expense,TransportExpenseAuthorityCheck::existing_charge));if(permission.payer_wallet_id!=original.genesis.source_wallet_ref||permission.principal.kind!=original.genesis.debtor.kind||permission.principal.id!=original.genesis.debtor.id||permission.beneficiary.kind!=original.genesis.creditor.kind||permission.beneficiary.id!=original.genesis.creditor.id||permission.authority_source_id.empty()||permission.authority_revision<1||permission.gross_limit<0||permission.gross_spent_before<0||permission.gross_spent_before>permission.gross_limit||r->expense.amount>permission.gross_limit-permission.gross_spent_before)fail("OriginalTransportPaymentAuthorityMismatch",r->id);return common_success();}catch(Failure&e){return {false,{},std::move(e.error)};}}
Outcome<ObligationResponsibilityEpoch>NativeTransportObligationOriginalRead::read_original_responsibility_genesis(const EconomicObligationOriginalSource&s)const{try{const auto*p=std::get_if<OriginalTransportExpensePayable>(&s);if(!p)return remainder_.read_original_responsibility_genesis(s);const auto*r=take(read_transport_payment(transport_,p->original_expense.source_ref,p->original_expense.source_schema));return {true,historical(common_,*r,context_).genesis,{}};}catch(Failure&e){return {false,{},std::move(e.error)};}}
Status NativeTransportObligationOriginalRead::validate_original_responsibility_epoch(const ObligationResponsibilityEpoch&e)const{try{
 const TransportPaymentReceipt*found=nullptr;for(const auto&r:transport_.payments)if(r.original_obligation_id==e.obligation_ref.obligation_id){if(found)fail("DuplicateOriginalTransportObligation",r.id);found=&r;}
 if(!found)return remainder_.validate_original_responsibility_epoch(e);if(e.epoch||e.previous_epoch_ref||e.previous_execution_event_ref)fail("OriginalTransportResponsibilityChangeRequired",e.obligation_ref.obligation_id,ErrorKind::unsupported_capability);if(e.obligation_ref.owner_domain!="local_economy")fail("OriginalTransportObligationOwnerMismatch",e.obligation_ref.obligation_id);return validate_original_obligation_genesis(e,historical(common_,*found,context_).genesis);
}catch(Failure&f){return {false,{},std::move(f.error)};}}
}
