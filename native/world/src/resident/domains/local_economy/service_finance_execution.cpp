#include "service_finance_execution.hpp"
#include "resident/storage/money_transfer_recipe.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident::service_finance {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char*code,const StableId&id={},ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,id,kind).error};}
void take(Status s){if(!s.ok)throw Failure{std::move(s.error)};}
template<class T>T take(Outcome<T>s){if(!s.ok)throw Failure{std::move(s.error)};return std::move(s.value);}
Integer sum(Integer a,Integer b){if(a<0||b<0||a>std::numeric_limits<Integer>::max()-b)fail("OriginalServiceFinancialOverflow");return a+b;}
void header(const CommonStore&c,const SourceRef&s){const auto*h=c.state().facts.find(s.id);if(!valid_source_ref(s)||!h||h->id!=s.id||h->domain!=s.domain||h->kind!=s.kind)fail("OriginalServiceFinancialCauseRequired",s.id);}
void money_leg(const CommonStore&c,const ResourceExecutionDraft&d,std::size_t n,const StableId&id,Integer amount,Integer before,Integer after){
 if(n>=d.operations.size()||n>=d.recipe.operations.size())fail("OriginalServiceMoneyLegMissing",id);const auto*op=std::get_if<WalletDelta>(&d.recipe.operations[n]);const auto&v=d.operations[n];
 if(!op||op->wallet_id!=id||op->delta!=amount||!op->require_spendable||v.operation_index!=n||v.next_instance_before!=d.next_instance_before||v.next_instance_after!=d.next_instance_after||v.wallets.size()!=1||!v.inventories.empty()||!v.lot_flows.empty())fail("OriginalServiceMoneyLegMismatch",id);
 const auto&w=v.wallets.front();const auto*current=c.wallet(id);if(w.wallet_id!=id||!w.before||w.before->id!=id||w.after.id!=id||w.before->balance!=before||w.after.balance!=after||!current||current->balance!=after)fail("OriginalServiceMoneyImageMismatch",id);
}
}
Outcome<const EconomicObligationReadContext*> original_obligation_read(const CommonStore&c,const LocalEconomyState&e,const EconomicObligationReadContext*x){
 if(!x)return {false,nullptr,common_failure("OriginalEconomicObligationContextRequired",{},ErrorKind::unsupported_capability).error};
 if(&x->common!=&c||&x->economy!=&e||!x->original.bound_to(c,e))return {false,nullptr,common_failure("OriginalEconomicObligationCandidateMismatch",{},ErrorKind::internal).error};return {true,x,{}};
}
Outcome<EconomicObligationWriteContext*> original_obligation_write(CommonStore&c,LocalEconomyState&e,const EconomicObligationReadContext*r,EconomicObligationWriteContext*w){
 if(!r||!w)return {false,nullptr,common_failure("OriginalEconomicObligationWriterRequired",{},ErrorKind::unsupported_capability).error};
 if(&r->common!=&c||&w->common!=&c||&r->economy!=&e||&w->economy!=&e||&r->sources!=&w->sources||&r->original!=&w->original||&r->clock!=&w->clock||!w->original.bound_to(c,e))return {false,nullptr,common_failure("OriginalEconomicObligationCandidateMismatch",{},ErrorKind::internal).error};return {true,w,{}};
}
Status post_payable_in_candidate(EconomicObligationWriteContext&x,const EconomicObligationOriginalSource&source){try{
 auto&c=x.common;auto&e=x.economy;if((!economic_obligation_sources_format_valid(x.sources)||!economic_obligation_original_source_supported(x.sources.schema_version,source))||!x.original.bound_to(c,e))fail("OriginalEconomicObligationCandidateMismatch");
 const auto actual=take(x.original.read_original_payable(source));const auto epoch=take(x.original.read_original_responsibility_genesis(source));const auto now=take(x.clock.read_current_moment());
 if(actual.obligation_ref.owner_domain!="local_economy"||actual.arrear_ref.domain!="local_economy"||actual.arrear_ref.kind!="contract_payment_due"||actual.original_due<=0||take(compare_world_moments(actual.established_at,now))>0||take(compare_world_moments(actual.due_at,actual.established_at))<0||!same_obligation_ref(epoch.obligation_ref,actual.obligation_ref)||epoch.epoch||epoch.previous_epoch_ref||epoch.previous_execution_event_ref||take(compare_world_moments(epoch.effective_at,actual.established_at)))fail("OriginalSourceOnlyEstablishmentMismatch",actual.obligation_ref.obligation_id);
 header(c,actual.canonical_charge_ref.source_ref);take(x.original.validate_original_responsibility_epoch(epoch));
 for(const auto&b:x.sources.obligation_sources)if(same_obligation_ref(b.obligation_ref,actual.obligation_ref)){if(!same_economic_obligation_original_source(b.original_source,source))fail("ConflictingOriginalPayableReplay",actual.obligation_ref.obligation_id);take(read_economic_payable(x.read(),actual.arrear_ref));return common_success();}
 if(take(compare_world_moments(actual.established_at,now)))fail("OriginalSourceOnlyActualEstablishmentBoundaryRequired",actual.arrear_ref.id);
 const auto&id=actual.obligation_ref.obligation_id;const auto&due=actual.arrear_ref.id;
 if(id.empty()||due.empty()||id==due||e.obligations.find(id)||e.obligation_remainders.find(id)||e.arrears.find(due)||c.state().facts.find(due)||!c.wallet(epoch.source_wallet_ref)||!c.wallet(epoch.destination_wallet_ref)||epoch.debtor.id.empty()||epoch.creditor.id.empty()||epoch.authority_ref.empty())fail("DuplicateOriginalServicePayable",due);
 RecurringObligation o;o.id=id;o.principal=epoch.debtor;o.beneficiary=epoch.creditor;o.kind=ObligationKind::contract_payment;o.source_wallet_or_inventory=epoch.source_wallet_ref;o.destination=epoch.destination_wallet_ref;o.authority_ref=epoch.authority_ref;o.period_ticks=0;o.next_due_tick=0;o.rate_profile.clear();o.active=false;o.last_receipt_id=due;
 ArrearRecord d{due,id,actual.due_at.half_day_tick,actual.original_due,0};
 if(!e.obligations.insert(id,std::move(o))||!e.obligation_remainders.insert(id,0)||!e.arrears.insert(due,std::move(d)))fail("OriginalServicePayableInsertionFailed",due);take(c.append_fact({due,"contract_payment_due","local_economy"}));
 x.sources.obligation_sources.push_back({actual.obligation_ref,EconomicObligationScheduler::source_only,source});x.sources.payable_sources.push_back({actual.arrear_ref,actual.obligation_ref,source,actual.canonical_charge_ref,actual.established_at,actual.due_at});x.sources.responsibility_epochs.push_back(epoch);
 take(read_economic_payable(x.read(),actual.arrear_ref));return common_success();
 }catch(Failure&f){return {false,{},std::move(f.error)};}}
