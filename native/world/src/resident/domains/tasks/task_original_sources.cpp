#include "task_original_sources.hpp"
#include "task_exact_execution.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include <algorithm>
#include <string_view>
#include "personal_messages/task_personal_message_catalog.hpp"
#include "personal_acceptances/task_personal_acceptance_catalog.hpp"
#include "personal_reports/task_personal_report_catalog.hpp"
namespace hanmo::resident {namespace {
bool same(const SourceRef&a,const SourceRef&b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
template<class T>Outcome<T> bad(const char*code,const StableId&id){return {false,{},common_failure(code,id).error};}
// Borrowed from the same immutable original row throughout this read.
struct OriginalTaskIdentity {std::string_view action_id,task_id;Integer tick=0;bool terminal=false;};
Outcome<OriginalTaskIdentity> original(const TaskState&s,const SourceRef&r){
 if(r.domain==task_personal_message_module&&s.personal_messages){if(r.kind==task_personal_message_started_kind){const auto*v=s.personal_messages->starts.find(r.id);if(v)return {true,{v->action_id,v->task_id,v->started_tick,false},{}};}else if(r.kind==task_personal_message_delivered_kind||r.kind==task_personal_message_interrupted_kind){const auto*v=s.personal_messages->endings.find(r.id);if(v&&v->success==(r.kind==task_personal_message_delivered_kind))return {true,{v->action_id,v->task_id,v->tick,true},{}};}}
 if(r.domain==task_personal_acceptance_module&&s.personal_acceptances){if(r.kind==task_personal_acceptance_started_kind){const auto*v=s.personal_acceptances->starts.find(r.id);if(v)return {true,{v->action_id,v->task_id,v->started_tick,false},{}};}else if(r.kind==task_personal_acceptance_completed_kind||r.kind==task_personal_acceptance_interrupted_kind){const auto*v=s.personal_acceptances->endings.find(r.id);if(v&&v->success==(r.kind==task_personal_acceptance_completed_kind))return {true,{v->action_id,v->task_id,v->tick,true},{}};}}
 if(r.domain==task_personal_report_module&&s.personal_reports){if(r.kind==task_personal_report_started_kind){const auto*v=s.personal_reports->starts.find(r.id);if(v)return {true,{v->action_id,v->task_id,v->started_tick,false},{}};}else if(r.kind==task_personal_report_completed_kind||r.kind==task_personal_report_interrupted_kind){const auto*v=s.personal_reports->endings.find(r.id);if(v&&v->success==(r.kind==task_personal_report_completed_kind))return {true,{v->action_id,v->task_id,v->tick,true},{}};}}
 if(r.domain==task_message_return_module&&s.message_returns){const auto&v=*s.message_returns;if(r.kind==task_message_return_report_started_kind){const auto*x=v.report_starts.find(r.id);if(x)return {true,{x->action_id,x->task_id,x->started_tick,false},{}};}if(r.kind==task_message_return_report_interrupted_kind){const auto*x=v.report_interruptions.find(r.id);if(x)return {true,{x->action_id,x->task_id,x->tick,true},{}};}if(r.kind==task_message_return_delivered_kind){const auto*x=v.deliveries.find(r.id);if(x)return {true,{x->source_action_id,x->task_id,x->tick,true},{}};}}
 if(r.domain=="tasks"){
  if(r.kind==task_message_handoff_kind&&s.message_handoffs){const auto*v=s.message_handoffs->receipts.find(r.id);if(v)return {true,{v->source_action_id,v->task_id,v->tick,true},{}};}
  if(r.kind==task_message_report_kind&&s.message_handoffs){const auto*v=s.message_handoffs->reports.find(r.id);if(v)return {true,{v->source_action_id,v->task_id,v->tick,true},{}};}
  if(r.kind==task_static_asset_fact_kind&&s.static_assets){const auto*v=s.static_assets->receipts.find(r.id);if(v&&!v->source_action_id.empty())return {true,{v->source_action_id,v->task_id,v->tick,true},{}};}
  if(r.kind==task_static_payment_fact_kind&&s.takeover_execution){const auto*v=s.takeover_execution->static_payments.find(r.id);if(v&&!v->source_action_id.empty())return {true,{v->source_action_id,v->task_id,v->tick,true},{}};}
  if(r.kind==task_contract_payment_fact_kind&&s.takeover_execution){const auto*v=s.takeover_execution->payments.find(r.id);if(v)return {true,{{},v->task_id,v->tick,true},{}};}
  if(r.kind==task_death_notice_bound_kind&&s.message_notices){const auto*v=s.message_notices->bindings.find(r.id);if(v)return {true,{v->accept_action_id,v->task_id,v->tick,true},{}};}
  if(r.kind=="task_transfer"){const auto*v=s.transfers.find(r.id);if(v)return {true,{v->source_action_id,v->task_id,v->completed_tick,true},{}};}
  if(r.kind=="task_delivery"){const auto*v=s.deliveries.find(r.id);if(v)return {true,{v->source_action_id,v->task_id,v->completed_tick,true},{}};}
  if(r.kind=="task_claim_established"&&s.claim_establishment){const auto*v=s.claim_establishment->claim_establishment_sources.find(r.id);if(v)return {true,{v->acceptance_action_id,v->task_id,v->established_at.half_day_tick,false},{}};}
 }
 return bad<OriginalTaskIdentity>("TaskOriginalReceiptPayloadRequired",r.id);
}
}
Status validate_task_original_applied_source(const CommonStore&c,const TaskState&s,const SourceRef&r,const TaskExactStartSource&start,bool terminal,const WorldMoment&at){
 const auto*h=c.state().facts.find(r.id);if(!h||h->id!=r.id||h->domain!=r.domain||h->kind!=r.kind)return common_failure("TaskOriginalReceiptHeaderMismatch",r.id);
 auto begin=read_task_exact_source_moment(c,s,{"tasks","task_action_started",start.id},1);if(!begin.ok)return {false,{},begin.error};
 if(s.exact_execution->starts.find(start.id)!=&start)return common_failure("TaskOriginalAppliedStartCandidateMismatch",r.id,ErrorKind::internal);
 const auto*start_header=c.state().facts.find(start.id);
 if(!start_header)return common_failure("TaskOriginalAppliedCommonOrderMissing",r.id,ErrorKind::internal);
 auto source_at=read_actual_common_fact_position(c,*h);if(!source_at.ok)return {false,{},source_at.error};
 auto start_at=read_actual_common_fact_position(c,*start_header);if(!start_at.ok)return {false,{},start_at.error};
 const WorldMoment*owner_at=&start.started_at;
 if(terminal){
  const TaskExactTerminalSource*end=nullptr;for(const auto&row:s.exact_execution->terminals.rows())if(row.second.action_id==start.action_id){if(end||row.first!=row.second.id)return common_failure("TaskOriginalAppliedTerminalAmbiguous",r.id,ErrorKind::internal);end=&row.second;}
  if(!end||end->start_source_id!=start.id)return common_failure("TaskOriginalAppliedTerminalRequired",r.id,ErrorKind::internal);
  auto finish=read_task_exact_source_moment(c,s,{"tasks","task_action_finished",end->id},task_exact_terminal_source_schema(s.exact_execution->version));if(!finish.ok)return {false,{},finish.error};
  const auto*end_header=c.state().facts.find(end->id);if(!end_header)return common_failure("TaskOriginalAppliedCommonOrderMissing",r.id,ErrorKind::internal);
  auto end_at=read_actual_common_fact_position(c,*end_header);if(!end_at.ok)return {false,{},end_at.error};
  if(start_at.value>=source_at.value||source_at.value>=end_at.value)return common_failure("TaskOriginalResultSourceCausalOrderMismatch",r.id,ErrorKind::internal);owner_at=&end->finished_at;
 }else if(source_at.value>=start_at.value)return common_failure("TaskOriginalStartSourceCausalOrderMismatch",r.id,ErrorKind::internal);
 auto applied_at=compare_world_moments(*owner_at,at);if(!applied_at.ok)return {false,{},applied_at.error};if(applied_at.value)return common_failure("TaskOriginalAppliedOwnerMomentMismatch",r.id,ErrorKind::internal);
 auto value=original(s,r);if(!value.ok)return {false,{},value.error};
 const WorldMoment*actual=nullptr;
 if(r.domain=="tasks"&&r.kind=="task_transfer"){const auto*row=s.transfers.find(r.id);if(!row||row->quantity_source_kind!=TaskQuantitySourceKind::procurement_exact||!row->completed_at)return common_failure("ExactQuantityTransferTimeSourceRequired",r.id,ErrorKind::unsupported_capability);actual=&*row->completed_at;}
 if(r.domain=="tasks"&&r.kind=="task_delivery"){const auto*row=s.deliveries.find(r.id);if(!row||row->quantity_source_kind!=TaskQuantitySourceKind::procurement_exact||!row->completed_at)return common_failure("ExactQuantityDeliveryTimeSourceRequired",r.id,ErrorKind::unsupported_capability);actual=&*row->completed_at;}
 if(r.domain=="tasks"&&r.kind=="task_claim_established"&&s.claim_establishment){const auto*row=s.claim_establishment->claim_establishment_sources.find(r.id);if(row)actual=&row->established_at;}
 if(actual){auto equal=compare_world_moments(*actual,at);if(!equal.ok)return {false,{},equal.error};if(equal.value)return common_failure("TaskOriginalExactAppliedMomentMismatch",r.id);}
 if((!value.value.action_id.empty()&&value.value.action_id!=std::string_view(start.action_id))||value.value.task_id!=std::string_view(start.accepted_action.task_id)||value.value.tick!=at.half_day_tick||value.value.terminal!=terminal)return common_failure("TaskOriginalReceiptBindingMismatch",r.id);return common_success();}
Outcome<TaskOriginalAppliedTime> read_task_original_applied_time(const CommonStore&c,const TaskState&s,const SourceRef&r){
 if(!s.exact_execution)return bad<TaskOriginalAppliedTime>("TaskExactExecutionRequired",r.id);
 std::optional<TaskOriginalAppliedTime>out;
 for(const auto&row:s.exact_execution->starts.rows())for(const auto&ref:row.second.actual_start_sources)if(same(ref,r)){
  if(out)return bad<TaskOriginalAppliedTime>("TaskOriginalSourceAppliedTwice",r.id);
  const SourceRef applied{"tasks","task_action_started",row.first};auto moment=read_task_exact_source_moment(c,s,applied,1);if(!moment.ok)return {false,{},moment.error};
  auto valid=validate_task_original_applied_source(c,s,r,row.second,false,moment.value);if(!valid.ok)return {false,{},valid.error};
  out=TaskOriginalAppliedTime{{applied,1},std::move(moment.value)};
 }
 for(const auto&row:s.exact_execution->terminals.rows())for(const auto&ref:row.second.actual_result_sources)if(same(ref,r)){
  if(out)return bad<TaskOriginalAppliedTime>("TaskOriginalSourceAppliedTwice",r.id);
  const SourceRef applied{"tasks","task_action_finished",row.first};auto moment=read_task_exact_source_moment(c,s,applied,task_exact_terminal_source_schema(s.exact_execution->version));if(!moment.ok)return {false,{},moment.error};
  const auto*start=s.exact_execution->starts.find(row.second.start_source_id);if(!start)return bad<TaskOriginalAppliedTime>("TaskExactStartMissing",r.id);
  auto valid=validate_task_original_applied_source(c,s,r,*start,true,moment.value);if(!valid.ok)return {false,{},valid.error};
  out=TaskOriginalAppliedTime{{applied,task_exact_terminal_source_schema(s.exact_execution->version)},std::move(moment.value)};
 }
 return out?Outcome<TaskOriginalAppliedTime>{true,std::move(*out),{}}:bad<TaskOriginalAppliedTime>("TaskOriginalAppliedMomentMissing",r.id);
}
Outcome<ExactOwnerFactRead> TaskOriginalResultOwnerRead::read_selected_fact(const FactRecord&h)const{auto at=read_task_original_applied_time(common_,tasks_,{h.domain,h.kind,h.id});if(!at.ok)return {false,{},at.error};ExactOwnerFactRead out;out.disposition=ExactOwnerFactRead::Disposition::no_dynamic_projection;out.supporting_sources.push_back(std::move(at.value.applied_source));return {true,std::move(out),{}};}
Outcome<WorldMoment> TaskOriginalResultOwnerRead::read_selected_source_moment(const MomentOwnerSource&r)const{if(r.source_schema!=1)return bad<WorldMoment>("TaskOriginalSourceSchemaMismatch",r.source_ref.id);auto at=read_task_original_applied_time(common_,tasks_,r.source_ref);return at.ok?Outcome<WorldMoment>{true,std::move(at.value.occurred_at),{}}:Outcome<WorldMoment>{false,{},at.error};}
}
