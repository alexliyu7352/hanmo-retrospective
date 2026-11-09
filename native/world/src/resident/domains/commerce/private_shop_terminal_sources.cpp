// Exact terminal provenance belongs to the original S7 result. Amounts remain
// in DayResult, actual cancellation in Common, and service credit only in S0.
#include "private_shop_operation_sources.hpp"
#include "private_shop_cached_receipt.hpp"
#include "private_shop_staff.hpp"
#include "household_retail.hpp"
#include "resident/kernel/common_command_sources.hpp"
#include "resident/kernel/command_types.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
[[noreturn]]void fail(const char*code,const StableId&id={}){throw Failure{common_failure(code,id).error};}
int cmp(const WorldMoment&a,const WorldMoment&b){return take(compare_world_moments(a,b));}
const PrivateShopOperationTerminalSource& original(const PrivateShopOperationSourceContext&x,const MomentOwnerSource&ref){
 if(ref.source_schema!=2||ref.source_ref.domain!=private_shop_module||
  (ref.source_ref.kind!="private_shop_operating_completed"&&ref.source_ref.kind!="private_shop_operating_interrupted"))fail("PrivateShopOriginalExactTerminalRequired",ref.source_ref.id);
 const PrivateShopOperationTerminalSource*found=nullptr;
 for(const auto&row:x.sources.terminal_sources)if(same_source_ref(row.original_result_ref,ref.source_ref)){if(found)fail("DuplicatePrivateShopTerminalSource",ref.source_ref.id);found=&row;}
 if(!found)fail("PrivateShopOriginalTerminalSourceMissing",ref.source_ref.id);
 const auto&a=*take(read_original_private_shop_operation_acceptance(x,{private_shop_operation_started_source(found->operation_ref),2}));
 const auto*o=x.shops.state.operations.find(found->operation_ref);const auto*r=x.shops.state.operating_results.find(ref.source_ref.id);
 const auto*shop=o?x.shops.state.shops.find(o->shop_id):nullptr;const auto*actor=o?x.shops.common.actor(o->actor_id):nullptr;
 const auto serial=organization_instance_sequence(ref.source_ref.id,"private_shop_day");
 if(!o||!r||!shop||!actor||!serial||*serial>=x.shops.common.state().next_instance_id||o->result_fact_id!=r->id||r->id!=ref.source_ref.id||
  r->operation_id!=o->id||r->shop_id!=o->shop_id||r->actor_id!=o->actor_id||r->item_id!=o->item_id||!o->attendance_ticks.empty()||
  (r->completed?"private_shop_operating_completed":"private_shop_operating_interrupted")!=ref.source_ref.kind||
  r->tick!=found->occurred_at.half_day_tick||cmp(found->occurred_at,a.accepted_at)<0||cmp(found->occurred_at,take(x.clock.read_current_moment()))>0||
  x.shops.common.action(o->id)||actor->action_id==o->id)fail("PrivateShopExactTerminalPayloadMismatch",ref.source_ref.id);
 for(const auto&row:x.sources.terminal_sources)if(row.operation_ref==o->id&& &row!=found)fail("DuplicatePrivateShopOperationTerminal",o->id);
 const auto position=take(read_actual_common_fact_position(x.shops.common,ref.source_ref));
 if(take(read_actual_common_fact_position(x.shops.common,private_shop_operation_started_source(o->id)))>=position)fail("PrivateShopTerminalBeforeStart",r->id);
 if(r->completed){
  if(!r->reason.empty()||found->actual_cause_ref.source_schema!=1)fail("PrivateShopCompletedTerminalCauseMismatch",r->id);
  const auto&labor=*take(read_original_private_shop_labor(x,found->actual_cause_ref));
  if(labor.operation_ref!=o->id||labor.work_period_ref.period_ordinal!=1||cmp(labor.occurred_at,found->occurred_at)||
   take(read_actual_common_fact_position(x.shops.common,found->actual_cause_ref.source_ref))>=position)fail("PrivateShopActualFinalLaborRequired",r->id);
 }else if(r->reason=="cancelled"){
  if(found->actual_cause_ref.source_schema!=1)fail("PrivateShopActualCancelCommandRequired",r->id);
  const auto command=take(read_common_command_source(x.shops.common,found->actual_cause_ref.source_ref));
  const auto*effect=command.result->details?std::get_if<InteractionEffect>(&command.result->details->data):nullptr;
  if(command.result->kind!="private_shop_cancel_operation"||command.history->actor_id!=o->actor_id||command.result->tick!=r->tick||
   !effect||effect->settlement_result!=r->id||!effect->encounter_id.empty()||effect->party_id||effect->person_id||effect->news_ids)
   fail("PrivateShopCancelOriginalReceiptMismatch",r->id);
  // This is the original allocation/history validator, not a guessed command
  // identity or a current wallet/actor flag standing in for prior permission.
  take(validate_private_shop_cached_results(x.shops.common,&x.shops.state));
 }else if(r->reason=="PrivateShopStaffTerminated"){
  if(found->actual_cause_ref.source_schema!=1)fail("PrivateShopActualStaffTerminationRequired",r->id);
  const auto staff=take(read_private_shop_exact_staff_terminal(x,found->actual_cause_ref.source_ref));
  if(staff.contract->id!=o->staff_contract_id||cmp(staff.occurred_at,found->occurred_at)||take(read_actual_common_fact_position(x.shops.common,found->actual_cause_ref.source_ref))>=position)fail("PrivateShopActualStaffTerminationMismatch",r->id);
 }else fail("PrivateShopOriginalExactStaffTerminalRequired",r->id);
 if(!household_retail_operation_selected(x.shops,o->id))fail("PrivateShopExactOriginalRetailSelectionRequired",o->id);
 take(validate_household_retail_operation_result(x.shops,*o,*r));
 if(r->rent_paid!=(x.shops.daily_rent?0:shop->terms.daily_rent)||r->wage_paid<0)fail("PrivateShopTerminalOriginalExpenseMismatch",r->id);
 Integer wages=0;for(const auto&row:x.shops.state.staff.services.rows())if(row.second.operation_id==o->id){
  if(row.second.amount<0||__builtin_add_overflow(wages,row.second.amount,&wages)||take(read_actual_common_fact_position(x.shops.common,{private_shop_module,"private_shop_staff_served",row.first}))>=position)fail("PrivateShopTerminalWageHistoryMismatch",r->id);
 }
 if(wages!=r->wage_paid||(o->staff_contract_id.empty()&&wages))fail("PrivateShopTerminalOriginalWageMismatch",r->id);
 take(validate_private_shop_staff_services(x.shops));
 Integer expenses=0,net=0;if(__builtin_add_overflow(r->cost_of_goods,r->rent_paid,&expenses)||__builtin_add_overflow(expenses,r->wage_paid,&expenses)||__builtin_sub_overflow(r->income,expenses,&net)||net!=r->net_income)fail("PrivateShopTerminalNetMismatch",r->id);
 return *found;
}
}
Outcome<const PrivateShopOperationTerminalSource*> read_original_private_shop_operation_terminal(const PrivateShopOperationSourceContext&x,const MomentOwnerSource&ref){
 try{return {true,&original(x,ref),{}};}catch(Failure&e){return {false,{},std::move(e.error)};}
}
Status visit_private_shop_operation_terminal_pins(const PrivateShopOperationSourceContext&x,const MomentOwnerSource&ref,PrivateShopOperationSourcePinSink&sink){
 try{const auto&row=original(x,ref);take(sink.source({private_shop_operation_started_source(row.operation_ref),2}));take(sink.source(row.actual_cause_ref));return common_success();}
 catch(Failure&e){return {false,{},std::move(e.error)};}
}
}