Status append_money_transfer_recipe(const CommonStore&c,ResourceRecipe&r,const StableId&payer,const StableId&payee,Integer amount){try{
 const auto*from=c.wallet(payer);const auto*to=c.wallet(payee);if(payer.empty()||payee.empty()||payer==payee||amount<0||!from||!to||from->balance<amount||to->balance<0||to->balance>std::numeric_limits<Integer>::max()-amount)fail("OriginalServiceMoneyBalanceMismatch",payer);
 r.expected_versions.push_back(c.read_version({ResourceKey::Kind::wallet,payer}));r.expected_versions.push_back(c.read_version({ResourceKey::Kind::wallet,payee}));take(resource_recipe::append_money_transfer(r,payer,payee,amount,resource_recipe::MoneyTransferPolicy::spendable));return common_success();
 }catch(Failure&f){return {false,{},std::move(f.error)};}}
Outcome<AppliedMoney> transfer_money_in_candidate(CommonStore&c,const StableId&payer,const StableId&payee,Integer amount,const SetReservation*reservation){try{
 if(payer.empty()||payee.empty()||payer==payee||amount<=0)fail("OriginalServiceMoneyTransferRequired",payer);const auto capture=c.resource_execution_capture_identity();if(!capture)fail("OriginalServiceMoneyCaptureRequired",payer,ErrorKind::unsupported_capability);
 const auto*from=c.wallet(payer);const auto*to=c.wallet(payee);if(!from||!to||from->balance<amount||to->balance<0||to->balance>std::numeric_limits<Integer>::max()-amount)fail("OriginalServiceMoneyBalanceMismatch",payer);if(reservation&&(reservation->wallet_id!=payer||reservation->amount<0||reservation->action_id.empty()))fail("OriginalServiceMoneyReservationMismatch",payer);
 AppliedMoney out;out.common_=&c;out.capture_=capture;out.draft_=c.resource_execution_drafts().size();out.payer_operation_=reservation?1:0;out.payer_before=from->balance;out.payer_after=from->balance-amount;out.payee_before=to->balance;out.payee_after=to->balance+amount;
 ResourceRecipe r;if(reservation)r.operations.push_back(*reservation);take(append_money_transfer_recipe(c,r,payer,payee,amount));take(c.apply_resources(r));const auto&drafts=c.resource_execution_drafts();if(c.resource_execution_capture_identity()!=capture||drafts.size()!=out.draft_+1)fail("OriginalServiceMoneyCaptureMismatch",payer);const auto&d=drafts.back();if(d.operations.size()!=out.payer_operation_+2||d.recipe.operations.size()!=d.operations.size()||d.next_instance_before!=d.next_instance_after)fail("OriginalServiceMoneyRecipeMismatch",payer);
 if(reservation){const auto*original=std::get_if<SetReservation>(&d.recipe.operations.front());const auto&v=d.operations.front();if(!original||original->wallet_id!=reservation->wallet_id||original->action_id!=reservation->action_id||original->amount!=reservation->amount||original->erase!=reservation->erase||v.operation_index||v.wallets.size()!=1||!v.inventories.empty()||!v.lot_flows.empty()||!v.wallets.front().before||v.wallets.front().before->balance!=out.payer_before||v.wallets.front().after.balance!=out.payer_before)fail("OriginalServiceReservationCaptureMismatch",payer);}
 money_leg(c,d,out.payer_operation_,payer,-amount,out.payer_before,out.payer_after);money_leg(c,d,out.payer_operation_+1,payee,amount,out.payee_before,out.payee_after);return {true,std::move(out),{}};
 }catch(Failure&f){return {false,{},std::move(f.error)};}}
