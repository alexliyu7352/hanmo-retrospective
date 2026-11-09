#include "resident/kernel/common_fact_prefix.hpp"
#include "task_quantity_time2.hpp"
#include "task_exact_execution.hpp"
#include "task_original_sources.hpp"
#include "resident/kernel/common_command_sources.hpp"
#include "resident/kernel/command_types.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {namespace {
template<class T>Outcome<T> bad(const char*code,const StableId&id={}){return {false,{},common_failure(code,id,ErrorKind::internal).error};}
bool same(const SourceRef&a,const SourceRef&b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
bool same(const TaskEffect&a,const TaskEffect&b){return a.success==b.success&&a.task_id==b.task_id&&a.code==b.code&&a.on_unavailable==b.on_unavailable&&a.retry==b.retry;}
Status at(const WorldMoment&a,const WorldMoment&b,const StableId&id){auto value=compare_world_moments(a,b);return !value.ok?Status{false,{},value.error}:value.value?common_failure("ExactQuantityMomentMismatch",id):common_success();}
Status header(const CommonStore&c,const StableId&id,const char*kind){const auto*h=c.state().facts.find(id);return h&&h->id==id&&h->domain=="tasks"&&h->kind==kind?common_success():common_failure("ExactQuantityOriginalHeaderRequired",id);}
Status before(const CommonStore&c,const StableId&a,const StableId&b){const auto*first=c.state().facts.find(a);const auto*last=c.state().facts.find(b);if(!first||!last)return common_failure("ExactQuantityOriginalOrderMismatch",b);auto x=read_actual_common_fact_position(c,*first);if(!x.ok)return {false,{},x.error};auto y=read_actual_common_fact_position(c,*last);if(!y.ok)return {false,{},y.error};return x.value<y.value?common_success():common_failure("ExactQuantityOriginalOrderMismatch",b);}
std::size_t references(const TaskExactTerminalSource&t,const SourceRef&r){std::size_t count=0;for(const auto&value:t.actual_result_sources)if(same(value,r))++count;return count;}
const TaskExactTerminalSource* latest_report(const CommonStore&c,const TaskState&t,const StableId&task){
 const TaskExactTerminalSource*out=nullptr;if(!t.exact_execution)return out;
 for(const auto&id:c.state().facts.order())if(const auto*end=t.exact_execution->terminals.find(id)){const auto*start=t.exact_execution->starts.find(end->start_source_id);if(start&&start->accepted_action.task_id==task&&start->action_kind=="report_offer"&&end->effect.success)out=end;}return out;
}
Outcome<const TaskExactStartSource*> acceptance(const CommonStore&c,const TaskState&t,const TaskRecord&task){
 if(!t.exact_quantities||!t.exact_execution||task.record_kind!=TaskRecordKind::exact_quantity||!task.exact_quantity||!task.contract_id||!task.offer_ref)return bad<const TaskExactStartSource*>("ExactQuantityTaskSourceRequired",task.id);
 const auto&q=*task.exact_quantity;const auto*contract=t.contracts.find(*task.contract_id);const TaskExactStartSource*start=nullptr;
 for(const auto&row:t.exact_execution->starts.rows())if(row.second.action_kind=="accept_offer"&&row.second.accepted_action.task_id==task.id){if(start||row.first!=row.second.id)return bad<const TaskExactStartSource*>("DuplicateExactQuantityAcceptanceStart",task.id);start=&row.second;}
 if(!contract||!start||start->action_id!=q.original_acceptance_action_ref||start->actor_id!=task.assignee||start->accepted_action.offer_id!=task.offer_id||start->required_half_days!=contract->frozen_terms.definition.accept_ticks||start->required_half_days<=0||!contract->executor_actor_id||*contract->executor_actor_id!=task.assignee||q.assignee.kind!=contract->assignee.kind||q.assignee.id!=contract->assignee.id||q.deadline_gate.tick!=contract->frozen_terms.deadline_tick||!q.deadline_gate.inclusive||task.deadline_tick!=q.deadline_gate.tick||task.accepted_tick!=0||task.objective_completed_tick||task.reported_tick)return bad<const TaskExactStartSource*>("ExactQuantityOriginalAcceptanceMismatch",task.id);
 auto h=header(c,start->id,"task_action_started");if(!h.ok)return {false,{},h.error};auto moment=compare_world_moments(start->started_at,start->started_at);if(!moment.ok)return {false,{},moment.error};return {true,start,{}};
}
Status actual_quantity_outcome(const CommonStore&c,const TaskState&t,const TaskRecord&task,const TaskExactLifeSources*life){
 const auto&q=*task.exact_quantity;const auto&contract=*t.contracts.find(*task.contract_id);Integer delivered=0;const DeliveryFact*completed=nullptr;
 for(const auto&id:c.state().facts.order())if(const auto*part=t.deliveries.find(id);part&&part->task_id==task.id){if(part->quantity_source_kind!=TaskQuantitySourceKind::procurement_exact||!part->completed_at||part->quantity<=0||delivered>std::numeric_limits<Integer>::max()-part->quantity)return common_failure("ExactQuantityCompletionPartsInvalid",id);delivered+=part->quantity;if(delivered>contract.frozen_terms.agreed_quantity)return common_failure("ExactQuantityCompletionExceedsTerms",id);if(delivered==contract.frozen_terms.agreed_quantity)completed=part;}
 if(delivered!=contract.delivered_quantity||bool(completed)!=q.objective_completed_at.has_value())return common_failure("ExactQuantityObjectiveCompletionMismatch",task.id);
 if(completed){auto s=at(*q.objective_completed_at,*completed->completed_at,task.id);if(!s.ok)return s;}
 if(q.reported_at.has_value()!=task.late.has_value())return common_failure("ExactQuantityReportLateGateMissing",task.id);
 if(q.reported_at){const auto*last=latest_report(c,t,task.id);if(!last)return common_failure("ExactQuantityReportResultRequired",task.id);auto gate=read_task_quantity_result_deadline_eligibility(c,t,task.id,{"tasks","task_action_finished",last->id},life);if(!gate.ok)return {false,{},gate.error};if(*task.late==gate.value)return common_failure("ExactQuantityReportLateGateMismatch",task.id);}
 return common_success();
}
Outcome<ExactQuantityActionCompletion> original(const CommonStore&c,const TaskState&t,const StableId&action){
 if(!t.exact_quantities||!t.exact_execution)return bad<ExactQuantityActionCompletion>("ExactQuantityExecutionSourcesRequired",action);
 ExactQuantityActionCompletion out;for(const auto&row:t.exact_execution->starts.rows())if(row.second.action_id==action){if(out.start||row.first!=row.second.id)return bad<ExactQuantityActionCompletion>("DuplicateExactQuantityStart",action);out.start=&row.second;}
 for(const auto&row:t.exact_execution->terminals.rows())if(row.second.action_id==action){if(out.terminal||row.first!=row.second.id)return bad<ExactQuantityActionCompletion>("DuplicateExactQuantityTerminal",action);out.terminal=&row.second;}
 if(!out.start||!out.terminal)return bad<ExactQuantityActionCompletion>("ExactQuantityCompletedExecutionRequired",action);
 const auto&s=*out.start;const auto&e=*out.terminal;out.task=t.tasks.find(s.accepted_action.task_id);out.contract=out.task&&out.task->contract_id?t.contracts.find(*out.task->contract_id):nullptr;
 if(!out.task||!out.contract||out.task->record_kind!=TaskRecordKind::exact_quantity||!out.task->exact_quantity||e.start_source_id!=s.id||e.action_id!=s.action_id||s.actor_id!=out.task->assignee||!out.contract->executor_actor_id||*out.contract->executor_actor_id!=s.actor_id||s.accepted_action.offer_id!=out.task->offer_id||(s.action_kind!="accept_offer"&&s.action_kind!="report_offer")||!e.effect.task_id||*e.effect.task_id!=out.task->id||e.disposition!=FixedTerminalDisposition::completed)return bad<ExactQuantityActionCompletion>("ExactQuantityCompletionIdentityMismatch",action);
 auto accepted=acceptance(c,t,*out.task);if(!accepted.ok)return {false,{},accepted.error};const auto&q=*out.task->exact_quantity;
 if(q.assignee.kind!=out.contract->assignee.kind||q.assignee.id!=out.contract->assignee.id||q.original_acceptance_action_ref.empty()||q.deadline_gate.tick!=out.contract->frozen_terms.deadline_tick||!q.deadline_gate.inclusive)return bad<ExactQuantityActionCompletion>("ExactQuantityAcceptedTermsMismatch",action);
 auto v=header(c,s.id,"task_action_started");if(!v.ok)return {false,{},v.error};v=header(c,e.id,"task_action_finished");if(!v.ok)return {false,{},v.error};v=before(c,s.id,e.id);if(!v.ok)return {false,{},v.error};auto time=compare_world_moments(s.started_at,e.finished_at);if(!time.ok)return {false,{},time.error};if(time.value>=0)return bad<ExactQuantityActionCompletion>("ExactQuantityPositiveServiceRequired",action);
 if(s.action_kind=="accept_offer"){
  if(q.original_acceptance_action_ref!=action||s.required_half_days!=out.contract->frozen_terms.definition.accept_ticks)return bad<ExactQuantityActionCompletion>("ExactQuantityAcceptanceActionMismatch",action);
  if(e.effect.success){if(!out.task->accepted_at)return bad<ExactQuantityActionCompletion>("ExactQuantityAcceptedMomentRequired",action);v=at(*out.task->accepted_at,e.finished_at,action);if(!v.ok)return {false,{},v.error};}
 }else if(s.required_half_days!=1||!s.accepted_action.delivery_quantity||*s.accepted_action.delivery_quantity<=0)return bad<ExactQuantityActionCompletion>("ExactQuantityReportTermsMismatch",action);
 if(e.effect.success&&s.action_kind=="report_offer"){
  const TaskTransferFact*transfer=nullptr;const DeliveryFact*delivery=nullptr;
  for(const auto&row:t.transfers.rows())if(row.second.source_action_id==action){const auto&f=row.second;if(f.quantity_source_kind!=TaskQuantitySourceKind::procurement_exact||transfer||row.first!=f.id||f.task_id!=out.task->id||f.contract_id!=out.contract->id||!f.completed_at||f.quantity!=*s.accepted_action.delivery_quantity||f.purpose!=(s.accepted_action.restitution?"restitution":"delivery")||references(e,{"tasks","task_transfer",f.id})!=1)return bad<ExactQuantityActionCompletion>("ExactQuantityTransferCoverageMismatch",action);v=header(c,f.id,"task_transfer");if(!v.ok)return {false,{},v.error};v=at(*f.completed_at,e.finished_at,f.id);if(!v.ok)return {false,{},v.error};v=before(c,s.id,f.id);if(!v.ok)return {false,{},v.error};v=before(c,f.id,e.id);if(!v.ok)return {false,{},v.error};transfer=&f;}
  if(!transfer)return bad<ExactQuantityActionCompletion>("ExactQuantityTransferRequired",action);
  for(const auto&row:t.deliveries.rows())if(row.second.source_action_id==action){const auto&f=row.second;if(f.quantity_source_kind!=TaskQuantitySourceKind::procurement_exact||delivery||s.accepted_action.restitution||row.first!=f.id||f.task_id!=out.task->id||f.transfer_fact_id!=transfer->id||!f.completed_at||f.quantity!=transfer->quantity||references(e,{"tasks","task_delivery",f.id})!=1)return bad<ExactQuantityActionCompletion>("ExactQuantityDeliveryCoverageMismatch",action);v=header(c,f.id,"task_delivery");if(!v.ok)return {false,{},v.error};v=at(*f.completed_at,e.finished_at,f.id);if(!v.ok)return {false,{},v.error};v=before(c,transfer->id,f.id);if(!v.ok)return {false,{},v.error};v=before(c,f.id,e.id);if(!v.ok)return {false,{},v.error};delivery=&f;}
  if(!s.accepted_action.restitution&&!delivery)return bad<ExactQuantityActionCompletion>("ExactQuantityDeliveryRequired",action);
 }
 return {true,out,{}};
}
Status committed(const CommonStore&c,ExactQuantityActionCompletion&v,const SourceRef&ref){
 auto original=read_common_command_source(c,ref);if(!original.ok)return {false,{},original.error};const auto&r=*original.value.result;const auto*advance=r.details?std::get_if<AdvanceDetails>(&r.details->data):nullptr;const auto*done=advance?advance->completed_actions.find(v.start->action_id):nullptr;
 if(!done||!done->ok||!done->completed||!*done->completed||!done->success||*done->success!=v.terminal->effect.success||!done->task||!same(*done->task,v.terminal->effect)||r.tick<v.terminal->finished_at.half_day_tick)return common_failure("ExactQuantitySealedCompletionMismatch",v.start->action_id,ErrorKind::internal);
 // The same original Action is completed once, even if the client later
 // retries its request. A replay points at the same retained result identity.
 for(const auto&row:c.state().results.rows())if(row.second&&row.second->result_id!=r.result_id){const auto*other=row.second->details?std::get_if<AdvanceDetails>(&row.second->details->data):nullptr;if(other&&other->completed_actions.find(v.start->action_id))return common_failure("ExactQuantityCompletionSealedTwice",v.start->action_id,ErrorKind::internal);}
 v.committed_result=&r;return common_success();
}
// 绑定唯一原始完工结果；已有不同来源时拒绝，不覆盖原绑定。
Status bind_quantity_completion_result(std::optional<SourceRef> &target,
                                       const SourceRef &source,
                                       const StableId &id) {
  if (target && !same(*target, source))
    return common_failure("ExactQuantityCompletionResultConflict", id,
                          ErrorKind::internal);
  target = source;
  return common_success();
}
}
Outcome<ExactQuantityActionCompletion> read_exact_quantity_action_completion(const CommonStore&c,const TaskState&t,const StableId&action,const SourceRef&result){auto value=original(c,t,action);if(!value.ok)return value;auto valid=committed(c,value.value,result);return valid.ok?value:Outcome<ExactQuantityActionCompletion>{false,{},valid.error};}
Outcome<ExactQuantityActionCompletion> read_exact_quantity_applied_source(const CommonStore&c,const TaskState&t,const SourceRef&source,Integer schema){
 if(schema!=2||source.domain!="tasks"||(source.kind!="task_transfer"&&source.kind!="task_delivery"))return bad<ExactQuantityActionCompletion>("ExactQuantityAppliedSourceKindMismatch",source.id);
 const StableId*action=nullptr;if(source.kind=="task_transfer"){const auto*row=t.transfers.find(source.id);if(!row||row->id!=source.id||row->quantity_source_kind!=TaskQuantitySourceKind::procurement_exact)return bad<ExactQuantityActionCompletion>("ExactQuantityTransferSourceMissing",source.id);action=&row->source_action_id;}else{const auto*row=t.deliveries.find(source.id);if(!row||row->id!=source.id||row->quantity_source_kind!=TaskQuantitySourceKind::procurement_exact)return bad<ExactQuantityActionCompletion>("ExactQuantityDeliverySourceMissing",source.id);action=&row->source_action_id;}
 auto value=original(c,t,*action);if(!value.ok)return value;if(!value.value.terminal->effect.success||references(*value.value.terminal,source)!=1)return bad<ExactQuantityActionCompletion>("ExactQuantityAppliedSourceNotAdopted",source.id);return value;
}
// 仅绑定已校核的实际完工结果；来源冲突或时点不符时拒绝。
Status bind_exact_quantity_action_result_in_candidate(const CommonStore &c,
                                                      TaskState &t,
                                                      const StableId &action,
                                                      const SourceRef &result) {
  auto read = read_exact_quantity_action_completion(c, t, action, result);
  if (!read.ok)
    return {false, {}, read.error};
  const auto &v = read.value;
  if (!v.terminal->effect.success)
    return common_success();
  auto *task = t.tasks.find(v.task->id);
  auto &q = *task->exact_quantity;
  if (v.start->action_kind == "accept_offer")
    return bind_quantity_completion_result(q.original_acceptance_result_ref,
                                           result, task->id);
  for (auto &row : t.transfers.rows())
    if (row.second.source_action_id == action) {
      auto s = bind_quantity_completion_result(
          row.second.original_completion_result_ref, result, row.first);
      if (!s.ok)
        return s;
    }
  for (auto &row : t.deliveries.rows())
    if (row.second.source_action_id == action) {
      auto s = bind_quantity_completion_result(
          row.second.original_completion_result_ref, result, row.first);
      if (!s.ok)
        return s;
    }
  if (latest_report(c, t, task->id) == v.terminal) {
    if (!q.reported_at)
      return common_failure("ExactQuantityReportedMomentRequired", task->id);
    auto s = at(*q.reported_at, v.terminal->finished_at, task->id);
    if (!s.ok)
      return s;
    return bind_quantity_completion_result(q.original_report_result_ref, result,
                                           task->id);
  }
  return common_success();
}
Status validate_exact_quantity_active_action(const CommonStore&c,const TaskState&t,const ActionCoreView&action){
 const auto*params=t.actions.find(action.id);const auto*task=params?t.tasks.find(params->task_id):nullptr;if(!task)return common_failure("ExactQuantityActionTaskMissing",action.id);auto accepted=acceptance(c,t,*task);if(!accepted.ok)return {false,{},accepted.error};
 const TaskExactStartSource*source=nullptr;for(const auto&row:t.exact_execution->starts.rows())if(row.second.action_id==action.id){if(source||row.first!=row.second.id)return common_failure("DuplicateExactQuantityActionStart",action.id);source=&row.second;}
 if(!source||source->action_kind!=action.kind||source->actor_id!=action.actor_id||source->participants!=action.participants||source->accepted_action.task_id!=params->task_id||source->accepted_action.offer_id!=params->offer_id||source->accepted_action.delivery_quantity!=params->delivery_quantity||source->accepted_action.restitution!=params->restitution||action.start_tick!=source->started_at.half_day_tick||action.remaining_ticks<0||action.remaining_ticks>source->required_half_days)return common_failure("ExactQuantityActiveActionSourceMismatch",action.id);
 for(const auto&row:t.exact_execution->terminals.rows())if(row.second.action_id==action.id)return common_failure("ExactQuantityTerminalStillActive",action.id);
 if((action.kind=="accept_offer"&&(source!=accepted.value||task->accepted_at))||(action.kind=="report_offer"&&(!task->accepted_at||source->required_half_days!=1)))return common_failure("ExactQuantityActiveExecutionTermsMismatch",action.id);
 return header(c,source->id,"task_action_started");
}
Status bind_exact_quantity_advance_result_in_candidate(const CommonStore&c,TaskState&t,const SourceRef&result,const TaskExactLifeSources*life){
 if(!t.exact_quantities||!t.exact_execution)return common_failure("ExactQuantityExecutionSourcesRequired",{},ErrorKind::unsupported_capability);
 auto sealed=read_common_command_source(c,result);if(!sealed.ok)return {false,{},sealed.error};const auto*advance=sealed.value.result->details?std::get_if<AdvanceDetails>(&sealed.value.result->details->data):nullptr;if(!advance)return common_failure("ExactQuantityAdvanceResultRequired",result.id,ErrorKind::internal);
 for(const auto&done:advance->completed_actions.rows())for(const auto&row:t.exact_execution->starts.rows())if(row.second.action_id==done.first){const auto*task=t.tasks.find(row.second.accepted_action.task_id);if(task&&task->record_kind==TaskRecordKind::exact_quantity){auto valid=bind_exact_quantity_action_result_in_candidate(c,t,done.first,result);if(!valid.ok)return valid;}}
 return validate_exact_quantity_result_bindings(c,t,life);
}
Status validate_exact_quantity_result_bindings(const CommonStore&c,const TaskState&t,const TaskExactLifeSources*life){
 for(const auto&row:t.tasks.rows()){
  const auto&task=row.second;if(task.record_kind!=TaskRecordKind::exact_quantity){if(task.exact_quantity)return common_failure("UnexpectedExactQuantityFields",row.first);continue;}
  if(row.first!=task.id||!t.exact_quantities||!task.exact_quantity||!task.contract_id||!task.offer_ref)return common_failure("ExactQuantityTaskSourceRequired",row.first);auto start=acceptance(c,t,task);if(!start.ok)return {false,{},start.error};const auto&q=*task.exact_quantity;auto outcome=actual_quantity_outcome(c,t,task,life);if(!outcome.ok)return outcome;
  if(task.accepted_at.has_value()!=q.original_acceptance_result_ref.has_value())return common_failure("ExactQuantityAcceptanceResultRequired",row.first);
  if(q.original_acceptance_result_ref){auto read=read_exact_quantity_action_completion(c,t,q.original_acceptance_action_ref,*q.original_acceptance_result_ref);if(!read.ok)return {false,{},read.error};if(read.value.task!=&task||!read.value.terminal->effect.success)return common_failure("ExactQuantityAcceptanceResultMismatch",row.first);}
  for(const auto&end:t.exact_execution->terminals.rows())if(end.second.start_source_id==start.value->id){if(end.second.effect.success!=task.accepted_at.has_value())return common_failure("ExactQuantityAcceptanceSuccessMismatch",row.first);}
  const auto*last=latest_report(c,t,row.first);if(bool(last)!=q.reported_at.has_value()||bool(last)!=q.original_report_result_ref.has_value())return common_failure("ExactQuantityReportResultRequired",row.first);
  if(last){auto read=read_exact_quantity_action_completion(c,t,last->action_id,*q.original_report_result_ref);if(!read.ok)return {false,{},read.error};auto s=at(*q.reported_at,last->finished_at,task.id);if(!s.ok)return s;}
 }
 for(const auto&row:t.transfers.rows())if(row.second.completed_at){const auto&f=row.second;if(!f.original_completion_result_ref)return common_failure("ExactQuantityTransferResultRequired",row.first);auto read=read_exact_quantity_action_completion(c,t,f.source_action_id,*f.original_completion_result_ref);if(!read.ok)return {false,{},read.error};if(!read.value.terminal->effect.success||references(*read.value.terminal,{"tasks","task_transfer",row.first})!=1)return common_failure("ExactQuantityTransferResultMismatch",row.first);}else if(row.second.original_completion_result_ref)return common_failure("UnexpectedLegacyTransferResult",row.first);
 for(const auto&row:t.deliveries.rows())if(row.second.completed_at){const auto&f=row.second;if(!f.original_completion_result_ref)return common_failure("ExactQuantityDeliveryResultRequired",row.first);auto read=read_exact_quantity_action_completion(c,t,f.source_action_id,*f.original_completion_result_ref);if(!read.ok)return {false,{},read.error};const auto*transfer=t.transfers.find(f.transfer_fact_id);if(!transfer||!transfer->original_completion_result_ref||!same(*transfer->original_completion_result_ref,*f.original_completion_result_ref))return common_failure("ExactQuantityTransferDeliveryResultMismatch",row.first);}else if(row.second.original_completion_result_ref)return common_failure("UnexpectedLegacyDeliveryResult",row.first);
 return common_success();
}
}
