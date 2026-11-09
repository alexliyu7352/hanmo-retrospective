#include "work_result_projection.hpp"
#include "resident/domains/tasks/task_result_time2.hpp"
namespace hanmo::resident {namespace {
template<class T>Outcome<T> fail(const char*code,const StableId&id={}){return {false,{},common_failure(code,id,ErrorKind::unsupported_capability).error};}
bool work_kind(const StableId&k){for(const auto*v:{"work_task_offered","work_task_accepted","work_accepted","work_period_receipt","work_actual_start","work_leader_changed","patrol_work_result","supervised_work_result","work_commission_task_terminal"})if(k==v)return true;return supervision_source_kind(k);}
}
bool native_work_result_kind(const FactRecord&h){return h.domain=="tasks"&&work_kind(h.kind);}
Outcome<WorldMoment> NativeWorkResultOwnerRead::read_selected_source_moment(const MomentOwnerSource&ref)const{
 const auto&x=access_;const auto&r=ref.source_ref;if(r.domain!="tasks"||!work_kind(r.kind))return fail<WorldMoment>("UnsupportedWorkResultSource",r.id);
 if(!x.definitions.bound_to(x.common,x.tasks))return fail<WorldMoment>("WorkResultReaderCandidateMismatch",r.id);
 if(supervision_source_kind(r.kind)){if(!x.supervision||&x.supervision->common!=&x.common||&x.supervision->tasks!=&x.tasks)return fail<WorldMoment>("SupervisionResultOriginalSourcesRequired",r.id);return read_supervision_source_moment(*x.supervision,r,ref.source_schema);}
 if(r.kind=="work_task_offered"||r.kind=="work_task_accepted")return x.definitions.read_source_moment(r,ref.source_schema);
 if(r.kind=="work_commission_task_terminal"){
  if(ref.source_schema!=1||!x.terminals||!x.terminal_stamp||!x.tasks.service_terminals||&x.terminals->common!=&x.common||&x.terminals->tasks!=&x.tasks)return fail<WorldMoment>("WorkTerminalOriginalSourcesRequired",r.id);
  const auto*t=x.tasks.service_terminals->work_commission_terminals.find(r.id);const auto*e=t&&x.tasks.work_execution?x.tasks.work_execution->episodes.find(t->accepted_scope_ref.work_id):nullptr;if(!t||!e)return fail<WorldMoment>("WorkTaskTerminalSourceMissing",r.id);
  WorkCommissionTerminal terminal{e->authorization.accepted_task_id,t->accepted_scope_ref.accepted_task_ref,t->accepted_scope_ref.work_id,r.id,ref.source_schema};auto read=read_work_commission_termination(*x.terminals,t->accepted_scope_ref,terminal,*x.terminal_stamp);return read.ok?Outcome<WorldMoment>{true,read.value->effective_game_time,{}}:Outcome<WorldMoment>{false,{},read.error};
 }
 return read_work_source_moment(x.common,x.tasks,x.original,r,ref.source_schema);
}
Outcome<ExactOwnerFactRead> NativeWorkResultOwnerRead::read_selected_fact(const FactRecord&h)const{
 const auto&x=access_;const auto*header=x.common.state().facts.find(h.id);if(!native_work_result_kind(h)||!header||header->domain!=h.domain||header->kind!=h.kind)return fail<ExactOwnerFactRead>("WorkResultHeaderMismatch",h.id);
 if(h.kind=="patrol_work_result"||h.kind=="supervised_work_result")return read_task_dynamic_fact_v2(x.common,x.tasks,x.affairs,h,&x.original);
 const Integer schema=h.kind=="work_period_receipt"?2:1;auto at=read_selected_source_moment({{h.domain,h.kind,h.id},schema});if(!at.ok)return {false,{},at.error};ExactOwnerFactRead out;out.disposition=ExactOwnerFactRead::Disposition::no_dynamic_projection;out.supporting_sources.push_back({{h.domain,h.kind,h.id},schema});return {true,std::move(out),{}};
}
}
