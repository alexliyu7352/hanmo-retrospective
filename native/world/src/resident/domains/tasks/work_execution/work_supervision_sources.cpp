#include "resident/kernel/common_fact_prefix.hpp"
#include "work_supervision_sources.hpp"
#include "work_skill_inputs.hpp"
#include "work_reassignment_sources.hpp"
#include "work_execution_detail.hpp"
#include "resident/kernel/fixed_service_history.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "hanmo/resident_world_api.hpp"
#include <limits>
#include <string_view>
namespace hanmo::resident {namespace {
using namespace work_detail;
struct Failure{RuleError error;};
[[noreturn]]void reject(const char*c,const StableId&id={},ErrorKind kind=ErrorKind::rule){throw Failure{common_failure(c,id,kind).error};}
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
template<class T,class F>Outcome<T> guarded(F f){try{return {true,f(),{}};}catch(Failure&e){return {false,{},std::move(e.error)};}}
int cmp(const WorldMoment&a,const WorldMoment&b){return take(compare_world_moments(a,b));}
SourceRef started(const StableId&id){return {"tasks","supervision_action_started",id};}
SourceRef closed(const StableId&id){return {"tasks","supervision_window_closed",id};}
SourceRef ended(const StableId&id){return {"tasks","supervision_action_ended",id};}
const TaskWorkExecutionState& state(const SupervisionSourceContext&x){
 if(!x.tasks.work_execution||!task_work_fixed_execution_terms_version(x.tasks.work_execution->version))reject("SupervisionOriginalFixedExecutionTermsRequired",{},ErrorKind::unsupported_capability);
 if(!x.definitions.bound_to(x.common,x.tasks)||!x.approvals.bound_to(x.common,x.tasks))reject("SupervisionCandidateSourcesMismatch",{},ErrorKind::internal);
 return *x.tasks.work_execution;
}
const WorkEpisode& episode(const SupervisionSourceContext&x,const StableId&id){const auto*p=state(x).episodes.find(id);if(!p||p->work_id!=id||!std::holds_alternative<SupervisedWorkPlan>(p->frozen_plan))reject("SupervisionWorkMissing",id);return *p;}
const SupervisionActionStartSource* by_action(const SupervisionSourceContext&x,std::string_view id){const SupervisionActionStartSource*out=nullptr;for(const auto&r:state(x).supervision_starts.rows())if(r.second.action_id==id){if(out)reject("DuplicateSupervisionActionStart",r.second.action_id);out=&r.second;}return out;}
const SupervisionWindowClosureSource* closure(const SupervisionSourceContext&x,const StableId&id){const SupervisionWindowClosureSource*out=nullptr;for(const auto&r:state(x).supervision_window_closures.rows())if(r.second.start_source_ref==id){if(out)reject("DuplicateSupervisionWindowClosure",id);out=&r.second;}return out;}
const SupervisionActionTerminalSource* terminal(const SupervisionSourceContext&x,const StableId&id){const SupervisionActionTerminalSource*out=nullptr;for(const auto&r:state(x).supervision_terminals.rows())if(r.second.start_source_ref==id){if(out)reject("DuplicateSupervisionTerminal",id);out=&r.second;}return out;}
std::size_t position(const CommonStore&c,const SourceRef&r){return take(read_actual_common_fact_position(c,r));}
// 完整原读口与无环前缀叶共用唯一 start/terms 绑定核，不从当前 Work phase 推历史。
const SupervisionExecutionTerms& retained_terms_binding(const TaskWorkExecutionState& state,const WorkEpisode& e,const SupervisionActionStartSource& s){
 const auto*v=state.supervision_execution_terms.find(s.action_id);
 if(!v||v->action_id!=s.action_id||v->work_id!=s.work_id||s.work_id!=e.work_id||!same(v->original_authorization_ref,e.authorization)||v->kind!=kind(e.frozen_plan)||!v->actual_start_source_ref||!same(*v->actual_start_source_ref,MomentOwnerSource{started(s.fact_id),1}))reject("SupervisionExecutionTermsBindingMismatch",s.action_id);
 take(validate_supervision_execution_terms_shape(*v));return *v;
}
const SupervisionExecutionTerms& terms(const SupervisionSourceContext&x,const SupervisionActionStartSource&s){
 const auto&e=episode(x,s.work_id);const auto*v=&retained_terms_binding(state(x),e,s);
 auto approved=take(x.approvals.read_original_supervision_execution(e,v->execution_ordinal,*v->actual_start_source_ref,s.started_at,x));take(validate_approved_supervision_execution(e,approved));
 if(v->service_limit_half_days!=approved.service_limit_half_days||v->kind!=approved.kind||!same(v->original_approved_execution_terms_ref,approved.original_terms_source))reject("SupervisionApprovedDurationMismatch",s.action_id);
 if(!approved.actual_executor_actor_id||s.actor_id!=*approved.actual_executor_actor_id||v->initial_successor_acceptance_ref.has_value()!=(approved.initial_successor_acceptance!=nullptr)||(approved.initial_successor_acceptance&&v->initial_successor_acceptance_ref->id!=approved.initial_successor_acceptance->fact_id))reject("SupervisionApprovedExecutorMismatch",s.action_id);
 const auto*c=closure(x,s.fact_id);const auto*t=terminal(x,s.fact_id);if(c&&t)reject("DuplicateSupervisionExecutionTerminal",s.action_id);
 std::optional<MomentOwnerSource> end;if(c)end=MomentOwnerSource{closed(c->fact_id),1};if(t)end=MomentOwnerSource{ended(t->fact_id),1};
 if(v->actual_terminal_result_ref.has_value()!=end.has_value()||(end&&!same(*v->actual_terminal_result_ref,*end)))reject("SupervisionTermsTerminalBacklinkMismatch",s.action_id);
 Integer ordinal=0;for(const auto&row:state(x).supervision_execution_terms.rows()){
  const auto&old=row.second;if(old.work_id!=s.work_id)continue;if(row.first!=old.action_id||old.execution_ordinal!=ordinal||ordinal==std::numeric_limits<Integer>::max())reject("SupervisionExecutionOrdinalMismatch",s.work_id);
  if(&old==v)break;
  const auto*begin=by_action(x,old.action_id);if(!begin||!old.actual_terminal_result_ref)reject("SupervisionPriorExecutionNotTerminated",s.work_id);
  const auto*old_close=closure(x,begin->fact_id);const auto*old_end=terminal(x,begin->fact_id);if((old_close!=nullptr)==(old_end!=nullptr))reject("SupervisionPriorTerminalMismatch",s.work_id);
  const auto at=old_close?old_close->closed_at:old_end->ended_at;
  if(cmp(at,s.started_at)>0||position(x.common,old.actual_terminal_result_ref->source_ref)>=position(x.common,started(s.fact_id)))reject("SupervisionExecutionOverlap",s.work_id);
  ++ordinal;
 }
 if(ordinal!=v->execution_ordinal)reject("SupervisionExecutionOrdinalMismatch",s.work_id);return *v;
}
WorldMoment finite_until(const SupervisionSourceContext&x,const SupervisionActionStartSource&s){const auto&t=terms(x,s);auto value=take(begin_fixed_elapsed(s.action_id,s.fact_id,t.original_approved_execution_terms_ref.source_ref.id,s.fact_id,s.started_at,t.service_limit_half_days,x.scale));return std::get<ElapsedRunning>(value.timing).completion_due_at;}
const SupervisionActionStartSource& start(const SupervisionSourceContext&x,const StableId&id){
 const auto*s=state(x).supervision_starts.find(id);if(!s||s->fact_id!=id||!header(x.common,id,"supervision_action_started")||s->action_id.empty()||s->actor_id.empty()||by_action(x,s->action_id)!=s)reject("SupervisionStartSourceMismatch",id);
 const auto&w=episode(x,s->work_id);const auto*a=state(x).acceptances.find(w.acceptance_source_ref);auto accepted=take(x.definitions.read_accepted_work(w.authorization));
 // until_tick is the unchanged legacy prefix projection of the goal deadline.
 // It does not establish the actual attempt duration or consume service.
 if(!a||!same(s->work_acceptance_ref,{"tasks","work_accepted",w.acceptance_source_ref})||s->until_tick!=deadline(w.frozen_plan).tick||cmp(s->started_at,accepted.earliest_service_at)<0||cmp(s->started_at,take(x.clock.read_current_moment()))>0||s->original_start_sources.empty()||!valid(s->accepted_presence_source)||s->condition_at_start.actor_id!=s->actor_id||position(x.common,s->work_acceptance_ref)>=position(x.common,started(id)))reject("SupervisionAcceptedStartMismatch",id);
 (void)terms(x,*s);
 for(std::size_t i=0;i<s->original_start_sources.size();++i){const auto&r=s->original_start_sources[i];if(!valid(r.source_ref)||r.source_schema<=0||same(r.source_ref,started(id)))reject("InvalidSupervisionStartEvidence",id);for(std::size_t j=0;j<i;++j)if(same(r,s->original_start_sources[j]))reject("DuplicateSupervisionStartEvidence",id);}
 take(x.eligibility.validate_historical_start_inputs(w,*s));const auto*skills=x.original.skill_inputs();if(!skills||!skills->bound_to(x.common,x.tasks))reject("WorkSkillInputsReadRequired",id,ErrorKind::unsupported_capability);take(skills->validate_start_qualification(w,*s));return *s;
}
void consumed_through(const SupervisionSourceContext&x,const SupervisionActionStartSource&s,const WorldMoment&through){
 const auto&w=episode(x,s.work_id);WorldMoment cursor=s.started_at;const WorkExecutionSource binding=WorkCommonActionBinding{s.action_id};
 for(const auto&id:w.accepted_period_refs){const auto*p=state(x).period_receipts.find(id);if(!p)reject("SupervisionPeriodMissing",id);if(!same(p->execution_source,binding))continue;
  if(cmp(p->window.from,cursor)||cmp(p->actual_until,through)>0||cmp(p->actual_until,p->window.from)<=0)reject("SupervisionServiceCoverageGap",id);
  take(validate_period_record(x.common,state(x),w,*p,x.original));cursor=p->actual_until;
 }
 if(cmp(cursor,through))reject("SupervisionPeriodDueUnconsumed",s.action_id);
}
void verify_closure(const SupervisionSourceContext&x,const SupervisionActionStartSource&s,const SupervisionWindowClosureSource&v){
 if(v.fact_id.empty()||v.start_source_ref!=s.fact_id||!header(x.common,v.fact_id,"supervision_window_closed")||cmp(v.closed_at,finite_until(x,s))||cmp(v.closed_at,take(x.clock.read_current_moment()))>0||position(x.common,started(s.fact_id))>=position(x.common,closed(v.fact_id)))reject("SupervisionWindowClosureMismatch",v.fact_id);
 const auto*elapsed=find_fixed_elapsed(x.elapsed,s.action_id);if(!elapsed||cmp(elapsed->last_settled_at,v.closed_at)||take(compare_fractions(elapsed->credited_units.value,elapsed->required_units.value)))reject("SupervisionClosureWithoutActualProvidedH",v.fact_id);
 consumed_through(x,s,v.closed_at);
}
void verify_terminal(const SupervisionSourceContext&x,const SupervisionActionStartSource&s,const SupervisionActionTerminalSource&v){
 if(v.fact_id.empty()||v.start_source_ref!=s.fact_id||!header(x.common,v.fact_id,"supervision_action_ended")||cmp(v.ended_at,s.started_at)<0||cmp(v.ended_at,finite_until(x,s))>0||cmp(v.ended_at,take(x.clock.read_current_moment()))>0||!valid(v.actual_release_cause.source_ref)||v.actual_release_cause.source_schema<=0||supervision_source_kind(v.actual_release_cause.source_ref.kind)||position(x.common,started(s.fact_id))>=position(x.common,ended(v.fact_id)))reject("SupervisionActualReleaseCauseRequired",v.fact_id);
 const auto&w=episode(x,s.work_id);take(x.eligibility.validate_actual_release(w,s,v.disposition,v.actual_release_cause,v.ended_at,x.original));
 const auto&cause=v.actual_release_cause;
 // The original eligibility reader above authenticates this precise typed source.
 // ConditionReceipt2 has no Common header; every other accepted cause must
 // precede the actual terminal append, including occurrences at the same M.
 const bool typed_condition=cause.source_ref.domain=="actor_foundation"&&cause.source_ref.kind=="condition_receipt"&&cause.source_schema==2;
 if(!typed_condition&&position(x.common,cause.source_ref)>=take(read_actual_common_fact_position(x.common,SourceRefBuildView{"tasks","supervision_action_ended",v.fact_id})))reject("SupervisionActualReleaseCauseRequired",v.fact_id);
 consumed_through(x,s,v.ended_at);
}
ActionTimeRegistration registration_value(Integer selected_version){ActionTimeRegistration r;r.action_kind="work_supervise";r.owner_domain="tasks";r.profile_id="task_work_execution";r.profile_version=selected_version;r.source_schema=1;return r;}
void registration(const ActionTimeRegistration&r,const SupervisionSourceContext&x){const auto a=registration_value(state(x).version);if(r.action_kind!=a.action_kind||r.owner_domain!=a.owner_domain||r.profile_id!=a.profile_id||r.profile_version!=a.profile_version||r.source_schema!=a.source_schema||r.mode!=ActionTimeMode::fixed_duration||r.remaining_meaning!=ActionRemainingMeaning::service_estimate||r.shared_gate_kind||!r.retained_wait_phases.empty())reject("SupervisionActionRegistrationMismatch",r.action_kind);}
// Preserve the original first-seen approval/input order. Only borrowed pointers
// are staged; the actual ordinary vector is charged to its exact final count
// before reserve/fill and remains owned through validation or S0 preparation.
class SupervisionInputs {
 CheckpointMemoryCharge copied_fields_;
 std::vector<FixedServiceInputRef> values_;
public:
 SupervisionInputs(const SupervisionSourceContext&x,const SupervisionActionStartSource&s){
  if(s.original_start_sources.size()==SIZE_MAX)throw std::bad_alloc();
  CheckpointVector<const MomentOwnerSource*> sources;sources.reserve(s.original_start_sources.size()+1);
  auto put=[&](const MomentOwnerSource&r){for(const auto*old:sources)if(old->source_schema==r.source_schema&&same(old->source_ref,r.source_ref))return;sources.push_back(&r);};
  put(terms(x,s).original_approved_execution_terms_ref);for(const auto&r:s.original_start_sources)put(r);


  (copied_fields_).grow(0);values_.reserve(sources.size());
  for(const auto*r:sources)values_.emplace_back(FixedAppliedInput{
   {checkpoint_fresh_string_copy(r->source_ref.domain),checkpoint_fresh_string_copy(r->source_ref.kind),checkpoint_fresh_string_copy(r->source_ref.id)},r->source_schema});
 }
 const std::vector<FixedServiceInputRef>& values()const noexcept{return values_;}
};
void candidate(const CommonStore&c,const TaskState&t,const ActionElapsedStore&elapsed,const SupervisionSourceContext&x){if(&c!=&x.common||&t!=&x.tasks||&elapsed!=&x.elapsed)reject("SupervisionCandidateMismatch",{},ErrorKind::internal);(void)state(x);}
// 同一真实开工前缀只读核；不伪造 start 来预验必须在 retained row 上读的资格。
template<class Fresh>auto supervision_start_request(const SupervisionSourceContext& x,const StableId& id,const StableId& actor,Fresh&& fresh){
 const auto now=take(x.clock.read_current_moment());WorkExecutionRead work(x.common,x.tasks,x.original);const auto*w=take(work.read_work(id));if(w->terminal_result_ref||w->current_execution||w->phase!=WorkPhase::paused)reject("SupervisionWorkNotReady",id);auto accepted=take(x.definitions.read_accepted_work(w->authorization));if(cmp(now,accepted.earliest_service_at)<0)reject("SupervisionBeforeAcceptedServiceWindow",id);
 Integer ordinal=0;for(const auto&row:state(x).supervision_execution_terms.rows())if(row.second.work_id==id){if(row.second.execution_ordinal!=ordinal||!row.second.actual_terminal_result_ref||ordinal==std::numeric_limits<Integer>::max())reject("SupervisionPriorExecutionRequired",id);++ordinal;}
 auto approved=take(x.approvals.read_current_supervision_execution(*w,ordinal,now,x));take(validate_approved_supervision_execution(*w,approved));auto eligible=take(x.eligibility.read_current_start_inputs(*w,now));const auto*a=x.common.actor(actor);if(!approved.actual_executor_actor_id||eligible.actor_id!=*approved.actual_executor_actor_id||eligible.actor_id!=actor||!a||!a->alive||!a->available||!a->action_id.empty()||eligible.actual_sources.empty())reject("SupervisionExecutorUnavailable",actor);
 return fresh(w,ordinal,approved,eligible,now);
}
void release_occupancy(CommonStore&c,const SupervisionActionStartSource&s){const auto*a=c.action(s.action_id);const auto*actor=c.actor(s.actor_id);if(!a||a->kind!="work_supervise"||a->actor_id!=s.actor_id||a->participants!=std::vector<StableId>{s.actor_id}||!actor||actor->action_id!=s.action_id)reject("SupervisionReleaseOccupancyMismatch",s.action_id);ResourceRecipe resources;resources.operations.emplace_back(ReleaseActionResources{s.action_id});take(c.apply_resources(resources));ActorMutation change;change.actor_id=s.actor_id;change.action_id=StableId{};take(c.apply_actor(change));ActionMutation erase;erase.kind=ActionMutation::Kind::erase;erase.action.id=s.action_id;take(c.apply_action(erase));}
void adopt_terminal(CommonStore&c,ActionElapsedStore&elapsed,const SupervisionSourceContext&x,const SupervisionActionStartSource&s,const SourceRef&result,const WorldMoment&at,bool exhausted){
 const auto*entry=find_fixed_elapsed(elapsed,s.action_id);const FixedServiceTransition*prior=nullptr;for(const auto&v:elapsed.fixed_service_transitions)if(v.action_ref==s.action_id){if(prior)reject("UnexpectedSupervisionFixedTransition",s.action_id);prior=&v;}
 if(!entry||!prior||prior->kind!=FixedTransitionKind::started)reject("SupervisionFixedStartRequired",s.action_id);
 WorkUnits credited=entry->credited_units;
 if(cmp(entry->last_settled_at,at)){auto slice=take(prepare_fixed_elapsed_slice(*entry,{entry->last_settled_at,at},x.scale));credited=std::move(slice.credited_units);}
 const bool full=take(compare_fractions(credited.value,entry->required_units.value))==0;if(exhausted&&!full)reject("SupervisionOriginalHNotProvided",s.action_id);
 const auto transition_id=take(c.next_owned_id("fixed_service_transition"));
 const std::vector<FixedServiceInputRef> no_inputs;
 FixedServiceTransitionInput owned{FixedServiceTerminalTransitionBuildView{
  transition_id.id(),1,s.action_id,1,prior->transition_id,FixedTransitionKind::terminal,at,credited,
  {"tasks","supervision_action_started",s.fact_id},1,{{result.domain,result.kind,result.id},1},
  no_inputs,prior->input_basis_refs,full?FixedTerminalDisposition::completed:FixedTerminalDisposition::interrupted}};
 const auto&transition=owned.row();
 SupervisionActionTimeRead reader(x);auto reg=registration_value(state(x).version);auto prepared=take(prepare_fixed_service_change(elapsed,s.action_id,transition,{},reg,x.scale,reader));take(apply_fixed_service_change_in_candidate(elapsed,std::move(prepared),reg,x.scale,reader));
}
}
bool supervision_action_kind(const StableId&v)noexcept{return v=="work_supervise";}
bool supervision_source_kind(const StableId&v)noexcept{return v=="supervision_action_started"||v=="supervision_window_closed"||v=="supervision_action_ended";}
std::vector<ActionTimeRegistration> supervision_action_time_registrations(Integer selected_version){return {registration_value(selected_version)};}
// 只读取已落原记录与 S0 关联，严格按实际 Common cut；不借未来 leader-change 自证。
Outcome<WorkExecutionBoundaryBeforeCut> read_supervision_execution_boundary_before_cut(
 const CommonStore& common,const TaskState& tasks,const ActionElapsedStore& elapsed,const TimeScaleRef& scale,
 const WorkEpisode& work,const SourceRef* cut){return guarded<WorkExecutionBoundaryBeforeCut>([&]{
 if(!tasks.work_execution||!task_work_fixed_execution_terms_version(tasks.work_execution->version)||tasks.work_execution->episodes.find(work.work_id)!=&work||!std::holds_alternative<SupervisedWorkPlan>(work.frozen_plan))reject("SupervisionBoundaryCandidateRequired",work.work_id);
 const auto& state=*tasks.work_execution;const auto& order=common.state().facts.order();
 const auto limit=cut?position(common,*cut):order.size();
 const auto* acceptance=state.acceptances.find(work.acceptance_source_ref);
 if(!acceptance||acceptance->fact_id!=work.acceptance_source_ref||acceptance->work_id!=work.work_id||!same(acceptance->authorization,work.authorization)||position(common,{"tasks","work_accepted",acceptance->fact_id})>=limit)reject("SupervisionBoundaryAcceptanceRequired",work.work_id);
 auto fact=[&](const StableId& id,const char* expected)->const FactRecord*{const auto* f=common.state().facts.find(id);if(!f||f->id!=id||f->domain!="tasks"||f->kind!=expected)reject("SupervisionBoundaryOriginalHeaderRequired",id);(void)position(common,{f->domain,f->kind,f->id});return f;};
 // Common 中任何本类事实都须有真实 payload，不能以缺 row 制造 never_started。
 for(std::size_t i=0;i<limit;++i){const auto* f=common.state().facts.find(order[i]);if(!f||f->id!=order[i])reject("SupervisionBoundaryOriginalHeaderRequired",order[i]);if(f->domain=="tasks"&&f->kind=="supervision_action_started"&&!state.supervision_starts.find(f->id))reject("SupervisionBoundaryStartCoverageRequired",f->id);}
 WorkExecutionBoundaryBeforeCut result;result.state=WorkExecutionBoundaryKind::never_started;
 Integer next=0;std::size_t previous_start=0;const WorldMoment* previous_end=nullptr;
 for(const auto& row:state.supervision_starts.rows()){
  const auto& start=row.second;if(start.work_id!=work.work_id)continue;
  if(row.first!=start.fact_id||start.action_id.empty()||start.actor_id.empty()||start.condition_at_start.actor_id!=start.actor_id||!same(start.work_acceptance_ref,{"tasks","work_accepted",acceptance->fact_id}))reject("SupervisionBoundaryStartMismatch",row.first);
  const auto* opening=fact(start.fact_id,"supervision_action_started");const auto opened=position(common,{opening->domain,opening->kind,opening->id});if(opened>=limit)continue;
  if(result.state==WorkExecutionBoundaryKind::active||(next&&opened<=previous_start)||cmp(start.started_at,acceptance->earliest_service_at)<0||(previous_end&&cmp(*previous_end,start.started_at)>0))reject("SupervisionBoundaryExecutionOverlap",start.fact_id);
  const auto& terms=retained_terms_binding(state,work,start);if(terms.execution_ordinal!=next||next==std::numeric_limits<Integer>::max())reject("SupervisionBoundaryOrdinalMismatch",start.fact_id);++next;previous_start=opened;
  std::size_t source_count=0;for(const auto& source:work.original_execution_sources)if(same(source,MomentOwnerSource{started(start.fact_id),1}))++source_count;if(source_count!=1)reject("SupervisionBoundaryExecutionCoverageRequired",start.fact_id);
  std::size_t action_count=0;for(const auto& other:state.supervision_starts.rows())if(other.second.action_id==start.action_id)++action_count;if(action_count!=1)reject("DuplicateSupervisionActionStart",start.action_id);
  const SupervisionWindowClosureSource* closure=nullptr;const SupervisionActionTerminalSource* terminal=nullptr;
  for(const auto& old:state.supervision_window_closures.rows())if(old.second.start_source_ref==start.fact_id){if(closure||old.first!=old.second.fact_id)reject("DuplicateSupervisionWindowClosure",start.fact_id);closure=&old.second;}
  for(const auto& old:state.supervision_terminals.rows())if(old.second.start_source_ref==start.fact_id){if(terminal||old.first!=old.second.fact_id)reject("DuplicateSupervisionTerminal",start.fact_id);terminal=&old.second;}
  if(closure&&terminal)reject("DuplicateSupervisionExecutionTerminal",start.fact_id);
  const FactRecord* ending=nullptr;const WorldMoment* ended_at=nullptr;
  if(closure){ending=fact(closure->fact_id,"supervision_window_closed");ended_at=&closure->closed_at;}
  if(terminal){ending=fact(terminal->fact_id,"supervision_action_ended");ended_at=&terminal->ended_at;}
  if(terms.actual_terminal_result_ref.has_value()!=(ending!=nullptr)||(ending&&(terms.actual_terminal_result_ref->source_schema!=1||!same(terms.actual_terminal_result_ref->source_ref,{ending->domain,ending->kind,ending->id}))))reject("SupervisionTermsTerminalBacklinkMismatch",start.action_id);
  result={WorkExecutionBoundaryKind::active,opening,nullptr,nullptr};previous_end=nullptr;
  if(!ending||position(common,{ending->domain,ending->kind,ending->id})>=limit)continue;
  if(position(common,{ending->domain,ending->kind,ending->id})<=opened||cmp(*ended_at,start.started_at)<0)reject("SupervisionBoundaryTerminalOrderRequired",start.fact_id);
  if(terminal){
   const auto& cause=terminal->actual_release_cause;
   if(!valid(cause.source_ref)||cause.source_schema<1||supervision_source_kind(cause.source_ref.kind))reject("SupervisionBoundaryTerminalCauseOrderRequired",start.fact_id);
   // 原 condition_receipt/2 没有 Common header；完整 release reader 仍核其真实条件源。
   const bool typed_condition=cause.source_ref.domain=="actor_foundation"&&cause.source_ref.kind=="condition_receipt"&&cause.source_schema==2;
   if(!typed_condition&&(position(common,cause.source_ref)<=opened||position(common,cause.source_ref)>=position(common,{ending->domain,ending->kind,ending->id})))reject("SupervisionBoundaryTerminalCauseOrderRequired",start.fact_id);
  }
  const auto* entry=find_fixed_elapsed(elapsed,start.action_id);const auto* timing=entry?std::get_if<ElapsedTerminal>(&entry->timing):nullptr;
  if(!entry||!timing||timing->outcome_ref!=ending->id||entry->execution_source_ref!=start.fact_id||entry->duration_definition_ref!=terms.original_approved_execution_terms_ref.source_ref.id||cmp(entry->started_at,start.started_at)||cmp(entry->last_settled_at,*ended_at))reject("SupervisionBoundaryElapsedTerminalRequired",start.action_id);
  const auto initial=take(begin_fixed_elapsed(start.action_id,start.fact_id,terms.original_approved_execution_terms_ref.source_ref.id,start.fact_id,start.started_at,terms.service_limit_half_days,scale));
  if(take(compare_fractions(entry->required_units.value,initial.required_units.value))||cmp(*ended_at,std::get<ElapsedRunning>(initial.timing).completion_due_at)>0||(closure&&take(compare_fractions(entry->credited_units.value,entry->required_units.value))))reject("SupervisionBoundaryElapsedTermsMismatch",start.action_id);
  const ActionElapsedTerminalBinding* binding=nullptr;for(const auto& old:elapsed.terminal_bindings)if(old.action_ref==start.action_id){if(binding)reject("SupervisionBoundaryDuplicateTerminalBinding",start.action_id);binding=&old;}
  const auto* bound_start=binding?std::get_if<MomentOwnerSource>(&binding->started_source_ref):nullptr;const auto* bound_end=binding?std::get_if<MomentOwnerSource>(&binding->ended_source_ref):nullptr;
  if(!binding||binding->action_kind!="work_supervise"||binding->actor_id!=start.actor_id||binding->participant_actor_ids.size()!=1||binding->participant_actor_ids.front()!=start.actor_id||!same(binding->original_execution_source_ref,started(start.fact_id))||binding->terminal_result_ref!=ending->id||!bound_start||!same(*bound_start,MomentOwnerSource{started(start.fact_id),1})||!bound_end||bound_end->source_schema!=1||!same(bound_end->source_ref,{ending->domain,ending->kind,ending->id}))reject("SupervisionBoundaryTerminalBindingRequired",start.action_id);
  const FixedServiceTransition* first=nullptr;const FixedServiceTransition* last=nullptr;
  for(const auto& transition:elapsed.fixed_service_transitions)if(transition.action_ref==start.action_id){if(transition.kind==FixedTransitionKind::started&&!first)first=&transition;else if(transition.kind==FixedTransitionKind::terminal&&!last)last=&transition;else reject("SupervisionUnexpectedFixedTransition",start.action_id);}
  const auto* adopted=first?std::get_if<FixedOwnerAdopted>(&first->cause):nullptr;const auto* stopped=last?std::get_if<FixedOwnerTerminated>(&last->cause):nullptr;
  if(!first||!last||!adopted||!stopped||first->transition_ordinal!=0||first->previous_transition_ref||last->transition_ordinal!=1||!last->previous_transition_ref||*last->previous_transition_ref!=first->transition_id||cmp(first->occurred_at,start.started_at)||cmp(last->occurred_at,*ended_at)||adopted->owner_source_schema!=1||!same(adopted->owner_source_ref,started(start.fact_id))||stopped->owner_result_schema!=1||!same(stopped->owner_result_ref,{ending->domain,ending->kind,ending->id})||!last->terminal_disposition||take(compare_fractions(last->credited_at.value,entry->credited_units.value)))reject("SupervisionBoundaryTransitionRequired",start.action_id);
  const auto full=take(compare_fractions(entry->credited_units.value,entry->required_units.value));
  if(full>0||*last->terminal_disposition!=(full==0?FixedTerminalDisposition::completed:FixedTerminalDisposition::interrupted))reject("SupervisionBoundaryTerminalDispositionMismatch",start.action_id);
  // 历史 cut 不拿今日别的 Action 当冲突；只禁止这个已终局 Action/占用仍存在。
  const auto* actor=common.actor(start.actor_id);if(common.action(start.action_id)||(actor&&actor->action_id==start.action_id)||(work.current_execution&&same(*work.current_execution,WorkExecutionSource{WorkCommonActionBinding{start.action_id}})))reject("SupervisionBoundaryActionNotReleased",start.action_id);
  result={WorkExecutionBoundaryKind::ended,opening,ending,ended_at};previous_end=ended_at;
 }
 // 两向覆盖原 Work binding 与 start/terms，不能只依当前 binding 的空值。
 for(const auto& source:work.original_execution_sources){if(source.source_schema!=1||source.source_ref.domain!="tasks"||source.source_ref.kind!="supervision_action_started")reject("SupervisionBoundaryExecutionSourceRequired",source.source_ref.id);const auto* start=state.supervision_starts.find(source.source_ref.id);if(!start||start->work_id!=work.work_id)reject("SupervisionBoundaryExecutionCoverageRequired",source.source_ref.id);(void)position(common,source.source_ref);}
 for(const auto& row:state.supervision_execution_terms.rows())if(row.second.work_id==work.work_id){const auto& terms=row.second;if(row.first!=terms.action_id||!terms.actual_start_source_ref)reject("SupervisionBoundaryTermsCoverageRequired",row.first);const auto* start=state.supervision_starts.find(terms.actual_start_source_ref->source_ref.id);if(!start||start->work_id!=work.work_id||start->action_id!=terms.action_id)reject("SupervisionBoundaryTermsCoverageRequired",row.first);}
 return result;
});}
Outcome<const SupervisionActionStartSource*> read_supervision_action_start(const SupervisionSourceContext&x,const StableId&id){return guarded<const SupervisionActionStartSource*>([&]{return &start(x,id);});}
Outcome<const SupervisionExecutionTerms*> read_supervision_execution_terms(const SupervisionSourceContext&x,const StableId&action){return guarded<const SupervisionExecutionTerms*>([&]{const auto*s=by_action(x,action);if(!s)reject("SupervisionStartMissing",action);return &terms(x,start(x,s->fact_id));});}
Outcome<WorldMoment> read_supervision_execution_until(const SupervisionSourceContext&x,const SupervisionActionStartSource&s){return guarded<WorldMoment>([&]{if(&start(x,s.fact_id)!=&s)reject("SupervisionOriginalStartRequired",s.fact_id);return finite_until(x,s);});}
Outcome<WorkExecutionSourceRead> read_supervision_work_execution(const SupervisionSourceContext&x,const WorkEpisode&w,const MomentOwnerSource&r){return guarded<WorkExecutionSourceRead>([&]{if(r.source_schema!=1||r.source_ref.domain!="tasks"||r.source_ref.kind!="supervision_action_started")reject("SupervisionExecutionSourceRequired",r.source_ref.id);const auto&s=start(x,r.source_ref.id);if(s.work_id!=w.work_id)reject("SupervisionExecutionWorkMismatch",s.fact_id);WorkExecutionSourceRead out;out.binding=WorkCommonActionBinding{s.action_id};out.original_source=r;out.actual_leader_actor_id=s.actor_id;out.started_at=s.started_at;out.finite_until=finite_until(x,s);
 // Narrow immutable projection does not recurse through WorkResult/period validation.
 if(const auto*t=terminal(x,s.fact_id)){out.original_terminal=MomentOwnerSource{ended(t->fact_id),1};out.ended_at=t->ended_at;}
 else if(const auto*c=closure(x,s.fact_id)){out.original_terminal=MomentOwnerSource{closed(c->fact_id),1};out.ended_at=c->closed_at;}return out;});}
Outcome<WorldMoment> read_supervision_source_moment(const SupervisionSourceContext&x,const SourceRef&r,Integer schema){return guarded<WorldMoment>([&]{if(schema!=1||r.domain!="tasks")reject("SupervisionSourceSchemaMismatch",r.id);if(r.kind=="supervision_action_started")return start(x,r.id).started_at;if(r.kind=="supervision_window_closed"){const auto*v=state(x).supervision_window_closures.find(r.id);if(!v||v->fact_id!=r.id)reject("SupervisionClosureMissing",r.id);verify_closure(x,start(x,v->start_source_ref),*v);return v->closed_at;}if(r.kind=="supervision_action_ended"){const auto*v=state(x).supervision_terminals.find(r.id);if(!v||v->fact_id!=r.id)reject("SupervisionTerminalMissing",r.id);verify_terminal(x,start(x,v->start_source_ref),*v);return v->ended_at;}reject("UnsupportedSupervisionSource",r.id);});}
Status validate_supervision_start_request(const SupervisionSourceContext& x,const StableId& id,const StableId& actor){return guarded<Done>([&]{
 (void)state(x);return supervision_start_request(x,id,actor,
 [](const WorkEpisode*,Integer,const ApprovedSupervisionExecution&,const SupervisionStartInputs&,const WorldMoment&){return Done{};});
});}
Outcome<SourceRef> begin_supervision_action_in_candidate(CommonStore&c,TaskState&t,ActionElapsedStore&elapsed,const SupervisionSourceContext&x,const StableId&id,const StableId&actor){return guarded<SourceRef>([&]{
 candidate(c,t,elapsed,x);return supervision_start_request(x,id,actor,
 [&](const WorkEpisode* w,Integer ordinal,const ApprovedSupervisionExecution& approved,SupervisionStartInputs& eligible,const WorldMoment& now)->SourceRef{
 SupervisionActionStartSource s;s.fact_id=take(c.next_id("supervision_action_started"));s.action_id=take(c.next_id("action"));s.work_id=id;s.actor_id=actor;s.work_acceptance_ref={"tasks","work_accepted",w->acceptance_source_ref};s.started_at=now;s.until_tick=deadline(w->frozen_plan).tick;s.accepted_presence_source=std::move(eligible.accepted_presence_source);s.condition_at_start=std::move(eligible.condition_at_start);s.original_start_sources=std::move(eligible.actual_sources);
 SupervisionExecutionTerms row{id,ordinal,w->authorization,approved.original_terms_source,s.action_id,approved.kind,approved.service_limit_half_days,MomentOwnerSource{started(s.fact_id),1},{}};
 if(approved.initial_successor_acceptance){if(!task_work_lifecycle_sources_version(state(x).version)||ordinal!=0)reject("SupervisionInitialSuccessorAcceptanceRequired",id);const auto&accepted=*approved.initial_successor_acceptance;row.initial_successor_acceptance_ref=SourceRef{"tasks","work_reassignment_accepted",accepted.fact_id};}
 take(validate_supervision_execution_terms_shape(row));
 auto entry=take(begin_fixed_elapsed(s.action_id,s.fact_id,row.original_approved_execution_terms_ref.source_ref.id,s.fact_id,now,row.service_limit_half_days,x.scale));ActionCoreView action{s.action_id,"work_supervise",actor,{actor},now.half_day_tick,take(fixed_elapsed_remaining_ticks(entry,x.scale))};ActionMutation insert;insert.kind=ActionMutation::Kind::insert;insert.action=action;take(c.apply_action(insert));ActorMutation occupy;occupy.actor_id=actor;occupy.action_id=s.action_id;take(c.apply_actor(occupy));take(c.append_fact({s.fact_id,"supervision_action_started","tasks"}));const auto ref=started(s.fact_id);
 if(!t.work_execution->supervision_starts.insert(ref.id,std::move(s))||!t.work_execution->supervision_execution_terms.insert(action.id,std::move(row)))reject("DuplicateSupervisionStart",ref.id);
 const auto*skills=x.original.skill_inputs();if(!skills||!skills->bound_to(c,t))reject("WorkSkillInputsReadRequired",ref.id,ErrorKind::unsupported_capability);auto*saved=t.work_execution->supervision_starts.find(ref.id);saved->skill_qualification=take(skills->read_start_qualification(*w,*saved));take(bind_original_work_execution_in_candidate(t,x.original,id,{ref,1},now));
 const SupervisionInputs original_inputs(x,*saved);
 const auto transition_id=take(c.next_owned_id("fixed_service_transition"));
 const std::vector<FixedServiceInputRef> no_inputs;
 FixedServiceTransitionInput owned{FixedServiceAdoptionTransitionBuildView{
  transition_id.id(),1,action.id,0,std::nullopt,FixedTransitionKind::started,now,entry.credited_units,
  {ref.domain,ref.kind,ref.id},1,{{ref.domain,ref.kind,ref.id},1},original_inputs.values(),no_inputs,std::nullopt}};
 const auto&transition=owned.row();SupervisionActionTimeRead reader(x);auto reg=registration_value(state(x).version);auto prepared=take(prepare_fixed_service_adoption(action,entry,transition,reg,x.scale,reader));take(apply_fixed_service_change_in_candidate(elapsed,std::move(prepared),reg,x.scale,reader));
 if(task_work_lifecycle_sources_version(t.work_execution->version)){
  if(!x.reassignments)reject("WorkReassignmentOriginalSourcesRequired",id);
  const auto identity=take(read_governance_supervision_executor_identity_at(x.reassignments->governance.history,*w,now,&ref));
  const auto predecessor=take(read_governance_work_executor_before_cut(x.reassignments->governance.history,*w,nullptr));
  if(identity.acceptance&&predecessor.actor_id&&*predecessor.actor_id!=*identity.actor_id){
   WorkLeaderChange change;change.work_id=id;change.old_actor_id=*predecessor.actor_id;change.new_actor_id=*identity.actor_id;change.authority_source={identity.acceptance->reassignment_approval_ref,1};change.actual_consent_source={{"tasks","work_reassignment_accepted",identity.acceptance->fact_id},1};change.actual_arrival_source={ref,1};change.replacement_at=now;change.new_execution_source=WorkCommonActionBinding{action.id};change.original_execution_source={ref,1};
   take(replace_work_leader_in_candidate(c,t,x.original,id,std::move(change),now));
  }
 }
 return ref;
 });
});}
Outcome<SourceRef> close_supervision_window_in_candidate(CommonStore&c,TaskState&t,ActionElapsedStore&elapsed,const SupervisionSourceContext&x,const StableId&id){return guarded<SourceRef>([&]{
 candidate(c,t,elapsed,x);const auto*begun=by_action(x,id);if(!begun)reject("SupervisionStartMissing",id);const auto&s=start(x,begun->fact_id);if(const auto*old=closure(x,s.fact_id)){verify_closure(x,s,*old);if(c.action(id))reject("SupervisionClosedActionStillPresent",id);return closed(old->fact_id);}if(terminal(x,s.fact_id)||!c.action(id))reject("SupervisionAlreadyReleased",id);const auto now=take(x.clock.read_current_moment());const auto*entry=find_fixed_elapsed(elapsed,id);if(!entry)reject("SupervisionFixedElapsedRequired",id);take(validate_fixed_elapsed_at_completion_due(*entry,now,x.scale));if(cmp(now,finite_until(x,s)))reject("SupervisionWindowCloseNotAtDue",id);consumed_through(x,s,now);
 SupervisionWindowClosureSource v{take(c.next_id("supervision_window_closed")),s.fact_id,now};take(c.append_fact({v.fact_id,"supervision_window_closed","tasks"}));const auto ref=closed(v.fact_id);if(!t.work_execution->supervision_window_closures.insert(ref.id,std::move(v)))reject("DuplicateSupervisionClosure",id);auto*terms_row=t.work_execution->supervision_execution_terms.find(id);terms_row->actual_terminal_result_ref=MomentOwnerSource{ref,1};auto*w=t.work_execution->episodes.find(s.work_id);if(!w||w->terminal_result_ref)reject("SupervisionWorkAlreadyTerminal",id);w->last_interruption_ref=ref.id;
 adopt_terminal(c,elapsed,x,s,ref,now,true);release_occupancy(c,s);take(close_original_work_execution_binding_in_candidate(t,x.original,w->work_id,{ref,1},now));return ref;
});}
Outcome<SourceRef> release_supervision_action_in_candidate(CommonStore&c,TaskState&t,ActionElapsedStore&elapsed,const SupervisionSourceContext&x,const StableId&id,SupervisionReleaseKind disposition,const MomentOwnerSource&cause){return guarded<SourceRef>([&]{
 candidate(c,t,elapsed,x);const auto*begun=by_action(x,id);if(!begun)reject("SupervisionStartMissing",id);const auto&s=start(x,begun->fact_id);const auto now=take(x.clock.read_current_moment());if(const auto*old=terminal(x,s.fact_id)){verify_terminal(x,s,*old);if(old->disposition!=disposition||!same(old->actual_release_cause,cause)||c.action(id))reject("SupervisionReleaseConflict",id);return ended(old->fact_id);}if(closure(x,s.fact_id))reject("SupervisionAttemptAlreadyEnded",id);const auto&w=episode(x,s.work_id);take(x.eligibility.validate_actual_release(w,s,disposition,cause,now,x.original));consumed_through(x,s,now);
 SupervisionActionTerminalSource v{take(c.next_id("supervision_action_ended")),s.fact_id,now,disposition,cause};take(c.append_fact({v.fact_id,"supervision_action_ended","tasks"}));const auto ref=ended(v.fact_id);if(!t.work_execution->supervision_terminals.insert(ref.id,std::move(v)))reject("DuplicateSupervisionTerminal",id);t.work_execution->supervision_execution_terms.find(id)->actual_terminal_result_ref=MomentOwnerSource{ref,1};if(disposition==SupervisionReleaseKind::actual_interruption)t.work_execution->episodes.find(s.work_id)->last_interruption_ref=ref.id;adopt_terminal(c,elapsed,x,s,ref,now,false);release_occupancy(c,s);take(close_original_work_execution_binding_in_candidate(t,x.original,w.work_id,{ref,1},now));return ref;
});}
Status refresh_supervision_remaining_in_candidate(CommonStore&c,const SupervisionSourceContext&x){return guarded<Done>([&]{if(&c!=&x.common)reject("SupervisionCandidateMismatch",{},ErrorKind::internal);for(const auto&r:state(x).supervision_starts.rows()){const auto&s=start(x,r.first);if(terminal(x,s.fact_id)||closure(x,s.fact_id))continue;const auto*a=c.action(s.action_id);const auto*e=find_fixed_elapsed(x.elapsed,s.action_id);if(!a||!e)reject("SupervisionFixedActionMissing",s.action_id);const auto n=take(fixed_elapsed_remaining_ticks(*e,x.scale));if(a->remaining_ticks!=n){ActionMutation m;m.action=*a;m.remaining_ticks=n;take(c.apply_action(m));}}return Done{};});}
Status SupervisionActionTimeRead::enumerate_action_time_owners(const ActionTimeRegistration&r,ActionTimeOwnerIdSink&sink)const{return guarded<Done>([&]{registration(r,context_);for(const auto&row:state(context_).supervision_starts.rows())take(sink.append(start(context_,row.first).action_id));return Done{};});}
Outcome<ActionTimeOwnerProof> SupervisionActionTimeRead::read_action_time_owner(const ActionTimeRegistration&r,const StableId&id)const{return guarded<ActionTimeOwnerProof>([&]{
 registration(r,context_);const auto*begun=by_action(context_,id);if(!begun)reject("SupervisionStartMissing",id);
 const auto&s=start(context_,begun->fact_id);ActionTimeOwnerPhase phase=ActionTimeOwnerPhase::working;
 std::optional<ElapsedTimeSourceBuildView> ended_source;std::optional<std::string_view> result;
 if(const auto*t=terminal(context_,s.fact_id)){
  verify_terminal(context_,s,*t);phase=ActionTimeOwnerPhase::terminal;
  ended_source=ElapsedTimeSourceBuildView{MomentOwnerSourceBuildView{{"tasks","supervision_action_ended",t->fact_id},1}};result=t->fact_id;
 }else if(const auto*c=closure(context_,s.fact_id)){
  verify_closure(context_,s,*c);phase=ActionTimeOwnerPhase::terminal;
  ended_source=ElapsedTimeSourceBuildView{MomentOwnerSourceBuildView{{"tasks","supervision_window_closed",c->fact_id},1}};result=c->fact_id;
 }else{
  const auto*a=context_.common.action(id);const auto*actor=context_.common.actor(s.actor_id);
  if(!a||a->kind!=r.action_kind||a->actor_id!=s.actor_id||a->participants.size()!=1||
     a->participants[0]!=s.actor_id||a->start_tick!=s.started_at.half_day_tick||!actor||actor->action_id!=id)
   reject("SupervisionActionOccupancyMismatch",id);
 }
 CheckpointVector<std::string_view> participants;participants.reserve(1);participants.push_back(s.actor_id);
 const CheckpointVector<std::string_view> no_gates;
 const SourceRefBuildView original{"tasks","supervision_action_started",s.fact_id};
 return ActionTimeOwnerProof{ActionTimeOwnerProofBuildView{id,"work_supervise",s.actor_id,{participants},
  original,1,MomentOwnerSourceBuildView{original,1},phase,std::nullopt,ended_source,result,{no_gates},std::nullopt}};
});}
Outcome<WorldMoment> SupervisionActionTimeRead::read_action_time_source(const ElapsedTimeSource&s)const{const auto*m=std::get_if<MomentOwnerSource>(&s);return m?read_supervision_source_moment(context_,m->source_ref,m->source_schema):fail<WorldMoment>("SupervisionRequiresActualMomentSource");}
Status SupervisionActionTimeRead::validate_fixed_action_sources(const ActionTimeRegistration&r,const ActionTimeOwnerProof&p,const ActionElapsedState&e,const std::vector<FixedServiceTransition>&rows)const{return guarded<Done>([&]{
 const auto actual=take(read_action_time_owner(r,p.action_ref));const auto*s=by_action(context_,p.action_ref);const auto&t=terms(context_,*s);const SupervisionInputs original_inputs(context_,*s);const auto&expected=original_inputs.values();
 if(!same(p.original_execution_source,actual.original_execution_source)||e.execution_source_ref!=s->fact_id||e.owner_cost_state_ref!=s->fact_id||e.duration_definition_ref!=t.original_approved_execution_terms_ref.source_ref.id||e.owner_work_remainder!=0||cmp(e.started_at,s->started_at))reject("SupervisionFixedEntryMismatch",p.action_ref);
 const auto required=take(fixed_duration_work_units(t.service_limit_half_days,context_.scale.units_per_half_day));if(take(compare_fractions(required.value,e.required_units.value)))reject("SupervisionFixedApprovedHMismatch",p.action_ref);
 const FixedServiceTransition*first=nullptr;const FixedServiceTransition*last=nullptr;std::size_t count=0;for(const auto&v:rows)if(v.action_ref==p.action_ref){++count;if(v.kind==FixedTransitionKind::started&&!first)first=&v;else if(v.kind==FixedTransitionKind::terminal&&!last)last=&v;else reject("SupervisionUnexpectedFixedTransition",p.action_ref);if(v.execution_source_schema!=1||!same(v.execution_source_ref,actual.original_execution_source))reject("SupervisionFixedOwnerMismatch",p.action_ref);}
 const bool complete=actual.phase==ActionTimeOwnerPhase::terminal;const auto*adopted=first?std::get_if<FixedOwnerAdopted>(&first->cause):nullptr;
 if(!first||!adopted||count!=(complete?2u:1u)||first->transition_ordinal!=0||first->previous_transition_ref||!first->previous_input_basis_refs.empty()||first->terminal_disposition||take(compare_fractions(first->credited_at.value,CanonicalFraction{}))||first->input_basis_refs.size()!=expected.size()||adopted->owner_source_schema!=1||!same(adopted->owner_source_ref,actual.original_execution_source)||cmp(first->occurred_at,s->started_at))reject("SupervisionFixedStartHistoryMismatch",p.action_ref);
 for(std::size_t i=0;i<expected.size();++i)if(!same_fixed_input_ref(first->input_basis_refs[i],expected[i]))reject("SupervisionFixedInputMismatch",p.action_ref);
 if(cmp(s->started_at,e.last_settled_at)<0)take(context_.eligibility.validate_actual_service_span(episode(context_,s->work_id),*s,{s->started_at,e.last_settled_at}));
 if(complete){const auto*cause=last?std::get_if<FixedOwnerTerminated>(&last->cause):nullptr;const auto*end=actual.ended_source?std::get_if<MomentOwnerSource>(&*actual.ended_source):nullptr;const bool full=take(compare_fractions(e.credited_units.value,e.required_units.value))==0;const auto disposition=full?FixedTerminalDisposition::completed:FixedTerminalDisposition::interrupted;
  if(!last||!cause||!end||last->transition_ordinal!=1||!last->previous_transition_ref||*last->previous_transition_ref!=first->transition_id||!last->input_basis_refs.empty()||last->previous_input_basis_refs.size()!=expected.size()||cause->owner_result_schema!=1||!same(cause->owner_result_ref,end->source_ref)||last->terminal_disposition!=disposition||cmp(last->occurred_at,take(read_action_time_source(*actual.ended_source))))reject("SupervisionFixedTerminalHistoryMismatch",p.action_ref);
  if(closure(context_,s->fact_id)&&!full)reject("SupervisionExhaustedBeforeApprovedH",p.action_ref);for(std::size_t i=0;i<expected.size();++i)if(!same_fixed_input_ref(last->previous_input_basis_refs[i],expected[i]))reject("SupervisionTerminalInputMismatch",p.action_ref);
 }return Done{};
});}
Status SupervisionActionTimeRead::validate_shared_action_sources(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const SharedWorkElapsedState&)const{return common_failure("SupervisionIsFixedService",{},ErrorKind::internal);}
Outcome<RunStop> SupervisionActionTimeRead::read_action_continuation_stop(const ActionTimeRegistration&r,const ActionTimeOwnerProof&p,const WorldMoment&)const{return guarded<RunStop>([&]{const auto proof=take(read_action_time_owner(r,p.action_ref));if(proof.phase!=ActionTimeOwnerPhase::terminal)return RunStop::budget_exhausted;const auto*s=by_action(context_,p.action_ref);if(const auto*t=terminal(context_,s->fact_id))return t->disposition==SupervisionReleaseKind::work_completed?RunStop::completed:RunStop::paused;return RunStop::paused;});}
Outcome<OriginalActionRemaining> SupervisionActionTimeRead::read_original_action_remaining(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const WorldMoment&)const{return fail<OriginalActionRemaining>("SupervisionRemainingOwnedByFixedElapsed");}
Outcome<ExhaustedAuthorizationWindow> SupervisionActionTimeRead::read_exhausted_authorization_window(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const WorldMoment&)const{return fail<ExhaustedAuthorizationWindow>("SupervisionIsNotAuthorizationWindow");}
Outcome<StableId> SupervisionActionTimeRead::read_gate_result_id(const SharedWorkGateResult&)const{return fail<StableId>("SupervisionHasNoSharedGate");}
Outcome<SharedWorkGateResult> SupervisionActionTimeRead::read_original_gate_result(const StableId&id)const{return fail<SharedWorkGateResult>("SupervisionHasNoSharedGate",id);}
Status SupervisionActionTimeRead::validate_original_gate_result(const SharedWorkGateResult&)const{return common_failure("SupervisionHasNoSharedGate");}
Status validate_supervision_sources(const SupervisionSourceContext&x){return guarded<Done>([&]{
 const auto&s=state(x);SupervisionActionTimeRead read(x);const auto reg=registration_value(state(x).version);
 class ValidateOriginal final:public ActionTimeOwnerIdSink {
  const SupervisionSourceContext&x_;const SupervisionActionTimeRead&read_;const ActionTimeRegistration&reg_;
 public:
  ValidateOriginal(const SupervisionSourceContext&x,const SupervisionActionTimeRead&r,const ActionTimeRegistration&reg):x_(x),read_(r),reg_(reg){}
  Status append(std::string_view borrowed_id)override{return guarded<Done>([&]{
   const auto*source=by_action(x_,borrowed_id);if(!source)reject("SupervisionStartMissing");
   const auto&id=source->action_id;auto proof=take(read_.read_action_time_owner(reg_,id));
   const auto*entry=find_fixed_elapsed(x_.elapsed,id);if(!entry)reject("SupervisionFixedElapsedMissing",id);
   take(read_.validate_fixed_action_sources(reg_,proof,*entry,x_.elapsed.fixed_service_transitions));
   if(proof.phase==ActionTimeOwnerPhase::terminal&&x_.common.action(id))reject("SupervisionTerminalActionNotReleased",id);
   return Done{};
  });}
 }owners(x,read,reg);
 take(read.enumerate_action_time_owners(reg,owners));
 for(const auto&row:s.supervision_starts.rows()){const auto&v=start(x,row.first);const auto&w=episode(x,v.work_id);Integer n=0;for(const auto&ref:w.original_execution_sources)if(same(ref,MomentOwnerSource{started(v.fact_id),1}))++n;if(n!=1)reject("SupervisionExecutionBacklinkMismatch",v.fact_id);}
 for(const auto&row:s.supervision_execution_terms.rows()){const auto*begun=by_action(x,row.first);if(!begun||row.first!=row.second.action_id||&terms(x,*begun)!=&row.second)reject("OrphanSupervisionExecutionTerms",row.first);}
 for(const auto&r:s.supervision_window_closures.rows()){if(r.first!=r.second.fact_id||closure(x,r.second.start_source_ref)!=&r.second)reject("SupervisionClosureKeyMismatch",r.first);take(read_supervision_source_moment(x,closed(r.first),1));}for(const auto&r:s.supervision_terminals.rows()){if(r.first!=r.second.fact_id||terminal(x,r.second.start_source_ref)!=&r.second)reject("SupervisionTerminalKeyMismatch",r.first);take(read_supervision_source_moment(x,ended(r.first),1));}
 for(const auto&r:x.common.state().facts.rows())if(r.second.domain=="tasks"&&supervision_source_kind(r.second.kind))take(read_supervision_source_moment(x,{"tasks",r.second.kind,r.first},1));for(const auto&id:x.common.action_order()){const auto*a=x.common.action(id);if(!a)continue;if(a->kind=="agronomy_supervision"||a->kind=="industry_supervision"||a->kind=="construction_supervision")reject("LegacySupervisionVExecutionUnsupported",id,ErrorKind::unsupported_capability);if(supervision_action_kind(a->kind)&&!by_action(x,id))reject("OrphanSupervisionAction",id);}return Done{};
});}
}
