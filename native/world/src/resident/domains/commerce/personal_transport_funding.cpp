// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
#include "personal_transport_funding.hpp"
#include "resident/domains/tasks/task_model.hpp"
#include "resident/domains/tasks/task_report_carriage.hpp"
#include "resident/domains/travel/spatial_carrier.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
[[noreturn]]void fail(const char*code,const StableId&id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
void take(Status s){if(!s.ok)throw Failure{std::move(s.error)};}
bool same(const SourceRef&a,const SourceRef&b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
bool same(const MomentOwnerSource&a,const MomentOwnerSource&b){return a.source_schema==b.source_schema&&same(a.source_ref,b.source_ref);}
int cmp(const WorldMoment&a,const WorldMoment&b){return take(compare_world_moments(a,b));}
SourceRef dispatched(const StableId&id){return {"tasks","task_report_carriage_dispatched",id};}
SourceRef expense(const StableId&id){return {"travel","movement_expense",id};}
std::size_t position(const CommonStore&c,const SourceRef&s){const auto*h=c.state().facts.find(s.id);const auto&order=c.state().facts.order();auto p=std::find(order.begin(),order.end(),s.id);if(!h||h->id!=s.id||h->domain!=s.domain||h->kind!=s.kind||p==order.end())fail("PersonalFundingActualSourceRequired",s.id);return static_cast<std::size_t>(p-order.begin());}
void context(const TaskPersonalMovementFundingContext&x){if(!x.movement_actions.bound_to(x.funding.common,x.movement)||x.funding.state.schema_version!=1)fail("PersonalFundingSameWorldRequired");}
void destination(const TaskPersonalMovementFundingContext&x,const StableId&site,const MovementAnchor&actual){
 const auto expected=take(read_original_spatial_site_anchor(x.common_catalog,x.travel_catalog,{"common_catalog","place",site}));
 if(!take(same_actual_movement_position(expected,actual,x.arithmetic_budget)))fail("PersonalFundingActualDestinationMismatch",site);
}
void spending_title(const TaskPersonalMovementFundingContext&x,const SelfTransportFundingTerms&t,const SourceRef*paid){
 const auto actual=paid?take(x.funding.titles.read_private_wallet_title_before_source(t.wallet_id,*paid)):take(x.funding.titles.read_current_private_wallet_title(t.wallet_id));
 if(actual.wallet_id!=t.wallet_id||actual.legal_owner.extra||actual.legal_owner.kind!=Owner::Kind::actor||actual.legal_owner.id!=t.principal_actor_id)fail("PersonalFundingActualWalletPermissionRequired",t.id);
}
// The source-local S3 reader supplies only its own actual acceptance/dispatch.
// Its complete owner validator remains responsible for knowledge and consent.
void retained_dispatch_headers(const TaskPersonalMovementFundingContext&x){
 const auto*state=x.funding.tasks.transport_agreements?&*x.funding.tasks.transport_agreements:nullptr;
 if(state){
  if(state->version!=1&&state->version!=2)fail("UnsupportedTaskTransportAgreementVersion");
  if(state->version==1&&(!state->report_carriages.empty()||!state->report_handover_sources.empty()))fail("TaskReportTransportPhysicalMismatch");
  for(const auto&row:state->report_carriages.rows())if(row.second.dispatch){
   const auto&d=*row.second.dispatch;if(row.first!=row.second.agreement_id||d.action_id.empty())fail("PersonalFundingDispatchIdentityMismatch",row.first);
   position(x.funding.common,dispatched(d.id));
   for(const auto&other:state->report_carriages.rows())if(other.first!=row.first&&other.second.dispatch&&(other.second.dispatch->id==d.id||other.second.dispatch->action_id==d.action_id))fail("DuplicatePersonalFundingDispatch",d.id);
  }
 }
 for(const auto&row:x.funding.common.state().facts.rows())if(row.second.domain=="tasks"&&row.second.kind=="task_report_carriage_dispatched"){
  const TaskReportCarriage*found=nullptr;if(state)for(const auto&candidate:state->report_carriages.rows())if(candidate.second.dispatch&&candidate.second.dispatch->id==row.first){if(found)fail("DuplicatePersonalFundingDispatch",row.first);found=&candidate.second;}
  if(!found)fail("PersonalFundingDispatchPayloadRequired",row.first);
 }
}
const SelfTransportFundingTerms& funding(const TaskPersonalMovementFundingContext&x,const TaskCarrierAgreement&a){
 const auto*authority=std::get_if<SelfFundedExpenseAuthority>(&a.terms.expense_authority);if(!authority)fail("PersonalMovementOriginalFundingRouteRequired",a.id);
 const auto actual=take(read_self_transport_funding(x.funding,*authority));
 if(!actual.terms||!same(actual.terms->journey_or_plan_ref,{"tasks","task_carrier_agreement_accepted",a.id})||actual.carrier.kind!=SubjectKind::actor||a.terms.carrier_subject.kind!=SubjectKind::actor||actual.carrier.id!=a.terms.carrier_subject.id||actual.destination_site_id!=a.terms.destination_site_id)fail("PersonalFundingAcceptedScopeMismatch",a.id);
 return *actual.terms;
}
struct Binding {TaskReportTransportBindingView original;const SelfTransportFundingTerms* terms=nullptr;};
std::optional<Binding> binding(const TaskPersonalMovementFundingContext&x,const StableId&action){
 context(x);retained_dispatch_headers(x);auto actual=take(read_original_task_report_transport_binding(x.funding.common,x.funding.tasks,action));if(!actual)return std::nullopt;
 if(!actual->agreement||!actual->carriage||!actual->dispatch)fail("PersonalFundingActualDispatchRequired",action);
 const auto&t=funding(x,*actual->agreement);const auto&d=*actual->dispatch;
 const auto departure=take(x.movement_actions.read_original_execution_destination(action));
 const auto adopted_funding=take(x.movement_actions.read_original_execution_funding_plan(action));
 if(!adopted_funding||adopted_funding->source_schema!=1||!same(adopted_funding->source_ref,t.journey_or_plan_ref))fail("PersonalFundingOriginalPlanBindingMismatch",action);
 if(departure.action_id!=action||departure.actor_id!=actual->carriage->terms.carrier_actor_id||departure.actor_id!=actual->agreement->terms.carrier_subject.id||!same(departure.execution_source,d.original_execution_source)||cmp(departure.started_at,t.valid_from)<0||cmp(departure.started_at,d.sent_at)>0)fail("PersonalFundingOriginalDepartureMismatch",action);
 take(x.movement_actions.validate_original_execution_common_order(action,t.journey_or_plan_ref,dispatched(d.id)));
 if(position(x.funding.common,t.acceptance_ref)>=position(x.funding.common,departure.execution_source.source_ref))fail("PersonalFundingNotAcceptedAtDeparture",action);
 destination(x,actual->agreement->terms.destination_site_id,departure.actual_destination_anchor);
 return Binding{*actual,&t};
}
void fee_scope(const TaskPersonalMovementFundingContext&x,const Binding&b,const StableId&actor,const StableId&payer,const WorldMoment&at,const SourceRef*paid){
 const auto&a=*b.original.agreement;const auto&d=*b.original.dispatch;const auto&t=*b.terms;
 if(actor!=a.terms.carrier_subject.id||actor!=b.original.carriage->terms.carrier_actor_id||payer!=t.wallet_id||cmp(at,t.valid_from)<0||cmp(at,d.sent_at)<0||cmp(at,take(x.funding.clock.read_current_moment()))>0)fail("PersonalFundingActualFeeScopeMismatch",a.id);
 // MC1 currently retains no distinct accepted safe-return execution. The
 // continuity tag alone cannot authorize a new fee beyond its real window.
 if(cmp(at,t.until)>=0)fail("PersonalFundingCommittedSafeReturnSourceRequired",a.id);
 if(paid&&position(x.funding.common,dispatched(d.id))>=position(x.funding.common,*paid))fail("PersonalFundingChargeBeforeDispatch",paid->id);
 if(a.stopped_source){if(!paid||position(x.funding.common,*a.stopped_source)<=position(x.funding.common,*paid))fail("PersonalFundingCommittedSafeReturnSourceRequired",a.id);}
 spending_title(x,t,paid);
}
Integer add(Integer a,Integer b){if(a<0||b<0||a>std::numeric_limits<Integer>::max()-b)fail("PersonalFundingGrossOverflow");return a+b;}
void original_debit(const TaskPersonalMovementFundingContext&x,const MovementExpenseSource&e){
 const auto*unit=x.movement.units.find(e.unit_ref);const auto*paid=unit?std::get_if<MovementPaymentConsumed>(&unit->payment):nullptr;
 const auto*execution=std::get_if<PersonalMovementExecution>(&e.execution);const auto*unit_execution=unit?std::get_if<PersonalMovementExecution>(&unit->execution):nullptr;
 if(!execution||!unit_execution||execution->action_id!=unit_execution->action_id||execution->journey_id!=unit_execution->journey_id||execution->segment_ordinal!=unit_execution->segment_ordinal||!paid||!same(paid->actual_expense_source,expense(e.id))||paid->payer_wallet_id!=e.payer_wallet_id||paid->amount!=e.amount)fail("PersonalFundingActualDebitRequired",e.id);
 take(validate_personal_movement_recorded_cost(x.funding.common,x.movement,x.travel_catalog,e,x.party_history,x.arithmetic_budget));
}
Integer gross(const TaskPersonalMovementFundingContext&x,const Binding&b){
 Integer total=0;const auto&action=b.original.dispatch->action_id;
 for(const auto&row:x.movement.expenses.rows()){
  const auto&e=row.second;const auto*execution=std::get_if<PersonalMovementExecution>(&e.execution);if(!execution||execution->action_id!=action)continue;
  if(row.first!=e.id)fail("PersonalFundingExpenseKeyMismatch",row.first);original_debit(x,e);
  const auto source=expense(e.id);fee_scope(x,b,b.original.carriage->terms.carrier_actor_id,e.payer_wallet_id,e.occurred_at,&source);total=add(total,e.amount);
 }
 if(total>b.terms->gross_cap)fail("SelfFundingGrossCapExceeded",b.terms->id);
 return total;
}
}
Outcome<AcceptedTaskPersonalMovementFunding> read_accepted_task_personal_movement_funding(const TaskPersonalMovementFundingContext&x,const SourceRef&plan,const StableId&actor,const MovementAnchor&target){try{
 context(x);take(validate_self_transport_fundings(x.funding));
 const auto original=take(read_original_task_carrier_acceptance(x.funding.common,x.funding.tasks,plan,1));
 if(!original.agreement||!original.report_delivery||original.agreement->terms.carrier_subject.kind!=SubjectKind::actor||original.agreement->terms.carrier_subject.id!=actor||original.report_delivery->carrier_actor_id!=actor)fail("PersonalFundingAcceptedCarrierRequired",plan.id);
 const auto&a=*original.agreement;const auto&t=funding(x,a);const auto now=take(x.funding.clock.read_current_moment());
 if(a.stopped_source||cmp(now,t.valid_from)<0||cmp(now,t.until)>=0)fail("PersonalFundingNewDepartureOutsideWindow",t.id);
 const auto*state=x.funding.tasks.transport_agreements?&*x.funding.tasks.transport_agreements:nullptr;const auto*carriage=state?state->report_carriages.find(a.id):nullptr;
 if(!carriage||carriage->dispatch)fail("PersonalFundingNewDepartureAlreadyDispatched",a.id);
 destination(x,a.terms.destination_site_id,target);
 spending_title(x,t,nullptr);
 // Actual spendable amount remains the original S1 planner/WalletDelta gate.
 return {true,{plan,t.acceptance_ref,t.wallet_id,t.valid_from,now,t.until,&t},{}};
}catch(Failure&e){return {false,{},std::move(e.error)};}}
bool NativeTaskPersonalMovementFundingRead::bound_to(const CommonRead&common,const MovementExactState&movement)const noexcept{return &x_.funding.common==&common&&&x_.movement==&movement&&x_.movement_actions.bound_to(common,movement);}
Status NativeTaskPersonalMovementFundingRead::validate_original_personal_execution_funding(const StableId&action,const std::optional<MomentOwnerSource>&supplied)const{try{
 context(x_);const auto actual=take(x_.movement_actions.read_original_execution_funding_plan(action));
 if(actual.has_value()!=supplied.has_value()||(actual&&!same(*actual,*supplied)))fail("PersonalFundingOpeningPayloadMismatch",action);
 const auto adopted=binding(x_,action);
 if(actual){if(actual->source_schema!=1||!adopted||!same(actual->source_ref,adopted->terms->journey_or_plan_ref))fail("PersonalFundingDispatchCoverageRequired",action);}
 else if(adopted)fail("PersonalFundingOriginalPlanBindingMismatch",action);
 return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Status NativeTaskPersonalMovementFundingRead::validate_new_personal_unit_cost(const PersonalMovementUnitCost&supplied)const{try{
 context(x_);const auto actual=take(read_unconsumed_personal_unit_cost(x_.funding.common,x_.travel,x_.movement,x_.travel_catalog,x_.travel_services,{supplied.prefix_id,supplied.actual_start},x_.funding.clock,x_.arithmetic_budget));
 if(actual.action_id!=supplied.action_id||actual.actor_id!=supplied.actor_id||actual.unit_ref!=supplied.unit_ref||actual.prefix_id!=supplied.prefix_id||actual.payer_wallet_id!=supplied.payer_wallet_id||actual.amount!=supplied.amount||actual.original_cost_rule_ref!=supplied.original_cost_rule_ref||!same(actual.unit_basis,supplied.unit_basis)||cmp(actual.occurred_at,supplied.occurred_at))fail("PersonalFundingUnverifiedCostInput",supplied.prefix_id);
 const auto original_plan=take(x_.movement_actions.read_original_execution_funding_plan(actual.action_id));take(validate_original_personal_execution_funding(actual.action_id,original_plan));
 const auto adopted=binding(x_,actual.action_id);if(!adopted)return common_success();
 fee_scope(x_,*adopted,actual.actor_id,actual.payer_wallet_id,actual.occurred_at,nullptr);
 const auto consumed=gross(x_,*adopted);if(actual.amount>adopted->terms->gross_cap-consumed)fail("SelfFundingGrossCapExceeded",adopted->terms->id);
 return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Status NativeTaskPersonalMovementFundingRead::validate_recorded_personal_unit_cost(const MovementExpenseSource&e)const{try{
 context(x_);const auto*execution=std::get_if<PersonalMovementExecution>(&e.execution);if(!execution||x_.movement.expenses.find(e.id)!=&e)fail("PersonalFundingOriginalExpenseRequired",e.id);
 const auto original_plan=take(x_.movement_actions.read_original_execution_funding_plan(execution->action_id));take(validate_original_personal_execution_funding(execution->action_id,original_plan));
 const auto adopted=binding(x_,execution->action_id);if(!adopted)return common_success();original_debit(x_,e);const auto source=expense(e.id);
 fee_scope(x_,*adopted,adopted->original.carriage->terms.carrier_actor_id,e.payer_wallet_id,e.occurred_at,&source);gross(x_,*adopted);return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Status validate_task_personal_movement_funding(const TaskPersonalMovementFundingContext&x){try{
 context(x);take(validate_self_transport_fundings(x.funding));retained_dispatch_headers(x);
 // 逐原首开段反查，包括尚未付款的执行，拒绝孤立资金采用及漏尾。
 NativeTaskPersonalMovementFundingRead reader(x);
 for(const auto&row:x.movement.exertion_bases.rows()){
  const auto&basis=row.second;const auto*execution=std::get_if<PersonalMovementExecution>(&basis.execution);
  if(!execution)continue;const auto*unit=x.movement.units.find(basis.unit_ref);
  if(!unit)fail("PersonalFundingOriginalUnitRequired",basis.id);
  if(unit->ordinal!=0||execution->segment_ordinal!=0)continue;
  const auto actual=take(x.movement_actions.read_original_execution_funding_plan(execution->action_id));
  take(reader.validate_original_personal_execution_funding(execution->action_id,actual));
 }
 if(x.funding.tasks.transport_agreements)for(const auto&row:x.funding.tasks.transport_agreements->report_carriages.rows())if(row.second.dispatch){auto actual=binding(x,row.second.dispatch->action_id);if(!actual)fail("PersonalFundingDispatchCoverageMismatch",row.first);gross(x,*actual);}
 return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}
}
