#include "task_exact_execution.hpp"
#include "task_original_sources.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/kernel/fixed_service_runtime.hpp"
#include "resident/kernel/fixed_service_history.hpp"
#include "resident/model/procurement_timing.hpp"
#include "hanmo/resident_world_api.hpp"
#include <algorithm>
#include <string_view>
namespace hanmo::resident {namespace {
template<class T>Outcome<T> bad(const char*code,const StableId&id={}){return {false,{},common_failure(code,id).error};}
bool same(const SourceRef&a,const SourceRef&b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
bool identity(const CatalogIdentity&a,const CatalogIdentity&b){return a.content_id==b.content_id&&a.content_version==b.content_version&&a.content_digest==b.content_digest&&a.ruleset_id==b.ruleset_id&&a.schema_sha256==b.schema_sha256&&a.pack_set_digest==b.pack_set_digest&&a.module_versions==b.module_versions&&a.artifact_sha256==b.artifact_sha256;}
SourceRef start_ref(const StableId&id){return {"tasks","task_action_started",id};}SourceRef terminal_ref(const StableId&id){return {"tasks","task_action_finished",id};}
bool same_context(const InteractionContext&a,const InteractionContext&b){return a.place_id==b.place_id&&a.provider_actor_id==b.provider_actor_id&&a.role_id==b.role_id&&a.kind==b.kind&&a.encounter_id==b.encounter_id&&a.input_error==b.input_error;}
bool same_params(const TaskAction&a,const TaskAction&b){return a.offer_id==b.offer_id&&a.task_id==b.task_id&&same_context(a.context,b.context)&&a.reward==b.reward&&a.source_kind==b.source_kind&&a.delivery_quantity==b.delivery_quantity&&a.restitution==b.restitution;}
const TaskExactStartSource* start(const TaskState&s,const StableId&action){const TaskExactStartSource*out=nullptr;if(s.exact_execution)for(const auto&row:s.exact_execution->starts.rows())if(row.second.action_id==action){if(out)return nullptr;out=&row.second;}return out;}
const TaskExactTerminalSource* terminal(const TaskState&s,const StableId&action){const TaskExactTerminalSource*out=nullptr;if(s.exact_execution)for(const auto&row:s.exact_execution->terminals.rows())if(row.second.action_id==action){if(out)return nullptr;out=&row.second;}return out;}
// Original rule identity, not another duration policy. The one temporary
// string is freshly sized and charged before fill, never append-grown.
struct TaskDurationRule {
 CheckpointMemoryCharge copied_fields;
 StableId value;
 static std::size_t size(const TaskExactStartSource&s){
  if(s.action_kind!="accept_offer")return std::string_view("task_handover_ticks").size();
  constexpr auto suffix=std::string_view("/accept_ticks");
  if(s.accepted_action.offer_id.size()>SIZE_MAX-suffix.size())throw std::bad_alloc();
  return s.accepted_action.offer_id.size()+suffix.size();
 }
 explicit TaskDurationRule(const TaskExactStartSource&s)
  :copied_fields([&]{return CheckpointMemoryCharge(0);}()),value(size(s),'\0'){
  if(s.action_kind=="accept_offer"){
   const auto&offer=s.accepted_action.offer_id;constexpr auto suffix=std::string_view("/accept_ticks");
   auto end=std::copy(offer.begin(),offer.end(),value.begin());std::copy(suffix.begin(),suffix.end(),end);
  }else{constexpr auto rule=std::string_view("task_handover_ticks");std::copy(rule.begin(),rule.end(),value.begin());}
 }
};
bool duration_rule_matches(const StableId&rule,const TaskExactStartSource&s){
 if(s.action_kind!="accept_offer")return rule=="task_handover_ticks";
 std::string_view rest(rule),offer(s.accepted_action.offer_id);
 if(rest.size()<offer.size()||rest.substr(0,offer.size())!=offer)return false;
 rest.remove_prefix(offer.size());return rest=="/accept_ticks";
}
bool original_start_ref(const SourceRef&ref,const StableId&id){return ref.domain=="tasks"&&ref.kind=="task_action_started"&&ref.id==id;}
bool original_terminal_ref(const SourceRef&ref,const StableId&id){return ref.domain=="tasks"&&ref.kind=="task_action_finished"&&ref.id==id;}

Status header(const CommonStore&c,std::string_view domain,std::string_view kind,const StableId&id){const auto*h=c.state().facts.find(id);return h&&h->id==id&&h->domain==domain&&h->kind==kind?common_success():common_failure("TaskExactSourceHeaderMismatch",id);}
Status header(const CommonStore&c,const SourceRef&s){return header(c,s.domain,s.kind,s.id);}
Status registration(const ActionTimeRegistration&r,Integer version){return task_exact_execution_version(version)&&exact_task_action_kind(r.action_kind)&&r.owner_domain=="tasks"&&r.profile_id=="task_execution_time2"&&r.profile_version==version&&r.source_schema==1&&r.mode==ActionTimeMode::fixed_duration&&r.remaining_meaning==ActionRemainingMeaning::service_estimate&&!r.shared_gate_kind&&r.retained_wait_phases.empty()?common_success():common_failure("TaskActionTimeRegistrationMismatch",r.action_kind);}
// Same Common append traversal. This local capture owns only the original
// applied-source suffix until the destination's existing owner makes its copy.
class TaskSourceSuffix final:public CommonActualFactPrefixSink {
 CheckpointMemoryCharge copied_fields_;std::size_t first_;
public:
 std::vector<SourceRef> sources;
 TaskSourceSuffix(std::size_t first,std::size_t count):first_(first){(copied_fields_).grow(0);sources.reserve(count);}
 Status actual_prefix_fact(std::size_t position,const FactRecord&h)override{
  if(position<first_)return common_success();
  if(sources.size()==sources.capacity())return common_failure("TaskActualSourceSuffixCountMismatch",h.id,ErrorKind::internal);
  (copied_fields_).grow(0);
  sources.push_back({checkpoint_fresh_string_copy(h.domain),checkpoint_fresh_string_copy(h.kind),checkpoint_fresh_string_copy(h.id)});
  return common_success();
 }
};
Outcome<ActionTimeRegistration> registration_for(const StableId&kind,Integer version){for(auto&r:task_action_time_registrations(version))if(r.action_kind==kind)return {true,std::move(r),{}};return bad<ActionTimeRegistration>("UnsupportedTaskActionTimeKind",kind);}
Status validate_source(const CommonStore&c,const TaskState&s,const TaskCatalog&catalog,const TaskExactStartSource&v,const CatalogIdentity&actual){
 auto h=header(c,"tasks","task_action_started",v.id);if(!h.ok)return h;const auto*t=s.tasks.find(v.accepted_action.task_id);const auto*d=t?resolve_task_definition(s,catalog,*t):nullptr;
 if(v.id.empty()||!exact_task_action_kind(v.action_kind)||!t||!d||t->assignee!=v.actor_id||t->offer_id!=v.accepted_action.offer_id||v.required_half_days<=0||v.required_half_days!=(v.action_kind=="accept_offer"?d->accept_ticks:task_handover_ticks)||!identity(v.catalog_identity,actual))return common_failure("TaskExactAcceptedTermsMismatch",v.id);
 auto valid=validate_terms_revision(v.terms_revision);if(!valid.ok)return valid;
 const StableId*expected=&d->id;if(t->contract_id){const auto*contract=s.contracts.find(*t->contract_id);if(!contract)return common_failure("TaskExactContractMissing",v.id);expected=&contract->frozen_terms.terms_version;}
 const auto*actual_terms=std::get_if<SourceTermsKey>(&v.terms_revision);
 if(!actual_terms||actual_terms->key!=*expected)return common_failure("TaskExactTermsRevisionMismatch",v.id);
 if(std::find(v.participants.begin(),v.participants.end(),v.actor_id)==v.participants.end()||std::find(v.participants.begin(),v.participants.end(),v.accepted_action.context.provider_actor_id)==v.participants.end())return common_failure("TaskExactParticipantsMismatch",v.id);
 for(std::size_t i=0;i<v.participants.size();++i){if(!c.actor(v.participants[i]))return common_failure("TaskExactParticipantMissing",v.id);for(std::size_t j=0;j<i;++j)if(v.participants[i]==v.participants[j])return common_failure("DuplicateTaskExactParticipant",v.id);}
 for(std::size_t i=0;i<v.actual_start_sources.size();++i){const auto&ref=v.actual_start_sources[i];auto checked=validate_task_original_applied_source(c,s,ref,v,false,v.started_at);if(!checked.ok)return checked;for(std::size_t j=0;j<i;++j)if(same(ref,v.actual_start_sources[j]))return common_failure("DuplicateTaskOriginalStartSource",v.id);}
 auto at=compare_world_moments(v.started_at,v.started_at);if(!at.ok)return {false,{},at.error};return common_success();
}
Status validate_terminal(const CommonStore&c,const TaskState&s,const TaskExactStartSource&begun,const TaskExactTerminalSource&v,const TaskExactLifeSources*life=nullptr){
 auto h=header(c,"tasks","task_action_finished",v.id);if(!h.ok)return h;if(v.start_source_id!=begun.id||v.action_id!=begun.action_id||(v.effect.task_id&&*v.effect.task_id!=begun.accepted_action.task_id)||v.task_status_after.empty()||!s.exact_execution||!task_exact_execution_version(s.exact_execution->version))return common_failure("TaskExactTerminalIdentityMismatch",v.id);
 auto order=compare_world_moments(begun.started_at,v.finished_at);if(!order.ok)return {false,{},order.error};if(order.value>0)return common_failure("TaskExactTerminalTimeOrder",v.id);
 if(v.participant_death){
  if(s.exact_execution->version!=2||v.disposition!=FixedTerminalDisposition::interrupted||
     v.effect.success||!v.effect.task_id||*v.effect.task_id!=begun.accepted_action.task_id||
     !v.effect.code||*v.effect.code!="TaskParticipantDied"||v.effect.on_unavailable||v.effect.retry||
     !v.effect.field_order.empty()||!v.actual_result_sources.empty()||!v.settlement_id_after.empty()||
     (begun.action_kind=="accept_offer"?v.task_status_after!="accepting":
       begun.action_kind=="deliver_message"?v.task_status_after!="active":
       (v.task_status_after!="active"&&v.task_status_after!="ready")))
   return common_failure("TaskExactDeathTerminalShapeMismatch",v.id);
  if(!life||&life->common!=&c)return common_failure("TaskExactOriginalLifeSourcesRequired",v.id,ErrorKind::unsupported_capability);
  const auto&death=*v.participant_death;
  if(death.role!=TaskExactDeathRole::assignee&&death.role!=TaskExactDeathRole::interaction_provider)
   return common_failure("TaskExactDeathRoleMismatch",v.id);
  const auto&actor=death.role==TaskExactDeathRole::assignee?begun.actor_id:begun.accepted_action.context.provider_actor_id;
  auto actual=read_verified_death4(c,life->deaths,life->causes,death.death_ref,life->actual_now);if(!actual.ok)return {false,{},actual.error};
  if(actual.value.actor_id!=actor||actual.value.effective_at.kind!=ActorFoundationAt::Kind::exact_moment||
     std::find(begun.participants.begin(),begun.participants.end(),actor)==begun.participants.end())
   return common_failure("TaskExactOriginalDeathParticipantMismatch",v.id);
  auto at=compare_world_moments(actual.value.effective_at.exact_moment,v.finished_at);if(!at.ok)return {false,{},at.error};
  if(at.value)return common_failure("TaskExactOriginalDeathMomentMismatch",v.id);
  const auto*start_header=c.state().facts.find(begun.id);const auto*death_header=c.state().facts.find(death.death_ref.id);const auto*end_header=c.state().facts.find(v.id);
  if(!start_header||!death_header||!end_header)return common_failure("TaskExactDeathCausalHeaderMissing",v.id);
  auto first=read_actual_common_fact_position(c,*start_header);if(!first.ok)return {false,{},first.error};
  auto middle=read_actual_common_fact_position(c,*death_header);if(!middle.ok)return {false,{},middle.error};
  auto last=read_actual_common_fact_position(c,*end_header);if(!last.ok)return {false,{},last.error};
  if(first.value>=middle.value||middle.value>=last.value)return common_failure("TaskExactDeathCausalOrderMismatch",v.id);
  return common_success();
 }
 if(v.disposition!=FixedTerminalDisposition::completed)return common_failure("TaskExactOriginalTerminalCauseRequired",v.id);
 if(!v.effect.success&&!v.effect.code)return common_failure("TaskExactFailureReasonMissing",v.id);
 const auto*task=s.tasks.find(begun.accepted_action.task_id);if(!task)return common_failure("TaskExactTerminalTaskMissing",v.id);
 if(v.effect.success&&begun.action_kind=="accept_offer"){
  if(!task->contract_id&&task->record_kind==TaskRecordKind::legacy_quantity&&!task->exact_quantity){
   // The original start binds this Task ID; only its one actual successful
   // acceptance terminal can supply M. No reconstructed base-record moment.
   const TaskExactTerminalSource*accepted=nullptr;
   for(const auto&row:s.exact_execution->terminals.rows()){
    const auto&candidate=row.second;const auto*original=s.exact_execution->starts.find(candidate.start_source_id);
    if(!original||original->action_kind!="accept_offer"||original->accepted_action.task_id!=task->id||!candidate.effect.success)continue;
    if(accepted)return common_failure("DuplicateTaskExactSuccessfulAcceptance",task->id);
    accepted=&candidate;
   }
   if(accepted!=&v||!v.effect.task_id||*v.effect.task_id!=task->id||v.task_status_after!="active"||
      task->accepted_tick!=v.finished_at.half_day_tick)
    return common_failure("TaskExactOriginalAcceptanceBindingMismatch",v.id);
  }else{
   // The independent contract/exact-quantity owner still requires its actual
   // persisted exact field; this ordinary convergence does not alter it.
   if(!task->accepted_at)return common_failure("TaskExactAcceptanceTimeMissing",v.id);
   auto eq=compare_world_moments(*task->accepted_at,v.finished_at);if(!eq.ok)return {false,{},eq.error};
   if(eq.value)return common_failure("TaskExactAcceptanceTimeMismatch",v.id);
  }
 }
 if(!v.settlement_id_after.empty()&&task->settlement_id!=v.settlement_id_after)return common_failure("TaskExactSettlementReferenceMismatch",v.id);
 for(std::size_t i=0;i<v.actual_result_sources.size();++i){const auto&source=v.actual_result_sources[i];h=validate_task_original_applied_source(c,s,source,begun,true,v.finished_at);if(!h.ok)return h;if(source.id==v.id)return common_failure("TaskExactTerminalSelfResult",v.id);for(std::size_t j=0;j<i;++j)if(same(source,v.actual_result_sources[j]))return common_failure("DuplicateTaskExactResultReference",v.id);}
 // Quantity handover must refer to the actual original transfer/delivery row, not the operation's success flag.
 if(v.effect.success&&begun.accepted_action.delivery_quantity){bool found=false;for(const auto&row:s.transfers.rows())if(row.second.source_action_id==begun.action_id){if(!row.second.completed_at||row.second.quantity!=*begun.accepted_action.delivery_quantity)return common_failure("TaskExactTransferPayloadMismatch",v.id);auto at=compare_world_moments(*row.second.completed_at,v.finished_at);if(!at.ok)return {false,{},at.error};if(at.value)return common_failure("TaskExactTransferTimeMismatch",v.id);bool pinned=false;for(const auto&ref:v.actual_result_sources)if(same(ref,{"tasks","task_transfer",row.first}))pinned=true;if(!pinned)return common_failure("TaskExactTransferCoverageMissing",v.id);found=true;}if(!found)return common_failure("TaskExactTransferMissing",v.id);}
 return common_success();
}
}
Status visit_task_exact_original_source_pins(
 const CommonStore&c,const TaskState&s,const TaskCatalog&catalog,const CatalogIdentity&catalog_identity,
 Integer selected_version,SourcePinSink&sink,const TaskExactLifeSources*life){
 auto checked=validate_task_exact_execution_headers(c,s,selected_version);if(!checked.ok)return checked;
 if(!selected_version)return common_success();
 auto emit=[&](std::size_t slot,const StableId&id,const SourceRefBuildView&source,SourcePinReason reason){
  const auto&declaration=task_exact_source_pin_slots[slot];
  if(id.empty()||source.domain.empty()||source.kind.empty()||source.id.empty())
   return common_failure("TaskExactSourcePinIdentityMismatch",id,ErrorKind::internal);
  const SourcePinBuildView pin{"tasks",declaration.consumer_kind,id,declaration.slot,
   source.domain,source.kind,source.id,reason};
  return sink.contains(pin)?common_success():sink.append(pin);
 };
 for(const auto&row:s.exact_execution->starts.rows()){
  const auto&v=row.second;checked=validate_source(c,s,catalog,v,catalog_identity);if(!checked.ok)return checked;
  const auto reason=terminal(s,v.action_id)?SourcePinReason::dependent_state:SourcePinReason::active_obligation;
  for(const auto&ref:v.actual_start_sources){checked=emit(0,row.first,{ref.domain,ref.kind,ref.id},reason);if(!checked.ok)return checked;}
  if(const auto*terms=std::get_if<SourceFactTerms>(&v.terms_revision)){
   const auto&ref=terms->source;checked=emit(1,row.first,{ref.domain,ref.kind,ref.id},reason);if(!checked.ok)return checked;
  }
 }
 for(const auto&row:s.exact_execution->terminals.rows()){
  const auto&v=row.second;const auto*begun=s.exact_execution->starts.find(v.start_source_id);
  if(!begun)return common_failure("TaskExactStartMissing",v.id);
  checked=validate_terminal(c,s,*begun,v,life);if(!checked.ok)return checked;
  checked=emit(2,row.first,{"tasks","task_action_started",begun->id},SourcePinReason::unique_outcome);if(!checked.ok)return checked;
  for(const auto&ref:v.actual_result_sources){checked=emit(3,row.first,{ref.domain,ref.kind,ref.id},SourcePinReason::unique_outcome);if(!checked.ok)return checked;}
  if(v.participant_death){
   // validate_terminal just proved this exact original Family4 death, actor,
   // role, M and Common causal order. Its cause is not this direct edge.
   const auto&ref=v.participant_death->death_ref;
   checked=emit(4,row.first,{ref.domain,ref.kind,ref.id},SourcePinReason::unique_outcome);if(!checked.ok)return checked;
  }
 }
 return common_success();
}
Status validate_task_exact_original_source_pin(
 const CommonStore&c,const TaskState&s,const TaskCatalog&catalog,const CatalogIdentity&catalog_identity,
 Integer selected_version,const SourcePin&requested,const TaskExactLifeSources*life){
 class Match final:public SourcePinSink {
  const SourcePin&requested_;
 public:
  bool found=false;
  explicit Match(const SourcePin&requested):requested_(requested){}
  bool contains(const SourcePinBuildView&)const noexcept override{return false;}
  Status append(const SourcePinBuildView&p)override{
   const auto&r=requested_;
   if(p.consumer_domain==r.consumer.domain&&p.consumer_kind==r.consumer.kind&&
      p.consumer_id==r.consumer.id&&p.consumer_slot==r.consumer.slot&&p.source_domain==r.source.domain&&
      p.source_kind==r.source.kind&&p.source_id==r.source.id&&p.reason==r.reason)found=true;
   return common_success();
  }
 }match(requested);
 auto checked=visit_task_exact_original_source_pins(c,s,catalog,catalog_identity,selected_version,match,life);
 if(!checked.ok)return checked;
 return match.found?common_success():common_failure("TaskExactSourcePinNotActual",requested.consumer.id);
}
Status validate_task_exact_ordinary_acceptance_record(
 const CommonStore&c,const TaskState&s,const TaskRecord&task,const TaskExactLifeSources*life){
 if(!s.exact_execution||!task_exact_execution_version(s.exact_execution->version)||
    s.tasks.find(task.id)!=&task||task.contract_id||task.record_kind!=TaskRecordKind::legacy_quantity||task.exact_quantity)
  return common_failure("TaskExactOrdinaryRecordOwnerMismatch",task.id);
 const TaskExactStartSource*begun=nullptr;
 for(const auto&row:s.exact_execution->starts.rows()){
  const auto&v=row.second;
  if(v.action_kind!="accept_offer"||v.accepted_action.task_id!=task.id)continue;
  if(begun||row.first!=v.id)return common_failure("TaskExactOrdinaryAcceptanceStartAmbiguous",task.id);
  begun=&v;
 }
 if(!begun||begun->actor_id!=task.assignee||begun->accepted_action.offer_id!=task.offer_id)
  return common_failure("TaskExactOrdinaryAcceptanceStartRequired",task.id);
 const auto*first=c.state().facts.find(begun->id);
 if(!first)return common_failure("TaskExactOriginalStartHeaderMissing",task.id);
 auto schema=read_task_exact_original_fact_schema(c,s,*first,s.exact_execution->version);
 if(!schema.ok)return {false,{},schema.error};
 if(schema.value!=1||first->kind!="task_action_started")return common_failure("TaskExactOriginalStartIdentityMismatch",task.id);
 const auto*ended=terminal(s,begun->action_id);
 const auto*action=c.action(begun->action_id);const auto*params=s.actions.find(begun->action_id);
 if(!ended){
  if(!action||!params||action->kind!="accept_offer"||action->actor_id!=begun->actor_id||
     action->participants!=begun->participants||!same_params(*params,begun->accepted_action)||
     task.status!="accepting"||task.accepted_tick!=0||task.deadline_tick!=0||
     task.objective_completed_tick||task.reported_tick||!task.settlement_id.empty())
   return common_failure("TaskExactOrdinaryAcceptanceNeitherActiveNorTerminal",task.id);
  return common_success();
 }
 const auto*last=c.state().facts.find(ended->id);
 if(!last)return common_failure("TaskExactOriginalTerminalHeaderMissing",task.id);
 schema=read_task_exact_original_fact_schema(c,s,*last,s.exact_execution->version);
 if(!schema.ok)return {false,{},schema.error};
 if(schema.value!=task_exact_terminal_source_schema(s.exact_execution->version)||last->kind!="task_action_finished")
  return common_failure("TaskExactOriginalTerminalIdentityMismatch",task.id);
 auto checked=validate_terminal(c,s,*begun,*ended,life);if(!checked.ok)return checked;
 if(action||params)return common_failure("TaskExactOrdinaryAcceptanceTerminalStillActive",task.id);
 if(ended->effect.success){
  // validate_terminal proves the unique successful accepted Task result and M.
  return task.status!="accepting"?common_success():common_failure("TaskExactOrdinaryAcceptedRecordMismatch",task.id);
 }
 // Original failed acceptance cancels before funding; actual participant
 // death preserves accepting. Neither is promoted to successful acceptance.
 const auto expected=ended->participant_death?"accepting":"cancelled";
 if(ended->task_status_after!=expected||task.status!=expected||task.accepted_tick!=0||task.deadline_tick!=0||
    task.objective_completed_tick||task.reported_tick||!task.settlement_id.empty())
  return common_failure("TaskExactOrdinaryUnacceptedRecordMismatch",task.id);
 return common_success();
}
Status validate_task_claim_acceptance_execution(const CommonStore&c,const TaskState&s,const TaskClaimEstablishedSource&claim,const TaskExactLifeSources*life){
 const auto*begun=start(s,claim.acceptance_action_id);const auto*t=s.tasks.find(claim.task_id);
 if(!begun||!t||begun->action_kind!="accept_offer"||begun->actor_id!=t->assignee||begun->accepted_action.task_id!=claim.task_id||!t->offer_ref||t->offer_ref->kind!=OfferRef::Kind::runtime)return common_failure("TaskClaimOriginalAcceptanceStartRequired",claim.source_id,ErrorKind::internal);
 auto checked=header(c,"tasks","task_action_started",begun->id);if(!checked.ok)return checked;auto at=compare_world_moments(begun->started_at,claim.established_at);if(!at.ok)return {false,{},at.error};if(at.value)return common_failure("TaskClaimOriginalStartMomentMismatch",claim.source_id,ErrorKind::internal);
 const SourceRef ref{"tasks","task_claim_established",claim.source_id};std::size_t count=0;for(const auto&r:begun->actual_start_sources)if(same(r,ref))++count;
 if(count!=1)return common_failure("TaskClaimOriginalStartCoverageMismatch",claim.source_id,ErrorKind::internal);
 checked=validate_task_original_applied_source(c,s,ref,*begun,false,begun->started_at);if(!checked.ok)return checked;
 const auto*claim_header=c.state().facts.find(claim.source_id);
 const auto*start_header=c.state().facts.find(begun->id);
 if(!claim_header||!start_header)return common_failure("TaskClaimOriginalStartHeaderMissing",claim.source_id,ErrorKind::internal);
 auto claim_position=read_actual_common_fact_position(c,*claim_header);if(!claim_position.ok)return {false,{},claim_position.error};
 auto start_position=read_actual_common_fact_position(c,*start_header);if(!start_position.ok)return {false,{},start_position.error};
 if(claim_position.value>=start_position.value)return common_failure("TaskClaimStartCausalOrderMismatch",claim.source_id,ErrorKind::internal);
 if(const auto*ended=terminal(s,claim.acceptance_action_id)){
  checked=validate_terminal(c,s,*begun,*ended,life);if(!checked.ok)return checked;
  const auto*end_header=c.state().facts.find(ended->id);if(!end_header)return common_failure("TaskClaimOriginalTerminalHeaderMissing",claim.source_id,ErrorKind::internal);
  auto end_position=read_actual_common_fact_position(c,*end_header);if(!end_position.ok)return {false,{},end_position.error};
  return start_position.value<end_position.value?common_success():common_failure("TaskClaimTerminalCausalOrderMismatch",claim.source_id,ErrorKind::internal);
 }
 const auto*action=c.action(claim.acceptance_action_id);const auto*params=s.actions.find(claim.acceptance_action_id);
 if(!action||!params||action->actor_id!=begun->actor_id||action->kind!=begun->action_kind||action->participants!=begun->participants||!same_params(*params,begun->accepted_action)||t->accepted_at)return common_failure("TaskClaimAcceptanceNeitherActiveNorTerminal",claim.source_id,ErrorKind::internal);
 return common_success();
}
Status guard_task_exact_release(const CommonStore&c,const TaskState&s,const ActionElapsedStore&elapsed,const StableId&id,const TaskExactLifeSources*life){if(!s.exact_execution)return common_success();const auto*begun=start(s,id);const auto*action=c.action(id);if(!begun)return action&&exact_task_action_kind(action->kind)?common_failure("TaskExactStartMissing",id):common_success();const auto*ended=terminal(s,id);const auto*entry=find_fixed_elapsed(elapsed,id);if(!ended||!entry||!std::holds_alternative<ElapsedTerminal>(entry->timing))return common_failure("TaskExactOwnerTerminalRequiredBeforeRelease",id);auto valid=validate_terminal(c,s,*begun,*ended,life);if(!valid.ok)return valid;bool bound=false;for(const auto&v:elapsed.terminal_bindings)if(v.action_ref==id){if(bound||v.terminal_result_ref!=ended->id)return common_failure("TaskExactTerminalBindingMismatch",id);bound=true;}return bound?common_success():common_failure("TaskExactTerminalBindingMissing",id);}
bool exact_task_action_kind(const StableId&kind)noexcept{return kind=="accept_offer"||kind=="report_offer"||kind=="deliver_message";}
bool task_exact_execution_version(Integer version)noexcept{return version==1||version==2;}
Integer task_exact_terminal_source_schema(Integer version)noexcept{return task_exact_execution_version(version)?version:0;}
Outcome<TaskExactExecutionState> make_initial_task_exact_execution(Integer version){
 if(!task_exact_execution_version(version))return bad<TaskExactExecutionState>("UnsupportedTaskExactExecutionSchema");
 TaskExactExecutionState initial;initial.version=version;return {true,std::move(initial),{}};
}
bool owns_task_exact_original_fact_kind(const StableId&kind)noexcept{return kind=="task_action_started"||kind=="task_action_finished";}
Outcome<Integer> read_task_exact_original_fact_schema(const CommonStore&c,const TaskState&s,const FactRecord&h,Integer selected){
 if(!owns_task_exact_original_fact_kind(h.kind))return {true,0,{}};
 if(!task_exact_execution_version(selected)||!s.exact_execution||s.exact_execution->version!=selected||
    h.id.empty()||h.domain!="tasks"||c.state().facts.find(h.id)!=&h)
  return bad<Integer>("TaskExactOriginalPayloadProfileMismatch",h.id);
 auto actual_position=read_actual_common_fact_position(c,h);if(!actual_position.ok)return {false,0,actual_position.error};
 if(h.kind=="task_action_started"){
  const auto*v=s.exact_execution->starts.find(h.id);
  if(!v||v->id!=h.id||v->action_id.empty()||!exact_task_action_kind(v->action_kind)||start(s,v->action_id)!=v)
   return bad<Integer>("TaskExactOriginalStartIdentityMismatch",h.id);
  return {true,1,{}};
 }
 const auto*v=s.exact_execution->terminals.find(h.id);const auto*begun=v?start(s,v->action_id):nullptr;
 if(!v||v->id!=h.id||!begun||v->start_source_id!=begun->id||terminal(s,v->action_id)!=v)
  return bad<Integer>("TaskExactOriginalTerminalIdentityMismatch",h.id);
 if((selected==1&&v->participant_death)||
    (v->participant_death?v->disposition!=FixedTerminalDisposition::interrupted:
     v->disposition!=FixedTerminalDisposition::completed))
  return bad<Integer>("TaskExactOriginalTerminalSchemaMismatch",h.id);
 const auto*first=c.state().facts.find(begun->id);
 if(!first||first->id!=begun->id||first->domain!="tasks"||first->kind!="task_action_started")
  return bad<Integer>("TaskExactOriginalStartHeaderMissing",begun->id);
 auto before=read_actual_common_fact_position(c,*first);if(!before.ok)return {false,0,before.error};
 if(before.value>=actual_position.value)return bad<Integer>("TaskExactOriginalTerminalOrderMismatch",h.id);
 return {true,task_exact_terminal_source_schema(selected),{}};
}
Status validate_task_exact_execution_headers(const CommonStore&c,const TaskState&s,Integer selected){
 if((selected&&!task_exact_execution_version(selected))||
    bool(selected)!=bool(s.exact_execution)||(s.exact_execution&&s.exact_execution->version!=selected))
  return common_failure("TaskExactExecutionSelectionMismatch");
 std::size_t starts=0,terminals=0;
 for(const auto&row:c.state().facts.rows()){
  const auto&h=row.second;if(!owns_task_exact_original_fact_kind(h.kind))continue;
  if(row.first!=h.id)return common_failure("TaskExactOriginalHeaderKeyMismatch",row.first);
  auto schema=read_task_exact_original_fact_schema(c,s,h,selected);if(!schema.ok)return {false,{},schema.error};
  if(h.kind=="task_action_started")++starts;else ++terminals;
 }
 if(!s.exact_execution)return common_success();
 if(starts!=s.exact_execution->starts.size()||terminals!=s.exact_execution->terminals.size())
  return common_failure("TaskExactOriginalPayloadCoverageMismatch");
 for(const auto&row:c.state().actions.rows())if(exact_task_action_kind(row.second.kind)){
  const auto*begun=start(s,row.first);const auto*params=s.actions.find(row.first);
  if(!begun||!params||row.second.id!=row.first||row.second.kind!=begun->action_kind||
     row.second.actor_id!=begun->actor_id||row.second.participants!=begun->participants||
     !same_params(*params,begun->accepted_action))
   return common_failure("TaskExactOriginalActionIdentityMismatch",row.first);
 }
 return common_success();
}
std::vector<ActionTimeRegistration> task_action_time_registrations(Integer version){std::vector<ActionTimeRegistration>out;for(const char*kind:{"accept_offer","report_offer","deliver_message"}){ActionTimeRegistration r;r.action_kind=kind;r.owner_domain="tasks";r.profile_id="task_execution_time2";r.profile_version=version;r.source_schema=1;out.push_back(std::move(r));}return out;}
// Capture-time source identity only. This never calls Knowledge, the final
// Common result seal, or the whole Task/financial validator.
Outcome<WorldMoment> read_task_exact_source_moment(const CommonStore&c,const TaskState&s,const SourceRef&ref,Integer schema,const TaskExactLifeSources*life){
 if(!s.exact_execution||!task_exact_execution_version(s.exact_execution->version)||ref.domain!="tasks"||schema!=(ref.kind=="task_action_started"?1:task_exact_terminal_source_schema(s.exact_execution->version)))return bad<WorldMoment>("TaskExactSourceProfileMismatch",ref.id);
 auto h=header(c,ref);if(!h.ok)return {false,{},h.error};
 if(ref.kind=="task_action_started"){
  const auto*v=s.exact_execution->starts.find(ref.id);const auto*t=v?s.tasks.find(v->accepted_action.task_id):nullptr;
  if(!v||v->id!=ref.id||v->action_id.empty()||start(s,v->action_id)!=v||!exact_task_action_kind(v->action_kind)||!t||t->id!=v->accepted_action.task_id||t->assignee!=v->actor_id||t->offer_id!=v->accepted_action.offer_id||v->required_half_days<=0||!c.actor(v->actor_id)||std::find(v->participants.begin(),v->participants.end(),v->actor_id)==v->participants.end())return bad<WorldMoment>("TaskExactStartIdentityMismatch",ref.id);
  for(std::size_t i=0;i<v->participants.size();++i){if(!c.actor(v->participants[i]))return bad<WorldMoment>("TaskExactParticipantMissing",ref.id);for(std::size_t j=0;j<i;++j)if(v->participants[i]==v->participants[j])return bad<WorldMoment>("DuplicateTaskExactParticipant",ref.id);}
  auto moment=compare_world_moments(v->started_at,v->started_at);if(!moment.ok)return {false,{},moment.error};return {true,v->started_at,{}};
 }
 if(ref.kind=="task_action_finished"){
  const auto*v=s.exact_execution->terminals.find(ref.id);const auto*begun=v?s.exact_execution->starts.find(v->start_source_id):nullptr;
  if(!v||v->id!=ref.id||!begun||begun->id!=v->start_source_id||v->action_id!=begun->action_id||terminal(s,v->action_id)!=v||v->task_status_after.empty()||(v->effect.task_id&&*v->effect.task_id!=begun->accepted_action.task_id)||(!v->effect.success&&!v->effect.code))return bad<WorldMoment>("TaskExactTerminalIdentityMismatch",ref.id);
  if(v->participant_death){auto valid=validate_terminal(c,s,*begun,*v,life);if(!valid.ok)return {false,{},valid.error};}
  else if(v->disposition!=FixedTerminalDisposition::completed)return bad<WorldMoment>("TaskExactOriginalTerminalCauseRequired",ref.id);
  auto begun_at=read_task_exact_source_moment(c,s,start_ref(begun->id),1,life);if(!begun_at.ok)return begun_at;
  auto moment=compare_world_moments(v->finished_at,v->finished_at);if(!moment.ok)return {false,{},moment.error};auto time=compare_world_moments(begun_at.value,v->finished_at);if(!time.ok)return {false,{},time.error};
  const auto*first=c.state().facts.find(begun->id);const auto*last=c.state().facts.find(v->id);
  if(!first||!last)return bad<WorldMoment>("TaskExactTerminalCausalOrderMismatch",ref.id);
  auto first_position=read_actual_common_fact_position(c,*first);if(!first_position.ok)return {false,{},first_position.error};
  auto last_position=read_actual_common_fact_position(c,*last);if(!last_position.ok)return {false,{},last_position.error};
  if((v->participant_death?time.value>0:time.value>=0)||first_position.value>=last_position.value)return bad<WorldMoment>("TaskExactTerminalCausalOrderMismatch",ref.id);
  return {true,v->finished_at,{}};
 }
 return bad<WorldMoment>("UnsupportedTaskExactSource",ref.id);
}
Outcome<OriginalSuccessfulTaskReport> read_original_successful_task_report(
 const CommonStore&c,const TaskState&s,const TaskCatalog&catalog,const CatalogIdentity&identity,
 const SourceRef&ref,Integer schema,const WorldMoment&now){
 if(!s.exact_execution||schema!=task_exact_terminal_source_schema(s.exact_execution->version)||ref.domain!="tasks"||ref.kind!="task_action_finished")
  return {false,{},common_failure("OriginalTaskReportSourceRequired",ref.id,ErrorKind::unsupported_capability).error};
 auto at=read_task_exact_source_moment(c,s,ref,schema);if(!at.ok)return {false,{},at.error};
 const auto*ended=s.exact_execution->terminals.find(ref.id);
 const auto*begun=ended?s.exact_execution->starts.find(ended->start_source_id):nullptr;
 const auto*task=begun?s.tasks.find(begun->accepted_action.task_id):nullptr;
 if(!begun||!ended||!task||begun->action_kind!="report_offer"||task->contract_id||
    task->record_kind!=TaskRecordKind::legacy_quantity||task->exact_quantity||
    !ended->effect.success||!ended->effect.task_id||*ended->effect.task_id!=task->id||
    ended->task_status_after!="success"||ended->settlement_id_after.empty()||
    !task->reported_tick||*task->reported_tick!=ended->finished_at.half_day_tick||
    begun->accepted_action.context.provider_actor_id.empty()||
    begun->accepted_action.context.provider_actor_id==begun->actor_id)
  return bad<OriginalSuccessfulTaskReport>("OriginalSuccessfulTaskReportRequired",ref.id);
 auto valid=validate_source(c,s,catalog,*begun,identity);if(!valid.ok)return {false,{},valid.error};
 valid=validate_terminal(c,s,*begun,*ended);if(!valid.ok)return {false,{},valid.error};
 auto time=compare_world_moments(at.value,now);if(!time.ok)return {false,{},time.error};
 if(time.value>0)return bad<OriginalSuccessfulTaskReport>("OriginalTaskReportAfterCurrentMoment",ref.id);
 // The original completion provider is frozen by the actual Task start. A
 // later office/person change never substitutes today's provider here.
 return {true,{begun,ended,task},{}};
}
Status TaskActionTimeSourceRead::enumerate_action_time_owners(const ActionTimeRegistration&r,ActionTimeOwnerIdSink&sink)const{
 auto valid=registration(r,tasks_.exact_execution?tasks_.exact_execution->version:0);if(!valid.ok)return valid;
 if(!tasks_.exact_execution)return common_failure("TaskExactExecutionRequired");
 // Original accepted execution sources establish coverage independently of ActionElapsed.
 for(const auto&row:tasks_.exact_execution->starts.rows()){
  const auto&v=row.second;if(v.action_kind!=r.action_kind)continue;
  if(row.first!=v.id||start(tasks_,v.action_id)!=&v)return common_failure("DuplicateTaskExactActionStart",row.first);
  auto checked=validate_source(common_,tasks_,catalog_,v,identity_);if(!checked.ok)return checked;
  checked=sink.append(v.action_id);if(!checked.ok)return checked;
 }
 for(const auto&row:tasks_.exact_execution->terminals.rows()){
  const auto&v=row.second;const auto*begun=start(tasks_,v.action_id);
  if(!begun)return common_failure("OrphanTaskExactTerminal",row.first);
  if(begun->action_kind!=r.action_kind)continue;
  if(row.first!=v.id||terminal(tasks_,v.action_id)!=&v)return common_failure("DuplicateTaskExactTerminal",row.first);
  auto checked=validate_terminal(common_,tasks_,*begun,v,life_);if(!checked.ok)return checked;
 }
 return common_success();
}
Outcome<ActionTimeOwnerProof> TaskActionTimeSourceRead::read_action_time_owner(const ActionTimeRegistration&r,const StableId&action)const{
 auto valid=registration(r,tasks_.exact_execution?tasks_.exact_execution->version:0);if(!valid.ok)return {false,{},valid.error};
 const auto*begun=start(tasks_,action);if(!begun||begun->action_kind!=r.action_kind)return bad<ActionTimeOwnerProof>("TaskExactStartMissing",action);
 valid=validate_source(common_,tasks_,catalog_,*begun,identity_);if(!valid.ok)return {false,{},valid.error};
 ActionTimeOwnerPhase phase=ActionTimeOwnerPhase::working;
 std::optional<ElapsedTimeSourceBuildView> ended_source;std::optional<std::string_view> result;
 if(const auto*ended=terminal(tasks_,action)){
  valid=validate_terminal(common_,tasks_,*begun,*ended,life_);if(!valid.ok)return {false,{},valid.error};
  phase=ActionTimeOwnerPhase::terminal;
  ended_source=ElapsedTimeSourceBuildView{MomentOwnerSourceBuildView{{"tasks","task_action_finished",ended->id},task_exact_terminal_source_schema(tasks_.exact_execution->version)}};
  result=ended->id;
 }else{
  const auto*actual=common_.action(action);const auto*params=tasks_.actions.find(action);
  if(!actual||!params||!same_params(*params,begun->accepted_action)||actual->actor_id!=begun->actor_id||
     actual->participants!=begun->participants||actual->kind!=begun->action_kind)
   return bad<ActionTimeOwnerProof>("TaskExactActiveSourceMismatch",action);
 }
 CheckpointVector<std::string_view> participants;participants.reserve(begun->participants.size());
 for(const auto&id:begun->participants)participants.push_back(id);
 const CheckpointVector<std::string_view> no_gates;
 const SourceRefBuildView original{"tasks","task_action_started",begun->id};
 return {true,ActionTimeOwnerProof{ActionTimeOwnerProofBuildView{action,begun->action_kind,begun->actor_id,
  {participants},original,1,MomentOwnerSourceBuildView{original,1},phase,std::nullopt,
  ended_source,result,{no_gates},std::nullopt}},{}};
}
Outcome<WorldMoment> TaskActionTimeSourceRead::read_action_time_source(const ElapsedTimeSource&ref)const{const auto*moment=std::get_if<MomentOwnerSource>(&ref);if(!moment)return bad<WorldMoment>("TaskFixedSourceRequiresMoment");return read_task_exact_source_moment(common_,tasks_,moment->source_ref,moment->source_schema,life_);}
Status TaskActionTimeSourceRead::validate_fixed_action_sources(const ActionTimeRegistration&r,const ActionTimeOwnerProof&proof,const ActionElapsedState&entry,const std::vector<FixedServiceTransition>&rows)const{
 auto actual=read_action_time_owner(r,proof.action_ref);if(!actual.ok)return {false,{},actual.error};const auto*begun=start(tasks_,proof.action_ref);const auto*ended=terminal(tasks_,proof.action_ref);if(!begun||!original_start_ref(proof.original_execution_source,begun->id)||entry.execution_source_ref!=begun->id||entry.owner_cost_state_ref!=begun->id||!duration_rule_matches(entry.duration_definition_ref,*begun)||entry.owner_work_remainder!=0)return common_failure("TaskFixedEntrySourceMismatch",proof.action_ref);
 auto required=fixed_duration_work_units(begun->required_half_days,scale_.units_per_half_day,budget_);if(!required.ok)return {false,{},required.error};auto same_units=compare_fractions(required.value.value,entry.required_units.value,budget_);if(!same_units.ok)return {false,{},same_units.error};if(same_units.value)return common_failure("TaskFixedDurationMismatch",proof.action_ref);
 const FixedServiceTransition*first=nullptr;const FixedServiceTransition*last=nullptr;std::size_t count=0;
 for(const auto&row:rows)if(row.action_ref==proof.action_ref){++count;if(row.kind==FixedTransitionKind::started&&!first)first=&row;else if(row.kind==FixedTransitionKind::terminal&&!last)last=&row;else return common_failure("TaskFixedUnexpectedTransition",proof.action_ref);if(row.execution_source_schema!=1||!original_start_ref(row.execution_source_ref,begun->id))return common_failure("TaskFixedTransitionOwnerMismatch",proof.action_ref);}
 if(!first||count!=(ended?2u:1u)||first->input_basis_refs.size()!=2)return common_failure("TaskFixedHistoryCoverageMismatch",proof.action_ref);
 const auto*execution_input=std::get_if<FixedAppliedInput>(&first->input_basis_refs[0]);
 const auto*rule_input=std::get_if<FixedLockedRuleInput>(&first->input_basis_refs[1]);
 if(!execution_input||execution_input->source_schema!=1||!original_start_ref(execution_input->source_ref,begun->id)||
    !rule_input||!identity(rule_input->catalog_identity,begun->catalog_identity)||!duration_rule_matches(rule_input->rule_ref,*begun))
  return common_failure("TaskFixedInputMismatch",proof.action_ref);
 const auto*cause=std::get_if<FixedOwnerAdopted>(&first->cause);if(!cause||!original_start_ref(cause->owner_source_ref,begun->id)||cause->owner_source_schema!=1)return common_failure("TaskFixedStartCauseMismatch",proof.action_ref);auto at=compare_world_moments(first->occurred_at,begun->started_at,budget_);if(!at.ok)return {false,{},at.error};if(at.value)return common_failure("TaskFixedStartTimeMismatch",proof.action_ref);
 if(ended){const auto*end=last?std::get_if<FixedOwnerTerminated>(&last->cause):nullptr;if(!end||!original_terminal_ref(end->owner_result_ref,ended->id)||end->owner_result_schema!=task_exact_terminal_source_schema(tasks_.exact_execution->version)||last->terminal_disposition!=std::optional<FixedTerminalDisposition>{ended->disposition})return common_failure("TaskFixedTerminalCauseMismatch",proof.action_ref);at=compare_world_moments(last->occurred_at,ended->finished_at,budget_);if(!at.ok)return {false,{},at.error};if(at.value)return common_failure("TaskFixedTerminalTimeMismatch",proof.action_ref);}return common_success();
}
Status TaskActionTimeSourceRead::validate_shared_action_sources(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const SharedWorkElapsedState&)const{return common_failure("TaskActionIsNotSharedWork");}
Outcome<RunStop> TaskActionTimeSourceRead::read_action_continuation_stop(const ActionTimeRegistration&r,const ActionTimeOwnerProof&p,const WorldMoment&)const{auto proof=read_action_time_owner(r,p.action_ref);if(!proof.ok)return {false,{},proof.error};if(proof.value.phase!=ActionTimeOwnerPhase::terminal)return {true,RunStop::awaiting_permit,{}};const auto*result=terminal(tasks_,p.action_ref);return {true,result->effect.success?RunStop::completed:RunStop::failed,{}};}
Outcome<OriginalActionRemaining> TaskActionTimeSourceRead::read_original_action_remaining(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const WorldMoment&)const{return bad<OriginalActionRemaining>("TaskRemainingOwnedByFixedElapsed");}
Outcome<StableId> TaskActionTimeSourceRead::read_gate_result_id(const SharedWorkGateResult&)const{return bad<StableId>("TaskActionHasNoSharedGate");}
Outcome<SharedWorkGateResult> TaskActionTimeSourceRead::read_original_gate_result(const StableId&id)const{return bad<SharedWorkGateResult>("TaskActionHasNoSharedGate",id);}
Status TaskActionTimeSourceRead::validate_original_gate_result(const SharedWorkGateResult&)const{return common_failure("TaskActionHasNoSharedGate");}
Status record_exact_task_started_in_candidate(CommonStore&c,TaskState&s,const TaskCatalog&catalog,const ActionCoreView&action,const TaskQuote&quote,TaskExactWrite&write){
 if(!s.exact_execution||!exact_task_action_kind(action.kind)||start(s,action.id)||!c.action(action.id)||!write.first_new_fact||*write.first_new_fact>c.state().facts.size())return common_failure("TaskExactActualStartRequired",action.id);const auto*params=s.actions.find(action.id);const auto*task=params?s.tasks.find(params->task_id):nullptr;const auto*definition=task?resolve_task_definition(s,catalog,*task):nullptr;if(!params||!task||!definition||quote.duration!=(action.kind=="accept_offer"?definition->accept_ticks:task_handover_ticks)||action.remaining_ticks!=quote.duration)return common_failure("TaskExactStartTermsMismatch",action.id);
 auto prefix=read_current_common_fact_prefix(c);if(!prefix.ok)return {false,{},prefix.error};if(*write.first_new_fact>static_cast<std::uint64_t>(prefix.value.common_fact_count))return common_failure("TaskActualSourceSuffixCountMismatch",action.id);TaskSourceSuffix capture(*write.first_new_fact,static_cast<std::size_t>(prefix.value.common_fact_count)-*write.first_new_fact);auto captured=visit_actual_common_fact_prefix(c,prefix.value,capture);if(!captured.ok)return captured;auto id=c.next_owned_id("task_action_started");if(!id.ok)return {false,{},id.error};
 const auto*contract=task->contract_id?s.contracts.find(*task->contract_id):nullptr;
 if(task->contract_id&&!contract)return common_failure("TaskExactContractMissing",task->id);
 const auto&terms_id=contract?contract->frozen_terms.terms_version:definition->id;
 auto terms_memory=CheckpointMemoryCharge(0);
 TermsRevisionRef terms=SourceTermsKey{checkpoint_fresh_string_copy(terms_id)};
 TaskExactStartSource source(TaskExactStartBuildView{id.value.id(),action.id,action.kind,action.actor_id,
  action.participants,*params,quote.duration,write.at,write.catalog_identity,terms,capture.sources});auto fact=c.append_fact_borrowed(source.id,"task_action_started","tasks");if(!fact.ok)return fact;if(!s.exact_execution->starts.insert(source.id,source))return common_failure("DuplicateTaskExactStart",source.id);
 TaskDurationRule rule(source);FixedServiceAdoptionInputs original_inputs({"tasks","task_action_started",source.id},1,source.catalog_identity,rule.value);
 auto entry=begin_fixed_elapsed(action.id,source.id,rule.value,source.id,write.at,source.required_half_days,write.scale);if(!entry.ok)return {false,{},entry.error};
 auto tid=c.next_owned_id("fixed_service_transition");if(!tid.ok)return {false,{},tid.error};
 const std::vector<FixedServiceInputRef> no_inputs;
 FixedServiceTransitionInput owned{FixedServiceAdoptionTransitionBuildView{tid.value.id(),1,action.id,0,
  std::nullopt,FixedTransitionKind::started,write.at,entry.value.credited_units,
  {"tasks","task_action_started",source.id},1,{{"tasks","task_action_started",source.id},1},
  original_inputs.values(),no_inputs,std::nullopt}};
 const auto&transition=owned.row();
 TaskActionTimeSourceRead read(c,s,catalog,write.scale,write.catalog_identity,{},write.life);auto reg=registration_for(action.kind,s.exact_execution->version);if(!reg.ok)return {false,{},reg.error};auto prepared=prepare_fixed_service_adoption(action,entry.value,transition,reg.value,write.scale,read);if(!prepared.ok)return {false,{},prepared.error};return apply_fixed_service_change_in_candidate(write.elapsed,std::move(prepared.value),reg.value,write.scale,read);
}
Status record_exact_task_terminal_in_candidate(CommonStore&c,TaskState&s,const TaskCatalog&catalog,const ActionCoreView&action,const TaskEffect&effect,std::size_t first_new_fact,TaskExactWrite&write){
 const auto*begun=start(s,action.id);if(!begun||terminal(s,action.id)||first_new_fact>c.state().facts.size())return common_failure("TaskExactStartedSourceRequired",action.id);const auto*task=s.tasks.find(begun->accepted_action.task_id);const auto*elapsed=find_fixed_elapsed(write.elapsed,action.id);if(!task||!elapsed||!std::holds_alternative<ElapsedRunning>(elapsed->timing))return common_failure("TaskExactRunningSourceRequired",action.id);
 auto due=compare_world_moments(std::get<ElapsedRunning>(elapsed->timing).completion_due_at,write.at);if(!due.ok)return {false,{},due.error};if(due.value)return common_failure("TaskExactCompletionGateMismatch",action.id);
 auto prefix=read_current_common_fact_prefix(c);if(!prefix.ok)return {false,{},prefix.error};
 if(first_new_fact>static_cast<std::uint64_t>(prefix.value.common_fact_count))return common_failure("TaskActualSourceSuffixCountMismatch",action.id);
 TaskSourceSuffix capture(first_new_fact,static_cast<std::size_t>(prefix.value.common_fact_count)-first_new_fact);
 auto captured=visit_actual_common_fact_prefix(c,prefix.value,capture);if(!captured.ok)return captured;
 auto id=c.next_owned_id("task_action_finished");if(!id.ok)return {false,{},id.error};
 TaskExactTerminalSource source(TaskExactTerminalBuildView{id.value.id(),begun->id,action.id,write.at,effect,
  task->status,task->settlement_id,capture.sources,FixedTerminalDisposition::completed,std::nullopt});
 auto fact=c.append_fact_borrowed(source.id,"task_action_finished","tasks");if(!fact.ok)return fact;
 if(!s.exact_execution->terminals.insert(id.value.id(),std::move(source)))return common_failure("DuplicateTaskExactTerminal",id.value.id());
 const FixedServiceTransition*previous=nullptr;for(const auto&row:write.elapsed.fixed_service_transitions)if(row.action_ref==action.id)previous=&row;if(!previous||previous->kind!=FixedTransitionKind::started)return common_failure("TaskExactTransitionStartMissing",action.id);
 auto tid=c.next_owned_id("fixed_service_transition");if(!tid.ok)return {false,{},tid.error};
 const std::vector<FixedServiceInputRef> no_inputs;
 FixedServiceTransitionInput owned{FixedServiceTerminalTransitionBuildView{tid.value.id(),1,action.id,1,
  std::string_view(previous->transition_id),FixedTransitionKind::terminal,write.at,elapsed->credited_units,
  {"tasks","task_action_started",begun->id},1,
  {{"tasks","task_action_finished",id.value.id()},task_exact_terminal_source_schema(s.exact_execution->version)},
  no_inputs,previous->input_basis_refs,FixedTerminalDisposition::completed}};
 const auto&transition=owned.row();
 TaskActionTimeSourceRead read(c,s,catalog,write.scale,write.catalog_identity,{},write.life);auto reg=registration_for(action.kind,s.exact_execution->version);if(!reg.ok)return {false,{},reg.error};auto prepared=prepare_fixed_service_change(write.elapsed,action.id,transition,{},reg.value,write.scale,read);if(!prepared.ok)return {false,{},prepared.error};return apply_fixed_service_change_in_candidate(write.elapsed,std::move(prepared.value),reg.value,write.scale,read);
}
Outcome<const TaskExactTerminalSource*> read_original_task_death_terminal(
 const CommonStore&c,const TaskState&s,const TaskCatalog&catalog,const CatalogIdentity&catalog_identity,
 const StableId&action,const DeathRef&death,const TaskExactLifeSources&life){
 const auto*begun=start(s,action);const auto*ended=terminal(s,action);
 if(!s.exact_execution||s.exact_execution->version!=2||!begun||!ended||!ended->participant_death||
    ended->participant_death->death_ref.domain!=death.domain||
    ended->participant_death->death_ref.kind!=death.kind||ended->participant_death->death_ref.id!=death.id)
  return bad<const TaskExactTerminalSource*>("TaskExactOriginalDeathTerminalRequired",action);
 auto checked=validate_source(c,s,catalog,*begun,catalog_identity);if(!checked.ok)return {false,{},checked.error};
 checked=validate_terminal(c,s,*begun,*ended,&life);if(!checked.ok)return {false,{},checked.error};
 return {true,ended,{}};
}
Outcome<const TaskExactTerminalSource*> stop_exact_task_for_death_in_candidate(
 CommonStore&c,TaskState&s,const TaskCatalog&catalog,const StableId&action_id,const DeathRef&death,
 TaskExactWrite&write,const TaskExactLifeSources&life){
 if(!s.exact_execution||s.exact_execution->version!=2||&life.common!=&c)
  return bad<const TaskExactTerminalSource*>("TaskExactDeathProfileRequired",action_id);
 if(terminal(s,action_id))return read_original_task_death_terminal(c,s,catalog,write.catalog_identity,action_id,death,life);
 const auto*begun=start(s,action_id);const auto*action=c.action(action_id);const auto*params=s.actions.find(action_id);
 const auto*task=begun?s.tasks.find(begun->accepted_action.task_id):nullptr;
 const auto*elapsed=find_fixed_elapsed(write.elapsed,action_id);
 if(!begun||!action||!params||!task||!elapsed||!std::holds_alternative<ElapsedRunning>(elapsed->timing)||
    action->actor_id!=begun->actor_id||action->kind!=begun->action_kind||action->participants!=begun->participants||
    !same_params(*params,begun->accepted_action)||!task->settlement_id.empty())
  return bad<const TaskExactTerminalSource*>("TaskExactOriginalRunningActionRequired",action_id);
 auto checked=validate_source(c,s,catalog,*begun,write.catalog_identity);if(!checked.ok)return {false,{},checked.error};
 auto actual=read_verified_death4(c,life.deaths,life.causes,death,life.actual_now);if(!actual.ok)return {false,{},actual.error};
 if(actual.value.effective_at.kind!=ActorFoundationAt::Kind::exact_moment)
  return bad<const TaskExactTerminalSource*>("TaskExactDeathMomentRequired",action_id);
 auto at=compare_world_moments(actual.value.effective_at.exact_moment,write.at);if(!at.ok)return {false,{},at.error};
 auto now=compare_world_moments(life.actual_now,write.at);if(!now.ok)return {false,{},now.error};
 if(at.value||now.value)return bad<const TaskExactTerminalSource*>("TaskExactDeathCurrentMomentMismatch",action_id);
 TaskExactDeathRole role;
 if(actual.value.actor_id==begun->actor_id)role=TaskExactDeathRole::assignee;
 else if(actual.value.actor_id==begun->accepted_action.context.provider_actor_id)role=TaskExactDeathRole::interaction_provider;
 else return bad<const TaskExactTerminalSource*>("TaskExactDeathParticipantMismatch",action_id);
 const auto*start_header=c.state().facts.find(begun->id);const auto*death_header=c.state().facts.find(death.id);
 if(!start_header||!death_header)return bad<const TaskExactTerminalSource*>("TaskExactDeathCausalHeaderMissing",action_id);
 auto first=read_actual_common_fact_position(c,*start_header);if(!first.ok)return {false,{},first.error};
 auto died=read_actual_common_fact_position(c,*death_header);if(!died.ok)return {false,{},died.error};
 if(first.value>=died.value)return bad<const TaskExactTerminalSource*>("TaskExactDeathPrecedesAction",action_id);
 const FixedServiceTransition*previous=nullptr;
 for(const auto&row:write.elapsed.fixed_service_transitions)if(row.action_ref==action_id){
  if(previous)return bad<const TaskExactTerminalSource*>("TaskExactOriginalStartTransitionRequired",action_id);previous=&row;
 }
 if(!previous||previous->kind!=FixedTransitionKind::started||previous->transition_ordinal!=0)
  return bad<const TaskExactTerminalSource*>("TaskExactOriginalStartTransitionRequired",action_id);
 // These temporary ordinary effect fields are charged before construction and
 // remain owned until the new first-member terminal lease makes its fresh copy.

 auto temporary_memory=CheckpointMemoryCharge(0);TaskEffect effect;effect.success=false;
 effect.task_id=checkpoint_fresh_string_copy(begun->accepted_action.task_id);effect.code="TaskParticipantDied";
 const std::vector<SourceRef> no_results;
 auto id=c.next_owned_id("task_action_finished");if(!id.ok)return {false,{},id.error};
 TaskExactTerminalSource source(TaskExactTerminalBuildView{id.value.id(),begun->id,action_id,write.at,effect,
  task->status,task->settlement_id,no_results,FixedTerminalDisposition::interrupted,
  TaskExactParticipantDeathBuildView{death,role}});
 checked=c.append_fact_borrowed(source.id,"task_action_finished","tasks");if(!checked.ok)return {false,{},checked.error};
 if(!s.exact_execution->terminals.insert(id.value.id(),std::move(source)))return bad<const TaskExactTerminalSource*>("DuplicateTaskExactTerminal",action_id);
 const auto*ended=terminal(s,action_id);if(!ended)return bad<const TaskExactTerminalSource*>("TaskExactOriginalTerminalMissing",action_id);
 auto settled=compare_world_moments(elapsed->last_settled_at,write.at);if(!settled.ok)return {false,{},settled.error};
 if(settled.value>0)return bad<const TaskExactTerminalSource*>("TaskExactDeathBeforeSettledWork",action_id);
 std::optional<FixedElapsedSlice> slice;
 if(settled.value<0){auto actual=prepare_fixed_elapsed_slice(*elapsed,{elapsed->last_settled_at,write.at},write.scale);if(!actual.ok)return {false,{},actual.error};slice.emplace(std::move(actual.value));}
 const auto&credited=slice?slice->credited_units:elapsed->credited_units;
 auto transition_id=c.next_owned_id("fixed_service_transition");if(!transition_id.ok)return {false,{},transition_id.error};
 const std::vector<FixedServiceInputRef> no_inputs;
 FixedServiceTransitionInput owned{FixedServiceTerminalTransitionBuildView{transition_id.value.id(),1,action_id,1,
  std::string_view(previous->transition_id),FixedTransitionKind::terminal,write.at,credited,
  {"tasks","task_action_started",begun->id},1,{{"tasks","task_action_finished",ended->id},2},
  no_inputs,previous->input_basis_refs,FixedTerminalDisposition::interrupted}};
 const auto&transition=owned.row();
 TaskActionTimeSourceRead read(c,s,catalog,write.scale,write.catalog_identity,{},&life);
 auto reg=registration_for(begun->action_kind,2);if(!reg.ok)return {false,{},reg.error};
 auto prepared=prepare_fixed_service_change(write.elapsed,action_id,transition,{},reg.value,write.scale,read);if(!prepared.ok)return {false,{},prepared.error};
 checked=apply_fixed_service_change_in_candidate(write.elapsed,std::move(prepared.value),reg.value,write.scale,read);if(!checked.ok)return {false,{},checked.error};
 return read_original_task_death_terminal(c,s,catalog,write.catalog_identity,action_id,death,life);
}
Status validate_task_exact_execution(const CommonStore&c,const TaskState&s,const TaskCatalog&catalog,const ActionElapsedStore&elapsed,const WorldMoment&now,const TimeScaleRef&scale,const CatalogIdentity&identity,const TaskExactLifeSources*life){
 if(!s.exact_execution||!task_exact_execution_version(s.exact_execution->version))return common_failure("TaskExactExecutionRequired");
 auto headers=validate_task_exact_execution_headers(c,s,s.exact_execution->version);if(!headers.ok)return headers;
 TaskActionTimeSourceRead read(c,s,catalog,scale,identity,{},life);
 for(const auto&row:s.exact_execution->starts.rows()){const auto&v=row.second;if(row.first!=v.id||start(s,v.action_id)!=&v)return common_failure("DuplicateTaskExactActionStart",row.first);auto reg=registration_for(v.action_kind,s.exact_execution->version);if(!reg.ok)return {false,{},reg.error};auto proof=read.read_action_time_owner(reg.value,v.action_id);if(!proof.ok)return {false,{},proof.error};auto projected=read_action_time_projection(reg.value,proof.value,elapsed,now,scale,read);if(!projected.ok)return {false,{},projected.error};const auto*action=c.action(v.action_id);if(proof.value.phase==ActionTimeOwnerPhase::terminal){if(action||s.actions.find(v.action_id))return common_failure("TaskExactTerminalStillActive",v.action_id);bool bound=false;for(const auto&b:elapsed.terminal_bindings)if(b.action_ref==v.action_id){if(bound||b.terminal_result_ref!=*proof.value.terminal_result_ref||b.original_execution_source_ref.id!=v.id)return common_failure("TaskExactTerminalBindingMismatch",v.action_id);bound=true;}if(!bound)return common_failure("TaskExactTerminalBindingMissing",v.action_id);}else if(!action||action->remaining_ticks!=projected.value.remaining_half_days||action->start_tick!=v.started_at.half_day_tick)return common_failure("TaskExactActionProjectionMismatch",v.action_id);}
 // Original Family4 coverage is independent of the retained terminal rows. A
 // deleted stop plus a still-present Action cannot hide an already dead party.
 if(s.exact_execution->version==2){
  if(!life||&life->common!=&c)return common_failure("TaskExactOriginalLifeSourcesRequired",{},ErrorKind::unsupported_capability);
  auto same_now=compare_world_moments(life->actual_now,now);if(!same_now.ok)return {false,{},same_now.error};if(same_now.value)return common_failure("TaskExactLifeMomentMismatch");
  for(const auto&row:s.exact_execution->starts.rows()){
   const auto&begun=row.second;const auto*ended=terminal(s,begun.action_id);
   for(const auto&entry:life->deaths.deaths.rows()){
    if(entry.second.actor_id!=begun.actor_id&&entry.second.actor_id!=begun.accepted_action.context.provider_actor_id)continue;
    auto ref_memory=CheckpointMemoryCharge(0);DeathRef ref{"family_life","family_death",checkpoint_fresh_string_copy(entry.first)};
    auto actual=read_verified_death4(c,life->deaths,life->causes,ref,now);if(!actual.ok)return {false,{},actual.error};
    if(actual.value.effective_at.kind!=ActorFoundationAt::Kind::exact_moment)return common_failure("TaskExactOriginalDeathMomentRequired",entry.first);
    const auto*first=c.state().facts.find(begun.id);const auto*death_header=c.state().facts.find(entry.first);
    if(!first||!death_header)return common_failure("TaskExactDeathCausalHeaderMissing",begun.action_id);
    auto first_position=read_actual_common_fact_position(c,*first);if(!first_position.ok)return {false,{},first_position.error};
    auto death_position=read_actual_common_fact_position(c,*death_header);if(!death_position.ok)return {false,{},death_position.error};
    if(first_position.value>=death_position.value)return common_failure("TaskExactActionStartedAfterDeath",begun.action_id);
    if(!ended)return common_failure("TaskExactDeathConsumptionMissing",begun.action_id);
    const auto*last=c.state().facts.find(ended->id);if(!last)return common_failure("TaskExactTerminalHeaderMissing",ended->id);
    auto last_position=read_actual_common_fact_position(c,*last);if(!last_position.ok)return {false,{},last_position.error};
    // Already-applied evidence is not rewritten by a later recorded death.
    // Once the actual death producer has run, only its true stop may follow.
    if(last_position.value<death_position.value)continue;
    if(!ended->participant_death)return common_failure("TaskExactDeathConsumptionMissing",begun.action_id);
    const auto*cause=c.state().facts.find(ended->participant_death->death_ref.id);if(!cause)return common_failure("TaskExactDeathCausalHeaderMissing",ended->id);
    auto cause_position=read_actual_common_fact_position(c,*cause);if(!cause_position.ok)return {false,{},cause_position.error};
    if(cause_position.value>death_position.value)return common_failure("TaskExactEarlierDeathConsumptionMissing",begun.action_id);
   }
  }
 }

 for(const auto&row:s.exact_execution->terminals.rows()){const auto&v=row.second;if(row.first!=v.id||!start(s,v.action_id)||terminal(s,v.action_id)!=&v)return common_failure("OrphanTaskExactTerminal",row.first);}
 for(const auto&row:s.actions.rows()){const auto*action=c.action(row.first);if(action&&exact_task_action_kind(action->kind)&&!start(s,row.first))return common_failure("MissingTaskExactStart",row.first);}
 return common_success();
}
}
