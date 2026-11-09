#include "resident/domains/local_economy/service_finance_execution.hpp"
#include "governance_work_settlement.hpp"
#include <set>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const StableId&code,const StableId&id={}){throw Failure{common_failure(code,id,ErrorKind::internal).error};}
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{v.error};return std::move(v.value);}void take(Status v){if(!v.ok)throw Failure{v.error};}
Integer add(Integer a,Integer b){Integer n;if(__builtin_add_overflow(a,b,&n))fail("WorkSettlementAmountOverflow");return n;}
int cmp(const WorldMoment&a,const WorldMoment&b){return take(compare_world_moments(a,b));}
bool terminal(const WorkCommissionTerminal&a,const WorkCommissionTerminal&b){return a.task_id==b.task_id&&same_source_ref(a.accepted_task_ref,b.accepted_task_ref)&&a.work_id==b.work_id&&a.task_terminal_fact_id==b.task_terminal_fact_id&&a.source_schema==b.source_schema;}
SourceRef terms_ref(const GovernanceWorkExpenseTerms&t){return {"local_economy","governance_work_expense_terms",t.expense_terms_id};}
SourceRef charge_ref(const GovernanceWorkChargeSource&p){return {"local_economy","governance_work_charge",p.id};}
ReservationRef reservation(const GovernanceWorkExpenseTerms&t){return {ReservationRef::Kind::domain_activity,t.expense_terms_id,"local_economy","governance_work_expense"};}
std::size_t header(const CommonStore&c,const SourceRef&r){const auto*f=c.state().facts.find(r.id);if(!f||f->id!=r.id||f->domain!=r.domain||f->kind!=r.kind)fail("WorkSettlementOriginalHeaderRequired",r.id);std::size_t n=0;for(const auto&row:c.state().facts.rows()){if(&row.second==f)return n;++n;}fail("WorkSettlementOriginalHeaderOrderMissing",r.id);}
const GovernanceWorkTerminationSettlement* settled(const GovernanceWorkExpenseState&s,const GovernanceWorkExpenseTerms&t){const GovernanceWorkTerminationSettlement*out=nullptr;for(const auto&r:s.termination_settlements.rows())if(same_source_ref(r.second.expense_terms_ref,terms_ref(t))){if(out)fail("DuplicateWorkTerminationSettlement",t.expense_terms_id);out=&r.second;}return out;}
const GovernanceWorkExpenseTerms& terms(const GovernanceWorkExpenseReadContext&x,const SourceRef&r){if(r.domain!="local_economy"||r.kind!="governance_work_expense_terms")fail("InvalidWorkExpenseTermsRef",r.id);const auto*t=x.state.terms.find(r.id);if(!t)fail("WorkExpenseTermsMissing",r.id);header(x.common,r);return *t;}
S3WorkCommission scope(const GovernanceWorkExpenseTerms&t){return {t.work_id,t.task_acceptance_source,t.work_acceptance_source,terms_ref(t)};}
const WorkCommissionTaskTerminalSource& original_terminal(const GovernanceWorkExpenseReadContext&x,const GovernanceWorkExpenseTerms&t,const WorkCommissionTerminal&r){if(!x.settlement)fail("WorkCommissionTerminationReaderRequired",t.expense_terms_id);const auto*actual=take(x.settlement->read_work_commission_termination_for_financial_evidence(scope(t),r));if(!actual||r.source_schema!=1||r.work_id!=t.work_id||r.task_id!=t.accepted_task_id||!same_source_ref(r.accepted_task_ref,t.task_acceptance_source)||actual->task_terminal_fact_id!=r.task_terminal_fact_id||actual->accepted_scope_ref.work_id!=t.work_id||!same_source_ref(actual->accepted_scope_ref.accepted_task_ref,t.task_acceptance_source)||!same_source_ref(actual->accepted_scope_ref.work_acceptance_source_ref,t.work_acceptance_source)||!same_source_ref(actual->accepted_scope_ref.expense_terms_ref,terms_ref(t))||!same_terms_revision(actual->accepted_terms_ref,t.frozen.accepted_terms_identity)||cmp(actual->effective_game_time,t.accepted_at)<0||cmp(actual->effective_game_time,take(x.clock.read_current_moment()))>0)fail("WorkCommissionOriginalTerminationMismatch",r.task_terminal_fact_id);return *actual;}
}
Outcome<bool>read_work_scope_financial_termination(const GovernanceWorkExpenseState&s,const GovernanceWorkExpenseTerms&t){try{return {true,settled(s,t)!=nullptr,{}};}catch(const Failure&e){return {false,{},e.error};}}
Outcome<Integer>read_work_fee_reserved_cash(const GovernanceWorkExpenseState&s,const GovernanceWorkExpenseTerms&t,const LocalEconomyState*e){try{
 if(settled(s,t))return {true,0,{}};Integer total=0;
 for(const auto&l:t.frozen.fee_lines)if(l.payment_gate==WorkFeePaymentGate::pay_on_charge){Integer paid=0;
  for(const auto&r:s.charges.rows())if(r.second.charge_key.canonical_scope_id==t.frozen.canonical_scope_id&&r.second.charge_key.line_id==l.line_id)for(const auto&id:r.second.original_payment_ids){if(!e)fail("WorkFeeOriginalPaymentsRequired",id);const auto*p=e->payments.find(id);const auto*source=s.payments.find(id);if(!p||!source||source->charge_id!=r.first||p->authority_ref!=r.first||p->amount<=0)fail("WorkFeeOriginalPaymentMismatch",id);paid=add(paid,p->amount);}
  if(paid>l.maximum_qian)fail("WorkFeeAcceptedCapExceeded",l.line_id);total=add(total,l.maximum_qian-paid);
 }return {true,total,{}};
}catch(const Failure&e){return {false,{},e.error};}}
Status validate_governance_work_settlements(const GovernanceWorkExpenseReadContext&x){try{
 const auto&s=x.state;if(s.version!=1&&s.version!=2)fail("UnsupportedGovernanceWorkExpenseVersion");if(s.version==1&&!s.termination_settlements.empty())fail("UnexpectedWorkSettlementSchema2");std::set<StableId>terms_seen,terminals;
 for(const auto&r:s.termination_settlements.rows()){
  const auto&v=r.second;const auto&t=terms(x,v.expense_terms_ref);const auto&actual=original_terminal(x,t,v.termination_ref);
  if(v.id!=r.first||!terms_seen.insert(t.expense_terms_id).second||!terminals.insert(v.termination_ref.task_terminal_fact_id).second||cmp(v.settled_at,actual.effective_game_time)<0||cmp(v.settled_at,take(x.clock.read_current_moment()))>0)fail("WorkOriginalSettlementMismatch",r.first);
  const auto settled_order=header(x.common,{"local_economy","work_service_settled",v.id});const auto*terminal_header=x.common.state().facts.find(v.termination_ref.task_terminal_fact_id);if(!terminal_header||header(x.common,{terminal_header->domain,terminal_header->kind,terminal_header->id})>=settled_order)fail("WorkSettlementBeforeOriginalTerminal",v.id);std::size_t n=0;
  for(const auto&ch:s.charges.rows())if(same_source_ref(ch.second.expense_terms_ref,v.expense_terms_ref)){
   if(n>=v.preserved_charge_refs.size()||!same_source_ref(v.preserved_charge_refs[n++],charge_ref(ch.second))||cmp(ch.second.earned_at,actual.effective_game_time)>0||header(x.common,charge_ref(ch.second))>=settled_order)fail("WorkSettlementChargeCoverage",ch.first);
  }if(n!=v.preserved_charge_refs.size())fail("WorkSettlementChargeCoverage",v.id);
 }
 for(const auto&r:x.common.state().facts.rows())if(r.second.domain=="local_economy"&&r.second.kind=="work_service_settled"&&!s.termination_settlements.find(r.first))fail("WorkSettlementPayloadMissing",r.first);
 return common_success();
}catch(const Failure&e){return {false,{},e.error};}}
Outcome<SourceRef>consume_work_service_actual_termination_in_candidate(CommonStore&c,LocalEconomyState&e,GovernanceWorkExpenseState&s,const GovernanceWorkExpenseReadContext&x,const S3WorkCommission&accepted,const WorkCommissionTerminal&terminal_ref){try{
 if(&c!=&x.common||&e!=&x.economy||&s!=&x.state||s.version!=2)fail("WorkSettlementCandidateMismatch");const auto&t=terms(x,accepted.expense_terms_ref);
 if(accepted.work_id!=t.work_id||!same_source_ref(accepted.accepted_task_ref,t.task_acceptance_source)||!same_source_ref(accepted.work_acceptance_source_ref,t.work_acceptance_source))fail("WorkCommissionSettlementScopeMismatch",t.expense_terms_id);const auto&source=original_terminal(x,t,terminal_ref);
 if(const auto*prior=settled(s,t)){if(!terminal(prior->termination_ref,terminal_ref))fail("WorkCommissionTerminalChanged",prior->id);take(validate_governance_work_settlements(x));return {true,{"local_economy","work_service_settled",prior->id},{}};}
 GovernanceWorkTerminationSettlement result{take(c.next_id("work_service_settlement")),terms_ref(t),terminal_ref,take(x.clock.read_current_moment()),{}};
 if(cmp(result.settled_at,source.effective_game_time)<0)fail("WorkSettlementBeforeTermination",result.id);
 for(const auto&r:s.charges.rows())if(same_source_ref(r.second.expense_terms_ref,terms_ref(t))){if(cmp(r.second.earned_at,source.effective_game_time)>0)fail("WorkChargeAfterActualTermination",r.first);if(r.second.earned_qian)take(read_economic_payable(*take(service_finance::original_obligation_read(c,e,x.obligations)),{"local_economy","contract_payment_due",r.second.original_arrear_id}));result.preserved_charge_refs.push_back(charge_ref(r.second));}
 ResourceRecipe release;release.expected_versions={c.read_version({ResourceKey::Kind::wallet,t.frozen.approval.payer_wallet_id})};release.operations={SetReservation{t.frozen.approval.payer_wallet_id,reservation(t),0,true}};take(c.apply_resources(release));take(c.append_fact({result.id,"work_service_settled","local_economy"}));if(!s.termination_settlements.insert(result.id,result))fail("DuplicateWorkTerminationSettlement",result.id);take(validate_governance_work_settlements(x));return {true,{"local_economy","work_service_settled",result.id},{}};
}catch(const Failure&e){return {false,{},e.error};}}
Outcome<std::optional<WorldMoment>>read_governance_work_settlement_moment(const GovernanceWorkExpenseReadContext&x,const SourceRef&r){try{
 if(r.domain!="local_economy"||r.kind!="work_service_settled")return {true,{}, {}};const auto*p=x.state.termination_settlements.find(r.id);if(!p)fail("WorkTerminationSettlementMissing",r.id);take(validate_governance_work_settlements(x));return {true,p->settled_at,{}};
}catch(const Failure&e){return {false,{},e.error};}}
}
