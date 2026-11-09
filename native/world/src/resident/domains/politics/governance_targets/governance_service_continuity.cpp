// 从唯一原Lifecycle/Office源重放真实Common切点，Grant离职/过期不倒销已经受领的责任。
#include "governance_service_continuity.hpp"
#include "governance_work_lifecycle_sources.hpp"
#include "resident/domains/politics/politics_governance.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const StableId& code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> x){if(!x.ok)throw Failure{x.error};return std::move(x.value);}
template<class T,class F>Outcome<T> guard(F&& f){try{return {true,f(),{}};}catch(const Failure& e){return {false,{},e.error};}}
int cmp(const WorldMoment&a,const WorldMoment&b){return take(compare_world_moments(a,b));}
std::size_t position(const CommonStore& c,const SourceRef& ref){
 const auto* f=c.state().facts.find(ref.id);const auto& order=c.state().facts.order();
 auto at=std::find(order.begin(),order.end(),ref.id);
 if(!f||f->domain!=ref.domain||f->kind!=ref.kind||at==order.end())fail("GovernanceContinuityOriginalSourceRequired",ref.id);
 return static_cast<std::size_t>(at-order.begin());
}
void append(std::vector<MomentOwnerSource>& out,const MomentOwnerSource& ref){
 for(const auto& old:out)if(same_source_ref(old.source_ref,ref.source_ref)){if(old.source_schema!=ref.source_schema)fail("GovernanceContinuitySourceSchemaMismatch",ref.source_ref.id);return;}out.push_back(ref);
}
struct HistoryState {
 std::optional<OrganizationStatus> organization;OfficeStatus office=OfficeStatus::vacant;std::optional<StableId> holder;
 MomentOwnerSource organization_source,office_source;
};
struct Event {MomentOwnerSource source;WorldMoment at;const OrganizationLifecycleTransition* organization=nullptr;const OrganizationFact* office=nullptr;std::size_t order=0;};
struct History {HistoryState initial;std::vector<Event> events;GovernanceOriginalContinuity policy;};
History approval_history(const GovernanceOrganizationContinuityContext& x,const GovernanceProgramApproval& approval,const GovernanceProgramSpec& specification){
 const auto& p=x.programs;const auto& c=p.common;
 if(p.state.approvals.find(approval.id)!=&approval||p.catalog.specs.find(specification.spec_ref.id)!=&specification||
    approval.spec_ref.id!=specification.spec_ref.id||approval.spec_ref.version!=specification.spec_ref.version||
    approval.spec_ref.catalog_digest!=specification.spec_ref.catalog_digest)
  fail("GovernanceContinuityActualApprovalRequired",approval.id);
 auto definition=take(read_governance_continuity_definition(p.catalog,specification));
 NativeGovernanceProgramAuthorityRead issues(p);
 auto basis=take(read_registered_authority_basis(c,p.organization_runtime.authority_bases,p.organization,p.organization_catalog,p.catalog.identity,approval.authority_basis_ref,issues,&p.budgets));
 take(validate_governance_continuity_policy_binding(p.catalog,specification,basis->continuity_policy_id));
 const auto now=take(p.clock.read_organization_authority_now());
 if(x.original_sources_stamp.mode()!=ReadMode::phase_current&&x.original_sources_stamp.mode()!=ReadMode::committed_current)fail("GovernanceContinuityCurrentSourcesStampRequired",approval.id);
 take(x.stamp_authority.validate(x.original_sources_stamp,c.state().identity,x.original_sources_stamp.mode(),c,x.versions));
 // 完整原来源反查保留为语义参照；不把缓存票/当前status冒历史证明。
 take(validate_organization_lifecycle(c,p.organization,p.organization_runtime,p.organization_catalog,x.lifecycle_catalog,x.original_sources_stamp,x.lifecycle_sources,now));
 take(validate_office_lifecycle(c,p.organization,p.organization_catalog,x.original_sources_stamp,x.office_sources,x.deaths));
 const auto& org_id=approval.issuer.organization_id;const auto* meta=p.organization_runtime.organization_metadata.find(org_id);
 const auto* origin=meta?p.organization_runtime.lifecycle_origins.find(meta->origin_ref.id):nullptr;
 const auto* office=p.organization_catalog.offices.find(approval.issuer.office_id);
 if(!meta||!origin||!office||office->organization_id!=org_id||position(c,meta->origin_ref)>=position(c,governance_program_source(approval)))
  fail("GovernanceContinuityOriginRequired",org_id);
 History out;out.policy=definition->original_grant_policy;out.initial.organization_source={meta->origin_ref,1};
 out.initial.office=office->status;out.initial.holder=office->holder_actor_id;
 // 初任Office证据已经在原批准Basis内，来源仍引用这个实际已签批准。
 out.initial.office_source={governance_program_source(approval),1};
 if(const auto* authored=std::get_if<OrganizationAuthoredOrigin>(&origin->origin)){
  const auto* seed=x.lifecycle_catalog.seed_policies.find(authored->lifecycle_seed_policy_ref.id);if(!seed)fail("GovernanceContinuitySeedMissing",org_id);out.initial.organization=seed->initial_lifecycle;
 }else if(const auto* migrated=std::get_if<OrganizationMigratedOrigin>(&origin->origin)){
  const auto* seed=p.organization_runtime.lifecycle_migration_decisions.find(migrated->migration_decision_ref.id);if(!seed)fail("GovernanceContinuityMigrationMissing",org_id);out.initial.organization=seed->selected_lifecycle;
 }
 // Founded的初态为空，必须由原已验证formation transition出现；不猜今天状态。
 for(const auto& id:c.state().facts.order()){
  if(const auto* t=p.organization_runtime.lifecycle_transitions.find(id);t&&t->organization_id==org_id){
   SourceRef ref{"organization","organization_lifecycle_transition",id};out.events.push_back({{ref,1},t->effective_moment,t,nullptr,position(c,ref)});
  }
  if(out.policy==GovernanceOriginalContinuity::stop_when_vacant){const auto* f=p.organization.facts.find(id);
   if(f&&f->kind=="organization_office_changed"&&f->office_id==office->id){
    if(!f->occurred_at||!f->office_change_payload||cmp(*f->occurred_at,f->office_change_payload->effective_moment))fail("GovernanceContinuityOfficeTime2Required",id);
    SourceRef ref{"organization",f->kind,id};out.events.push_back({{ref,1},*f->occurred_at,nullptr,f,position(c,ref)});
   }
  }
 }
 for(std::size_t i=1;i<out.events.size();++i)if(out.events[i-1].order>=out.events[i].order||cmp(out.events[i-1].at,out.events[i].at)>0)fail("GovernanceContinuityOriginalOrderMismatch",org_id);
 return out;
}
GovernanceOrganizationContinuityContext organization_context(const GovernanceServiceContinuityContext& x){
 return {x.history.programs,x.lifecycle_catalog,x.lifecycle_sources,x.office_sources,x.deaths,
  x.original_sources_stamp,x.stamp_authority,x.versions};
}
History history(const GovernanceServiceContinuityContext& x,const GovernanceAcceptedWorkHistory& h,const WorkEpisode& e){
 const auto& p=x.history.programs;
 auto original=take(read_governance_accepted_work_history(x.history,e));
 if(!h.approval||!h.supervision||!original.supervision||h.approval!=original.approval||
    h.specification!=original.specification||h.supervision!=original.supervision||&x.body.common!=&p.common)
  fail("GovernanceContinuityCandidateMismatch",e.work_id);
 if(cmp(take(p.clock.read_organization_authority_now()),x.body.actual_now))fail("GovernanceContinuityBodyPhaseChanged",e.work_id);
 return approval_history(organization_context(x),*h.approval,*h.specification);
}
void apply(HistoryState& state,const Event& event){
 if(event.organization){state.organization=event.organization->after_status;state.organization_source=event.source;}
 else{state.holder=event.office->office_change_payload->new_holder;state.office=state.holder?OfficeStatus::active:OfficeStatus::vacant;state.office_source=event.source;}
}
std::optional<MomentOwnerSource> blocked(const HistoryState& state,GovernanceOriginalContinuity policy){
 if(!state.organization)fail("GovernanceOrganizationNotYetFormed");
 if(*state.organization==OrganizationStatus::terminal)return state.organization_source;
 // 新形成/合并/流亡等不是原两算法的隐含业务默认；其具体连续性尚需原规则来源。
 if(*state.organization!=OrganizationStatus::active&&*state.organization!=OrganizationStatus::limited)fail("GovernanceLifecycleContinuityRuleRequired");
 if(policy==GovernanceOriginalContinuity::stop_when_vacant&&(state.office!=OfficeStatus::active||!state.holder))return state.office_source;
 return {};
}
GovernanceServiceContinuityView replay_at(const CommonStore& common,const GovernanceProgramApproval& a,
 const History& all,const WorldMoment& when,const SourceRef* cut){
 const auto limit=cut?position(common,*cut):common.state().facts.order().size();
 HistoryState state=all.initial;GovernanceServiceContinuityView out;
 append(out.actual_sources,{governance_program_source(a),1});append(out.actual_sources,state.organization_source);
 for(const auto& event:all.events)if(event.order<limit){
  if(cmp(event.at,when)>0)fail("GovernanceContinuitySourceAfterMoment",event.source.source_ref.id);
  apply(state,event);append(out.actual_sources,event.source);
 }else if(cmp(event.at,when)<0)fail("GovernanceContinuityCutAfterLaterHistory",event.source.source_ref.id);
 out.actual_blocking_source=blocked(state,all.policy);return out;
}
GovernanceServiceContinuityView at(const GovernanceServiceContinuityContext& x,const GovernanceAcceptedWorkHistory& h,
 const WorkEpisode& e,const WorldMoment& when,const SourceRef* cut){
 const auto& p=x.history.programs;auto all=history(x,h,e);const auto now=take(p.clock.read_organization_authority_now());
 if(cmp(when,now)>0||(!cut&&cmp(when,now)))fail("GovernanceContinuityActualMomentRequired",e.work_id);
 // 历史切点必须属于本Work的真实开工或后继签发，不接受任意Common头代替权限时点。
 if(cut){
  if(cut->domain=="tasks"&&cut->kind=="supervision_action_started"){
   const auto* row=x.history.tasks.work_execution?x.history.tasks.work_execution->supervision_starts.find(cut->id):nullptr;
   if(!row||row->fact_id!=cut->id||row->work_id!=e.work_id||cmp(row->started_at,when))fail("GovernanceContinuityActualCutRequired",cut->id);
  }else if(cut->domain=="politics"&&cut->kind=="governance_supervision_execution_approved"){
   const auto* row=p.state.supervision_execution_approvals.find(cut->id);
   if((p.state.schema_version!=2&&p.state.schema_version!=3)||p.catalog.schema_version!=4||!row||row->id!=cut->id||row->work_id!=e.work_id||
      !same_source_ref(row->authorization.program_ref,e.authorization.program_ref)||cmp(row->issued_at,when))
    fail("GovernanceContinuityActualCutRequired",cut->id);
  }else if(cut->domain=="politics"&&cut->kind=="governance_work_reassignment_approved"){
   const auto* row=p.state.work_reassignment_approvals.find(cut->id);
   if(p.state.schema_version!=3||!row||row->id!=cut->id||row->work_id!=e.work_id||!same_source_ref(row->authorization.program_ref,e.authorization.program_ref)||cmp(row->issued_at,when))fail("GovernanceContinuityActualCutRequired",cut->id);
  }else fail("GovernanceContinuityActualCutRequired",cut->id);
 }
 return replay_at(p.common,*h.approval,all,when,cut);
}
}
// 同一 replay/blocked 核给签发阶段分类；不把 vacancy 放行为实际服务许可。
Status validate_governance_reassignment_continuity(const GovernanceOrganizationContinuityContext& x,const GovernanceProgramApproval& a,const GovernanceProgramSpec& spec,const WorldMoment& when,const SourceRef* cut){return guard<Done>([&]{
 const auto now=take(x.programs.clock.read_organization_authority_now());
 if(cmp(when,a.issued_at)<0||cmp(when,now)>0||(!cut&&cmp(when,now)))fail("GovernanceReassignmentActualMomentRequired",a.id);
 if(cut){const auto* source=x.programs.state.work_reassignment_approvals.find(cut->id);if(x.programs.state.schema_version!=3||cut->domain!="politics"||cut->kind!="governance_work_reassignment_approved"||!source||source->id!=cut->id||!same_source_ref(source->authorization.program_ref,governance_program_source(a))||cmp(source->issued_at,when))fail("GovernanceReassignmentActualCutRequired",cut->id);}
 auto all=approval_history(x,a,spec);const auto permission=replay_at(x.programs.common,a,all,when,cut);
 if(!permission.actual_blocking_source)return Done{};
 const auto& blocked=permission.actual_blocking_source->source_ref;
 if(blocked.domain=="organization"&&blocked.kind=="organization_lifecycle_transition")fail("GovernanceReassignmentPrincipalSuccessionRequired",blocked.id);
 const auto* office=x.programs.organization.facts.find(blocked.id);
 if(all.policy!=GovernanceOriginalContinuity::stop_when_vacant||blocked.domain!="organization"||blocked.kind!="organization_office_changed"||!office||office->kind!=blocked.kind||office->office_id!=a.issuer.office_id||!office->office_change_payload||office->office_change_payload->new_holder)fail("GovernanceReassignmentBlockingSourceUnsupported",blocked.id);
 return Done{};
});}
Outcome<GovernanceServiceContinuityView> read_governance_program_continuity_now(
 const GovernanceOrganizationContinuityContext& x,const GovernanceProgramApproval& a,const GovernanceProgramSpec& spec){
 return guard<GovernanceServiceContinuityView>([&]{
  auto all=approval_history(x,a,spec);const auto now=take(x.programs.clock.read_organization_authority_now());
  if(cmp(now,a.issued_at)<0)fail("GovernanceContinuityBeforeApproval",a.id);
  return replay_at(x.programs.common,a,all,now,nullptr);
 });
}
Outcome<GovernanceServiceContinuityView> read_governance_patrol_continuity_before_application(
 const GovernanceOrganizationContinuityContext& x,const PoliticsGovernanceState& security,
 const GovernanceProgramApproval& a,const GovernanceProgramSpec& spec,const SecurityConsequence& effect){
 return guard<GovernanceServiceContinuityView>([&]{
  if(security.security_consequences.find(effect.id)!=&effect||!std::holds_alternative<GovernancePatrol>(a.target)||
     !same_source_ref(effect.program_ref,governance_program_source(a))||effect.target_line_id!=spec.spec_ref.id||
     effect.jurisdiction_id!=std::get<GovernancePatrol>(a.target).jurisdiction_id||
     effect.program_spec_ref.id!=spec.spec_ref.id||effect.program_spec_ref.version!=spec.spec_ref.version||
     effect.program_spec_ref.catalog_digest!=spec.spec_ref.catalog_digest)
   fail("GovernanceContinuityPatrolEffectMismatch",effect.id);
  const SourceRef cut{"politics","political_security_consequence",effect.id};
  if(position(x.programs.common,cut)<=position(x.programs.common,governance_program_source(a))||
     cmp(effect.applied_at,a.issued_at)<0||cmp(effect.applied_at,take(x.programs.clock.read_organization_authority_now()))>0)
   fail("GovernanceContinuityPatrolEffectTimeMismatch",effect.id);
  auto all=approval_history(x,a,spec);return replay_at(x.programs.common,a,all,effect.applied_at,&cut);
 });
}
Outcome<GovernanceServiceContinuityView> NativeGovernanceSupervisionContinuityRead::read_accepted_supervision_continuity_at(
 const GovernanceAcceptedWorkHistory& h,const WorkEpisode& e,const WorldMoment& when,const SourceRef* cut)const{
 return guard<GovernanceServiceContinuityView>([&]{return at(context_,h,e,when,cut);});
}
Outcome<GovernanceServiceContinuityView> NativeGovernanceSupervisionContinuityRead::read_accepted_supervision_continuity_span(
 const GovernanceAcceptedWorkHistory& h,const WorkEpisode& e,const SupervisionActionStartSource& start,const MomentSpan& span)const{
 return guard<GovernanceServiceContinuityView>([&]{
  take(validate_governance_supervision_service_window(context_.history,e,start,span));auto all=history(context_,h,e);
  const auto& common=context_.history.programs.common;const auto cut=position(common,{"tasks","supervision_action_started",start.fact_id});
  HistoryState state=all.initial;GovernanceServiceContinuityView out;append(out.actual_sources,{governance_program_source(*h.approval),1});append(out.actual_sources,state.organization_source);
  WorldMoment cursor=span.from;
  for(const auto& event:all.events){
   const auto relative=cmp(event.at,span.from);if(relative<0||(relative==0&&event.order<cut)){apply(state,event);append(out.actual_sources,event.source);continue;}
   if(cmp(event.at,span.to)>=0)break;
   if(cmp(cursor,event.at)<0&&!out.actual_blocking_source)out.actual_blocking_source=blocked(state,all.policy);
   apply(state,event);append(out.actual_sources,event.source);cursor=event.at;
  }
  if(cmp(cursor,span.to)<0&&!out.actual_blocking_source)out.actual_blocking_source=blocked(state,all.policy);
  return out;
 });
}
Status NativeGovernanceSupervisionContinuityRead::validate_actual_supervision_interruption(
 const GovernanceAcceptedWorkHistory& h,const WorkEpisode& e,const SupervisionActionStartSource& start,
 const MomentOwnerSource& cause,const WorldMoment& when)const{
 return guard<Done>([&]{
  const auto& p=context_.history.programs;
  auto accepted=take(read_governance_accepted_work_history(context_.history,e));
  if(!h.supervision||!accepted.supervision||h.approval!=accepted.approval||h.specification!=accepted.specification||h.supervision!=accepted.supervision||&context_.body.common!=&p.common)
   fail("GovernanceContinuityCandidateMismatch",e.work_id);
  const auto now=take(p.clock.read_organization_authority_now());
  if(cmp(now,context_.body.actual_now)||cmp(when,now)>0)fail("GovernanceContinuityBodyPhaseChanged",e.work_id);
  if(context_.original_sources_stamp.mode()!=ReadMode::phase_current&&context_.original_sources_stamp.mode()!=ReadMode::committed_current)fail("GovernanceContinuityCurrentSourcesStampRequired",e.work_id);
  take(context_.stamp_authority.validate(context_.original_sources_stamp,p.common.state().identity,context_.original_sources_stamp.mode(),p.common,context_.versions));
  // 真实绝对期限决定先于实际释放；不借尚未生成的 WorkResult 证明原因。
  if(cause.source_ref.domain=="politics"&&cause.source_ref.kind=="governance_work_deadline_terminated"){
   if(cause.source_schema!=1||!lifecycle_||&lifecycle_->history!=&context_.history||&lifecycle_->continuity.history!=&context_.history||&lifecycle_->continuity.body!=&context_.body||&lifecycle_->continuity.original_sources_stamp!=&context_.original_sources_stamp)fail("GovernanceWorkDeadlineContextRequired",cause.source_ref.id);
   const auto* decision=take(read_governance_work_deadline_termination(*lifecycle_,cause.source_ref,context_.original_sources_stamp));
   const auto* actual=context_.history.tasks.work_execution?context_.history.tasks.work_execution->supervision_starts.find(start.fact_id):nullptr;
   const SourceRef start_source{"tasks","supervision_action_started",start.fact_id};
   const auto* executor=take(read_governance_supervision_executor_at(context_.history,e,start.started_at,&start_source));
   if(actual!=&start||start.work_id!=e.work_id||!executor||*executor!=start.actor_id||decision->work_id!=e.work_id||cmp(decision->effective_at,when)||position(p.common,cause.source_ref)<=position(p.common,start_source))fail("GovernanceSupervisionDeadlineInterruptionMismatch",cause.source_ref.id);
   return Done{};
  }
  if(cause.source_ref.domain=="actor_foundation"&&cause.source_ref.kind=="condition_receipt"&&cause.source_schema==2){
   const auto* receipt=take(read_verified_character_condition_change(context_.body,{cause.source_ref.id}));
   if(receipt->actor_id!=start.actor_id||receipt->condition_revision_before<start.condition_at_start.condition_revision||receipt->condition_revision_after!=receipt->condition_revision_before+1||receipt->effective_at.kind!=ActorFoundationAt::Kind::exact_moment||cmp(receipt->effective_at.exact_moment,when))fail("GovernanceSupervisionBodyInterruptionMismatch",cause.source_ref.id);
   const auto* purpose=context_.body.catalog.capability_purpose_bindings.find("s3_fixed_work_supervisor");if(!purpose)fail("SupervisionCapabilityPurposeRequired");
   ActorConditionHead before=start.condition_at_start;before.condition_revision=receipt->condition_revision_before;before.latest_receipt_ref.reset();
   if(before.condition_revision){for(const auto& row:context_.body.state.character_condition_extensions.applied_receipts.rows())if(row.second.actor_id==start.actor_id&&row.second.condition_revision_after==before.condition_revision){if(before.latest_receipt_ref)fail("GovernanceSupervisionBodyPredecessorAmbiguous",cause.source_ref.id);before.latest_receipt_ref=ConditionReceiptRefV2{row.first};}if(!before.latest_receipt_ref)fail("GovernanceSupervisionBodyPredecessorMissing",cause.source_ref.id);}
   auto prior=take(read_character_capability_at_head(context_.body,before,purpose->purpose_ref,std::nullopt,when));
   ActorConditionHead after=before;after.condition_revision=receipt->condition_revision_after;after.latest_receipt_ref=ConditionReceiptRefV2{receipt->result_id};
   auto capability=take(read_character_capability_at_head(context_.body,after,purpose->purpose_ref,std::nullopt,when));
   if(!prior.allowed||capability.allowed)fail("GovernanceSupervisionBodyDidNotInterrupt",cause.source_ref.id);
   return Done{};
  }
  auto all=history(context_,h,e);const auto boundary=position(p.common,cause.source_ref);
  if(boundary<=position(p.common,{"tasks","supervision_action_started",start.fact_id}))fail("GovernanceSupervisionInterruptionBeforeStart",cause.source_ref.id);
  HistoryState state=all.initial;const Event* actual=nullptr;
  for(const auto& event:all.events){if(event.order>boundary)break;apply(state,event);if(event.order==boundary)actual=&event;}
  if(!actual||actual->source.source_schema!=cause.source_schema||!same_source_ref(actual->source.source_ref,cause.source_ref)||cmp(actual->at,when))fail("GovernanceSupervisionActualInterruptionRequired",cause.source_ref.id);
  auto interruption=blocked(state,all.policy);
  if(!interruption||!same_source_ref(interruption->source_ref,cause.source_ref))fail("GovernanceSupervisionSourceDidNotInterrupt",cause.source_ref.id);
  return Done{};
 });
}
}