Status record_payment_in_candidate(EconomicObligationWriteContext&x,const PaymentReceipt&p,AppliedMoney&&money){try{
 auto&c=x.common;auto&e=x.economy;const auto read=x.read();const auto before=take(read_economic_payable(read,{"local_economy","contract_payment_due",p.arrear_id}));const auto at=take(x.clock.read_current_moment());const auto&epoch=*before.current_responsibility;
 if(money.consumed_||money.common_!=&c||!money.capture_||money.capture_!=c.resource_execution_capture_identity()||money.draft_>=c.resource_execution_drafts().size())fail("OriginalServicePaymentCaptureMismatch",p.id);
 if(p.id.empty()||p.obligation_id!=before.obligation->id||p.authority_ref!=epoch.authority_ref||p.from_wallet_id!=epoch.source_wallet_ref||p.to_wallet_id!=epoch.destination_wallet_ref||p.due_tick!=before.arrear->due_tick||p.tick!=at.half_day_tick||take(compare_world_moments(at,before.source->due_at))<0||p.amount<=0||p.amount>before.outstanding||p.amount!=money.payer_before-money.payer_after||p.amount!=money.payee_after-money.payee_before||e.payments.find(p.id)||c.state().facts.find(p.id))fail("OriginalServicePaymentReceiptMismatch",p.id);
 const auto&actual=c.resource_execution_drafts()[money.draft_];money_leg(c,actual,money.payer_operation_,p.from_wallet_id,-p.amount,money.payer_before,money.payer_after);money_leg(c,actual,money.payer_operation_+1,p.to_wallet_id,p.amount,money.payee_before,money.payee_after);
 if(!e.payments.insert(p.id,p))fail("OriginalServicePaymentInsertionFailed",p.id);take(c.append_fact({p.id,"obligation_payment","local_economy"}));
 x.sources.payment_responsibility_bindings.push_back({{"local_economy","obligation_payment",p.id},before.source->obligation_ref,before.source->original_arrear_ref,{before.source->obligation_ref,epoch.epoch},at,before.source->canonical_charge_ref,before.last_execution_event});
 e.arrears.find(p.arrear_id)->paid_amount=sum(before.cash_paid,p.amount);e.obligations.find(p.obligation_id)->last_receipt_id=p.id;money.consumed_=true;
 // The original owner now appends its precise payment adoption and invokes the
 // full P11 reader before publication. No in-flight result can escape alone.
 return common_success();
 }catch(Failure&f){return {false,{},std::move(f.error)};}}
}
