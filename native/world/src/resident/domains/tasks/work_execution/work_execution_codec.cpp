#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "work_execution_contract.hpp"
#include "work_supervision_execution_terms_fields.hpp"
#include "work_result_source_codec.hpp"
#include "resident/domains/growth/growth_skill_qualification_codec_fields.hpp"
#include "resident/domains/actor_foundation/condition_head_codec_fields.hpp"
#include "../task_dynamic_projection.hpp"
#include "resident/model/commitment_ref_codec.hpp"
namespace hanmo::resident {namespace work_codec {
using namespace dynamic_codec;
template<class A>Status source(A&a,SourceRef&v){a.field("domain",v.domain);a.field("kind",v.kind);return a.field("id",v.id);}
template<class A>Status moment_source(A&a,MomentOwnerSource&v){a.object("source_ref",[&](auto&c){return source(c,v.source_ref);});return a.field("source_schema",v.source_schema);}
template<class A>Status sources(A&a,std::string_view key,std::vector<SourceRef>&v){return a.sequence(key,v,[](auto&c,auto&x){return source(c,x);});}
template<class A>Status moment_sources(A&a,std::string_view key,std::vector<MomentOwnerSource>&v){return a.sequence(key,v,[](auto&c,auto&x){return moment_source(c,x);});}
template<class A>Status ids(A&a,std::string_view key,std::vector<StableId>&v){return a.sequence(key,v,[](auto&c,auto&x){return c.field("value",x);});}
template<class A>Status span(A&a,MomentSpan&v){a.object("from",[&](auto&c){return exact_moment(c,v.from);});return a.object("to",[&](auto&c){return exact_moment(c,v.to);});}
template<class A>Status authorization(A&a,WorkAuthorizationRef&v){return work_source_codec::authorization(a,v);}
template<class A>Status gate(A&a,StateNeedDeadline&v){a.field("tick",v.tick);return a.field("inclusive",v.inclusive);}
template<class A>Status work_ref(A&a,WorkRef&v){a.field("owner_domain",v.owner_domain);enumeration(a,"kind",v.kind,2);return a.field("work_id",v.work_id);}
template<class A>Status patrol_route(A&a,std::variant<PatrolCircuit,StationWatch>&v){Integer tag=v.index();a.field("kind",tag);if(!a.status().ok)return a.status();if(tag<0||tag>1)return a.reject("UnknownWorkPatrolRoute");if(a.reading()){if(tag==0){if(!prepare_checkpoint_default_values<PatrolCircuit>(a).ok)return a.status();v=PatrolCircuit{};}else {if(!prepare_checkpoint_default_values<StationWatch>(a).ok)return a.status();v=StationWatch{};}}if(auto*c=std::get_if<PatrolCircuit>(&v)){sources(a,"checkpoint_refs",c->checkpoint_refs);return sources(a,"allowed_passage_refs",c->allowed_passage_refs);}return a.object("station_ref",[&](auto&c){return source(c,std::get<StationWatch>(v).station_ref);});}
// 按原判别值访问工作计划，保持读取预备与非法类型拒绝顺序。
template <class A> Status plan(A &a, WorkPlan &v) {
  Integer tag = v.index();
  a.field("kind", tag);
  if (!a.status().ok)
    return a.status();
  if (tag < 0 || tag > 1)
    return a.reject("UnknownWorkPlan");
  if (a.reading()) {
    if (tag == 0) {
      if (!prepare_checkpoint_default_values<PatrolPlan>(a).ok)
        return a.status();
      v = PatrolPlan{};
    } else {
      if (!prepare_checkpoint_default_values<SupervisedWorkPlan>(a).ok)
        return a.status();
      v = SupervisedWorkPlan{};
    }
  }
  if (auto *p = std::get_if<PatrolPlan>(&v)) {
    a.field("jurisdiction_id", p->jurisdiction_id);
    a.object("authority_scope", [&](auto &c) {
      return scope(c, p->authority_scope);
    });
    a.field("leader_actor_id", p->leader_actor_id);
    ids(a, "unit_ids", p->unit_ids);
    a.object("route", [&](auto &c) {
      return patrol_route(c, p->route);
    });
    a.field("minimum_available_people", p->minimum_available_people);
    a.object("supply_order_ref", [&](auto &c) {
      return source(c, p->supply_order_ref);
    });
  } else {
    auto &supervised = std::get<SupervisedWorkPlan>(v);
    enumeration(a, "work_kind", supervised.kind, 3);
    a.field("supervisor_actor_id", supervised.supervisor_actor_id);
    a.object("economic_work_ref", [&](auto &c) {
      return work_ref(c, supervised.economic_work_ref);
    });
    a.object("target_site_or_anchor_ref", [&](auto &c) {
      return source(c, supervised.target_site_or_anchor_ref);
    });
    a.optional("required_skill_ref", supervised.required_skill_ref,
               [](auto &c, auto &x) {
                 return source(c, x);
               });
  }
  return std::visit(
      [&](auto &p) {
        a.field("required_half_days", p.required_half_days);
        a.object("completion_deadline", [&](auto &c) {
          return gate(c, p.completion_deadline);
        });
        a.object("expense_terms_ref", [&](auto &c) {
          return source(c, p.expense_terms_ref);
        });
        a.object("continuity_policy_ref", [&](auto &c) {
          return source(c, p.continuity_policy_ref);
        });
        a.field("rule_id", p.rule_id);
        return a.field("rule_version", p.rule_version);
      },
      v);
}
template<class A>Status execution(A&a,WorkExecutionSource&v){Integer tag=v.index();a.field("kind",tag);if(!a.status().ok)return a.status();if(tag<0||tag>1)return a.reject("UnknownWorkExecution");if(a.reading()){if(tag==0){if(!prepare_checkpoint_default_values<WorkCommonActionBinding>(a).ok)return a.status();v=WorkCommonActionBinding{};}else {if(!prepare_checkpoint_default_values<WorkMilitaryActivityBinding>(a).ok)return a.status();v=WorkMilitaryActivityBinding{};}}if(auto*p=std::get_if<WorkCommonActionBinding>(&v))return a.field("action_id",p->action_id);auto&p=std::get<WorkMilitaryActivityBinding>(v);a.object("military_activity_ref",[&](auto&c){return source(c,p.military_activity_ref);});return a.field("source_schema",p.source_schema);}
template<class A>Status military_period(A&a,WorkMilitaryPeriodSource&v){a.object("fact_ref",[&](auto&c){return source(c,v.fact_ref);});return a.field("source_schema",v.source_schema);}
template<class A>Status period_source(A&a,WorkPeriodSource&v){Integer tag=v.index();a.field("kind",tag);if(!a.status().ok)return a.status();if(tag<0||tag>1)return a.reject("UnknownWorkPeriodSource");if(a.reading()){if(tag==0){if(!prepare_checkpoint_default_values<WorkMilitaryPeriodSource>(a).ok)return a.status();v=WorkMilitaryPeriodSource{};}else {if(!prepare_checkpoint_default_values<WorkEconomicPeriodSource>(a).ok)return a.status();v=WorkEconomicPeriodSource{};}}if(auto*p=std::get_if<WorkMilitaryPeriodSource>(&v))return military_period(a,*p);auto&p=std::get<WorkEconomicPeriodSource>(v);a.object("work_ref",[&](auto&c){return work_ref(c,p.work_ref);});a.object("window",[&](auto&c){return span(c,p.window);});return moment_sources(a,"source_parts",p.source_parts);}
template<class A>Status actual_start_basis(A&a,std::variant<WorkMilitaryStartBasis,WorkSupervisionStartBasis>&v){Integer tag=v.index();a.field("kind",tag);if(!a.status().ok)return a.status();if(tag<0||tag>1)return a.reject("UnknownWorkActualStartBasis");if(a.reading()){if(tag==0){if(!prepare_checkpoint_default_values<WorkMilitaryStartBasis>(a).ok)return a.status();v=WorkMilitaryStartBasis{};}else {if(!prepare_checkpoint_default_values<WorkSupervisionStartBasis>(a).ok)return a.status();v=WorkSupervisionStartBasis{};}}if(auto*p=std::get_if<WorkMilitaryStartBasis>(&v))return a.object("period_fact_ref",[&](auto&c){return military_period(c,p->period_fact_ref);});auto&p=std::get<WorkSupervisionStartBasis>(v);a.field("work_period_receipt_ref",p.work_period_receipt_ref);return moment_sources(a,"economic_period_refs",p.economic_period_refs);}
template<class A>Status acceptance(A&a,WorkAcceptanceSource&v){a.field("fact_id",v.fact_id);a.field("work_id",v.work_id);a.object("authorization",[&](auto&c){return authorization(c,v.authorization);});a.object("original_task_acceptance",[&](auto&c){return moment_source(c,v.original_task_acceptance);});a.object("expense_terms_ref",[&](auto&c){return source(c,v.expense_terms_ref);});a.object("accepted_at",[&](auto&c){return exact_moment(c,v.accepted_at);});return a.object("earliest_service_at",[&](auto&c){return exact_moment(c,v.earliest_service_at);});}
template<class A>Status episode(A&a,WorkEpisode&v){a.field("work_id",v.work_id);a.field("acceptance_source_ref",v.acceptance_source_ref);a.object("authorization",[&](auto&c){return authorization(c,v.authorization);});a.object("frozen_plan",[&](auto&c){return plan(c,v.frozen_plan);});enumeration(a,"phase",v.phase,3);a.optional("current_execution",v.current_execution,[](auto&c,auto&x){return execution(c,x);});ids(a,"accepted_period_refs",v.accepted_period_refs);ids(a,"leader_change_fact_refs",v.leader_change_fact_refs);moment_sources(a,"original_execution_sources",v.original_execution_sources);a.optional("last_interruption_ref",v.last_interruption_ref,[](auto&c,auto&x){return c.field("value",x);});a.optional("terminal_result_ref",v.terminal_result_ref,[](auto&c,auto&x){return c.field("value",x);});return a.optional("actual_start_ref",v.actual_start_ref,[](auto&c,auto&x){return c.field("value",x);});}
template<class A>Status period(A&a,WorkPeriodReceipt2&v){a.field("fact_id",v.fact_id);a.field("work_id",v.work_id);enumeration(a,"kind",v.kind,3);a.object("window",[&](auto&c){return span(c,v.window);});a.object("actual_until",[&](auto&c){return exact_moment(c,v.actual_until);});a.field("leader_actor_id",v.leader_actor_id);a.object("source",[&](auto&c){return period_source(c,v.source);});a.object("execution_source",[&](auto&c){return execution(c,v.execution_source);});moment_sources(a,"actual_anchor_sources",v.actual_anchor_sources);a.field("credited_half_days",v.credited_half_days);enumeration(a,"disposition",v.disposition,2);enumeration(a,"reason",v.reason,9);a.optional("reason_source",v.reason_source,[](auto&c,auto&x){return moment_source(c,x);});return a.field("rule_version",v.rule_version);}
template<class A>Status actual_start(A&a,WorkActualStartSource&v){a.field("start_fact_id",v.start_fact_id);a.field("work_id",v.work_id);a.object("authorization",[&](auto&c){return authorization(c,v.authorization);});a.object("execution",[&](auto&c){return execution(c,v.execution);});a.object("actual_basis",[&](auto&c){return actual_start_basis(c,v.actual_basis);});a.object("started_at",[&](auto&c){return exact_moment(c,v.started_at);});return a.field("source_schema",v.source_schema);}
template<class A>Status leader_change(A&a,WorkLeaderChange&v){a.field("fact_id",v.fact_id);a.field("work_id",v.work_id);a.field("old_actor_id",v.old_actor_id);a.field("new_actor_id",v.new_actor_id);a.object("authority_source",[&](auto&c){return moment_source(c,v.authority_source);});a.object("actual_consent_source",[&](auto&c){return moment_source(c,v.actual_consent_source);});a.object("actual_arrival_source",[&](auto&c){return moment_source(c,v.actual_arrival_source);});a.object("replacement_at",[&](auto&c){return exact_moment(c,v.replacement_at);});a.object("new_execution_source",[&](auto&c){return execution(c,v.new_execution_source);});return a.object("original_execution_source",[&](auto&c){return moment_source(c,v.original_execution_source);});}
template<class A>Status result(A&a,WorkResult&v,bool skill_inputs){a.field("fact_id",v.fact_id);a.field("work_id",v.work_id);a.field("task_id",v.task_id);a.object("authorization",[&](auto&c){return authorization(c,v.authorization);});enumeration(a,"kind",v.kind,3);a.field("plan_rule_version",v.plan_rule_version);a.optional("first_work_at",v.first_work_at,[](auto&c,auto&x){return exact_moment(c,x);});a.object("finished_at",[&](auto&c){return exact_moment(c,v.finished_at);});a.field("credited_half_days",v.credited_half_days);a.field("required_half_days",v.required_half_days);moment_sources(a,"checkpoint_completion_refs",v.checkpoint_completion_refs);moment_sources(a,"period_summary_refs",v.period_summary_refs);moment_sources(a,"actual_leader_refs",v.actual_leader_refs);moment_sources(a,"encounter_result_refs",v.encounter_result_refs);moment_sources(a,"actual_stage_result_refs",v.actual_stage_result_refs);sources(a,"expense_receipt_refs",v.expense_receipt_refs);enumeration(a,"disposition",v.disposition,2);a.optional("stop_reason",v.stop_reason,[](auto&c,auto&x){return enumeration(c,"value",x,6);});a.optional("actual_stop_source",v.actual_stop_source,[](auto&c,auto&x){return moment_source(c,x);});if(!skill_inputs)return v.skill_input?a.reject("WorkSkillInputsRequirePhysical4"):a.status();return a.optional("skill_input",v.skill_input,[](auto&c,auto&x){c.object("condition_before",[&](auto&z){return actor_condition_codec::head(z,x.condition_before);});c.field("skill_id",x.skill_id);return c.optional("qualification",x.qualification,[](auto&z,auto&q){return growth_skill_qualification_codec::fields(z,q);});});}
template<class A>Status task_offer(A&a,WorkTaskOfferSource&v){a.field("fact_id",v.fact_id);a.object("authorization",[&](auto&c){c.object("program_ref",[&](auto&x){return source(x,v.authorization.program_ref);});c.field("target_line_id",v.authorization.target_line_id);return c.object("terms_revision",[&](auto&x){return commitment_codec::terms(x,v.authorization.terms_revision);});});a.field("executor_actor_id",v.executor_actor_id);a.field("presenting_actor_id",v.presenting_actor_id);a.object("presentation_contact",[&](auto&c){return moment_source(c,v.presentation_contact);});a.object("offered_at",[&](auto&c){return exact_moment(c,v.offered_at);});return a.object("valid_until",[&](auto&c){return exact_moment(c,v.valid_until);});}
template<class A>Status task_acceptance(A&a,WorkTaskAcceptanceSource&v){a.field("fact_id",v.fact_id);a.field("task_id",v.task_id);a.object("offered_task_ref",[&](auto&c){return source(c,v.offered_task_ref);});a.field("executor_actor_id",v.executor_actor_id);a.object("acceptance_contact",[&](auto&c){return moment_source(c,v.acceptance_contact);});a.object("accepted_at",[&](auto&c){return exact_moment(c,v.accepted_at);});a.object("earliest_service_at",[&](auto&c){return exact_moment(c,v.earliest_service_at);});return a.optional("accepted_period_id",v.accepted_period_id,[](auto&c,auto&id){return c.field("value",id);});}
// 替任接受使用原字段编码器，字段顺序与唯一 /6 持久合同一致。
template<class A>Status reassignment_acceptance(A&a,WorkReassignmentAcceptanceSource&v){a.field("fact_id",v.fact_id);a.field("work_id",v.work_id);a.object("reassignment_approval_ref",[&](auto&c){return source(c,v.reassignment_approval_ref);});a.object("work_acceptance_ref",[&](auto&c){return source(c,v.work_acceptance_ref);});a.field("new_actor_id",v.new_actor_id);a.object("acceptance_contact",[&](auto&c){return moment_source(c,v.acceptance_contact);});a.object("accepted_at",[&](auto&c){return exact_moment(c,v.accepted_at);});a.object("earliest_service_at",[&](auto&c){return exact_moment(c,v.earliest_service_at);});return a.optional("accepted_period_id",v.accepted_period_id,[](auto&c,auto&id){return c.field("value",id);});}
template<class A>Status supervision_start(A&a,SupervisionActionStartSource&v,bool skill_inputs){a.field("fact_id",v.fact_id);a.field("action_id",v.action_id);a.field("work_id",v.work_id);a.field("actor_id",v.actor_id);a.object("work_acceptance_ref",[&](auto&c){return source(c,v.work_acceptance_ref);});a.object("started_at",[&](auto&c){return exact_moment(c,v.started_at);});a.field("until_tick",v.until_tick);a.object("accepted_presence_source",[&](auto&c){return source(c,v.accepted_presence_source);});a.object("condition_at_start",[&](auto&c){return actor_condition_codec::head(c,v.condition_at_start);});moment_sources(a,"original_start_sources",v.original_start_sources);if(!skill_inputs)return v.skill_qualification?a.reject("WorkSkillInputsRequirePhysical4"):a.status();return a.optional("skill_qualification",v.skill_qualification,[](auto&c,auto&q){return growth_skill_qualification_codec::fields(c,q);});}
template<class A>Status supervision_close(A&a,SupervisionWindowClosureSource&v){a.field("fact_id",v.fact_id);a.field("start_source_ref",v.start_source_ref);return a.object("closed_at",[&](auto&c){return exact_moment(c,v.closed_at);});}
template<class A>Status supervision_terminal(A&a,SupervisionActionTerminalSource&v){a.field("fact_id",v.fact_id);a.field("start_source_ref",v.start_source_ref);a.object("ended_at",[&](auto&c){return exact_moment(c,v.ended_at);});enumeration(a,"disposition",v.disposition,2);return a.object("actual_release_cause",[&](auto&c){return moment_source(c,v.actual_release_cause);});}
template<class A>Status fields(A&a,TaskWorkExecutionState&v){a.field("version",v.version);if(a.status().ok&&!task_work_execution_version(v.version))return a.reject("UnsupportedTaskWorkExecutionVersion");table(a,"work_task_offers",v.work_task_offers,[](auto&c,auto&x){return task_offer(c,x);});table(a,"work_task_acceptances",v.work_task_acceptances,[](auto&c,auto&x){return task_acceptance(c,x);});table(a,"acceptances",v.acceptances,[](auto&c,auto&x){return acceptance(c,x);});table(a,"episodes",v.episodes,[](auto&c,auto&x){return episode(c,x);});table(a,"period_receipts",v.period_receipts,[](auto&c,auto&x){return period(c,x);});table(a,"actual_starts",v.actual_starts,[](auto&c,auto&x){return actual_start(c,x);});table(a,"leader_changes",v.leader_changes,[](auto&c,auto&x){return leader_change(c,x);});table(a,"results",v.results,[&](auto&c,auto&x){return result(c,x,task_work_skill_inputs_version(v.version));});table(a,"supervision_starts",v.supervision_starts,[&](auto&c,auto&x){return supervision_start(c,x,task_work_skill_inputs_version(v.version));});table(a,"supervision_window_closures",v.supervision_window_closures,[](auto&c,auto&x){return supervision_close(c,x);});table(a,"supervision_terminals",v.supervision_terminals,[](auto&c,auto&x){return supervision_terminal(c,x);});if(!task_work_fixed_execution_terms_version(v.version)){if(!v.supervision_execution_terms.empty())return a.reject("SupervisionExecutionTermsRequirePhysical5");}else table(a,"supervision_execution_terms",v.supervision_execution_terms,[&](auto&c,auto&x){return supervision_execution_codec::fields(c,x,v.version);});if(!task_work_lifecycle_sources_version(v.version))return v.work_reassignment_acceptances.empty()?a.status():a.reject("WorkReassignmentAcceptanceRequiresPhysical6");return table(a,"work_reassignment_acceptances",v.work_reassignment_acceptances,[](auto&c,auto&x){return reassignment_acceptance(c,x);});}
}
Outcome<TaskWorkExecutionState> decode_task_work_execution(const BoundaryValue&v){dynamic_codec::JsonArchive a(v);auto status=prepare_checkpoint_default_values<TaskWorkExecutionState>(a);if(!status.ok)return {false,{},status.error};TaskWorkExecutionState state;status=work_codec::fields(a,state);if(status.ok)status=a.finish();return status.ok?Outcome<TaskWorkExecutionState>{true,std::move(state),{}}:Outcome<TaskWorkExecutionState>{false,{},status.error};}
Outcome<BoundaryValue> encode_task_work_execution(const TaskWorkExecutionState&v){return dynamic_projection::encode(v,[](auto&a,auto&state){return work_codec::fields(a,state);});}
Status visit_task_work_execution_checkpoint(CodecVisitor&a,TaskWorkExecutionState&v){return work_codec::fields(a,v);}
Status emit_task_work_execution_projection(ProjectionWriter&w,std::string_view key,const TaskWorkExecutionState&v){return dynamic_projection::emit(w,key,v,[](auto&a,auto&state){return work_codec::fields(a,state);});}
namespace {
template<class F>Status visit_original_work_fact(const TaskWorkExecutionState&s,const FactRecord&h,F&&visit){
 if(!task_work_execution_version(s.version)||h.domain!="tasks"||!owns_task_work_original_fact_kind(h.kind))return common_failure("TaskWorkOriginalFactProfileMismatch",h.id,ErrorKind::internal);
 auto row=[&](const auto&table,auto fields)->Status{
  const auto*v=table.find(h.id);if(!v)return common_failure("TaskWorkOriginalFactPayloadMissing",h.id,ErrorKind::internal);
  return visit(*v,fields);
 };
 if(h.kind=="work_task_offered")return row(s.work_task_offers,[](auto&a,auto&v){return work_codec::task_offer(a,v);});
 if(h.kind=="work_task_accepted")return row(s.work_task_acceptances,[](auto&a,auto&v){return work_codec::task_acceptance(a,v);});
 if(h.kind=="work_accepted")return row(s.acceptances,[](auto&a,auto&v){return work_codec::acceptance(a,v);});
 if(h.kind=="work_reassignment_accepted"){if(!task_work_lifecycle_sources_version(s.version))return common_failure("WorkReassignmentAcceptanceRequiresPhysical6",h.id,ErrorKind::unsupported_capability);return row(s.work_reassignment_acceptances,[](auto&a,auto&v){return work_codec::reassignment_acceptance(a,v);});}
 if(h.kind=="work_period_receipt")return row(s.period_receipts,[](auto&a,auto&v){return work_codec::period(a,v);});
 if(h.kind=="work_actual_start")return row(s.actual_starts,[](auto&a,auto&v){return work_codec::actual_start(a,v);});
 if(h.kind=="work_leader_changed")return row(s.leader_changes,[](auto&a,auto&v){return work_codec::leader_change(a,v);});
 if(h.kind=="patrol_work_result"||h.kind=="supervised_work_result"){
  const auto*v=s.results.find(h.id);if(!v||(v->kind==WorkKind::patrol)!=(h.kind=="patrol_work_result"))return common_failure("TaskWorkOriginalResultKindMismatch",h.id,ErrorKind::internal);
  return visit(*v,[&](auto&a,auto&v){return work_codec::result(a,v,task_work_skill_inputs_version(s.version));});
 }
 if(h.kind=="supervision_action_started")return row(s.supervision_starts,[&](auto&a,auto&v){return work_codec::supervision_start(a,v,task_work_skill_inputs_version(s.version));});
 if(h.kind=="supervision_window_closed")return row(s.supervision_window_closures,[](auto&a,auto&v){return work_codec::supervision_close(a,v);});
 return row(s.supervision_terminals,[](auto&a,auto&v){return work_codec::supervision_terminal(a,v);});
}
template<class T>const StableId&work_fact_id(const T&v){return v.fact_id;}
const StableId&work_fact_id(const WorkActualStartSource&v){return v.start_fact_id;}
}
bool owns_task_work_original_fact_kind(const StableId&k)noexcept{
 return k=="work_task_offered"||k=="work_task_accepted"||k=="work_accepted"||k=="work_reassignment_accepted"||k=="work_period_receipt"||k=="work_actual_start"||k=="work_leader_changed"||k=="patrol_work_result"||k=="supervised_work_result"||k=="supervision_action_started"||k=="supervision_window_closed"||k=="supervision_action_ended";
}
Outcome<TaskWorkExecutionState> make_initial_task_work_execution(Integer selected){
 if(!task_work_execution_version(selected))return {false,{},common_failure("UnsupportedTaskWorkExecutionVersion",{},ErrorKind::unsupported_capability).error};
 TaskWorkExecutionState state;state.version=selected;return {true,std::move(state),{}};
}
Outcome<bool> emit_task_work_original_fact_payload(ProjectionWriter&w,const TaskWorkExecutionState*s,const FactRecord&h){
 if(!owns_task_work_original_fact_kind(h.kind))return {true,false,{}};
 if(!s)return {false,{},common_failure("TaskWorkOriginalFactStateRequired",h.id,ErrorKind::unsupported_capability).error};
 auto status=visit_original_work_fact(*s,h,[&](const auto&v,auto fields)->Status{
  if(work_fact_id(v)!=h.id)return common_failure("TaskWorkOriginalFactKeyMismatch",h.id,ErrorKind::internal);
  dynamic_projection::Archive a(w);using T=std::remove_cv_t<std::remove_reference_t<decltype(v)>>;return fields(a,const_cast<T&>(v));
 });
 return status.ok?Outcome<bool>{true,true,{}}:Outcome<bool>{false,{},status.error};
}
Status validate_task_work_original_fact_headers(const CommonStore&common,const TaskWorkExecutionState*s){
 if(s&&!task_work_execution_version(s->version))return common_failure("UnsupportedTaskWorkExecutionVersion",{},ErrorKind::unsupported_capability);
 std::size_t found=0;
 for(const auto&row:common.state().facts.rows()){
  const auto&h=row.second;if(!owns_task_work_original_fact_kind(h.kind))continue;
  if(!s||row.first!=h.id)return common_failure("TaskWorkOriginalFactStateMismatch",row.first,ErrorKind::internal);
  auto valid=visit_original_work_fact(*s,h,[&](const auto&v,auto)->Status{return work_fact_id(v)==row.first?common_success():common_failure("TaskWorkOriginalFactKeyMismatch",row.first,ErrorKind::internal);});if(!valid.ok)return valid;++found;
 }
 if(!s)return common_success();
 const auto expected=s->work_task_offers.size()+s->work_task_acceptances.size()+s->acceptances.size()+s->period_receipts.size()+s->actual_starts.size()+s->leader_changes.size()+s->results.size()+s->supervision_starts.size()+s->supervision_window_closures.size()+s->supervision_terminals.size()+s->work_reassignment_acceptances.size();
 return found==expected?common_success():common_failure("TaskWorkOriginalFactCoverageMismatch",{},ErrorKind::internal);
}

}
