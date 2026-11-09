// Actual charge -> SourceOnly payable -> captured Common cash -> original adoption.
// Every write remains inside the outer World candidate; no in-flight result is
// publishable until the real movement/entry owner and full P11 cold checks pass.
#include "economic_transport_sources.hpp"
#include "obligations/transport_original_finance_detail.hpp"
#include "service_finance_execution.hpp"
#include "resident/domains/travel/transport_authority_codec.hpp"
#include <set>
#include <tuple>
namespace hanmo::resident {using namespace transport_finance_detail;
namespace {
// This stack-only continuation exists after the real cost owner has appended its
// charge head, but before the original transport paid receipt can truthfully be
// appended. It borrows the already verified adoption, never a caller-supplied sum.
// The stable NativeTransport reader has no corresponding bypass.
class ApplyingTransportOriginal final:public EconomicObligationOriginalRead {
 const EconomicObligationOriginalRead&rest_;const CommonStore&common_;const LocalEconomyState&economy_;
 const TransportPaymentReceipt&row_;const OriginalTransportFinance&verified_;
 bool matches(const EconomicObligationOriginalSource&s)const{const auto*p=std::get_if<OriginalTransportExpensePayable>(&s);return p&&source(p->original_expense,expense(row_));}
public:
 ApplyingTransportOriginal(const EconomicObligationOriginalRead&r,const CommonStore&c,const LocalEconomyState&e,const TransportPaymentReceipt&v,const OriginalTransportFinance&f):rest_(r),common_(c),economy_(e),row_(v),verified_(f){}
 bool bound_to(const CommonStore&c,const LocalEconomyState&e)const noexcept override{return &c==&common_&&&e==&economy_&&rest_.bound_to(c,e);}
 Outcome<OriginalEconomicPayable>read_original_payable(const EconomicObligationOriginalSource&s)const override{if(matches(s))return {true,verified_.payable,{}};return rest_.read_original_payable(s);}
 Outcome<OriginalEconomicAdjustment>read_original_adjustment(const MomentOwnerSource&s)const override{return rest_.read_original_adjustment(s);}
 Outcome<OriginalEconomicCashPayment>read_original_cash_payment(const SourceRef&s)const override{if(!same_source_ref(s,payment(row_)))return rest_.read_original_cash_payment(s);try{return {true,cash(common_,economy_,row_),{}};}catch(Failure&e){return {false,{},std::move(e.error)};}}
 Status validate_original_cash_payment_authority(const SourceRef&s)const override{if(!same_source_ref(s,payment(row_)))return rest_.validate_original_cash_payment_authority(s);try{cash(common_,economy_,row_);return common_success();}catch(Failure&e){return {false,{},std::move(e.error)};}}
 Outcome<ObligationResponsibilityEpoch>read_original_responsibility_genesis(const EconomicObligationOriginalSource&s)const override{if(matches(s))return {true,verified_.genesis,{}};return rest_.read_original_responsibility_genesis(s);}
 Status validate_original_responsibility_epoch(const ObligationResponsibilityEpoch&e)const override{if(same_obligation_ref(e.obligation_ref,verified_.payable.obligation_ref))return validate_original_obligation_genesis(e,verified_.genesis);return rest_.validate_original_responsibility_epoch(e);}
};
void allowance(const TransportExpenseAllowance&a,const ActualTransportExpense&cost){if(a.payer_wallet_id.empty()||a.authority_source_id.empty()||a.authority_revision<1||a.principal.id.empty()||a.beneficiary.id.empty()||a.gross_limit<0||a.gross_spent_before<0||a.gross_spent_before>a.gross_limit||cost.amount>a.gross_limit-a.gross_spent_before)fail("TransportExpenseAllowanceExceeded",cost.journey_id,ErrorKind::rule);}
}
Outcome<const TransportPaymentReceipt*>read_transport_payment(const EconomicTransportExpenseState&s,const SourceRef&source,Integer schema){const TransportPaymentReceipt*out=nullptr;for(const auto&r:s.payments)if(same_source_ref(r.request.actual_movement_or_handling_source,source)&&r.request.source_schema==schema){if(out)return {false,nullptr,common_failure("DuplicateTransportExpenseSource",source.id,ErrorKind::internal).error};out=&r;}if(!out)return {false,nullptr,common_failure("TransportPaymentSourceMissing",source.id,ErrorKind::internal).error};return {true,out,{}};}
Outcome<TransportPaymentReceipt>pay_transport_expense_in_candidate(CommonStore&c,LocalEconomyState&e,EconomicTransportExpenseState&s,const EconomicTransportPaymentContext&x,const TransportExpenseRequest&request){try{
 if(!valid_source_ref(request.actual_movement_or_handling_source)||request.source_schema<1)fail("InvalidTransportExpenseSource",request.actual_movement_or_handling_source.id);
 const auto cost=take(x.actual_sources.read_actual_transport_expense(request.actual_movement_or_handling_source,request.source_schema));
 if(!same_source_ref(cost.source_ref,request.actual_movement_or_handling_source)||cost.source_schema!=request.source_schema||cost.journey_id.empty()||cost.segment_or_handling_id.empty()||cost.cost_rule_id.empty()||cost.amount<0||cost.payee_wallet_id.empty()||cost.destination_id.empty())fail("TransportExpenseSourceMismatch",cost.source_ref.id);
 header(c,cost.source_ref);take(x.actual_sources.validate_transport_expense_authority(cost.source_ref,cost.source_schema,request.authority));
 const TransportPaymentReceipt*replay=nullptr;for(const auto&r:s.payments)if(same_source_ref(r.request.actual_movement_or_handling_source,cost.source_ref)&&r.request.source_schema==cost.source_schema){if(replay)fail("DuplicateTransportExpenseSource",cost.source_ref.id);replay=&r;}
 const auto permission=take(x.authorities.read_transport_expense_allowance(request.authority,cost,replay?TransportExpenseAuthorityCheck::existing_charge:TransportExpenseAuthorityCheck::new_charge));allowance(permission,cost);
 if(replay){if(replay->payer_wallet_id!=permission.payer_wallet_id||!same_cost(replay->expense,cost)||!same_transport_expense_authority(replay->request.authority,request.authority))fail("TransportExpenseReplayMismatch",replay->id);header(c,adoption(*replay));if(cost.amount){const auto*r=take(service_finance::original_obligation_read(c,e,x.obligations));take(read_economic_payable(*r,arrear(*replay)));cash(c,e,*replay);}else if(!replay->original_obligation_id.empty()||!replay->original_arrear_id.empty()||!replay->original_payment_receipt_id.empty())fail("ZeroTransportExpenseHasMoneyLeg",replay->id);return {true,*replay,{}};}
 const auto at=take(x.actual_sources.current_transport_expense_moment());if(compare(cost.occurred_at,at)||at.half_day_tick<c.tick())fail("TransportExpenseActualMomentMismatch",cost.source_ref.id);
 const auto*payer=c.wallet(permission.payer_wallet_id);const auto*payee=c.wallet(cost.payee_wallet_id);if(!payer||!payee||payer->id==payee->id||!c.valid_resource_owner(payer->owner)||!c.valid_resource_owner(payee->owner))fail("TransportExpenseWalletMismatch",cost.journey_id);
 if(take(c.spendable(payer->id))<cost.amount)fail("InsufficientFunds",payer->id,ErrorKind::rule);if(payee->balance>INT64_MAX-cost.amount)fail("MoneyOverflow",payee->id);
 TransportPaymentReceipt result{take(c.next_id("transport_expense")),request,cost,payer->id,{},{},{}};
 if(!cost.amount){take(c.append_fact({result.id,"transport_expense_paid","local_economy"}));s.payments.push_back(result);return {true,std::move(result),{}};}
 auto*root=take(service_finance::original_obligation_write(c,e,x.obligations,x.obligation_writes));if(compare(at,take(root->clock.read_current_moment())))fail("TransportSourceOnlyClockMismatch",result.id);
 const auto parties=take(x.authorities.read_original_transport_responsibility(request.authority,cost,TransportExpenseAuthorityCheck::new_charge));
 if(parties.payer_wallet_id!=permission.payer_wallet_id||parties.debtor.kind!=permission.principal.kind||parties.debtor.id!=permission.principal.id||parties.creditor.kind!=permission.beneficiary.kind||parties.creditor.id!=permission.beneficiary.id)fail("TransportAcceptedResponsibilityChanged",result.id);
 result.original_obligation_id=take(c.next_id("transport_obligation"));result.original_arrear_id=take(c.next_id("contract_payment_due"));result.original_payment_receipt_id=take(c.next_id("obligation_payment"));
 const auto verified=original(c,result,cost,parties);ApplyingTransportOriginal original_read(root->original,c,e,result,verified);EconomicObligationWriteContext financial{c,e,root->sources,original_read,root->clock};
 take(service_finance::post_payable_in_candidate(financial,OriginalTransportExpensePayable{expense(result)}));
 auto moved=take(service_finance::transfer_money_in_candidate(c,result.payer_wallet_id,cost.payee_wallet_id,cost.amount));
 const PaymentReceipt paid{result.original_payment_receipt_id,result.original_arrear_id,result.original_obligation_id,result.payer_wallet_id,cost.payee_wallet_id,at.half_day_tick,cost.occurred_at.half_day_tick,cost.amount,result.id};
 take(service_finance::record_payment_in_candidate(financial,paid,std::move(moved)));
 take(c.append_fact({result.id,"transport_expense_paid","local_economy"}));s.payments.push_back(result);
 const auto actual=take(read_economic_payable(financial.read(),arrear(result)));if(actual.cash_paid!=cost.amount||actual.outstanding)fail("TransportActualCashClosureMismatch",result.id);
 return {true,std::move(result),{}};
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Status validate_economic_transport_payments(const CommonStore&c,const LocalEconomyState&e,const EconomicTransportExpenseState&s,const EconomicTransportPaymentContext&x){try{
 std::set<StableId>ids,obligations,arrears,payments;std::set<std::tuple<StableId,StableId,StableId,Integer>>origins;
 const auto now=take(x.actual_sources.current_transport_expense_moment());
 for(const auto&r:s.payments){
  if(r.id.empty()||!ids.insert(r.id).second||!origins.emplace(r.request.actual_movement_or_handling_source.domain,r.request.actual_movement_or_handling_source.kind,r.request.actual_movement_or_handling_source.id,r.request.source_schema).second)fail("DuplicateTransportPaymentSource",r.id);
  if(header(c,adoption(r))<=header(c,r.expense.source_ref))fail("TransportExpenseAdoptionOrderMismatch",r.id);
  const auto actual=take(x.actual_sources.read_actual_transport_expense(r.request.actual_movement_or_handling_source,r.request.source_schema));if(!same_cost(r.expense,actual)||actual.amount<0||compare(actual.occurred_at,now)>0)fail("TransportExpenseSourceMismatch",r.id);take(x.actual_sources.validate_transport_expense_authority(actual.source_ref,actual.source_schema,r.request.authority));
  const auto permission=take(x.authorities.read_transport_expense_allowance(r.request.authority,actual,TransportExpenseAuthorityCheck::existing_charge));allowance(permission,actual);if(permission.payer_wallet_id!=r.payer_wallet_id)fail("TransportExpensePayerMismatch",r.id);
  if(!actual.amount){if(!r.original_obligation_id.empty()||!r.original_arrear_id.empty()||!r.original_payment_receipt_id.empty())fail("ZeroTransportExpenseHasMoneyLeg",r.id);continue;}
  if(!obligations.insert(r.original_obligation_id).second||!arrears.insert(r.original_arrear_id).second||!payments.insert(r.original_payment_receipt_id).second)fail("DuplicateTransportOriginalMoneyLeg",r.id);
  const auto*root=take(service_finance::original_obligation_read(c,e,x.obligations));const auto payable=take(read_economic_payable(*root,arrear(r)));cash(c,e,r);
  const auto*source=std::get_if<OriginalTransportExpensePayable>(&payable.source->original_source);if(!source||!transport_finance_detail::source(source->original_expense,expense(r))||payable.obligation->id!=r.original_obligation_id||payable.arrear->original_due!=actual.amount||payable.cash_paid!=actual.amount||payable.outstanding||payable.positive_increments||payable.unpaid_void)fail("TransportOriginalPayableMismatch",r.id);
 }
 for(const auto&f:c.state().facts.rows())if(f.second.domain=="local_economy"&&f.second.kind=="transport_expense_paid"&&!ids.count(f.first))fail("MissingTransportPaymentPayload",f.first);
 for(const auto&o:e.obligations.rows())if(ids.count(o.second.authority_ref)&&!obligations.count(o.first))fail("OrphanTransportObligation",o.first);
 for(const auto&d:e.arrears.rows())if(obligations.count(d.second.obligation_id)&&!arrears.count(d.first))fail("OrphanTransportArrear",d.first);
 for(const auto&p:e.payments.rows())if((ids.count(p.second.authority_ref)||obligations.count(p.second.obligation_id))&&!payments.count(p.first))fail("OrphanTransportPayment",p.first);
 return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Outcome<std::optional<WorldMoment>> read_economic_transport_fact_moment(const CommonStore& common,const LocalEconomyState& economy,const EconomicTransportExpenseState& state,const SourceRef& source){
 if(source.domain!="local_economy")return {true,{}, {}};
 auto transport_header=[&](const StableId& id){const auto* f=common.state().facts.find(id);return f&&f->domain=="local_economy"&&f->kind=="transport_expense_paid";};
 const TransportPaymentReceipt* found=nullptr;
 for(const auto& row:state.payments){bool matches=(source.kind=="transport_expense_paid"&&row.id==source.id)||(source.kind=="contract_payment_due"&&!row.original_arrear_id.empty()&&row.original_arrear_id==source.id)||(source.kind=="obligation_payment"&&!row.original_payment_receipt_id.empty()&&row.original_payment_receipt_id==source.id);if(!matches)continue;if(found)return {false,{},common_failure("DuplicateTransportFactMoment",source.id).error};found=&row;}
 if(!found){
  bool owned=source.kind=="transport_expense_paid";
  if(source.kind=="contract_payment_due"){const auto* due=economy.arrears.find(source.id);const auto* obligation=due?economy.obligations.find(due->obligation_id):nullptr;owned=obligation&&transport_header(obligation->authority_ref);}
  if(source.kind=="obligation_payment"){const auto* paid=economy.payments.find(source.id);const auto* obligation=paid?economy.obligations.find(paid->obligation_id):nullptr;owned=(paid&&transport_header(paid->authority_ref))||(obligation&&transport_header(obligation->authority_ref));}
  if(owned)return {false,{},common_failure("TransportPaymentSourceMissing",source.id).error};
  return {true,{}, {}};
 }
 const auto& r=*found;const auto* fact=common.state().facts.find(source.id);
 if(!fact||fact->domain!=source.domain||fact->kind!=source.kind||!transport_header(r.id)||!valid_source_ref(r.request.actual_movement_or_handling_source)||r.request.source_schema<1||!same_source_ref(r.request.actual_movement_or_handling_source,r.expense.source_ref)||r.request.source_schema!=r.expense.source_schema||r.expense.amount<0||!encode_world_moment(r.expense.occurred_at).ok||r.payer_wallet_id.empty()||r.expense.payee_wallet_id.empty()||r.payer_wallet_id==r.expense.payee_wallet_id)return {false,{},common_failure("TransportFactSourceMismatch",source.id).error};
 if(!r.expense.amount){if(!r.original_obligation_id.empty()||!r.original_arrear_id.empty()||!r.original_payment_receipt_id.empty())return {false,{},common_failure("ZeroTransportExpenseHasMoneyLeg",r.id).error};return {true,r.expense.occurred_at,{}};}
 const auto* debt=economy.obligations.find(r.original_obligation_id);const auto* due=economy.arrears.find(r.original_arrear_id);const auto* paid=economy.payments.find(r.original_payment_receipt_id);const auto tick=r.expense.occurred_at.half_day_tick;
 auto header=[&](const StableId& id,const char* kind){const auto* f=common.state().facts.find(id);return f&&f->domain=="local_economy"&&f->kind==kind;};
 if(!debt||!due||!paid||!header(due->id,"contract_payment_due")||!header(paid->id,"obligation_payment")||debt->kind!=ObligationKind::contract_payment||debt->active||debt->source_wallet_or_inventory!=r.payer_wallet_id||debt->destination!=r.expense.payee_wallet_id||!debt->rate_profile.empty()||debt->authority_ref!=r.id||debt->last_receipt_id!=paid->id||debt->next_due_tick!=0||debt->period_ticks!=0||due->obligation_id!=debt->id||due->due_tick!=tick||due->original_due!=r.expense.amount||due->paid_amount!=r.expense.amount||paid->arrear_id!=due->id||paid->obligation_id!=debt->id||paid->from_wallet_id!=r.payer_wallet_id||paid->to_wallet_id!=r.expense.payee_wallet_id||paid->tick!=tick||paid->due_tick!=tick||paid->amount!=r.expense.amount||paid->authority_ref!=r.id)return {false,{},common_failure("TransportOriginalMoneyLegMismatch",r.id).error};
 try{transport_finance_detail::cash(common,economy,r);}catch(transport_finance_detail::Failure&e){return {false,{},std::move(e.error)};}
 return {true,r.expense.occurred_at,{}};
}
}
