#include "care_personal_stop.hpp"
#include "profile_control_source.hpp"
#include "resident/domains/actor_foundation/condition_source_read.hpp"
#include "resident/domains/director/director_command_acceptance.hpp"
#include "resident/kernel/command_types.hpp"
#include <algorithm>
#include <charconv>
#include <string_view>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={},ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,id,kind).error};}
void check(Status v){if(!v.ok)throw Failure{std::move(v.error)};}
template<class T>T take(Outcome<T> v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
bool same(const SourceRef& a,const SourceRef& b){return same_source_ref(a,b);}
bool same(const MomentOwnerSource& a,const MomentOwnerSource& b){return a.source_schema==b.source_schema&&same(a.source_ref,b.source_ref);}
bool same(const WorldMoment& a,const WorldMoment& b){return take(compare_world_moments(a,b))==0;}
bool head_equal(const ActorConditionHead& a,const ActorConditionHead& b){return a.actor_id==b.actor_id&&a.condition_revision==b.condition_revision&&a.initial_condition_seed_ref.seed_key==b.initial_condition_seed_ref.seed_key&&a.latest_receipt_ref.has_value()==b.latest_receipt_ref.has_value()&&(!a.latest_receipt_ref||a.latest_receipt_ref->result_id==b.latest_receipt_ref->result_id);}
bool text(const StableId& s){return !s.empty()&&s.size()<=256;}
bool catalog(const CatalogIdentity& a,const CatalogIdentity& b){return a.content_id==b.content_id&&a.content_version==b.content_version&&a.content_digest==b.content_digest&&a.ruleset_id==b.ruleset_id&&a.schema_sha256==b.schema_sha256&&a.pack_set_digest==b.pack_set_digest&&a.module_versions==b.module_versions&&a.artifact_sha256==b.artifact_sha256;}
bool field(const LifeCatalogFieldRef& a,const LifeCatalogFieldRef& b){return catalog(a.catalog_identity,b.catalog_identity)&&a.scenario_id==b.scenario_id&&a.entity_id==b.entity_id&&a.field_selector==b.field_selector;}
bool route_equal(const LifeActorRoute& a,const LifeActorRoute& b){
 if(a.index()!=b.index())return false;
 if(const auto* p=std::get_if<LifePlayerActorRoute>(&a)){const auto& q=std::get<LifePlayerActorRoute>(b);const auto& x=p->original_control_source_ref;const auto& y=q.original_control_source_ref;if(x.index()!=y.index())return false;if(const auto* ref=std::get_if<SourceRef>(&x))return same(*ref,std::get<SourceRef>(y));if(const auto* f=std::get_if<LifeCatalogFieldRef>(&x))return field(*f,std::get<LifeCatalogFieldRef>(y));return false;}
 const auto& x=std::get<LifeNpcActorRoute>(a);const auto& y=std::get<LifeNpcActorRoute>(b);const auto* p=std::get_if<LifeCatalogFieldRef>(&x.policy_ref);const auto* q=std::get_if<LifeCatalogFieldRef>(&y.policy_ref);
 return x.subject_actor_id==y.subject_actor_id&&x.registration_epoch==y.registration_epoch&&x.decision_seq==y.decision_seq&&p&&q&&field(*p,*q);
}
const CareGoalSourceContext& context(const CareFamilyActivitySources& x){
 const auto& a=x.coverage.handovers.acceptance;const auto& g=a.goals;
 if(g.rules.selected_care_version()!=6||&x.commands.handovers!=&x.coverage.handovers||x.coverage.originals.family_activity_sources()!=&x||!x.authority.bound_to(g.common)||&a.controls.common!=&g.common||a.controls.foundation!=&a.body.state||a.controls.foundation_catalog!=&a.body.catalog||!a.controls.actual_clock||!a.people.bound_to(g.common,g.deaths))fail("CarePersonalStopOriginalSourcesRequired",{},ErrorKind::unsupported_capability);
 const auto now=take(g.clock.read_current_moment());if(!same(now,a.body.actual_now)||!same(now,take(a.controls.actual_clock->read_current_moment())))fail("CarePersonalStopActualClockMismatch");return g;
}
std::size_t index(const CommonStore& c,const SourceRef& ref){const auto* h=c.state().facts.find(ref.id);const auto& o=c.state().facts.order();const auto at=std::find(o.begin(),o.end(),ref.id);if(!h||h->id!=ref.id||h->domain!=ref.domain||h->kind!=ref.kind||at==o.end()||std::count(o.begin(),o.end(),ref.id)!=1)fail("CarePersonalStopOriginalHeaderRequired",ref.id);return static_cast<std::size_t>(at-o.begin());}
void before(const CommonStore& c,const SourceRef& a,const SourceRef& b){if(index(c,a)>=index(c,b))fail("CarePersonalStopOriginalOrderMismatch",b.id);}
Integer allocation_serial(const CommonStore& c,const StableId& value,std::string_view prefix){
 const std::string_view id(value);if(id.size()<=prefix.size()||id.substr(0,prefix.size())!=prefix)fail("CarePersonalStopCanonicalAllocationRequired",value);
 const auto digits=id.substr(prefix.size());Integer n=0;const auto parsed=std::from_chars(digits.data(),digits.data()+digits.size(),n);
 if(digits.front()=='0'||parsed.ec!=std::errc{}||parsed.ptr!=digits.data()+digits.size()||n<1||n>=c.state().next_instance_id)fail("CarePersonalStopCanonicalAllocationRequired",value);return n;
}
Integer allocated(const CommonStore& c,const SourceRef& ref){take(care_personal_stop_source_schema(ref));index(c,ref);return allocation_serial(c,ref.id,"care_stop:");}
struct OriginalExpression {const InitialConditionSeed* seed=nullptr;const ConditionReceiptV2* receipt=nullptr;};
OriginalExpression original_expression(const CareFamilyActivitySources& x,const StableId& actor,const SourceRef& source,const WorldMoment& at){
 const auto& body=x.coverage.handovers.acceptance.body;const auto cut=allocated(body.common,source);
 // Both identities are the real synchronous S4 writer allocation points.
 // No arbitrary source kind, caller-supplied serial, lexical ID ordering or
 // same-M inference is accepted. This read is private to this actual stop.
 auto current=take(read_current_character_condition(body,actor)); // full sources and reverse coverage
 ActorConditionHead head{actor,0,current.head.initial_condition_seed_ref,{}};
 const auto* seed=body.state.actor_start_context.initial_condition_seeds.find(head.initial_condition_seed_ref.seed_key);
 if(!seed||seed->seed_key!=head.initial_condition_seed_ref.seed_key||seed->actor_id!=actor)fail("CarePersonalStopOriginalConditionSeedRequired",actor);
 const ConditionReceiptV2* last=nullptr;Integer previous=0;
 for(const auto& row:body.state.character_condition_extensions.applied_receipts.rows())if(row.second.actor_id==actor){
  const auto n=allocation_serial(body.common,row.second.result_id,"condition:");
  if(row.first!=row.second.result_id||n<=previous||n==cut)fail("CarePersonalStopConditionAllocationOrderMismatch",row.first);previous=n;
  if(n<cut){head.condition_revision=row.second.condition_revision_after;head.latest_receipt_ref=ConditionReceiptRefV2{row.second.result_id};last=&row.second;}
 }
 if(!take(read_character_expression_at_head(body,head,at)).allowed)fail("CarePersonalStopOriginalExpressionDenied",actor);
 return {seed,last};
}

const CommitResult& sealed(const CareFamilyActivitySources& x,const CarePersonalStop& stop,const WorldMoment& at){
 const auto& g=context(x);const auto& command=stop.actual_command;const auto& b=command.command_binding;
 const auto* cached=g.common.state().results.find(command.actor_id+":"+std::to_string(b.request_seq));
 if(!cached||!*cached||!(*cached)->ok||!(*cached)->details||(*cached)->kind!="stop_care"||(*cached)->kind!=b.command_kind||(*cached)->result_id!=b.result_id||(*cached)->tick!=at.half_day_tick||(*cached)->revision<1||(*cached)->revision>g.common.revision())fail("CarePersonalStopSealedCommandRequired",stop.actual_stop_result.source_ref.id);
 const auto& result=**cached;const auto* effect=std::get_if<FamilyEffect>(&result.details->data);
 if(!effect||!effect->success||!*effect->success||!effect->actor_id||*effect->actor_id!=command.actor_id||!effect->fact_id||*effect->fact_id!=stop.actual_stop_result.source_ref.id)fail("CarePersonalStopActualResultMismatch",result.result_id);
 std::size_t matches=0;for(const auto& h:g.common.state().history)if(h.result_id==result.result_id){++matches;if(h.actor_id!=command.actor_id||h.kind!=result.kind||h.tick!=result.tick)fail("CarePersonalStopHistoryMismatch",result.result_id);}if(matches!=1)fail("CarePersonalStopHistoryRequired",result.result_id);return result;
}
LifePlayerActorRoute player(const CareFamilyActivitySources& x,const ProfileControlEligibility& control){
 const auto& a=x.coverage.handovers.acceptance;LifePlayerActorRoute out;
 std::visit([&](const auto& value){using T=std::decay_t<decltype(value)>;
  if constexpr(std::is_same_v<T,AuthoredProfileController>)out.original_control_source_ref=LifeCatalogFieldRef{a.body.catalog.identity,value.scenario_id,value.actor_id,"player_actor"};
  else {const StableId* id=nullptr;if constexpr(std::is_same_v<T,CreatedProfileController>)id=&value.creation_fact_id;else if constexpr(std::is_same_v<T,SuccessionProfileController>)id=&value.succession_fact_id;else if constexpr(std::is_same_v<T,ContinuationProfileController>)id=&value.activation_id;else id=&value.activation_fact_id;const auto* h=a.goals.common.state().facts.find(*id);if(!h)fail("CarePersonalStopControlSourceRequired",*id);SourceRef ref{h->domain,h->kind,*id};auto original=take(read_profile_controller_actual_source(a.controls,a.body.catalog.identity,ref));if(!original.actor_id||*original.actor_id!=control.actor_id)fail("CarePersonalStopControlActorMismatch",*id);out.original_control_source_ref=std::move(ref);}
 },control.source);return out;
}
LifeActorRoute route(const CareFamilyActivitySources& x,const CarePersonalStop& stop,const WorldMoment& at){
 const auto& g=context(x);const auto& command=stop.actual_command;sealed(x,stop,at);allocated(g.common,stop.actual_stop_result.source_ref);
 auto control=take(read_profile_control_before_fact(x.coverage.handovers.acceptance.controls,stop.actual_stop_result.source_ref));
 if(control.actor_id==command.actor_id)return player(x,control);
 if(!x.commands.director||!x.commands.director_catalog||!x.commands.director->subject2)fail("CarePersonalStopNpcAdoptionRequired",command.actor_id,ErrorKind::unsupported_capability);
 const DirectorAdoptionResult2* decision=nullptr;for(const auto& row:x.commands.director->subject2->adoption_results.rows())if(row.second.accepted_command&&row.second.accepted_command->result_id==command.command_binding.result_id){if(decision)fail("CarePersonalStopNpcAdoptionNotUnique",command.actor_id);decision=&row.second;}
 if(!decision)fail("CarePersonalStopNpcAdoptionRequired",command.actor_id,ErrorKind::unsupported_capability);
 auto adopted=take(read_director_accepted_command(g.common,*x.commands.director,*x.commands.director_catalog,x.coverage.handovers.acceptance.body.catalog.identity,{command.actor_id,decision->registration_epoch,decision->decision_seq,command.command_binding.command_kind,command.command_binding.request_seq,command.command_binding.result_id}));
 if(!adopted.command||!same(adopted.command->accepted_at,at)||!same(adopted.command->actual_effect,stop.actual_stop_result))fail("CarePersonalStopNpcActualSourceMismatch",command.actor_id);
 LifeNpcActorRoute out;out.subject_actor_id=command.actor_id;out.registration_epoch=decision->registration_epoch;out.decision_seq=decision->decision_seq;out.policy_ref=LifeCatalogFieldRef{adopted.command->policy_catalog,g.common.state().metadata.scenario_id,adopted.command->policy_id,"director_policy"};return out;
}
struct Current {CareFamilyActivityRead view;const CareBinding* binding=nullptr;const CareRequirement* goal=nullptr;ActorConditionHead condition;};
Current current(const CarePersonalStopContext& x,const StableId& actor,const StableId& id){
 const auto& g=context(x.sources);const auto now=take(g.clock.read_current_moment());if(!same(now,take(x.reads.read_current_moment())))fail("CarePersonalStopReadClockMismatch");
 auto view=take(read_care_family_activity(x.sources,{care_family_activity_started_source(id),1}));
 if(view.activity->carer_actor_id!=actor||view.activity->ended_by)fail("CarePersonalStopActualActiveCarerRequired",actor,ErrorKind::rule);
 const CareBinding* binding=nullptr;for(const auto& row:x.sources.coverage.tables.bindings.rows())if(same(row.second.care_activity_ref,MomentOwnerSource{care_family_activity_started_source(id),1})){if(binding)fail("CarePersonalStopBindingNotUnique",id);binding=&row.second;}
 if(!binding||binding->phase!=CareBindingPhase::active)fail("CarePersonalStopActiveBindingRequired",id,ErrorKind::rule);
 const auto* goal=g.goals.requirements.find(view.accepted->requirement_id);if(!goal||goal->state!=StateGoalState::satisfied||!goal->current_care_binding_ref||*goal->current_care_binding_ref!=binding->binding_id||!same(goal->current_transition_ref,view.completed->original_transition_ref))fail("CarePersonalStopActiveGoalRequired",id,ErrorKind::rule);
 if(!take(x.sources.coverage.handovers.acceptance.people.read_current_presence(actor)).alive)fail("CarePersonalStopLivingCarerRequired",actor,ErrorKind::rule);
 check(x.sources.authority.validate_original_activity_continuation(*view.activity,*view.accepted,{view.activity->started_at,now},nullptr));
 auto condition=take(read_current_character_condition(x.sources.coverage.handovers.acceptance.body,actor));
 if(!take(read_character_expression_at_head(x.sources.coverage.handovers.acceptance.body,condition.head,now)).allowed)fail("CarePersonalStopExpressionDenied",actor,ErrorKind::rule);
 return {view,binding,goal,std::move(condition.head)};
}
} // namespace
SourceRef care_personal_stop_source(const StableId& id){return {"care_requirements","care_stopped",id};}
Outcome<Integer> care_personal_stop_source_schema(const SourceRef& ref){if(ref.domain=="care_requirements"&&ref.kind=="care_stopped"&&text(ref.id))return {true,1,{}};return {false,{},common_failure("CarePersonalStopOriginalSourceRequired",ref.id,ErrorKind::unsupported_capability).error};}
Status validate_care_personal_stop_shape(const CareFamilyActivity& a,const CarePersonalStop& s){
 const auto& c=s.actual_command;const auto source=care_personal_stop_source_schema(s.actual_stop_result.source_ref);
 if(!source.ok||s.actual_stop_result.source_schema!=1||c.actor_id!=a.carer_actor_id||c.accepted_terms_ref!=a.accepted_care_terms_ref||c.command_binding.command_kind!="stop_care"||c.command_binding.request_seq<1||!text(c.command_binding.result_id)||!c.authority_source_refs.empty())return common_failure("InvalidCarePersonalStop",a.activity_id,ErrorKind::internal);
 // This finite command is the actual caregiver's own cessation, not a guardian
 // or institution's substituted choice. It grants no property or future care.
 if(const auto* npc=std::get_if<LifeNpcActorRoute>(&c.route_basis)){if(npc->subject_actor_id!=c.actor_id||npc->registration_epoch<0||npc->decision_seq<1||!std::holds_alternative<LifeCatalogFieldRef>(npc->policy_ref))return common_failure("InvalidCarePersonalStopRoute",a.activity_id,ErrorKind::internal);}
 else {const auto& ref=std::get<LifePlayerActorRoute>(c.route_basis).original_control_source_ref;if(!std::holds_alternative<SourceRef>(ref)&&!std::holds_alternative<LifeCatalogFieldRef>(ref))return common_failure("InvalidCarePersonalStopRoute",a.activity_id,ErrorKind::internal);}
 return common_success();
}
bool care_personal_stop_equal(const CarePersonalStop& a,const CarePersonalStop& b){const auto& x=a.actual_command;const auto& y=b.actual_command;if(!same(a.actual_stop_result,b.actual_stop_result)||x.actor_id!=y.actor_id||x.accepted_terms_ref!=y.accepted_terms_ref||x.command_binding.command_kind!=y.command_binding.command_kind||x.command_binding.request_seq!=y.command_binding.request_seq||x.command_binding.result_id!=y.command_binding.result_id||!route_equal(x.route_basis,y.route_basis)||x.authority_source_refs.size()!=y.authority_source_refs.size())return false;for(std::size_t i=0;i<x.authority_source_refs.size();++i)if(!same(x.authority_source_refs[i],y.authority_source_refs[i]))return false;return true;}
Outcome<CarePersonalStopSourceRead> read_care_personal_stop_source(const CareFamilyActivitySources& x,const MomentOwnerSource& source){try{
 const auto& g=context(x);if(source.source_schema!=take(care_personal_stop_source_schema(source.source_ref)))fail("CarePersonalStopSchemaMismatch",source.source_ref.id);allocated(g.common,source.source_ref);
 const CareFamilyActivity* activity=nullptr;const CarePersonalStop* stop=nullptr;
 for(const auto& row:x.activities.rows())if(row.second.ended_by)if(const auto* s=std::get_if<CarePersonalStop>(&row.second.ended_by->cause);s&&same(s->actual_stop_result,source)){if(activity||row.first!=row.second.activity_id)fail("CarePersonalStopPayloadNotUnique",source.source_ref.id);activity=&row.second;stop=s;}
 if(!activity||!stop)fail("CarePersonalStopActualPayloadRequired",source.source_ref.id);
 check(validate_care_personal_stop_shape(*activity,*stop));const auto& end=*activity->ended_by;
 const auto start=take(read_care_family_activity_start_provenance(x,{care_family_activity_started_source(activity->activity_id),1}));
 if(start.activity!=activity||take(compare_world_moments(end.ended_at,take(g.clock.read_current_moment())))>0||take(compare_world_moments(end.ended_at,activity->started_at))<0)fail("CarePersonalStopOriginalMomentMismatch",source.source_ref.id);
 before(g.common,start.completed->original_transition_ref,source.source_ref);before(g.common,source.source_ref,care_family_activity_ended_source(end.end_id));
 if(!take(x.coverage.handovers.acceptance.people.read_presence_before_original_fact(activity->carer_actor_id,end.ended_at.half_day_tick,source.source_ref)).alive)fail("CarePersonalStopActorNotAliveAtSource",source.source_ref.id);
 if(!route_equal(stop->actual_command.route_basis,route(x,*stop,end.ended_at)))fail("CarePersonalStopFrozenRouteMismatch",source.source_ref.id);
 const auto expression=original_expression(x,activity->carer_actor_id,source.source_ref,end.ended_at);
 return {true,{activity,&end,stop,expression.seed,expression.receipt},{}};
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Outcome<PreparedCarePersonalStop> prepare_care_personal_stop(const CarePersonalStopContext& x,const StableId& actor,const StopCareRequest& request){try{
 const auto& g=context(x.sources);check(validate_care_coverage_source_closure(x.sources.coverage));const auto actual=current(x,actor,request.activity_id);const auto now=take(g.clock.read_current_moment());
 const auto body_source=take(read_character_condition_head_source(x.sources.coverage.handovers.acceptance.body,actual.condition,now));
 auto stamp=take(x.reads.capture({{ResourceKey::Kind::actor,actor}}, {{"actor_foundation","condition_head",actor}}, {actual.view.activity->activity_id,actual.view.accepted->accepted_id,actual.goal->current_transition_ref.id,body_source.source.id}));
 if(!stamp||(stamp->mode()!=ReadMode::committed_current&&stamp->mode()!=ReadMode::phase_current)||!same(stamp->span().to,now))fail("CarePersonalStopActualStampRequired",actor);check(x.stamps.validate(*stamp,g.common.state().identity,stamp->mode(),g.common,x.versions));
 PreparedCarePersonalStop out;out.common_=&g.common;out.sources_=&x.sources;out.actor_=actor;out.activity_=request.activity_id;out.binding_=actual.binding->binding_id;out.goal_=actual.goal->id;out.transition_=actual.goal->current_transition_ref.id;out.at_=now;out.condition_=actual.condition;out.stamp_=std::move(stamp);return {true,std::move(out),{}};
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Outcome<PendingCarePersonalStop> apply_care_personal_stop_in_candidate(CommonStore& c,const CarePersonalStopContext& x,PreparedCarePersonalStop&& p){try{
 const auto& g=context(x.sources);if(&g.common!=&c||p.common_!=&c||p.sources_!=&x.sources||!p.stamp_||!same(p.at_,take(g.clock.read_current_moment())))fail("CarePersonalStopSamePreparedCandidateRequired");check(x.stamps.validate(*p.stamp_,c.state().identity,p.stamp_->mode(),c,x.versions));
 const auto actual=current(x,p.actor_,p.activity_);if(actual.binding->binding_id!=p.binding_||actual.goal->id!=p.goal_||actual.goal->current_transition_ref.id!=p.transition_||!head_equal(actual.condition,p.condition_))fail("CarePersonalStopPreparedInputChanged",p.activity_);
 PendingCarePersonalStop out;out.common_=&c;out.sources_=&x.sources;out.actor_=p.actor_;out.activity_=p.activity_;out.binding_=p.binding_;out.goal_=p.goal_;out.transition_=p.transition_;out.at_=p.at_;out.condition_=std::move(p.condition_);out.stop_=take(c.next_id("care_stop"));check(c.append_fact({out.stop_,"care_stopped","care_requirements"}));p.common_=nullptr;p.stamp_.reset();return {true,std::move(out),{}};
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Outcome<MomentOwnerSource> read_pending_care_personal_stop_source(const PendingCarePersonalStop& p,const CarePersonalStopContext& x){try{
 const auto& g=context(x.sources);if(p.common_!=&g.common||p.sources_!=&x.sources||!same(p.at_,take(g.clock.read_current_moment())))fail("CarePersonalStopLivePendingRequired",p.stop_);
 allocated(g.common,care_personal_stop_source(p.stop_));if(g.common.state().facts.order().empty()||g.common.state().facts.order().back()!=p.stop_)fail("CarePersonalStopImmediatePendingRequired",p.stop_);
 const auto actual=current(x,p.actor_,p.activity_);if(actual.binding->binding_id!=p.binding_||actual.goal->id!=p.goal_||actual.goal->current_transition_ref.id!=p.transition_||!head_equal(actual.condition,p.condition_))fail("CarePersonalStopPendingInputChanged",p.stop_);before(g.common,actual.view.completed->original_transition_ref,care_personal_stop_source(p.stop_));return {true,{care_personal_stop_source(p.stop_),1},{}};
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Outcome<BirthCareNeedApplication> bind_care_personal_stop_command_in_candidate(CommonStore& c,CareGoalWriteTables goals,CareCoverageWriteTables coverage,CareFamilyActivityWriteBinding& write,const CarePersonalStopContext& x,PendingCarePersonalStop& pending,const StableId& actor,Integer sequence,const CommitResult& result,AffairsState& affairs,const StateNeedPolicyRead& policies,const StateGoalRead& goal_read,StateTaskConsumer& tasks){try{
 const auto& g=context(x.sources);const auto now=take(g.clock.read_current_moment());if(&c!=&g.common||pending.common_!=&c||pending.sources_!=&x.sources||&write.sources!=&x.sources||&write.activities!=&x.sources.activities||coverage.family_activities!=&write||actor!=pending.actor_||!same(now,pending.at_))fail("CarePersonalStopSamePendingCandidateRequired",pending.stop_);
 const auto* retained=c.state().results.find(actor+":"+std::to_string(sequence));const auto* seq=c.state().request_sequences.find(actor);if(sequence<1||!retained||retained->get()!=&result||!seq||*seq!=sequence||result.revision!=c.revision()||c.state().history.empty())fail("CarePersonalStopActualSealedResultRequired",result.result_id);const auto& h=c.state().history.back();if(h.actor_id!=actor||h.result_id!=result.result_id||h.kind!=result.kind||h.tick!=result.tick)fail("CarePersonalStopActualSealedHistoryRequired",result.result_id);
 auto* a=write.activities.find(pending.activity_);if(!a)fail("CarePersonalStopOriginalActivityMissing",pending.activity_);
 CarePersonalStop stop;stop.actual_stop_result={care_personal_stop_source(pending.stop_),1};stop.actual_command.actor_id=actor;stop.actual_command.command_binding={result.kind,sequence,result.result_id};stop.actual_command.accepted_terms_ref=a->accepted_care_terms_ref;stop.actual_command.route_basis=route(x.sources,stop,now);check(validate_care_personal_stop_shape(*a,stop));
 if(a->ended_by){const auto* prior=std::get_if<CarePersonalStop>(&a->ended_by->cause);if(!prior||!same(a->ended_by->ended_at,now)||!care_personal_stop_equal(*prior,stop))fail("ConflictingCarePersonalStopSeal",pending.stop_);take(read_care_personal_stop_source(x.sources,stop.actual_stop_result));}
 else {take(read_pending_care_personal_stop_source(pending,x));CareFamilyActivityEnd end;end.end_id=take(c.next_id("care_family_activity_end"));end.ended_at=now;end.cause=std::move(stop);const auto id=end.end_id;a->ended_by.emplace(std::move(end));check(validate_care_family_activity_shape(*a,6));check(c.append_fact({id,"care_family_activity_ended","care_requirements"}));}
 auto applied=take(lose_care_coverage_after_personal_stop_in_candidate(c,goals,coverage,g,pending.binding_,{care_personal_stop_source(pending.stop_),1}));
 auto need=take(reconcile_applied_care_goal_in_candidate(c,goals,g,std::move(applied),affairs,policies,goal_read,tasks));check(validate_care_coverage_source_closure(x.sources.coverage));return {true,std::move(need),{}};
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Status visit_care_personal_stop_command_result_references(const CareFamilyActivitySources& x,CarePersonalStopCommandReferenceSink& sink){try{
 check(validate_care_family_activity_sources(x));check(validate_care_coverage_source_closure(x.coverage));
 for(const auto& row:x.activities.rows())if(row.second.ended_by)if(const auto* stop=std::get_if<CarePersonalStop>(&row.second.ended_by->cause)){const auto actual=take(read_care_personal_stop_source(x,stop->actual_stop_result));check(sink.actual_reference({actual.activity,actual.stop}));}
 return common_success();}catch(Failure& e){return {false,{},std::move(e.error)};}}
}
