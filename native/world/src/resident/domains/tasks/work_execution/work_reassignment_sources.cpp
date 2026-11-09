#include "work_reassignment_sources.hpp"
#include "work_supervision_sources.hpp"
#include "work_execution_detail.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
[[noreturn]] void reject(const char* code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
void take(Status value){if(!value.ok)throw Failure{std::move(value.error)};}
template<class T,class F>Outcome<T> guard(F&& fn){try{return {true,fn(),{}};}catch(Failure& f){return {false,{},std::move(f.error)};}}
int compare(const WorldMoment& a,const WorldMoment& b){return take(compare_world_moments(a,b));}
std::size_t position(const CommonStore& c,const SourceRef& ref){const auto* h=c.state().facts.find(ref.id);const auto& ids=c.state().facts.order();const auto found=std::find(ids.begin(),ids.end(),ref.id);if(!h||h->id!=ref.id||h->domain!=ref.domain||h->kind!=ref.kind||found==ids.end())reject("WorkReassignmentSourceMissing",ref.id);return static_cast<std::size_t>(found-ids.begin());}
// 所有依赖必须是本次同 Work/治理根，原 stamp 的完整校验仍由原权源负责。
const TaskWorkExecutionState& selected(const WorkReassignmentSourceContext& x){
 const auto& t=x.tasks;const auto& h=x.governance.history;
 if(!t.tasks.work_execution||!task_work_lifecycle_sources_version(t.tasks.work_execution->version)||&t.common!=&h.programs.common||&t.tasks!=&h.tasks||&t.governance!=&h.programs||!h.task_definitions.bound_to(t.common,t.tasks))reject("WorkReassignmentCandidateMismatch");
 take(validate_governance_work_lifecycle_bindings(x.governance,x.original_sources_stamp));return *t.tasks.work_execution;
}
const WorkEpisode& episode(const WorkReassignmentSourceContext& x,const StableId& id){const auto* e=selected(x).episodes.find(id);if(!e||e->work_id!=id)reject("WorkReassignmentWorkMissing",id);return *e;}
// 冷接受按真实批准 cut 重验；本人接触不能由批准、同位置或后来的 leader change 代替。
const WorkReassignmentAcceptanceSource& accepted(const WorkReassignmentSourceContext& x,const SourceRef& ref){
 const auto& state=selected(x);const auto& t=x.tasks;
 const auto* a=state.work_reassignment_acceptances.find(ref.id);
 if(ref.domain!="tasks"||ref.kind!="work_reassignment_accepted"||!a||a->fact_id!=ref.id)reject("WorkReassignmentAcceptanceRequired",ref.id);
 const auto cut=position(t.common,ref);const auto& e=episode(x,a->work_id);
 const auto* authority=take(read_governance_work_reassignment(x.governance,a->reassignment_approval_ref,x.original_sources_stamp));
 if(authority->work_id!=e.work_id||authority->new_actor_id!=a->new_actor_id||!work_detail::same(a->work_acceptance_ref,authority->work_acceptance_ref)||!work_detail::same(a->work_acceptance_ref,{"tasks","work_accepted",e.acceptance_source_ref})||position(t.common,a->reassignment_approval_ref)>=cut||compare(authority->issued_at,a->accepted_at)>0||compare(a->accepted_at,take(t.clock.read_current_moment()))>0)reject("WorkReassignmentAcceptanceMismatch",ref.id);
 const auto actual=take(read_governance_work_executor_before_cut(x.governance.history,e,&ref));
 if(!actual.actor_id||!actual.predecessor||authority->old_actor_id!=*actual.actor_id||authority->executor_predecessor_ref.source_schema!=1||!work_detail::same(authority->executor_predecessor_ref.source_ref,{actual.predecessor->domain,actual.predecessor->kind,actual.predecessor->id}))reject("WorkReassignmentAcceptancePredecessorChanged",ref.id);
 if(const auto* supervision=std::get_if<SupervisedWorkPlan>(&e.frozen_plan);supervision&&compare(a->accepted_at,WorldMoment{supervision->completion_deadline.tick,{}})>=0)reject("WorkReassignmentAcceptanceOutsideWindow",e.work_id);
 take(validate_governance_work_reassignment_acceptance_boundary(x.governance,e,a->accepted_at,&ref,x.original_sources_stamp));
 take(validate_work_acceptance_contact(t,a->acceptance_contact,authority->issuer.actor_id,a->new_actor_id,a->accepted_at));
 if(position(t.common,a->acceptance_contact.source_ref)>=cut)reject("WorkReassignmentContactAfterAcceptance",ref.id);
 take(work_detail::time_order(a->accepted_at,a->earliest_service_at,"WorkReassignmentServiceBeforeAcceptance"));
 take(validate_work_acceptance_service_window(t,a->accepted_at,a->earliest_service_at,a->accepted_period_id));
 for(const auto& row:state.work_reassignment_acceptances.rows())if(&row.second!=a&&row.second.work_id==e.work_id){
  const auto* other=t.governance.state.work_reassignment_approvals.find(row.second.reassignment_approval_ref.id);
  if(other&&work_detail::same(other->executor_predecessor_ref,authority->executor_predecessor_ref))reject("WorkReassignmentAcceptanceConflict",e.work_id);
 }
 return *a;
}
// 不在预检创建事实或接受。候选提交重新运行此同一核，不保留跨 clone 捕获。
template<class Existing,class Fresh>auto acceptance_request(const WorkReassignmentSourceContext& x,const TaskWorkExecutionState& state,const SourceRef& ref,const StableId& actor,Existing&& existing,Fresh&& fresh){
 const auto* authority=take(read_governance_work_reassignment(x.governance,ref,x.original_sources_stamp));
 if(actor.empty()||actor!=authority->new_actor_id)reject("WorkReassignmentWrongAcceptingActor",actor);
 for(const auto& row:state.work_reassignment_acceptances.rows())if(work_detail::same(row.second.reassignment_approval_ref,ref)){SourceRef prior{"tasks","work_reassignment_accepted",row.first};(void)accepted(x,prior);return existing(std::move(prior));}
 const auto& e=episode(x,authority->work_id);const auto actual=take(read_governance_work_executor_before_cut(x.governance.history,e,nullptr));
 if(!actual.actor_id||!actual.predecessor||e.terminal_result_ref||e.current_execution||authority->old_actor_id!=*actual.actor_id||!work_detail::same(authority->executor_predecessor_ref.source_ref,{actual.predecessor->domain,actual.predecessor->kind,actual.predecessor->id}))reject("WorkReassignmentNoLongerCurrent",e.work_id);
 for(const auto& row:state.work_reassignment_acceptances.rows())if(row.second.work_id==e.work_id){const auto* prior=x.tasks.governance.state.work_reassignment_approvals.find(row.second.reassignment_approval_ref.id);if(prior&&work_detail::same(prior->executor_predecessor_ref,authority->executor_predecessor_ref))reject("WorkReassignmentAcceptanceConflict",e.work_id);}
 const auto now=take(x.tasks.clock.read_current_moment());
 if(const auto* supervision=std::get_if<SupervisedWorkPlan>(&e.frozen_plan);supervision&&compare(now,WorldMoment{supervision->completion_deadline.tick,{}})>=0)reject("WorkReassignmentAcceptanceOutsideWindow",e.work_id);
 take(validate_governance_work_reassignment_acceptance_boundary(x.governance,e,now,nullptr,x.original_sources_stamp));
 const auto contact=take(read_current_face_to_face_contact_source(x.tasks.common,x.tasks.interaction,x.tasks.interaction_catalog,x.tasks.interaction_access,actor,authority->issuer.actor_id,now));
 const auto earliest=take(read_work_acceptance_earliest_service(x.tasks,now));
 return fresh(e,*authority,now,contact,earliest);
}
}
Outcome<const WorkReassignmentAcceptanceSource*> read_work_reassignment_acceptance(const WorkReassignmentSourceContext& x,const SourceRef& ref){return guard<const WorkReassignmentAcceptanceSource*>([&]{return &accepted(x,ref);});}
Status validate_work_reassignment_acceptance_request(const WorkReassignmentSourceContext& x,const SourceRef& ref,const StableId& actor){return guard<Done>([&]{
 const auto& state=selected(x);return acceptance_request(x,state,ref,actor,
 [](const SourceRef&){return Done{};},
 [](const WorkEpisode&,const GovernanceWorkReassignmentApproval&,const WorldMoment&,const SourceRef&,const WorldMoment&){return Done{};});
});}
Outcome<SourceRef> accept_work_reassignment_in_candidate(CommonStore& common,TaskState& tasks,const WorkReassignmentSourceContext& x,const SourceRef& ref,const StableId& actor){return guard<SourceRef>([&]{
 const auto& state=selected(x);if(&common!=&x.tasks.common||&tasks!=&x.tasks.tasks)reject("WorkReassignmentCandidateMismatch");
 return acceptance_request(x,state,ref,actor,
 [](SourceRef&& prior)->SourceRef{return std::move(prior);},
 [&](const WorkEpisode& e,const GovernanceWorkReassignmentApproval& authority_value,const WorldMoment& now,const SourceRef& contact,const WorldMoment& earliest)->SourceRef{
 const auto* authority=&authority_value;
 WorkReassignmentAcceptanceSource row;row.fact_id=take(common.next_id("work_reassignment_accepted"));row.work_id=e.work_id;row.reassignment_approval_ref=ref;row.work_acceptance_ref=authority->work_acceptance_ref;row.new_actor_id=actor;row.acceptance_contact={contact,1};row.accepted_at=now;row.earliest_service_at=earliest;if(x.tasks.current_open_period)row.accepted_period_id=x.tasks.current_open_period->period_id;
 const SourceRef source{"tasks","work_reassignment_accepted",row.fact_id};
 take(common.append_fact({source.id,source.kind,source.domain}));if(!tasks.work_execution->work_reassignment_acceptances.insert(source.id,std::move(row)))reject("DuplicateWorkReassignmentAcceptance",source.id);
 (void)accepted(x,source);return source;
 });
});}
Status validate_work_reassignment_acceptances(const WorkReassignmentSourceContext& x){return guard<Done>([&]{
 const auto& s=selected(x);for(const auto& row:s.work_reassignment_acceptances.rows()){if(row.first!=row.second.fact_id)reject("WorkReassignmentAcceptanceKeyMismatch",row.first);(void)accepted(x,{"tasks","work_reassignment_accepted",row.first});}
 for(const auto& row:x.tasks.common.state().facts.rows())if(row.second.domain=="tasks"&&row.second.kind=="work_reassignment_accepted"&&!s.work_reassignment_acceptances.find(row.first))reject("WorkReassignmentAcceptancePayloadMissing",row.first);
 return Done{};
});}
}
