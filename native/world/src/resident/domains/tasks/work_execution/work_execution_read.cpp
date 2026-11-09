#include "work_execution_detail.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "work_skill_inputs.hpp"
#include "work_reassignment_sources.hpp"
#include "resident/kernel/dynamic_result_time2.hpp"
namespace hanmo::resident {
using namespace work_detail;
// Single original comparison body, also used by S6 approval consumers.
bool same_work_plan_terms(const WorkPlan&a,const WorkPlan&b){if(a.index()!=b.index())return false;auto common=[](const auto&x,const auto&y){return x.required_half_days==y.required_half_days&&x.completion_deadline.tick==y.completion_deadline.tick&&x.completion_deadline.inclusive==y.completion_deadline.inclusive&&same(x.continuity_policy_ref,y.continuity_policy_ref)&&x.rule_id==y.rule_id&&x.rule_version==y.rule_version;};if(const auto*x=std::get_if<PatrolPlan>(&a)){const auto&y=std::get<PatrolPlan>(b);if(!common(*x,y)||x->jurisdiction_id!=y.jurisdiction_id||x->authority_scope.kind!=y.authority_scope.kind||x->authority_scope.id!=y.authority_scope.id||x->leader_actor_id!=y.leader_actor_id||x->unit_ids!=y.unit_ids||x->minimum_available_people!=y.minimum_available_people||!same(x->supply_order_ref,y.supply_order_ref)||x->route.index()!=y.route.index())return false;if(const auto*r=std::get_if<PatrolCircuit>(&x->route)){const auto&s=std::get<PatrolCircuit>(y.route);return same_refs(r->checkpoint_refs,s.checkpoint_refs)&&same_refs(r->allowed_passage_refs,s.allowed_passage_refs);}return same(std::get<StationWatch>(x->route).station_ref,std::get<StationWatch>(y.route).station_ref);}const auto&x=std::get<SupervisedWorkPlan>(a);const auto&y=std::get<SupervisedWorkPlan>(b);return common(x,y)&&x.kind==y.kind&&x.supervisor_actor_id==y.supervisor_actor_id&&same(x.economic_work_ref,y.economic_work_ref)&&same(x.target_site_or_anchor_ref,y.target_site_or_anchor_ref)&&x.required_skill_ref.has_value()==y.required_skill_ref.has_value()&&(!x.required_skill_ref||same(*x.required_skill_ref,*y.required_skill_ref));}
namespace {

Status interruption(const CommonStore& common,const TaskWorkExecutionState& s,const WorkEpisode& e){
 // The retained value points to the latest actual adopted interruption, not to
 // today's condition or a deadline inferred from the Work plan.
 std::optional<StableId> expected;std::size_t latest=0;
 auto adopt=[&](const StableId&id,const char*kind)->Status{if(!header(common,id,kind))return common_failure("WorkInterruptionHeaderMissing",id);const auto*head=common.state().facts.find(id);if(!head)return common_failure("WorkInterruptionHeaderMissing",id);auto order=read_actual_common_fact_position(common,*head);if(!order.ok)return {false,{},order.error};if(!expected||order.value>latest){expected=id;latest=order.value;}return common_success();};
 for(const auto&id:e.accepted_period_refs){const auto*p=s.period_receipts.find(id);if(!p)return common_failure("MissingWorkPeriod",id);if(p->disposition==S3WorkPeriodDisposition::interrupted){auto v=adopt(id,"work_period_receipt");if(!v.ok)return v;}}
 if(task_work_fixed_execution_terms_version(s.version)){for(const auto&row:s.supervision_window_closures.rows()){const auto*start=s.supervision_starts.find(row.second.start_source_ref);if(start&&start->work_id==e.work_id){auto v=adopt(row.first,"supervision_window_closed");if(!v.ok)return v;}}
  for(const auto&row:s.supervision_terminals.rows()){const auto*start=s.supervision_starts.find(row.second.start_source_ref);if(start&&start->work_id==e.work_id&&row.second.disposition==SupervisionReleaseKind::actual_interruption){auto v=adopt(row.first,"supervision_action_ended");if(!v.ok)return v;}}
 }
 return e.last_interruption_ref==expected?common_success():common_failure("WorkLatestInterruptionMismatch",e.work_id);
}
template<class Original>
Status acceptance(const CommonStore&common,const TaskState&tasks,const TaskWorkExecutionState&state,const WorkEpisode&e,const Original&o){const auto*a=state.acceptances.find(e.acceptance_source_ref);const auto*t=tasks.tasks.find(e.authorization.accepted_task_id);if(e.work_id.empty()||!a||a->work_id!=e.work_id||a->fact_id!=e.acceptance_source_ref||!header(common,a->fact_id,"work_accepted")||!same(a->authorization,e.authorization)||!same(a->expense_terms_ref,expense(e.frozen_plan))||!t||t->id!=e.authorization.accepted_task_id||t->assignee!=initial_leader(e.frozen_plan))return common_failure("WorkAcceptanceTaskMismatch",e.work_id);auto valid=authorization_shape(e.authorization);if(!valid.ok)return valid;valid=plan_shape(e.frozen_plan);if(!valid.ok)return valid;auto original=o.read_accepted_work(e.authorization);if(!original.ok)return {false,{},original.error};const auto&v=original.value;if(!same(v.frozen_request.authorization,e.authorization)||!plan_matches(v.frozen_request.plan,e.frozen_plan)||!same(v.actual_task_acceptance,a->original_task_acceptance)||!v.actual_expense_terms||!same(*v.actual_expense_terms,a->expense_terms_ref))return common_failure("WorkFrozenAcceptanceChanged",e.work_id);auto at=same_moment(v.accepted_at,a->accepted_at);if(!at.ok)return {false,{},at.error};auto earliest=same_moment(v.earliest_service_at,a->earliest_service_at);if(!earliest.ok)return {false,{},earliest.error};if(!at.value||!earliest.value||t->accepted_tick!=a->accepted_at.half_day_tick||t->deadline_tick!=deadline(e.frozen_plan).tick)return common_failure("WorkAcceptanceTimeMismatch",e.work_id);valid=o.validate_original_source(a->original_task_acceptance);if(!valid.ok)return valid;return o.validate_original_source({a->expense_terms_ref,1});}
template<class Original>
Outcome<const WorkAcceptanceSource*> accepted_source(const CommonStore& common,const TaskState& tasks,const Original& originals,const StableId&id){
 if(!tasks.work_execution||!task_work_execution_version(tasks.work_execution->version))return fail<const WorkAcceptanceSource*>("TaskWorkExecution3Required",id);
 const auto&s=*tasks.work_execution;const auto*a=s.acceptances.find(id);const auto*e=a?s.episodes.find(a->work_id):nullptr;
 if(!a||a->fact_id!=id||!e||e->acceptance_source_ref!=id)return fail<const WorkAcceptanceSource*>("WorkAcceptanceSourceMissing",id);
 // Same original acceptance validation as read_work, without traversing later
 // periods or terminal results that may legitimately cite this immutable source.
 auto valid=acceptance(common,tasks,s,*e,originals);return valid.ok?Outcome<const WorkAcceptanceSource*>{true,a,{}}:Outcome<const WorkAcceptanceSource*>{false,{},valid.error};
}
template<class Original>
Outcome<WorkPatrolAssignmentRead> patrol_assignment_source(const CommonStore& common,const TaskState& tasks,const Original& originals,const SourceRef&ref,Integer schema){
 if(ref.domain!="tasks"||ref.id.empty()||schema!=1||!tasks.work_execution)return fail<WorkPatrolAssignmentRead>("PatrolWorkAssignmentSchemaMismatch",ref.id);
 const WorkReassignmentAcceptanceSource* successor=nullptr;const StableId* original_acceptance=&ref.id;
 if(ref.kind=="work_reassignment_accepted"){
  if(!task_work_lifecycle_sources_version(tasks.work_execution->version))return fail<WorkPatrolAssignmentRead>("WorkReassignmentProfileRequired",ref.id);
  auto valid=originals.validate_original_source({ref,1});if(!valid.ok)return {false,{},valid.error};
  successor=tasks.work_execution->work_reassignment_acceptances.find(ref.id);if(!successor||successor->fact_id!=ref.id||successor->work_acceptance_ref.domain!="tasks"||successor->work_acceptance_ref.kind!="work_accepted")return fail<WorkPatrolAssignmentRead>("PatrolReplacementAcceptanceRequired",ref.id);original_acceptance=&successor->work_acceptance_ref.id;
 }else if(ref.kind!="work_accepted")return fail<WorkPatrolAssignmentRead>("PatrolWorkAssignmentSchemaMismatch",ref.id);
 // 此原读口证明接受/条款；S2 实际开工及冷读仍须核本次 accepted source 在真实 cut 的身份。
 auto actual=accepted_source(common,tasks,originals,*original_acceptance);if(!actual.ok)return {false,{},actual.error};
 const auto*episode=tasks.work_execution->episodes.find(actual.value->work_id);const auto*plan=episode?std::get_if<PatrolPlan>(&episode->frozen_plan):nullptr;
 if(!plan||(successor&&successor->work_id!=episode->work_id))return fail<WorkPatrolAssignmentRead>("PatrolWorkPlanRequired",ref.id);
 return {true,{actual.value,plan,successor?&successor->new_actor_id:&plan->leader_actor_id,successor},{}};
}
Status acceptance_roots(const WorkAcceptanceSourceContext& x){
 const auto& t=x.tasks;
 if(&t.governance.common!=&t.common||!x.definitions.bound_to(t.common,t.tasks))return common_failure("WorkAcceptanceOriginalCandidateMismatch");
 return common_success();
}
struct NativeAcceptanceOriginal {
 const WorkAcceptanceSourceContext& sources;const WorkReassignmentSourceContext* reassignments;
 Outcome<WorkAcceptedRead> read_accepted_work(const WorkAuthorizationRef& authorization)const{return sources.definitions.read_accepted_work(authorization);}
 Status validate_original_source(const MomentOwnerSource& source)const{
  if(!work_detail::valid(source.source_ref)||source.source_schema<1)return common_failure("WorkOriginalSourceRequired");
  Outcome<WorldMoment> at;
  if(source.source_ref.domain=="tasks"){
   if(source.source_ref.kind=="work_task_offered"||source.source_ref.kind=="work_task_accepted")at=sources.definitions.read_source_moment(source.source_ref,source.source_schema);
   else if(source.source_ref.kind=="work_reassignment_accepted"){
    if(source.source_schema!=1||!reassignments)return common_failure("WorkReassignmentOriginalSourcesRequired",source.source_ref.id);
    const auto& t=sources.tasks;const auto& r=*reassignments;
    if(&r.tasks!=&t||&r.governance.history.task_definitions!=&sources.definitions)return common_failure("WorkAcceptanceOriginalCandidateMismatch",source.source_ref.id);
    auto actual=read_work_reassignment_acceptance(r,source.source_ref);if(!actual.ok)return {false,{},actual.error};at={true,actual.value->accepted_at,{}};
   }else return common_failure("UnsupportedWorkOriginalTaskSource",source.source_ref.id);
  }else at=sources.original_sources.read_selected_source_moment(source);
  if(!at.ok)return {false,{},at.error};auto now=sources.tasks.clock.read_current_moment();if(!now.ok)return {false,{},now.error};return time_order(at.value,now.value,"WorkOriginalSourceFromFuture");
 }
};
}
WorkExecutionRead::WorkExecutionRead(const CommonStore&c,const TaskState&t,const WorkOriginalRead&o):common_(c),tasks_(t),originals_(o){}
Outcome<const WorkAcceptanceSource*> WorkExecutionRead::read_acceptance(const StableId&id)const{return accepted_source(common_,tasks_,originals_,id);}
Outcome<WorkPatrolAssignmentRead> WorkExecutionRead::read_patrol_assignment(const SourceRef&ref,Integer schema)const{return patrol_assignment_source(common_,tasks_,originals_,ref,schema);}
Outcome<const WorkAcceptanceSource*> read_work_acceptance_source(const WorkAcceptanceSourceContext& x,const StableId& id){
 auto valid=acceptance_roots(x);if(!valid.ok)return {false,{},valid.error};
 return accepted_source(x.tasks.common,x.tasks.tasks,NativeAcceptanceOriginal{x,nullptr},id);
}
Outcome<WorkPatrolAssignmentRead> read_work_patrol_assignment_source(const WorkAcceptanceSourceContext& x,const SourceRef& ref,Integer schema,const WorkReassignmentSourceContext* reassignments){
 auto valid=acceptance_roots(x);if(!valid.ok)return {false,{},valid.error};
 return patrol_assignment_source(x.tasks.common,x.tasks.tasks,NativeAcceptanceOriginal{x,reassignments},ref,schema);
}
Outcome<const WorkEpisode*> WorkExecutionRead::read_work(const StableId&id)const{if(!tasks_.work_execution||!task_work_execution_version(tasks_.work_execution->version))return fail<const WorkEpisode*>("TaskWorkExecution3Required",id);const auto&s=*tasks_.work_execution;const auto*e=s.episodes.find(id);if(!e||e->work_id!=id)return fail<const WorkEpisode*>("UnknownWork",id);auto valid=acceptance(common_,tasks_,s,*e,originals_);if(!valid.ok)return {false,{},valid.error};auto total=credited(s,*e);if(!total.ok)return {false,{},total.error};if(static_cast<Integer>(e->phase)<0||static_cast<Integer>(e->phase)>3||(e->phase==WorkPhase::work_complete||e->phase==WorkPhase::stopped)!=e->terminal_result_ref.has_value())return fail<const WorkEpisode*>("InvalidWorkPhase",id);const WorkPeriodReceipt2*previous=nullptr;for(const auto&ref:e->accepted_period_refs){const auto*p=s.period_receipts.find(ref);if(!p)return fail<const WorkEpisode*>("MissingWorkPeriod",ref);valid=validate_period_record(common_,s,*e,*p,originals_);if(!valid.ok)return {false,{},valid.error};if(previous){valid=time_order(previous->actual_until,p->window.from,"OverlappingWorkReceiptHistory");if(!valid.ok)return {false,{},valid.error};}previous=p;}std::optional<WorkExecutionSourceRead> previous_execution;for(std::size_t i=0;i<e->original_execution_sources.size();++i){const auto&ref=e->original_execution_sources[i];for(std::size_t j=0;j<i;++j)if(same(ref,e->original_execution_sources[j]))return fail<const WorkEpisode*>("DuplicateWorkExecutionSource",id);auto v=originals_.read_original_execution(*e,ref);if(!v.ok)return {false,{},v.error};if(!same(v.value.original_source,ref)||v.value.original_terminal.has_value()!=v.value.ended_at.has_value())return fail<const WorkEpisode*>("WorkExecutionSourceMismatch",id);valid=execution_shape(v.value.binding);if(!valid.ok)return {false,{},valid.error};valid=time_order(v.value.started_at,v.value.finite_until,"InvalidWorkExecutionWindow",true);if(!valid.ok)return {false,{},valid.error};if(previous_execution){if(!previous_execution->ended_at)return fail<const WorkEpisode*>("OverlappingWorkExecutionHistory",id);valid=time_order(*previous_execution->ended_at,v.value.started_at,"OverlappingWorkExecutionHistory");if(!valid.ok)return {false,{},valid.error};}previous_execution=std::move(v.value);}if(e->current_execution&&(!previous_execution||!same(*e->current_execution,previous_execution->binding)))return fail<const WorkEpisode*>("WorkCurrentExecutionMismatch",id);if(e->phase==WorkPhase::active&&!e->current_execution)return fail<const WorkEpisode*>("ActiveWorkWithoutOriginalExecution",id);valid=interruption(common_,s,*e);if(!valid.ok)return {false,{},valid.error};return {true,e,{}};}
Outcome<const WorkPeriodReceipt2*> WorkExecutionRead::read_period(const StableId&id)const{if(!tasks_.work_execution)return fail<const WorkPeriodReceipt2*>("TaskWorkExecution3Required");const auto&s=*tasks_.work_execution;const auto*p=s.period_receipts.find(id);if(!p||p->fact_id!=id)return fail<const WorkPeriodReceipt2*>("MissingWorkPeriod",id);const auto*e=s.episodes.find(p->work_id);if(!e)return fail<const WorkPeriodReceipt2*>("WorkPeriodEpisodeMissing",id);auto valid=acceptance(common_,tasks_,s,*e,originals_);if(!valid.ok)return {false,{},valid.error};valid=validate_period_record(common_,s,*e,*p,originals_);return valid.ok?Outcome<const WorkPeriodReceipt2*>{true,p,{}}:Outcome<const WorkPeriodReceipt2*>{false,{},valid.error};}
// 核对首个正服务期的原始开工来源；缺失或不一致仍按原错误拒绝。
Outcome<const WorkActualStartSource *>
WorkExecutionRead::read_actual_start(const StableId &work,
                                     const StableId &start) const {
  auto episode = read_work(work);
  if (!episode.ok)
    return {false, {}, episode.error};
  const auto &e = *episode.value;
  const auto &s = *tasks_.work_execution;
  const auto *a = s.actual_starts.find(start);
  if (!a || a->start_fact_id != start || a->work_id != work ||
      a->source_schema != 1 || e.actual_start_ref != start ||
      !same(a->authorization, e.authorization) ||
      !header(common_, start, "work_actual_start"))
    return fail<const WorkActualStartSource *>("InvalidWorkActualStart", start);
  bool found = false;
  for (const auto &id : e.accepted_period_refs) {
    const auto *p = s.period_receipts.find(id);
    if (!p)
      return fail<const WorkActualStartSource *>("MissingWorkPeriod", id);
    auto original = originals_.read_original_period(e, p->source);
    if (!original.ok)
      return {false, {}, original.error};
    if (!original.value.actual_positive_start)
      continue;
    if (found)
      return fail<const WorkActualStartSource *>("WorkStartSearchUnexpected",
                                                 start);
    found = true;
    auto at = same_moment(*original.value.actual_positive_start, a->started_at);
    if (!at.ok)
      return {false, {}, at.error};
    if (!at.value || !same(a->execution, p->execution_source))
      return fail<const WorkActualStartSource *>(
          "WorkStartNotFirstPositiveService", start);
    if (const auto *b = std::get_if<WorkMilitaryStartBasis>(&a->actual_basis)) {
      const auto *m = std::get_if<WorkMilitaryPeriodSource>(&p->source);
      if (!m || m->source_schema != b->period_fact_ref.source_schema ||
          !same(m->fact_ref, b->period_fact_ref.fact_ref))
        return fail<const WorkActualStartSource *>("WorkStartDutyMismatch",
                                                   start);
    } else {
      const auto &supervision =
          std::get<WorkSupervisionStartBasis>(a->actual_basis);
      const auto *m = std::get_if<WorkEconomicPeriodSource>(&p->source);
      if (!m || supervision.work_period_receipt_ref != id ||
          !same_refs(supervision.economic_period_refs, m->source_parts))
        return fail<const WorkActualStartSource *>("WorkStartEconomicMismatch",
                                                   start);
    }
    break;
  }
  return found ? Outcome<const WorkActualStartSource *>{true, a, {}}
               : fail<const WorkActualStartSource *>(
                     "WorkStartWithoutPositiveService", start);
}
Outcome<const WorkResult*> WorkExecutionRead::read_result_for_expense_evidence(const SourceRef&ref,Integer schema)const{if(ref.domain!="tasks"||schema!=1||(ref.kind!="patrol_work_result"&&ref.kind!="supervised_work_result")||!tasks_.work_execution)return fail<const WorkResult*>("UnsupportedWorkResultSource",ref.id);const auto&s=*tasks_.work_execution;const auto*r=s.results.find(ref.id);if(!r||r->fact_id!=ref.id||result_kind(*r)!=ref.kind||!header(common_,r->fact_id,ref.kind))return fail<const WorkResult*>("WorkResultSourceMissing",ref.id);auto episode=read_work(r->work_id);if(!episode.ok)return {false,{},episode.error};const auto&e=*episode.value;auto total=credited(s,e);if(!total.ok)return {false,{},total.error};if(e.terminal_result_ref!=ref.id||r->task_id!=e.authorization.accepted_task_id||!same(r->authorization,e.authorization)||r->kind!=kind(e.frozen_plan)||r->plan_rule_version!=rule(e.frozen_plan)||r->credited_half_days!=total.value||r->required_half_days!=required(e.frozen_plan)||r->period_summary_refs.size()!=e.accepted_period_refs.size()||static_cast<Integer>(r->disposition)<0||static_cast<Integer>(r->disposition)>2)return fail<const WorkResult*>("WorkResultEpisodeMismatch",ref.id);
 for(std::size_t i=0;i<e.accepted_period_refs.size();++i){if(!same(r->period_summary_refs[i],MomentOwnerSource{{"tasks","work_period_receipt",e.accepted_period_refs[i]},2}))return fail<const WorkResult*>("WorkResultPeriodOrderMismatch",ref.id);auto valid=time_order(s.period_receipts.find(e.accepted_period_refs[i])->actual_until,r->finished_at,"WorkResultBeforeService");if(!valid.ok)return {false,{},valid.error};}
 if(r->first_work_at.has_value()!=e.actual_start_ref.has_value())return fail<const WorkResult*>("WorkResultStartMismatch",ref.id);if(e.actual_start_ref){auto start=read_actual_start(e.work_id,*e.actual_start_ref);if(!start.ok)return {false,{},start.error};auto at=same_moment(*r->first_work_at,start.value->started_at);if(!at.ok)return {false,{},at.error};if(!at.value)return fail<const WorkResult*>("WorkResultStartMismatch",ref.id);}
 auto completion=originals_.read_completion(e,e.accepted_period_refs,r->finished_at);if(!completion.ok)return {false,{},completion.error};if(!same_refs(r->checkpoint_completion_refs,completion.value.checkpoint_completion_refs)||!same_refs(r->actual_stage_result_refs,completion.value.actual_stage_result_refs)||!same_refs(r->actual_leader_refs,completion.value.actual_leader_refs))return fail<const WorkResult*>("WorkResultOriginalCompletionMismatch",ref.id);
 for(const auto*refs:{&r->checkpoint_completion_refs,&r->actual_stage_result_refs,&r->actual_leader_refs,&r->encounter_result_refs}){auto valid=original_refs(originals_,*refs);if(!valid.ok)return {false,{},valid.error};}
 if(r->disposition==WorkResultDisposition::completed){if(e.phase!=WorkPhase::work_complete||r->stop_reason||r->actual_stop_source||total.value!=required(e.frozen_plan)||!completion.value.route_or_stage_complete)return fail<const WorkResult*>("WorkCompletedWithoutOriginalRequirements",ref.id);const auto&gate=deadline(e.frozen_plan);auto timely=calendar_deadline_accepts(gate.tick,gate.inclusive,r->finished_at);if(!timely.ok)return {false,{},timely.error};if(!timely.value)return fail<const WorkResult*>("LateWorkCompletion",ref.id);}else {if(e.phase!=WorkPhase::stopped||!r->stop_reason||!r->actual_stop_source||(total.value>0)!=(r->disposition==WorkResultDisposition::partial_stopped))return fail<const WorkResult*>("WorkStopSourceMissing",ref.id);auto stop=originals_.read_actual_stop(e,*r->actual_stop_source,r->finished_at);if(!stop.ok)return {false,{},stop.error};if(!stop.value.execution_stopped||stop.value.reason!=*r->stop_reason||!same(stop.value.actual_cause,*r->actual_stop_source))return fail<const WorkResult*>("WorkStopOriginalMismatch",ref.id);}
 if(task_work_skill_inputs_version(s.version)){const auto*skills=originals_.skill_inputs();if(!skills||!skills->bound_to(common_,tasks_))return fail<const WorkResult*>("WorkSkillInputsReadRequired",ref.id);auto v=skills->validate_result_input(e,*r);if(!v.ok)return {false,{},v.error};}else if(r->skill_input)return fail<const WorkResult*>("WorkSkillInputsRequirePhysical4",ref.id);
 // The expense-evidence dependency cut validates the whole original Work plus
 // retained fee reference shape. read_result below additionally calls S5; S5
 // uses this named evidence read while validating those same fee sources.
 for(std::size_t i=0;i<r->expense_receipt_refs.size();++i){if(!valid(r->expense_receipt_refs[i]))return fail<const WorkResult*>("InvalidWorkExpenseSource",ref.id);for(std::size_t j=0;j<i;++j)if(same(r->expense_receipt_refs[i],r->expense_receipt_refs[j]))return fail<const WorkResult*>("DuplicateWorkExpenseSource",ref.id);}return {true,r,{}};
}
Outcome<const WorkResult*> WorkExecutionRead::read_result(const SourceRef&ref,Integer schema)const{
 auto actual=read_result_for_expense_evidence(ref,schema);if(!actual.ok)return actual;
 for(const auto&receipt:actual.value->expense_receipt_refs){auto v=originals_.validate_original_source({receipt,1});if(!v.ok)return {false,{},v.error};}
 return actual;
}
Outcome<WorldMoment> read_work_source_moment(const CommonStore&c,const TaskState&t,const WorkOriginalRead&o,const SourceRef&ref,Integer schema){if(ref.domain!="tasks"||!t.work_execution)return fail<WorldMoment>("UnsupportedWorkSource",ref.id);WorkExecutionRead read(c,t,o);const auto&s=*t.work_execution;if(ref.kind=="work_period_receipt"&&schema==2){auto p=read.read_period(ref.id);return p.ok?Outcome<WorldMoment>{true,p.value->actual_until,{}}:Outcome<WorldMoment>{false,{},p.error};}if(ref.kind=="work_actual_start"&&schema==1){const auto*a=s.actual_starts.find(ref.id);if(!a)return fail<WorldMoment>("MissingWorkActualStart",ref.id);auto p=read.read_actual_start(a->work_id,ref.id);return p.ok?Outcome<WorldMoment>{true,p.value->started_at,{}}:Outcome<WorldMoment>{false,{},p.error};}if(ref.kind=="work_reassignment_accepted"&&schema==1){if(!task_work_lifecycle_sources_version(s.version))return fail<WorldMoment>("WorkReassignmentProfileRequired",ref.id);auto valid=o.validate_original_source({ref,1});if(!valid.ok)return {false,{},valid.error};const auto*a=s.work_reassignment_acceptances.find(ref.id);if(!a||a->fact_id!=ref.id||!header(c,ref.id,ref.kind))return fail<WorldMoment>("WorkReassignmentAcceptanceMissing",ref.id);return {true,a->accepted_at,{}};}if(ref.kind=="work_accepted"&&schema==1){const auto*a=s.acceptances.find(ref.id);if(!a)return fail<WorldMoment>("MissingWorkAcceptance",ref.id);auto p=read.read_work(a->work_id);return p.ok?Outcome<WorldMoment>{true,a->accepted_at,{}}:Outcome<WorldMoment>{false,{},p.error};}if(ref.kind=="work_leader_changed"&&schema==1){const auto*a=s.leader_changes.find(ref.id);if(!a||!header(c,ref.id,ref.kind))return fail<WorldMoment>("MissingWorkLeaderChange",ref.id);auto e=read.read_work(a->work_id);if(!e.ok)return {false,{},e.error};bool found=false;for(const auto&id:e.value->leader_change_fact_refs)if(id==ref.id)found=true;if(!found)return fail<WorldMoment>("UnreferencedWorkLeaderChange",ref.id);auto v=o.validate_leader_change(*e.value,*a);return v.ok?Outcome<WorldMoment>{true,a->replacement_at,{}}:Outcome<WorldMoment>{false,{},v.error};}auto r=read.read_result(ref,schema);return r.ok?Outcome<WorldMoment>{true,r.value->finished_at,{}}:Outcome<WorldMoment>{false,{},r.error};}
Status validate_task_work_execution(const CommonStore&common,const TaskState&tasks,const WorkOriginalRead&o,const WorldMoment&now){if(!tasks.work_execution)return common_success();const auto&s=*tasks.work_execution;if(!task_work_execution_version(s.version))return common_failure("TaskWorkExecution3Required");if(!task_work_fixed_execution_terms_version(s.version)&&!s.supervision_execution_terms.empty())return common_failure("SupervisionExecutionTermsRequirePhysical5");if(!task_work_lifecycle_sources_version(s.version)&&!s.work_reassignment_acceptances.empty())return common_failure("WorkReassignmentAcceptanceRequiresPhysical6");WorkExecutionRead read(common,tasks,o);for(const auto&row:s.episodes.rows()){for(const auto&other:s.episodes.rows())if(other.first<row.first&&(other.second.authorization.accepted_task_id==row.second.authorization.accepted_task_id||other.second.acceptance_source_ref==row.second.acceptance_source_ref))return common_failure("DuplicateWorkTaskAcceptance",row.first);auto e=read.read_work(row.first);if(!e.ok)return {false,{},e.error};const auto&a=*s.acceptances.find(e.value->acceptance_source_ref);auto v=time_order(a.accepted_at,now,"FutureWorkAcceptance");if(!v.ok)return v;bool positive=false;for(const auto&id:e.value->accepted_period_refs){const auto*p=s.period_receipts.find(id);auto original=o.read_original_period(*e.value,p->source);if(!original.ok)return {false,{},original.error};positive|=original.value.actual_positive_start.has_value();v=time_order(p->actual_until,now,"FutureWorkPeriod");if(!v.ok)return v;}if(positive!=e.value->actual_start_ref.has_value())return common_failure("WorkActualStartCoverageMismatch",row.first);if(e.value->actual_start_ref){auto start=read.read_actual_start(row.first,*e.value->actual_start_ref);if(!start.ok)return {false,{},start.error};}if(e.value->terminal_result_ref){const auto*r=s.results.find(*e.value->terminal_result_ref);if(!r)return common_failure("MissingWorkTerminal",row.first);auto verified=read.read_result({"tasks",result_kind(*r),r->fact_id},1);if(!verified.ok)return {false,{},verified.error};v=time_order(r->finished_at,now,"FutureWorkResult");if(!v.ok)return v;}}
 // All original tables and headers are covered in both directions; no orphan can
 // be hidden behind an empty episode list or a current-phase-only projection.
 auto check=[&](const StableId&key,const StableId&kind,Integer schema){auto at=read_work_source_moment(common,tasks,o,{"tasks",kind,key},schema);if(!at.ok)return Status{false,{},at.error};return time_order(at.value,now,"FutureWorkSource");};for(const auto&r:s.acceptances.rows()){if(r.first!=r.second.fact_id)return common_failure("WorkAcceptanceKeyMismatch",r.first);auto v=check(r.first,"work_accepted",1);if(!v.ok)return v;}for(const auto&r:s.work_reassignment_acceptances.rows()){if(r.first!=r.second.fact_id)return common_failure("WorkReassignmentAcceptanceKeyMismatch",r.first);auto v=check(r.first,"work_reassignment_accepted",1);if(!v.ok)return v;}for(const auto&r:s.period_receipts.rows()){for(const auto&other:s.period_receipts.rows())if(other.first<r.first){auto equal=same_period(other.second.source,r.second.source);if(!equal.ok)return {false,{},equal.error};if(equal.value)return common_failure("DuplicateOriginalWorkPeriodConsumption",r.first);}if(r.first!=r.second.fact_id)return common_failure("WorkPeriodKeyMismatch",r.first);auto v=check(r.first,"work_period_receipt",2);if(!v.ok)return v;}for(const auto&r:s.actual_starts.rows()){if(r.first!=r.second.start_fact_id)return common_failure("WorkStartKeyMismatch",r.first);auto v=check(r.first,"work_actual_start",1);if(!v.ok)return v;}for(const auto&r:s.leader_changes.rows()){if(r.first!=r.second.fact_id)return common_failure("WorkLeaderKeyMismatch",r.first);auto v=check(r.first,"work_leader_changed",1);if(!v.ok)return v;}for(const auto&r:s.results.rows()){if(r.first!=r.second.fact_id)return common_failure("WorkResultKeyMismatch",r.first);auto v=check(r.first,result_kind(r.second),1);if(!v.ok)return v;}for(const auto&r:common.state().facts.rows())if(r.second.domain=="tasks"&&(r.second.kind=="work_accepted"||r.second.kind=="work_reassignment_accepted"||r.second.kind=="work_period_receipt"||r.second.kind=="work_actual_start"||r.second.kind=="work_leader_changed"||r.second.kind=="patrol_work_result"||r.second.kind=="supervised_work_result")){auto v=check(r.first,r.second.kind,r.second.kind=="work_period_receipt"?2:1);if(!v.ok)return v;}return common_success();}
}
