#include "craft_interval.hpp"
#include "craft_action_time.hpp"
#include "craft_character_sources.hpp"
#include "craft_spatial_sources.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include <algorithm>
#include <new>
#include <limits>
#include <optional>
#include <stdexcept>
namespace hanmo::resident {
namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char*code,const StableId&id={},ErrorKind kind=ErrorKind::internal){auto error=common_failure(code,id).error;error.kind=kind;throw Failure{std::move(error)};}
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
bool same(const WorldMoment&a,const WorldMoment&b,ExactArithmeticBudget budget){return take(compare_world_moments(a,b,budget))==0;}
bool same_span(const MomentSpan&a,const MomentSpan&b,ExactArithmeticBudget budget){return same(a.from,b.from,budget)&&same(a.to,b.to,budget);}
bool same_proof(const WorkServiceIntervalProof&a,const WorkServiceIntervalProof&b,ExactArithmeticBudget budget){
 if(!same_work_binding(a.execution_binding,b.execution_binding)||!same_span(a.actual_span,b.actual_span,budget)||a.actual_labor_allocations!=b.actual_labor_allocations||a.facility_claim_refs!=b.facility_claim_refs||a.actual_actor_roles.size()!=b.actual_actor_roles.size()||a.participation_sources.size()!=b.participation_sources.size())return false;
 for(std::size_t i=0;i<a.actual_actor_roles.size();++i)if(a.actual_actor_roles[i].actor_id!=b.actual_actor_roles[i].actor_id||a.actual_actor_roles[i].role!=b.actual_actor_roles[i].role)return false;
 for(std::size_t i=0;i<a.participation_sources.size();++i)if(a.participation_sources[i].source_schema!=b.participation_sources[i].source_schema||!same_source_ref(a.participation_sources[i].source_ref,b.participation_sources[i].source_ref))return false;
 return true;
}
WorldMoment actual_now(const CraftJobReadContext&x){const auto at=take(x.clock.read_current_moment());take(compare_world_moments(at,at,x.arithmetic_budget));if(at.half_day_tick<x.materials.common.tick())fail("CraftClockBeforeCommon");return at;}
// The same original proof is passed through S0's complete participation checks.
// Only the return value is frozen here; source validation always calls its owner.
struct PreparedCraftServiceSpan {
 CheckpointMemoryCharge copied_fields;
 const CraftServiceSpan row;
 explicit PreparedCraftServiceSpan(const CraftServiceSpanBuildView& source)
  :copied_fields(reserve(source)),row{source.id,source.job_id,source.gate_ordinal,source.actual}{}
 PreparedCraftServiceSpan(const PreparedCraftServiceSpan&)=delete;
 PreparedCraftServiceSpan& operator=(const PreparedCraftServiceSpan&)=delete;
 PreparedCraftServiceSpan(PreparedCraftServiceSpan&&)=delete;
 PreparedCraftServiceSpan& operator=(PreparedCraftServiceSpan&&)=delete;
private:
 static CheckpointMemoryCharge reserve(const CraftServiceSpanBuildView& source){
  return CheckpointMemoryCharge(0);
 }
};
// Own the full augmented proof before any copied payload is constructed.
// The old copied source vector and the independently built final vector are
// both covered until assembly completes; the peak reservation stays alive
// through the synchronous S0 read and the independent Slice copy.
class PreparedCraftIntervalProof final {
 CheckpointMemoryCharge copied_fields_;
 WorkServiceIntervalProof proof_;
 static std::size_t source_count(const WorkServiceIntervalProof& original,const std::vector<MomentOwnerSource>& bodies,const std::vector<MomentOwnerSource>& locations){
  const auto limit=std::numeric_limits<std::size_t>::max();
  auto count=original.participation_sources.size();
  if(bodies.size()>limit-count)throw std::bad_alloc();count+=bodies.size();
  if(locations.size()>limit-count)throw std::bad_alloc();return count+locations.size();
 }

