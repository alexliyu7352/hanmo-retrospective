#include "work_supervision_original.hpp"
#include "work_execution_detail.hpp"
#include "work_skill_inputs.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/checkpoint/checkpoint_json_decode.hpp"
#include <initializer_list>
#include <limits>
#include <new>
#include <string_view>
#include <algorithm>
namespace hanmo::resident { namespace {
struct Failure {RuleError error;};
[[noreturn]] void reject(const char* code,const StableId& id={}) {throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
void take(Status value){if(!value.ok)throw Failure{std::move(value.error)};}
template<class T,class Fn>Outcome<T> guard(Fn fn){try{return {true,fn(),{}};}catch(Failure& e){return {false,{},std::move(e.error)};}}
int cmp(const WorldMoment& a,const WorldMoment& b){return take(compare_world_moments(a,b));}
const SupervisedWorkPlan& plan(const WorkEpisode& e){const auto* p=std::get_if<SupervisedWorkPlan>(&e.frozen_plan);if(!p)reject("SupervisionPlanRequired",e.work_id);return *p;}
bool should_append_supervision_source(const std::vector<MomentOwnerSource>& refs,const MomentOwnerSource& value){
 if(!work_detail::valid(value.source_ref)||value.source_schema<1)reject("SupervisionOriginalSourceRequired",value.source_ref.id);
 for(const auto& old:refs)if(work_detail::same(old,value))return false;
 return true;
}
void append(std::vector<MomentOwnerSource>& refs,const MomentOwnerSource& value){if(should_append_supervision_source(refs,value))refs.push_back(value);}
void append(std::vector<MomentOwnerSource>& refs,const std::vector<MomentOwnerSource>& values){for(const auto& value:values)append(refs,value);}
void context(const SupervisionNativeContext& x){
 if(&x.common!=&x.body.common||!x.tasks.work_execution||!task_work_execution_version(x.tasks.work_execution->version))reject("SupervisionCandidateSourcesRequired");
 if(cmp(take(x.clock.read_current_moment()),x.body.actual_now))reject("SupervisionBodyReadPhaseChanged");
}
const ActorFoundationRuleRef& purpose(const SupervisionNativeContext& x){
 const auto* p=x.body.catalog.capability_purpose_bindings.find("s3_fixed_work_supervisor");
 if(!p)reject("SupervisionCapabilityPurposeRequired");return p->purpose_ref;
}
// The original normal-work reducer is body-only. Permanent skill level is a
// separate original qualification gate, checked after the real Start cut and
// again before every service span; it is never multiplied by body efficiency.
void validate_start_skill_rule(const SupervisionNativeContext&x,const WorkEpisode&e){
 if(task_work_skill_inputs_version(x.tasks.work_execution->version)){if(!x.skill_inputs||!x.skill_inputs->bound_to(x.common,x.tasks))reject("WorkSkillInputsReadRequired",e.work_id);(void)take(x.skill_inputs->read_start_requirement(e));return;}
 if(plan(e).required_skill_ref)reject("SupervisionSkillRequiresPhysical4",e.work_id);return;
}
void validate_saved_skill(const SupervisionNativeContext&x,const WorkEpisode&e,const SupervisionActionStartSource&s){
 if(task_work_skill_inputs_version(x.tasks.work_execution->version)){if(!x.skill_inputs||!x.skill_inputs->bound_to(x.common,x.tasks))reject("WorkSkillInputsReadRequired",e.work_id);take(x.skill_inputs->validate_start_qualification(e,s));return;}
 if(plan(e).required_skill_ref||s.skill_qualification)reject("SupervisionSkillRequiresPhysical4",e.work_id);return;
}
SourceRef anchor_source(const MovementAnchor& anchor){return std::visit([](const auto& v)->SourceRef{using T=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<T,TerrainContinuousAnchor>)return v.actual_prefix_close_source;else return v.actual_source;},anchor);}
void scope_shape(const WorkEpisode& e,const SupervisionScopeView& scope){
 (void)plan(e);if(scope.actor_id.empty()||scope.actual_sources.empty())reject("SupervisionOriginalScopeMismatch",e.work_id);
 for(const auto& ref:scope.actual_sources)if(!work_detail::valid(ref.source_ref)||ref.source_schema<1)reject("SupervisionScopeSourceRequired",e.work_id);
}
MomentOwnerSource head_source(const SupervisionNativeContext& x,const ActorConditionHead& h,const WorldMoment& at){auto r=take(read_character_condition_head_source(x.body,h,at));return {r.source,r.source_schema};}
template<class Visitor>void visit_supervision_start_source_inputs(const SupervisionNativeContext& x,const SupervisionScopeView& scope,const SourceRef& presence,const ActorConditionHead& body,const WorldMoment& at,Visitor&& visitor){
 for(const auto& value:scope.actual_sources)visitor(value);
 {
  const auto presence_memory=CheckpointMemoryCharge(0);
  visitor(MomentOwnerSource{presence,1});
 }
 visitor(head_source(x,body,at));
}
std::vector<MomentOwnerSource> start_sources(const SupervisionNativeContext& x,const SupervisionScopeView& scope,const SourceRef& presence,const ActorConditionHead& body,const WorldMoment& at){
 std::vector<MomentOwnerSource> out;visit_supervision_start_source_inputs(x,scope,presence,body,at,[&](const MomentOwnerSource& value){append(out,value);});return out;
}
class PreparedSupervisionHistoricalStartSources {
 CheckpointMemoryCharge source_memory_;
 std::vector<MomentOwnerSource> sources_;
 static std::size_t source_capacity(std::size_t count){if(count>std::numeric_limits<std::size_t>::max()-2)throw std::bad_alloc();return count+2;}
 static CheckpointMemoryCharge shell_memory(std::size_t count){return CheckpointMemoryCharge(0);}
 PreparedSupervisionHistoricalStartSources(const SupervisionNativeContext& x,const SupervisionScopeView& scope,const SourceRef& presence,const ActorConditionHead& body,const WorldMoment& at,std::size_t count)
  :source_memory_(shell_memory(count)){
  sources_.reserve(count);
  visit_supervision_start_source_inputs(x,scope,presence,body,at,[&](const MomentOwnerSource& value){
   if(!should_append_supervision_source(sources_,value))return;
   if(sources_.size()>=count)throw std::bad_alloc();
   (source_memory_).grow(0);
   sources_.push_back(value);
  });
 }
public:
 PreparedSupervisionHistoricalStartSources(const SupervisionNativeContext& x,const SupervisionScopeView& scope,const SourceRef& presence,const ActorConditionHead& body,const WorldMoment& at)
  :PreparedSupervisionHistoricalStartSources(x,scope,presence,body,at,source_capacity(scope.actual_sources.size())){}
 PreparedSupervisionHistoricalStartSources(const PreparedSupervisionHistoricalStartSources&)=delete;
 PreparedSupervisionHistoricalStartSources& operator=(const PreparedSupervisionHistoricalStartSources&)=delete;
 PreparedSupervisionHistoricalStartSources(PreparedSupervisionHistoricalStartSources&&)=delete;
 PreparedSupervisionHistoricalStartSources& operator=(PreparedSupervisionHistoricalStartSources&&)=delete;
 const std::vector<MomentOwnerSource>& sources()const noexcept{return sources_;}
};
void source_header(const CommonStore& common,const SourceRef& ref){const auto* h=common.state().facts.find(ref.id);if(!h||h->domain!=ref.domain||h->kind!=ref.kind)reject("SupervisionOriginalHeaderMissing",ref.id);}
const SupervisionActionStartSource& window_start(const SupervisionSourceContext& x,const WorkEpisode& e,const MomentSpan& span){
 const SupervisionActionStartSource* found=nullptr;
 for(const auto& row:x.tasks.work_execution->supervision_starts.rows()){
  const auto& s=row.second;if(s.work_id!=e.work_id||cmp(s.started_at,span.from)>0||cmp(span.to,take(read_supervision_execution_until(x,s)))>0)continue;
  bool ended=false;for(const auto& t:x.tasks.work_execution->supervision_terminals.rows())if(t.second.start_source_ref==s.fact_id&&cmp(t.second.ended_at,span.to)<0)ended=true;
  if(ended)continue;
  if(found)reject("AmbiguousSupervisionWindowExecution",e.work_id);
  found=take(read_supervision_action_start(x,row.first));
 }
 if(!found)reject("SupervisionWindowExecutionMissing",e.work_id);return *found;
}
// 每片身体与已经实际结算的经济正服务求交，既不从世界等待造开工，
// 也不以窗口最终零验工抹去此前真实正服务。
class CapabilityWindow final:public CharacterCapabilitySpanVisitor {
 const SupervisionNativeContext& x_;const EconomicWorkWindowRead2& economic_;WorkPeriodRead& out_;bool scope_allowed_;
public:
 bool all_allowed=true;
 CapabilityWindow(const SupervisionNativeContext& x,const EconomicWorkWindowRead2& e,WorkPeriodRead& out,bool allowed):x_(x),economic_(e),out_(out),scope_allowed_(allowed){}
 Status visit_capability_span(const MomentSpan& span,const CharacterCapabilityView& capability)override{return guard<Done>([&]{
  const auto body=head_source(x_,capability.condition_head,span.from);append(out_.actual_anchor_sources,body);
  if(!capability.allowed){all_allowed=false;if(out_.reason==WorkPeriodReason::qualified){out_.reason=WorkPeriodReason::leader_incapacitated;out_.reason_source=body;}return Done{};}
  if(!scope_allowed_)return Done{};
  for(const auto& part:economic_.source_parts){
   if(take(compare_fractions(part.actual_worker_halfdays,CanonicalFraction{}))<=0)continue;
   const auto from=cmp(span.from,part.window.from)<0?part.window.from:span.from;
   const auto to=cmp(span.to,part.window.to)<0?span.to:part.window.to;
   if(cmp(from,to)<0&&(!out_.actual_positive_start||cmp(from,*out_.actual_positive_start)<0))out_.actual_positive_start=from;
  }
  return Done{};
 });}
};
}
Outcome<SupervisionStartInputs> NativeSupervisionEligibilityRead::read_current_start_inputs(const WorkEpisode& e,const WorldMoment& at)const{return guard<SupervisionStartInputs>([&]{
 const auto& x=context_;context(x);if(cmp(at,take(x.clock.read_current_moment())))reject("SupervisionStartNotAtActualMoment",e.work_id);
 auto scope=take(x.scopes.read_start_scope(e,at,nullptr));scope_shape(e,scope);if(scope.actual_blocking_source)reject("SupervisionAuthorityCurrentlyBlocked",e.work_id);
 // 原 scope 的同候选批准链选定执行者；不从命令 actor 或旧 frozen plan 猜替任。
 const auto& actor_id=scope.actor_id;
 const auto* actor=x.common.actor(actor_id);if(!actor||!actor->alive||!actor->available||!actor->action_id.empty())reject("SupervisionExecutorUnavailable",actor_id);
 auto carried=take(read_actual_transport_participation_presence(x.spatial.carriers,x.spatial.originals,actor_id));if(carried)reject("SupervisionExecutorCarried",actor_id);
 if(const auto* binding=[&]()->const SpatialWriterBinding*{
  const std::initializer_list<std::string_view> pieces{"actor:",actor_id};
  const auto key_memory=[&]()->CheckpointMemoryCharge{
   if(checkpoint_boundary_decode_active())return CheckpointMemoryCharge{};
   std::size_t count=0;for(auto piece:pieces){if(piece.size()>std::numeric_limits<std::size_t>::max()-count)throw std::bad_alloc();count+=piece.size();}
   return CheckpointMemoryCharge(0);
  }();
  const auto key=checkpoint_decode_boundary_join(pieces);
  return x.spatial.movement.spatial_bindings.find(key);
 }();binding&&!std::holds_alternative<StableMovementWriter>(binding->writer))reject("SupervisionStablePresenceRequired",actor_id);
 auto spatial=take(read_actual_actor_spatial_location(x.common,x.spatial.movement,x.spatial.originals,x.spatial.battle,actor_id));
 const MovementAnchor& actual_anchor=*take(require_actor_ordinary_movement_anchor(spatial));
 if(!take(same_spatial_anchor(scope.original_target_anchor,actual_anchor)))reject("SupervisionExecutorOffTarget",actor_id);
 auto body=take(read_current_character_condition(x.body,actor_id));
 validate_start_skill_rule(x,e);auto capability=take(read_character_capability_at_head(x.body,body.head,purpose(x),std::nullopt,at));if(!capability.allowed)reject("SupervisionExecutorIncapacitated",actor_id);
 SupervisionStartInputs out;out.actor_id=actor_id;out.accepted_presence_source=anchor_source(actual_anchor);out.condition_at_start=body.head;
 out.actual_sources=start_sources(x,scope,out.accepted_presence_source,out.condition_at_start,at);return out;
});}
Status NativeSupervisionEligibilityRead::validate_historical_start_inputs(const WorkEpisode& e,const SupervisionActionStartSource& s)const{return guard<Done>([&]{
 const auto& x=context_;context(x);
 const auto cut_memory=CheckpointMemoryCharge(0);
 const SourceRef cut{"tasks","supervision_action_started",s.fact_id};source_header(x.common,cut);
 const auto* saved=x.tasks.work_execution->supervision_starts.find(s.fact_id);if(saved!=&s||s.work_id!=e.work_id||s.condition_at_start.actor_id!=s.actor_id)reject("SupervisionHistoricalStartMismatch",s.fact_id);
 auto scope=take(x.scopes.read_start_scope(e,s.started_at,&cut));scope_shape(e,scope);if(scope.actor_id!=s.actor_id)reject("SupervisionHistoricalStartExecutorMismatch",s.fact_id);if(scope.actual_blocking_source)reject("SupervisionStartAuthorityInvalid",s.fact_id);
 auto spatial=take(read_actor_spatial_location_before_source(x.common,x.spatial.catalog,x.spatial.movement,x.spatial.carriers,x.spatial.registrations,x.body.state,x.body.catalog,x.spatial.originals,s.actor_id,s.accepted_presence_source,cut,s.started_at,x.spatial.military_handoffs));
 if(!take(same_spatial_anchor(scope.original_target_anchor,spatial.actual_anchor)))reject("SupervisionHistoricalTargetMismatch",s.fact_id);
 validate_saved_skill(x,e,s);auto capability=take(read_character_capability_at_head(x.body,s.condition_at_start,purpose(x),std::nullopt,s.started_at));if(!capability.allowed)reject("SupervisionHistoricalCapabilityRejected",s.fact_id);
 const PreparedSupervisionHistoricalStartSources expected(x,scope,s.accepted_presence_source,s.condition_at_start,s.started_at);
 if(!work_detail::same_refs(expected.sources(),s.original_start_sources))reject("SupervisionHistoricalInputsChanged",s.fact_id);
 return Done{};
});}
Status NativeSupervisionEligibilityRead::validate_actual_service_span(const WorkEpisode&e,const SupervisionActionStartSource&s,const MomentSpan&span)const{return guard<Done>([&]{
 const auto&x=context_;context(x);if(cmp(span.from,span.to)>=0||cmp(span.from,s.started_at)<0||cmp(span.to,take(x.clock.read_current_moment()))>0||s.work_id!=e.work_id)reject("SupervisionActualServiceSpanRequired",s.fact_id);
 // F records actual supervisor service even where the economic result is zero.
 // All authority, stationary presence and body/skill eligibility remain real.
 auto scope=take(x.scopes.read_service_scope(e,s,span));scope_shape(e,scope);if(scope.actor_id!=s.actor_id)reject("SupervisionServiceExecutorMismatch",s.fact_id);if(scope.actual_blocking_source)reject("SupervisionServiceAuthorityBlocked",s.fact_id);
 auto spatial=take(read_actor_stationary_location_span(x.common,x.spatial.catalog,x.spatial.movement,x.spatial.carriers,x.spatial.registrations,x.body.state,x.body.catalog,x.spatial.originals,s.actor_id,s.accepted_presence_source,span,x.spatial.military_handoffs));
 if(!take(same_spatial_anchor(scope.original_target_anchor,spatial.actual_anchor)))reject("SupervisionServiceOffTarget",s.fact_id);
 validate_saved_skill(x,e,s);take(x.skill_inputs->validate_service_skill(e,s,span));auto head=take(read_character_condition_at_moment(x.body,s.actor_id,span.from));
 class Eligible final:public CharacterCapabilitySpanVisitor {public:Status visit_capability_span(const MomentSpan&,const CharacterCapabilityView&v)override{return v.allowed?common_success():common_failure("SupervisionActualServiceIncapacitated",v.condition_head.actor_id);}} visitor;
 take(visit_character_capability_span(x.body,head.head,purpose(x),std::nullopt,span,visitor));return Done{};
});}
Status NativeSupervisionEligibilityRead::validate_actual_release(const WorkEpisode& e,const SupervisionActionStartSource& s,SupervisionReleaseKind kind,const MomentOwnerSource& cause,const WorldMoment& at,const WorkOriginalRead& original)const{
 return context_.scopes.validate_actual_release(e,s,kind,cause,at,original);
}
Outcome<WorkPeriodRead> read_supervised_economic_period(const SupervisionNativeContext& x,const SupervisionSourceContext& execution,const SupervisionEconomicSources& economic,const WorkEpisode& e,const WorkEconomicPeriodSource& source){return guard<WorkPeriodRead>([&]{
 context(x);if(&x.common!=&execution.common||&x.tasks!=&execution.tasks)reject("SupervisionPeriodCandidateMismatch",e.work_id);
 const auto& p=plan(e);if(!work_detail::same(p.economic_work_ref,source.work_ref)||source.work_ref.owner_domain!="local_economy"||source.work_ref.kind==EconomicWorkKind::facility_construction||source.source_parts.empty())reject("SupervisionEconomicOwnerRequired",e.work_id);
 auto duration=take(elapsed_world_duration(source.window.from,source.window.to));if(take(compare_fractions(duration.value,take(normalize_fraction("1","1")))))reject("WorkPeriodMustBeOneExactHalfDay",e.work_id);
 take(validate_economic_work_sources(x.common,economic.economy,economic.catalog,economic.rules,economic.state,economic.originals,economic.units_per_half_day));
 WorldMoment actual_until=source.window.from;
 for(const auto& ref:source.source_parts){
  if(ref.source_schema!=2||ref.source_ref.domain!="local_economy"||ref.source_ref.kind!="production_input_segment")reject("SupervisionAppliedEconomicSegmentRequired",ref.source_ref.id);
  const auto* segment=take(read_verified_production_input_segment(x.common,economic.economy,economic.catalog,economic.rules,economic.state,economic.originals,ref.source_ref,ref.source_schema,economic.units_per_half_day));if(!segment||segment->source.unit_id!=source.work_ref.work_id)reject("SupervisionEconomicSegmentMismatch",ref.source_ref.id);
  source_header(x.common,ref.source_ref);const auto end=cmp(segment->source.to,source.window.to)>0?source.window.to:segment->source.to;if(cmp(actual_until,end)<0)actual_until=end;
 }
 if(cmp(actual_until,source.window.from)<=0||cmp(actual_until,take(x.clock.read_current_moment()))>0)reject("SupervisionActualServiceWindowRequired",e.work_id);
 const MomentSpan supplied{source.window.from,actual_until};const auto& start=window_start(execution,e,supplied);
 auto original=take(read_economic_work_window(x.common,economic.economy,economic.rules,economic.state,economic.originals,source.work_ref,supplied,x.common.state().identity));
 std::vector<MomentOwnerSource> parts;for(const auto& part:original.source_parts)append(parts,{part.receipt_ref,2});if(!work_detail::same_refs(parts,source.source_parts))reject("SupervisionEconomicSourceListMismatch",e.work_id);
 auto scope=take(x.scopes.read_service_scope(e,start,supplied));scope_shape(e,scope);if(scope.actor_id!=start.actor_id)reject("SupervisionServiceExecutorMismatch",start.fact_id);
 auto spatial=take(read_actor_stationary_location_span(x.common,x.spatial.catalog,x.spatial.movement,x.spatial.carriers,x.spatial.registrations,x.body.state,x.body.catalog,x.spatial.originals,start.actor_id,start.accepted_presence_source,supplied,x.spatial.military_handoffs));
 if(!take(same_spatial_anchor(scope.original_target_anchor,spatial.actual_anchor)))reject("SupervisionServiceOffTarget",e.work_id);
 WorkPeriodRead out;out.canonical_source=source;out.window=source.window;out.actual_until=actual_until;out.leader_actor_id=start.actor_id;out.execution_source=WorkCommonActionBinding{start.action_id};out.reason=WorkPeriodReason::qualified;
 append(out.actual_anchor_sources,spatial.original_sources);append(out.actual_anchor_sources,scope.actual_sources);
 bool all_economic=true;
 for(const auto& part:original.source_parts){
  if(!take(same_spatial_anchor(scope.original_target_anchor,OriginalMovementAnchor{part.site_or_anchor_ref,part.receipt_ref})))reject("SupervisionEconomicSiteMismatch",part.receipt_ref.id);
  const auto amount=take(compare_fractions(part.actual_worker_halfdays,CanonicalFraction{}));if(amount<0)reject("SupervisionNegativeOriginalWork",part.receipt_ref.id);
  if(amount==0){all_economic=false;if(out.reason==WorkPeriodReason::qualified){out.reason=WorkPeriodReason::source_no_work;out.reason_source=MomentOwnerSource{part.receipt_ref,2};}}
 }
 if(scope.actual_blocking_source){out.reason=WorkPeriodReason::authority_paused;out.reason_source=scope.actual_blocking_source;}
 // 身体的起始head取真实服务from已应用链，不用最早Action开工head跨过后续变化。
 auto opening_body=take(read_character_condition_at_moment(x.body,start.actor_id,supplied.from));
 validate_saved_skill(x,e,start);if(task_work_skill_inputs_version(x.tasks.work_execution->version))take(x.skill_inputs->validate_service_skill(e,start,supplied));CapabilityWindow visitor(x,original,out,!scope.actual_blocking_source);take(visit_character_capability_span(x.body,opening_body.head,purpose(x),std::nullopt,supplied,visitor));
 const bool complete=cmp(actual_until,source.window.to)==0;
 out.source_provided=complete&&all_economic&&visitor.all_allowed&&!scope.actual_blocking_source;
 if(!complete&&out.reason==WorkPeriodReason::qualified){out.reason=WorkPeriodReason::source_interrupted;out.reason_source=parts.back();}
 if(out.source_provided){out.reason=WorkPeriodReason::qualified;out.reason_source.reset();}
 if(!out.source_provided&&!out.reason_source)reject("SupervisionNonServiceCauseRequired",e.work_id);
 return out;
});}
Outcome<WorkCompletionRead> read_supervised_economic_completion(const SupervisionNativeContext&x,const SupervisionSourceContext&execution,const SupervisionEconomicSources&economy,const WorkEpisode&e,const std::vector<StableId>&ids,const WorldMoment&through){return guard<WorkCompletionRead>([&]{
 context(x);const auto&p=plan(e);if(p.kind!=WorkKind::agronomy_supervision&&p.kind!=WorkKind::industry_supervision)reject("ConstructionOriginalCompletionReaderRequired",e.work_id);
 if(ids!=e.accepted_period_refs)reject("SupervisionCompletionReceiptOrderMismatch",e.work_id);WorkCompletionRead out;bool actual_service=false;
 for(const auto&id:ids){const auto*r=x.tasks.work_execution->period_receipts.find(id);if(!r||r->work_id!=e.work_id||!work_detail::header(x.common,id,"work_period_receipt"))reject("SupervisionCompletionReceiptMissing",id);const auto*source=std::get_if<WorkEconomicPeriodSource>(&r->source);if(!source)reject("SupervisionCompletionPeriodKindMismatch",id);auto actual=take(read_supervised_economic_period(x,execution,economy,e,*source));if(cmp(actual.actual_until,through)>0)reject("SupervisionCompletionBeforeActualWork",id);actual_service=actual_service||actual.source_provided;
  const auto*action=std::get_if<WorkCommonActionBinding>(&actual.execution_source);if(!action)reject("SupervisionActualActionRequired",id);const SupervisionActionStartSource*start=nullptr;for(const auto&row:x.tasks.work_execution->supervision_starts.rows())if(row.second.action_id==action->action_id){if(start)reject("SupervisionCompletionAmbiguousExecution",id);start=&row.second;}if(!start||start->work_id!=e.work_id)reject("SupervisionCompletionExecutionMissing",id);append(out.actual_leader_refs,{{"tasks","supervision_action_started",start->fact_id},1});append(out.actual_stage_result_refs,actual.stage_result_refs);
 }
 // The S3 supervision requirement is real accepted service. Its later production
 // improvement or facility application remains the original professional result.
 out.route_or_stage_complete=actual_service;return out;
});}
Outcome<StableId> consume_supervised_economic_window_in_candidate(CommonStore& common,TaskState& tasks,const SupervisionNativeContext& x,const SupervisionSourceContext& execution,const SupervisionEconomicSources& economic,EconomicProductionContext& production,WorkExpenseConsumer& expenses,const StableId& id,const MomentSpan& scheduled){return guard<StableId>([&]{
 context(x);if(&common!=&x.common||&tasks!=&x.tasks||&economic.state!=&production.state||&economic.rules!=&production.catalog||&economic.originals!=&production.inputs)reject("SupervisionEconomicWriterCandidateMismatch",id);
 auto now=take(x.clock.read_current_moment());if(cmp(now,take(production.clock.read_current_moment()))||cmp(now,scheduled.from)<=0||cmp(now,scheduled.to)>0)reject("SupervisionEconomicWriterMomentMismatch",id);
 WorkExecutionRead reader(common,tasks,execution.original);const auto* e=take(reader.read_work(id));const auto& start=window_start(execution,*e,{scheduled.from,now});
 // 同一已关闭源重入只反读原receipt，不能再关输入段、重复收费或生成新ID。
 for(const auto& receipt_id:e->accepted_period_refs){const auto* saved=tasks.work_execution->period_receipts.find(receipt_id);const auto* original=saved?std::get_if<WorkEconomicPeriodSource>(&saved->source):nullptr;if(original&&cmp(original->window.from,scheduled.from)==0&&cmp(original->window.to,scheduled.to)==0){take(reader.read_period(receipt_id));return receipt_id;}}
 SupervisedEconomicWindowRequest request{{id},{"tasks","work_accepted",e->acceptance_source_ref},{{"tasks","supervision_action_started",start.fact_id},1},{scheduled.from,now}};
 // Only S5 can construct this scope, after it has validated the old set and
 // actually closed the requested input segment. The borrowed reader never
 // escapes the synchronous callback or changes the published cold provider.
 class AppliedPeriodOriginal final:public WorkOriginalRead {
  const WorkOriginalRead& original_;const SupervisionNativeContext& native_;
  const SupervisionSourceContext& execution_;const SupervisionEconomicSources& economic_;
  const WorkEpisode& work_;const WorkEconomicPeriodSource& source_;
 public:
  AppliedPeriodOriginal(const WorkOriginalRead&o,const SupervisionNativeContext&n,const SupervisionSourceContext&x,const SupervisionEconomicSources&e,const WorkEpisode&w,const WorkEconomicPeriodSource&s):original_(o),native_(n),execution_(x),economic_(e),work_(w),source_(s){}
  const WorkSkillInputsRead* skill_inputs()const noexcept override{return original_.skill_inputs();}
  Outcome<WorkAcceptedRead> read_accepted_work(const WorkAuthorizationRef&a)const override{return original_.read_accepted_work(a);}
  Status validate_current_start(const WorkAcceptedRead&a,const WorldMoment&m)const override{return original_.validate_current_start(a,m);}
  Outcome<WorkExecutionSourceRead> read_original_execution(const WorkEpisode&w,const MomentOwnerSource&r)const override{return original_.read_original_execution(w,r);}
  Outcome<WorkPeriodRead> read_original_period(const WorkEpisode&w,const WorkPeriodSource&r)const override{
   auto same=work_detail::same_period(r,WorkPeriodSource{source_});if(!same.ok)return {false,{},same.error};
   if(&w==&work_&&same.value)return read_supervised_economic_period(native_,execution_,economic_,w,source_);
   return original_.read_original_period(w,r);
  }
  Outcome<WorkCompletionRead> read_completion(const WorkEpisode&w,const std::vector<StableId>&r,const WorldMoment&m)const override{return original_.read_completion(w,r,m);}
  Outcome<WorkStopRead> read_actual_stop(const WorkEpisode&w,const MomentOwnerSource&r,const WorldMoment&m)const override{return original_.read_actual_stop(w,r,m);}
  Status validate_leader_change(const WorkEpisode&w,const WorkLeaderChange&r)const override{return original_.validate_leader_change(w,r);}
  Status validate_original_source(const MomentOwnerSource&r)const override{
   // Both original period qualification and its canonical source list must
   // still validate the exact applied row, including the original inputs.
   for(const auto& part:source_.source_parts)if(work_detail::same(part,r)){
    auto v=read_verified_production_input_segment(native_.common,economic_.economy,economic_.catalog,economic_.rules,economic_.state,economic_.originals,r.source_ref,r.source_schema,economic_.units_per_half_day);
    if(!v.ok)return {false,{},v.error};
    if(!v.value||v.value->source.unit_id!=source_.work_ref.work_id)return common_failure("SupervisionAppliedWindowSourceMismatch",r.source_ref.id,ErrorKind::internal);
    return common_success();
   }
   return original_.validate_original_source(r);
  }
 };
 class ConsumeApplied final:public AppliedSupervisedEconomicWindowConsumer {
  CommonStore& common_;TaskState& tasks_;const SupervisionNativeContext& native_;
  const SupervisionSourceContext& execution_;const SupervisionEconomicSources& economic_;
  WorkExpenseConsumer& expenses_;const WorkEpisode& work_;const MomentSpan& scheduled_;const WorldMoment& now_;
 public:
  ConsumeApplied(CommonStore&c,TaskState&t,const SupervisionNativeContext&n,const SupervisionSourceContext&x,const SupervisionEconomicSources&e,WorkExpenseConsumer&fees,const WorkEpisode&w,const MomentSpan&s,const WorldMoment&m):common_(c),tasks_(t),native_(n),execution_(x),economic_(e),expenses_(fees),work_(w),scheduled_(s),now_(m){}
  Outcome<StableId> consume_applied_window(const EconomicWorkWindowRead2&closed,const ProductionInputSourceRead&inputs)override{return guard<StableId>([&]{
   SupervisionEconomicSources scoped{economic_.economy,economic_.catalog,economic_.rules,economic_.state,inputs,economic_.units_per_half_day};
   WorkEconomicPeriodSource source{plan(work_).economic_work_ref,scheduled_,{}};for(const auto&part:closed.source_parts)append(source.source_parts,{part.receipt_ref,2});
   AppliedPeriodOriginal original(execution_.original,native_,execution_,scoped,work_,source);
   take(read_supervised_economic_period(native_,execution_,scoped,work_,source));
   return take(consume_work_period_in_candidate(common_,tasks_,original,expenses_,work_.work_id,source,now_));
  });}
 } consumer(common,tasks,x,execution,economic,expenses,*e,scheduled,now);
 return take(close_and_consume_supervised_economic_window_in_candidate(common,economic.economy,economic.catalog,production,reader,execution.original,request,consumer));
});}
}