 static CheckpointMemoryCharge memory(const WorkServiceIntervalProof& original,const std::vector<MomentOwnerSource>& bodies,const std::vector<MomentOwnerSource>& locations){


  return CheckpointMemoryCharge(0);
 }
 static void append(std::vector<MomentOwnerSource>& target,const std::vector<MomentOwnerSource>& sources){
  for(const auto& source:sources)target.push_back(source);
 }
public:
 PreparedCraftIntervalProof(const WorkServiceIntervalProof& original,const std::vector<MomentOwnerSource>& bodies,const std::vector<MomentOwnerSource>& locations)
  :copied_fields_(memory(original,bodies,locations)),proof_(original){
  std::vector<MomentOwnerSource> sources;sources.reserve(source_count(original,bodies,locations));
  append(sources,original.participation_sources);append(sources,bodies);append(sources,locations);
  proof_.participation_sources.swap(sources);
 }
 PreparedCraftIntervalProof(const PreparedCraftIntervalProof&)=delete;
 PreparedCraftIntervalProof& operator=(const PreparedCraftIntervalProof&)=delete;
 PreparedCraftIntervalProof(PreparedCraftIntervalProof&&)=delete;
 PreparedCraftIntervalProof& operator=(PreparedCraftIntervalProof&&)=delete;
 const WorkServiceIntervalProof& proof()const noexcept{return proof_;}
};
class AdoptedProofRead final:public WorkServiceIntervalRead {
 const WorkServiceIntervalProof& proof_;const CraftJobOriginalRead& original_;
 const CraftCharacterSourceContext& characters_;const CraftJob& job_;ExactArithmeticBudget budget_;
 const CraftSpatialSourceContext& spatial_;
public:
 AdoptedProofRead(const WorkServiceIntervalProof&p,const CraftJobOriginalRead&o,const CraftCharacterSourceContext&characters,const CraftJob&job,ExactArithmeticBudget b,const CraftSpatialSourceContext&spatial):proof_(p),original_(o),characters_(characters),job_(job),budget_(b),spatial_(spatial){}
 Outcome<const WorkServiceIntervalProof*> read_actual_work_service_interval(const ActorActionWorkBinding&binding,const MomentSpan&span)const override{
  try{if(!same_work_binding(proof_.execution_binding,binding)||!same_span(proof_.actual_span,span,budget_))fail("CraftIntervalProofScopeMismatch");return {true,&proof_,{}};}catch(Failure&e){return {false,{},std::move(e.error)};}
 }
 Status validate_work_service_participation_source(const WorkServiceIntervalProof&p,const MomentOwnerSource&s)const override{
  if(is_craft_participant_body_source(s))return validate_prepared_craft_participant_body_source(characters_,job_,p,s);
  if(is_craft_participant_spatial_source(s))return validate_prepared_craft_participant_spatial_source(spatial_,job_,p,s);
  return original_.validate_prepared_craft_service_source(p,s);
 }
};
void action_scope(const CommonStore&common,const CraftJob&job){
 const auto*action=common.action(job.execution_binding.action_id);if(!action||action->kind!="craft_job"||action->actor_id!=job.terms.maker.id||action->participants.size()!=job.terms.accepted_helpers.size()+1||action->participants.front()!=job.terms.maker.id||!std::equal(job.terms.accepted_helpers.begin(),job.terms.accepted_helpers.end(),action->participants.begin()+1))fail("CraftIntervalActionScopeMismatch",job.id);
 for(const auto&actor:action->participants){const auto*row=common.actor(actor);if(!row||row->action_id!=action->id)fail("CraftIntervalParticipationChanged",job.id);}
}
RuleError exhausted(){auto error=common_failure("CraftIntervalResourceExhausted").error;error.kind=ErrorKind::internal;return error;}
}
struct CraftIntervalCandidate::Impl final:CheckpointAllocatedObject {
private:
 CheckpointMemoryCharge copied_metadata_;
 static CheckpointMemoryCharge metadata_memory(const WorldIdentity& identity,const MomentSpan& span,const TimeScaleRef& scale){
  return CheckpointMemoryCharge(0);
 }
public:
 struct Slice final:CheckpointAllocatedObject {
 private:
  CheckpointMemoryCharge copied_inputs_;
  static CheckpointMemoryCharge input_memory(const StableId& id,const WorkServiceIntervalProof& actual){
   return CheckpointMemoryCharge(0);
  }
 public:
  StableId job_id;PreparedSharedWorkSlice prepared;WorkServiceIntervalProof proof;
  std::size_t prior_service_count=0;std::optional<ProducedStableId> applied_service;
  Slice(const StableId& id,const PreparedSharedWorkSlice& ready,const WorkServiceIntervalProof& actual,std::size_t prior)
   :copied_inputs_(input_memory(id,actual)),job_id(id),prepared(ready),proof(actual),prior_service_count(prior){}
  Slice(const Slice&)=delete;
  Slice& operator=(const Slice&)=delete;
  Slice(Slice&&)=delete;
  Slice& operator=(Slice&&)=delete;
 };
 const CommonStore* common=nullptr;const CraftJobsState* jobs=nullptr;const ActionElapsedStore* elapsed=nullptr;
 const CommonCatalog* common_catalog=nullptr;const CraftRecipeCatalog* recipes=nullptr;
 const TravelCatalog* travel_catalog=nullptr;const MovementExactState* movement=nullptr;
 const SpatialCarrierState* carriers=nullptr;const TransportRegistrationState* registrations=nullptr;
 const AuthoredActorFoundationState* foundation=nullptr;const ActorFoundationCatalog* actor_catalog=nullptr;
 const MilitarySpatialHandoffState* military_handoffs=nullptr;const FamilyCatalog* family_catalog=nullptr;
 WorldIdentity identity;MomentSpan span;TimeScaleRef scale;ExactArithmeticBudget budget{};
 CheckpointVector<std::unique_ptr<Slice>> slices;bool consumed=false;
 Impl(const WorldIdentity& identity_value,const MomentSpan& span_value,const TimeScaleRef& scale_value)
  :copied_metadata_(metadata_memory(identity_value,span_value,scale_value)),identity(identity_value),span(span_value),scale(scale_value){}
 Impl(const Impl&)=delete;
 Impl& operator=(const Impl&)=delete;
 Impl(Impl&&)=delete;
 Impl& operator=(Impl&&)=delete;
};
CraftIntervalCandidate::CraftIntervalCandidate(std::unique_ptr<Impl>p):impl_(std::move(p)){}
CraftIntervalCandidate::~CraftIntervalCandidate()=default;
Outcome<std::unique_ptr<CraftIntervalCandidate>> begin_craft_interval_candidate(const CraftJobReadContext&x,const MomentSpan&span){try{
 take(make_moment_span(span.from,span.to,x.arithmetic_budget));if(take(compare_world_moments(span.from,span.to,x.arithmetic_budget))>=0||!same(actual_now(x),span.from,x.arithmetic_budget))fail("CraftIntervalActualFromRequired");
 if(!x.character_sources)fail("CraftCharacterSourceProviderRequired",{},ErrorKind::unsupported_capability);
 if(!x.spatial_sources)fail("CraftSpatialSourceProviderRequired",{},ErrorKind::unsupported_capability);
 if(&x.spatial_sources->craft!=&x)fail("CraftSpatialSourceContextMismatch");
 if(&x.character_sources->body.common!=&x.materials.common||&x.character_sources->jobs!=&x.materials.jobs||&x.character_sources->recipes!=&x.materials.recipes)fail("CraftCharacterSourceContextMismatch");
 take(validate_craft_jobs(x));auto state=std::make_unique<CraftIntervalCandidate::Impl>(x.materials.common.state().identity,span,x.scale);state->common=&x.materials.common;state->jobs=&x.materials.jobs;state->elapsed=&x.elapsed;state->common_catalog=&x.materials.common_catalog;state->recipes=&x.materials.recipes;state->budget=x.arithmetic_budget;
 const auto&space=*x.spatial_sources;state->travel_catalog=&space.travel_catalog;state->movement=&space.movement;state->carriers=&space.carriers;state->registrations=&space.registrations;state->foundation=&space.foundation;state->actor_catalog=&space.actor_catalog;state->military_handoffs=space.military_handoffs;state->family_catalog=space.family_catalog;
 for(const auto&entry:x.materials.jobs.jobs.rows()){
  const auto&job=entry.second;if(job.phase!=CraftJobPhase::working)continue;action_scope(x.materials.common,job);
  const auto original_proof=take(x.original.prepare_craft_service_interval(job.execution_binding,span));
  // Body inputs are captured by their actual owner here, never supplied by a
  // caller or inferred later from the terminal head at the slice's to.
  for(const auto&source:original_proof.participation_sources)if(is_craft_participant_body_source(source)||is_craft_participant_spatial_source(source))fail("CraftPreparedParticipantInputOwnedByCapture",job.id);
  const auto bodies=take(capture_craft_participant_body_inputs(*x.character_sources,job,span));
  const auto locations=take(capture_craft_participant_spatial_inputs(*x.spatial_sources,job,original_proof));
  const PreparedCraftIntervalProof augmented(original_proof,bodies.sources(),locations.sources());const auto& proof=augmented.proof();
  AdoptedProofRead adopted(proof,x.original,*x.character_sources,job,x.arithmetic_budget,*x.spatial_sources);
  auto prepared=take(prepare_shared_work_slice(*take(read_craft_actual_work(x,job)),*x.materials.common.action(job.execution_binding.action_id),job.execution_binding,span,actual_now(x),x.scale,adopted,x.arithmetic_budget));
  if(proof.facility_claim_refs.empty())fail("CraftIntervalFacilityCoverageRequired",job.id,ErrorKind::unsupported_capability);
  state->slices.push_back(std::make_unique<CraftIntervalCandidate::Impl::Slice>(job.id,prepared,proof,job.actual_service_refs.size()));
 }
 return {true,std::unique_ptr<CraftIntervalCandidate>(new CraftIntervalCandidate(std::move(state))),{}};
}catch(Failure&e){return {false,{},std::move(e.error)};}catch(const std::bad_alloc&){return {false,{},exhausted()};}catch(const std::length_error&){return {false,{},exhausted()};}}
Status consume_craft_interval_candidate_in_candidate(CommonStore&common,CraftJobsState&jobs,ActionElapsedStore&elapsed,const CraftJobWriteContext&x,CraftIntervalCandidate&candidate,const MomentSpan&span){try{
 auto*p=candidate.impl_.get();if(!p||p->common!=&common||p->jobs!=&jobs||p->elapsed!=&elapsed||&x.read.materials.common!=&common||&x.read.materials.jobs!=&jobs||&x.read.elapsed!=&elapsed||p->common_catalog!=&x.read.materials.common_catalog||p->recipes!=&x.read.materials.recipes||!x.effects.bound_to(common,jobs)||!same_span(p->span,span,p->budget)||p->scale.definition_id!=x.read.scale.definition_id||p->scale.version!=x.read.scale.version||p->scale.units_per_half_day!=x.read.scale.units_per_half_day)fail("CraftIntervalCandidateMismatch");
 if(!x.read.character_sources)fail("CraftCharacterSourceProviderRequired",{},ErrorKind::unsupported_capability);
 if(!x.read.spatial_sources)fail("CraftSpatialSourceProviderRequired",{},ErrorKind::unsupported_capability);
 if(&x.read.spatial_sources->craft!=&x.read)fail("CraftSpatialSourceContextMismatch");
 const auto&space=*x.read.spatial_sources;if(p->travel_catalog!=&space.travel_catalog||p->movement!=&space.movement||p->carriers!=&space.carriers||p->registrations!=&space.registrations||p->foundation!=&space.foundation||p->actor_catalog!=&space.actor_catalog||p->military_handoffs!=space.military_handoffs||p->family_catalog!=space.family_catalog)fail("CraftIntervalSpatialCandidateMismatch");
 if(&x.read.character_sources->body.common!=&common||&x.read.character_sources->jobs!=&jobs||&x.read.character_sources->recipes!=p->recipes)fail("CraftCharacterSourceContextMismatch");
 if(!same(actual_now(x.read),span.to,p->budget))fail("CraftIntervalActualToRequired");const auto&identity=common.state().identity;if(identity.world_id!=p->identity.world_id||identity.execution_epoch!=p->identity.execution_epoch)fail("CraftIntervalWorldMismatch");
 if(p->consumed)return validate_craft_interval_candidate_consumed(candidate,common,span);
 std::size_t working=0;for(const auto&entry:jobs.jobs.rows())if(entry.second.phase==CraftJobPhase::working){++working;if(std::none_of(p->slices.begin(),p->slices.end(),[&](const auto&slice){return slice->job_id==entry.first;}))fail("CraftIntervalNewWorkingOwner",entry.first);}if(working!=p->slices.size())fail("CraftIntervalOwnerSetChanged");
 // All coverage is re-read at the actual endpoint before any service is credited.
 for(const auto&owned_slice:p->slices){const auto&slice=*owned_slice;const auto*job=jobs.jobs.find(slice.job_id);if(!job||job->phase!=CraftJobPhase::working||job->actual_service_refs.size()!=slice.prior_service_count||slice.applied_service.has_value())fail("CraftIntervalSourceChanged",slice.job_id);action_scope(common,*job);
  take(validate_craft_participant_body_span(*x.read.character_sources,*job,slice.proof));
  take(validate_craft_participant_spatial_span(*x.read.spatial_sources,*job,slice.proof));
  take(x.read.original.validate_adopted_craft_service_interval(slice.proof));
  for(const auto&source:slice.proof.participation_sources)if(!is_craft_participant_body_source(source)&&!is_craft_participant_spatial_source(source))take(x.read.original.validate_work_service_participation_source(slice.proof,source));
 }
 for(auto&owned_slice:p->slices){auto&slice=*owned_slice;auto&job=*jobs.jobs.find(slice.job_id);const auto*gate=std::get_if<CraftWorkGate>(&slice.prepared.before.gate);if(!gate)fail("CraftIntervalGateRequired",slice.job_id);
 auto produced_id=take(common.next_owned_id("craft_service"));
 {
  const auto& id=produced_id.id();
  {
   const PreparedCraftServiceSpan prepared({id,job.id,gate->gate_ordinal,slice.proof});
   take(common.append_fact_borrowed(id,"craft_service_span","crafting"));
   if(!jobs.service_spans.insert(id,prepared.row))fail("DuplicateCraftService",id);
  }
  job.actual_service_refs.push_back(id);
 }
 slice.applied_service.emplace(std::move(produced_id));
 take(read_craft_actual_service(x.read,slice.applied_service->id()));take(apply_shared_work_slice_in_candidate(elapsed,std::move(slice.prepared),p->budget));}
 p->consumed=true;return validate_craft_interval_candidate_consumed(candidate,common,span);
}catch(Failure&e){return {false,{},std::move(e.error)};}catch(const std::bad_alloc&){return {false,{},exhausted()};}catch(const std::length_error&){return {false,{},exhausted()};}}
Status validate_craft_interval_candidate_consumed(const CraftIntervalCandidate&candidate,const CommonStore&common,const MomentSpan&span){try{
 const auto*p=candidate.impl_.get();if(!p||p->common!=&common||!p->jobs||!p->elapsed||!p->consumed||!same_span(p->span,span,p->budget))fail("UnconsumedCraftInterval");const auto&identity=common.state().identity;if(identity.world_id!=p->identity.world_id||identity.execution_epoch!=p->identity.execution_epoch)fail("CraftIntervalWorldMismatch");
 for(const auto&owned_slice:p->slices){const auto&slice=*owned_slice;const auto*job=p->jobs->jobs.find(slice.job_id);const StableId* applied=slice.applied_service?&slice.applied_service->id():nullptr;const auto*source=applied?p->jobs->service_spans.find(*applied):nullptr;const auto*header=applied?common.state().facts.find(*applied):nullptr;if(!job||!source||!applied||applied->empty()||source->id!=*applied||source->job_id!=slice.job_id||!header||header->domain!="crafting"||header->kind!="craft_service_span"||job->actual_service_refs.size()<=slice.prior_service_count||job->actual_service_refs[slice.prior_service_count]!=*applied||std::count(job->actual_service_refs.begin(),job->actual_service_refs.end(),*applied)!=1||!same_proof(source->actual,slice.proof,p->budget))fail("CraftIntervalAppliedSourceMismatch",slice.job_id);
  const SharedWorkElapsedState*work=nullptr;for(const auto&entry:p->elapsed->entries)if(action_elapsed_entry_ref(entry)==job->execution_binding.action_id){if(work)fail("CraftIntervalDuplicateElapsed",slice.job_id);work=std::get_if<SharedWorkElapsedState>(&entry);if(!work)fail("CraftIntervalSharedElapsedRequired",slice.job_id);}const auto*gate=work?std::get_if<CraftWorkGate>(&work->gate):nullptr;if(!gate||!same(gate->last_accounted_at,span.to,p->budget))fail("CraftIntervalElapsedNotConsumed",slice.job_id);
 }
 return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}
}
