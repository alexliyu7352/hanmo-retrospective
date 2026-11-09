#include "birth_due_execution.hpp"
#include "born_character_creation.hpp"
#include "family_birth.hpp"
#include "family_continuation_people4.hpp"
#include <algorithm>
#include <tuple>
namespace hanmo::resident {namespace {
Status broken(const StableId& code,const StableId& id={}){return common_failure(code,id,ErrorKind::internal);}
bool same(const SourceRef& a,const SourceRef& b){return std::tie(a.domain,a.kind,a.id)==std::tie(b.domain,b.kind,b.id);}
bool same(const WorldMoment& a,const WorldMoment& b){auto cmp=compare_world_moments(a,b);return cmp.ok&&cmp.value==0;}
bool same(const ScheduledBirthSource& a,const ScheduledBirthSource& b){return std::tie(a.arrangement_id,a.terms_version,a.due_tick,a.selected_policy_ref.id,a.selected_policy_ref.version,a.selected_policy_ref.catalog_digest)==std::tie(b.arrangement_id,b.terms_version,b.due_tick,b.selected_policy_ref.id,b.selected_policy_ref.version,b.selected_policy_ref.catalog_digest)&&same(a.occurrence,b.occurrence)&&same(a.source_ref,b.source_ref);}
// 幂等仅比较同一原终态的全部引用，不把不同 Born 或 Cancelled 当成功重放。
bool same_born_result(const BornBirthDueResult& a,const BornBirthDueResult& b){
 const auto same_optional=[](const std::optional<SourceRef>& x,const std::optional<SourceRef>& y){return x.has_value()==y.has_value()&&(!x||same(*x,*y));};
 return same(a.birth_fact_ref,b.birth_fact_ref)&&a.child_actor_id==b.child_actor_id&&same(a.location_result_ref,b.location_result_ref)&&a.transport_binding_ref==b.transport_binding_ref&&a.care.kind==b.care.kind&&same(a.care.care_assignment_ref,b.care.care_assignment_ref)&&a.care.care_need_ref==b.care.care_need_ref&&same(a.population_transition_ref,b.population_transition_ref)&&same(a.provision_binding_ref,b.provision_binding_ref)&&same_optional(a.carrier_load_result_ref,b.carrier_load_result_ref);
}
bool before(const CommonStore& c,const StableId& a,const StableId& b){const auto& order=c.state().facts.order();auto x=std::find(order.begin(),order.end(),a),y=std::find(order.begin(),order.end(),b);return x!=order.end()&&y!=order.end()&&x<y;}
Status selected(const CommonStore& c,const BirthArrangementCatalog& catalog,const BirthArrangementAccess& access){
 if(!catalog.enabled||(catalog.selected_version<2||catalog.selected_version>4)||!catalog.due_policy_ref)return common_failure("BirthDueAtActualLocationProfileRequired",{},ErrorKind::unsupported_capability);
 const auto& rule=*catalog.due_policy_ref;if(rule.id!="birth_due_at_actual_location"||rule.version!=1||rule.catalog_digest!=c.state().metadata.content_digest)return broken("BirthDueSelectedPolicyMismatch");
 if(!access.people||!access.people->bound_to(c)||!access.life||!access.people->bound_to(c,*access.life))return common_failure("BirthCompletePeopleReadRequired",{},ErrorKind::unsupported_capability);
 return common_success();
}
const BirthDueExecution* executed(const FamilyBirthState& s,const StableId& accepted){const BirthDueExecution* out=nullptr;for(const auto& row:s.birth_due_executions.rows())if(row.second.source.arrangement_id==accepted){if(out)return nullptr;out=&row.second;}return out;}
}
Outcome<ScheduledBirthSource> read_scheduled_birth_source(const CommonStore& c,const InteractionCatalog& interactions,const FamilyCatalog& family,const BirthArrangementCatalog& catalog,const FamilyBirthState& state,const BirthArrangementAccess& access,const StableId& accepted_id){
 auto valid=selected(c,catalog,access);if(!valid.ok)return {false,{},valid.error};const auto* accepted=state.replies.find(accepted_id);const auto* head=c.state().facts.find(accepted_id);
 if(!accepted||accepted->decision!="accepted"||accepted->kind!="family_birth_accepted"||!accepted->confirmation||!head||head->id!=accepted_id||head->domain!="family_birth"||head->kind!=accepted->kind)return {false,{},broken("BirthAcceptedArrangementRequired",accepted_id).error};
 const auto& original=*accepted->confirmation;valid=validate_birth_arrangement_source(c,interactions,family,catalog,original,access.people,accepted_id);if(!valid.ok)return {false,{},valid.error};const auto* proposal=state.proposals.find(accepted->proposal_id);const auto* offered=c.state().facts.find(accepted->proposal_id);
 if(!proposal||!offered||offered->id!=accepted->proposal_id||offered->domain!="family_birth"||offered->kind!="family_birth_proposed"||proposal->kind!=offered->kind||!before(c,accepted->proposal_id,accepted_id)||accepted->actor_id!=original.listener_actor_id||accepted->tick!=original.tick||accepted->revision!=original.revision||original.tick>2147483647-original.terms.birth_duration_ticks||accepted->due_tick!=original.tick+original.terms.birth_duration_ticks||accepted->reserved_food!=original.terms.food_per_period*original.terms.prepaid_care_periods)return {false,{},broken("BirthAcceptanceBindingMismatch",accepted_id).error};
 if(std::tie(original.arrangement_id,original.speaker_actor_id,original.listener_actor_id,original.dialogue_id,original.node_id,original.terms_version)!=std::tie(proposal->source.arrangement_id,proposal->source.speaker_actor_id,proposal->source.listener_actor_id,proposal->source.dialogue_id,proposal->source.node_id,proposal->source.terms_version)||!birth_care_terms_equal(original.terms,proposal->source.terms))return {false,{},broken("BirthAcceptanceBindingMismatch",accepted_id).error};
 for(const auto& input:{std::pair<const BirthArrangementSource*,const StableId*>{&proposal->source,&accepted->proposal_id},{&original,&accepted_id}}){valid=validate_birth_arrangement_source(c,interactions,family,catalog,*input.first,access.people,*input.second);if(!valid.ok)return {false,{},valid.error};if(!access.player_before||!access.decision_maker_before||!access.eligible_pair_before)return {false,{},common_failure("BirthAcceptanceHistoryRequired",{},ErrorKind::unsupported_capability).error};auto player=access.player_before(input.first->tick,*input.second);if(!player.ok)return {false,{},player.error};if(player.value!=input.first->listener_actor_id)return {false,{},broken("BirthPersonalDecisionRequired",*input.second).error};valid=access.decision_maker_before(player.value,input.first->tick,*input.second);if(!valid.ok)return {false,{},valid.error};valid=access.eligible_pair_before(input.first->speaker_actor_id,input.first->listener_actor_id,input.first->tick,*input.second);if(!valid.ok)return {false,{},valid.error};}
 return {true,{accepted_id,original.terms_version,accepted->due_tick,WorldMoment{accepted->due_tick,{}},{"family_birth","family_birth_accepted",accepted_id},*catalog.due_policy_ref},{}};
}
Status validate_birth_due_executions(const CommonStore& c,const InteractionCatalog& interactions,const FamilyCatalog& family,const BirthArrangementCatalog& catalog,const FamilyBirthState& state,const BirthArrangementAccess& access){
 if(catalog.selected_version==1)return state.birth_due_executions.empty()?common_success():broken("BirthDueProfileRequired");auto valid=selected(c,catalog,access);if(!valid.ok)return valid;auto now=access.people->read_current_moment();if(!now.ok)return {false,{},now.error};
 for(const auto& row:state.birth_due_executions.rows()){
  const auto& execution=row.second;auto expected=read_scheduled_birth_source(c,interactions,family,catalog,state,access,execution.source.arrangement_id);if(!expected.ok)return {false,{},expected.error};if(!same(expected.value,execution.source))return broken("ScheduledBirthSourceMismatch",row.first);auto time=compare_world_moments(execution.source.occurrence,now.value);if(!time.ok)return {false,{},time.error};if(time.value>0)return broken("BirthDueExecutionInFuture",row.first);
  for(const auto& other:state.birth_due_executions.rows())if(other.first!=row.first&&other.second.source.arrangement_id==execution.source.arrangement_id)return broken("DuplicateBirthDueExecution",row.first);
  const auto* accepted=state.replies.find(execution.source.arrangement_id);const auto& terms=accepted->confirmation->terms;
  if(const auto* cancelled=std::get_if<BirthCancellationResult>(&execution.result)){
   const auto* closure=state.closures.find(row.first);const SourceRef terminal{"family_birth","family_birth_closed",row.first};const auto* header=c.state().facts.find(row.first);
   if(!closure||!header||header->id!=row.first||header->domain!=terminal.domain||header->kind!=terminal.kind||!before(c,execution.source.source_ref.id,row.first)||cancelled->result_id!=row.first||cancelled->arrangement_id!=execution.source.arrangement_id||cancelled->terms_version!=execution.source.terms_version||cancelled->reason!="delivery_actor_dead"||!same(cancelled->effective_at,execution.source.occurrence)||closure->accepted_id!=cancelled->arrangement_id||closure->actor_id!=terms.gestating_actor_id||closure->reason!="parent_death"||closure->source_fact_id!=cancelled->death_ref.id||closure->tick!=execution.source.due_tick||closure->released_food!=accepted->reserved_food)return broken("BirthCancellationSourceMismatch",row.first);
   valid=access.people->life().require_death_before_original_fact(cancelled->death_ref,terms.gestating_actor_id,closure->tick,terminal);if(!valid.ok)return valid;auto death=access.people->life().read_actor_death(terms.gestating_actor_id);if(!death.ok)return {false,{},death.error};if(!death.value||death.value->effective_at.kind!=ActorFoundationAt::Kind::exact_moment||death.value->recorded_at.kind!=ActorFoundationAt::Kind::exact_moment)return broken("BirthVerifiedDeath4Required",row.first);for(const auto& at:{death.value->effective_at.exact_moment,death.value->recorded_at.exact_moment}){auto compare=compare_world_moments(at,cancelled->effective_at);if(!compare.ok)return {false,{},compare.error};if(compare.value>0)return broken("BirthCancellationBeforeActualDeath",row.first);}
   // This producer closes only the original finite unused food reservation.
   // Other responsibility owners must provide their real results before a
   // wider branch can be admitted; they are not inferred from an empty list.
   if(cancelled->original_responsibility_refs.size()!=1||!same(cancelled->original_responsibility_refs.front(),execution.source.source_ref)||cancelled->closed_or_pending_disposition.kind!=BirthResponsibilityDisposition::Kind::closed||cancelled->closed_or_pending_disposition.original_refs.size()!=1||!same(cancelled->closed_or_pending_disposition.original_refs.front(),terminal))return broken("BirthCancellationResponsibilityMismatch",row.first);
   valid=validate_birth_care_reservation(c,terms,execution.source.arrangement_id,0);if(!valid.ok)return valid;for(const auto& birth:state.births.rows())if(birth.second.accepted_id==cancelled->arrangement_id)return broken("CancelledBirthHasChild",row.first);
  }else{
   const auto& born=std::get<BornBirthDueResult>(execution.result);const auto* original=state.births.find(row.first);if(!original||born.birth_fact_ref.domain!="family_birth"||born.birth_fact_ref.kind!="family_birth_recorded"||born.birth_fact_ref.id!=row.first||original->actor_id!=born.child_actor_id||original->accepted_id!=execution.source.arrangement_id||original->tick!=execution.source.due_tick)return broken("BirthDueOriginalBirthMismatch",row.first);
   if(catalog.selected_version==4&&(!original->location_result_ref||!same(original->location_result_ref->source_ref,born.location_result_ref)))return broken("BirthDueLocationResultMismatch",row.first);
   if(!access.born_sources||!access.born_sources->bound_to(c,state))return common_failure("BirthActualPlacementPopulationCareSourcesRequired",row.first,ErrorKind::unsupported_capability);valid=access.born_sources->validate_actual_born_result(execution.source,born);if(!valid.ok)return valid;
  }
 }
 // Reverse coverage: deleting the due row must not make a cancellation or
 // child fall back to the old integer-only producer.
 for(const auto& row:state.closures.rows())if(row.second.reason=="parent_death"){const auto* result=state.birth_due_executions.find(row.first);if(!result||!std::holds_alternative<BirthCancellationResult>(result->result))return broken("BirthCancellationExecutionRequired",row.first);}
 for(const auto& row:state.births.rows()){const auto* result=state.birth_due_executions.find(row.first);if(!result||!std::holds_alternative<BornBirthDueResult>(result->result))return broken("BirthDueExecutionRequired",row.first);}
 return common_success();
}
Outcome<BirthAtLocationOutcome> cancel_scheduled_birth_after_actual_death_in_candidate(CommonStore& c,const InteractionCatalog& interactions,const FamilyCatalog& family,const BirthArrangementCatalog& catalog,FamilyBirthState& state,const BirthArrangementAccess& access,const ScheduledBirthSource& requested,const ReadStamp& stamp,const ReadStampAuthority& stamps,const DomainReadVersionSource& versions){
 auto valid=selected(c,catalog,access);if(!valid.ok)return {false,{},valid.error};auto expected=read_scheduled_birth_source(c,interactions,family,catalog,state,access,requested.arrangement_id);if(!expected.ok)return {false,{},expected.error};if(!same(expected.value,requested))return {false,{},broken("ScheduledBirthSourceMismatch").error};auto history=validate_birth_arrangements(c,interactions,family,catalog,state,access);if(!history.ok)return {false,{},history.error};
 if(const auto* old=executed(state,requested.arrangement_id)){const SourceRef ref=std::holds_alternative<BirthCancellationResult>(old->result)?SourceRef{"family_birth","family_birth_closed",std::get<BirthCancellationResult>(old->result).result_id}:std::get<BornBirthDueResult>(old->result).birth_fact_ref;return {true,BirthAlreadyTerminal{ref},{}};}
 for(const auto& row:state.closures.rows())if(row.second.accepted_id==requested.arrangement_id)return {true,BirthAlreadyTerminal{{"family_birth",row.second.kind,row.first}},{}};
 auto now=access.people->read_current_moment();if(!now.ok)return {false,{},now.error};if(!same(now.value,requested.occurrence))return {false,{},broken("BirthDueExactCalendarBoundaryRequired").error};valid=stamps.validate(stamp,c.state().identity,ReadMode::phase_current,c,versions);if(!valid.ok)return {false,{},valid.error};
 const auto* accepted=state.replies.find(requested.arrangement_id);const auto& terms=accepted->confirmation->terms;bool actor_read=false;for(const auto& r:stamp.resources())if(r.resource.kind==ResourceKey::Kind::actor&&r.resource.id==terms.gestating_actor_id)actor_read=true;if(stamp.phase()!="birth_due"||!same(stamp.span().to,now.value)||!actor_read||std::find(stamp.source_ids().begin(),stamp.source_ids().end(),requested.source_ref.id)==stamp.source_ids().end())return {false,{},broken("BirthDueReadStampMismatch").error};
 auto death=access.people->life().read_actor_death(terms.gestating_actor_id);if(!death.ok)return {false,{},death.error};if(!death.value)return {false,{},common_failure("DeliveryActorNotDead").error};if(std::find(stamp.source_ids().begin(),stamp.source_ids().end(),death.value->death_ref.id)==stamp.source_ids().end())return {false,{},broken("BirthDeathReadStampRequired").error};
 const auto* remaining=history.value.remaining_food.find(requested.arrangement_id);if(!remaining||*remaining!=accepted->reserved_food)return {false,{},broken("BirthUnusedReservationRequired").error};auto recipe=release_birth_care_recipe(c,terms,requested.arrangement_id,*remaining);if(!recipe.ok)return {false,{},recipe.error};
 auto id=c.next_id("family_birth");if(!id.ok)return {false,{},id.error};BirthClosure closure;closure.accepted_id=requested.arrangement_id;closure.actor_id=terms.gestating_actor_id;closure.reason="parent_death";closure.source_fact_id=death.value->death_ref.id;closure.tick=now.value.half_day_tick;closure.revision=c.revision();closure.released_food=*remaining;
 valid=c.apply_resources(recipe.value);if(!valid.ok)return {false,{},valid.error};if(!state.closures.insert(id.value,std::move(closure)))return {false,{},broken("DuplicateBirthClosure").error};valid=c.append_fact({id.value,"family_birth_closed","family_birth"});if(!valid.ok)return {false,{},valid.error};
 BirthCancellationResult result;result.result_id=id.value;result.arrangement_id=requested.arrangement_id;result.terms_version=requested.terms_version;result.death_ref=death.value->death_ref;result.effective_at=now.value;result.original_responsibility_refs.push_back(requested.source_ref);result.closed_or_pending_disposition.kind=BirthResponsibilityDisposition::Kind::closed;result.closed_or_pending_disposition.original_refs.push_back({"family_birth","family_birth_closed",id.value});if(!state.birth_due_executions.insert(id.value,{requested,result}))return {false,{},broken("DuplicateBirthDueExecution").error};
 auto after=validate_birth_arrangements(c,interactions,family,catalog,state,access);if(!after.ok)return {false,{},after.error};return {true,std::move(result),{}};
}
Outcome<BirthAtLocationOutcome> adopt_born_birth_due_in_candidate(BornCharacterCreationContext& x,const ScheduledBirthSource& requested,const BornCharacterCreated& created,const BornBirthDueResult& result,const ReadStamp& stamp){
 auto& c=x.common;auto& state=x.births;
 if(x.catalog.selected_version!=4||!x.catalog.enabled||!x.access.people||!x.access.people->bound_to(c)||!x.placement.bound_to(c)||x.interactions.common.get()!=&x.common_catalog||x.access.lineage!=&x.lineage||x.access.foundation!=&x.foundation||x.access.foundation_catalog!=&x.foundation_catalog||x.access.placement!=&x.placement)
  return {false,{},common_failure("BornCharacterCreation4SourcesRequired",{},ErrorKind::unsupported_capability).error};
 auto valid=selected(c,x.catalog,x.access);if(!valid.ok)return {false,{},std::move(valid.error)};
 if(!x.access.born_sources||!x.access.born_sources->bound_to(c,state))return {false,{},common_failure("BirthActualPlacementPopulationCareSourcesRequired",result.birth_fact_ref.id,ErrorKind::unsupported_capability).error};
 auto expected=read_scheduled_birth_source(c,x.interactions,x.family_catalog,x.catalog,state,x.access,requested.arrangement_id);if(!expected.ok)return {false,{},std::move(expected.error)};
 if(!same(expected.value,requested))return {false,{},broken("ScheduledBirthSourceMismatch",requested.arrangement_id).error};
 const auto* birth=state.births.find(result.birth_fact_ref.id);const auto* header=c.state().facts.find(result.birth_fact_ref.id);
 if(!birth||!header||header->id!=result.birth_fact_ref.id||header->domain!="family_birth"||header->kind!="family_birth_recorded"||birth->kind!=header->kind||result.birth_fact_ref.domain!=header->domain||result.birth_fact_ref.kind!=header->kind||birth->actor_id!=result.child_actor_id||birth->accepted_id!=requested.arrangement_id||birth->tick!=requested.due_tick||!before(c,requested.source_ref.id,result.birth_fact_ref.id)||!birth->location_result_ref||!same(birth->location_result_ref->source_ref,result.location_result_ref))
  return {false,{},broken("BirthDueOriginalBirthMismatch",result.birth_fact_ref.id).error};
 if(result.care.kind!=BirthCareDisposition::Kind::accepted_care&&result.care.kind!=BirthCareDisposition::Kind::care_need_opened)return {false,{},broken("BirthDueCareDispositionMismatch",result.birth_fact_ref.id).error};
 if(created.birth_ref.birth_fact_id!=result.birth_fact_ref.id||created.child_actor_id!=result.child_actor_id||created.actual_birth_source.source_schema!=4||!same(created.actual_birth_source.source_ref,result.birth_fact_ref)||created.location_result.source_schema!=birth->location_result_ref->source_schema||!same(created.location_result.source_ref,result.location_result_ref)||created.transport_binding_ref!=result.transport_binding_ref||created.condition_creation.actor_id!=result.child_actor_id)
  return {false,{},broken("BirthDueActualCreationMismatch",result.birth_fact_ref.id).error};
 const BirthDueExecution* prior=nullptr;const StableId* prior_key=nullptr;
 for(const auto& row:state.birth_due_executions.rows())if(row.second.source.arrangement_id==requested.arrangement_id){if(prior)return {false,{},broken("DuplicateBirthDueExecution",requested.arrangement_id).error};prior=&row.second;prior_key=&row.first;}
 if(prior){
  const auto* born=std::get_if<BornBirthDueResult>(&prior->result);
  if(!born||*prior_key!=result.birth_fact_ref.id||!same(prior->source,requested)||!same_born_result(*born,result))return {false,{},broken("BirthDueTerminalConflict",requested.arrangement_id).error};
  auto history=validate_birth_arrangements(c,x.interactions,x.family_catalog,x.catalog,state,x.access);if(!history.ok)return {false,{},std::move(history.error)};
  return {true,BirthAlreadyTerminal{born->birth_fact_ref},{}};
 }
 if(state.birth_due_executions.find(result.birth_fact_ref.id))return {false,{},broken("BirthDueTerminalConflict",result.birth_fact_ref.id).error};
 // 此时真 Birth row 已存在，不能调用创建前“尚无出生”门，也不能在插入
 // execution 前调用要求全反向覆盖的整 Birth 验证器。
 auto now=x.clock.read_current_moment();if(!now.ok)return {false,{},std::move(now.error)};
 auto people_now=x.access.people->read_current_moment();if(!people_now.ok)return {false,{},std::move(people_now.error)};
 if(!same(now.value,people_now.value)||!same(now.value,requested.occurrence)||!same(now.value,WorldMoment{requested.due_tick,{}}))return {false,{},broken("BirthDueExactCalendarBoundaryRequired").error};
 valid=x.stamps.validate(stamp,c.state().identity,ReadMode::phase_current,c,x.versions);if(!valid.ok)return {false,{},std::move(valid.error)};
 const auto* accepted=state.replies.find(requested.arrangement_id);const auto& delivery=accepted->confirmation->terms.gestating_actor_id;
 bool delivery_read=false,child_read=false;for(const auto& read:stamp.resources())if(read.resource.kind==ResourceKey::Kind::actor){delivery_read=delivery_read||read.resource.id==delivery;child_read=child_read||read.resource.id==result.child_actor_id;}
 const auto has_source=[&](const StableId& id){return !id.empty()&&std::find(stamp.source_ids().begin(),stamp.source_ids().end(),id)!=stamp.source_ids().end();};
 if(stamp.phase()!="birth_due"||!same(stamp.span().to,now.value)||!delivery_read||!child_read||!has_source(requested.source_ref.id)||!has_source(result.birth_fact_ref.id)||!has_source(result.location_result_ref.id)||!has_source(result.population_transition_ref.id)||!has_source(result.provision_binding_ref.id)||(result.carrier_load_result_ref&&!has_source(result.carrier_load_result_ref->id))||(result.care.kind==BirthCareDisposition::Kind::accepted_care?!has_source(result.care.care_assignment_ref.id):!has_source(result.care.care_need_ref)))
  return {false,{},broken("BirthDueReadStampMismatch",result.birth_fact_ref.id).error};
 auto creation=read_actual_actor_condition_creation(c,x.foundation,created.condition_creation);if(!creation.ok)return {false,{},std::move(creation.error)};
 BornCharacterSourceContext original{c,x.common_catalog,x.family_catalog,x.lineage,x.interactions,x.catalog,state,x.access};
 valid=validate_born_character_identity_and_placement(original,x.foundation,x.foundation_catalog,x.placement,created.birth_ref);if(!valid.ok)return {false,{},std::move(valid.error)};
 // 原结果读口必须只反读已实际写入的各 owner 源，不能回调本次尚待采用的
 // execution 或整 World 验证；空/伪造 refs 不在这里补成成功。
 valid=x.access.born_sources->validate_actual_born_result(requested,result);if(!valid.ok)return {false,{},std::move(valid.error)};
 valid=x.stamps.validate(stamp,c.state().identity,ReadMode::phase_current,c,x.versions);if(!valid.ok)return {false,{},std::move(valid.error)};
 // 入表前先持真正的返回值和待採用 row；requested/result 即使借原表，
 // 後續 insert 亦不再讀它們，不能讓表增長使最後 return 借懸空。
 BornBirthDueResult returned=result;BirthDueExecution execution{requested,returned};
 if(!state.birth_due_executions.insert(returned.birth_fact_ref.id,std::move(execution)))return {false,{},broken("DuplicateBirthDueExecution",returned.birth_fact_ref.id).error};
 auto history=validate_birth_arrangements(c,x.interactions,x.family_catalog,x.catalog,state,x.access);if(!history.ok)return {false,{},std::move(history.error)};
 // completed_through 由整批末尾调用：本次完成不能提前拒绝同 M 尚待逐项
 // 处理的其他真实 due；原全源/反向覆盖已由上项完整 Birth 验证恢复。
 return {true,std::move(returned),{}};
}
Status validate_birth_due_completed_through(const CommonStore& c,const InteractionCatalog& interactions,const FamilyCatalog& family,const BirthArrangementCatalog& catalog,const FamilyBirthState& state,const BirthArrangementAccess& access){
 auto history=validate_birth_arrangements(c,interactions,family,catalog,state,access);if(!history.ok)return {false,{},history.error};auto valid=selected(c,catalog,access);if(!valid.ok)return valid;auto now=access.people->read_current_moment();if(!now.ok)return {false,{},now.error};
 for(const auto& row:state.replies.rows())if(row.second.decision=="accepted"&&!history.value.closed_acceptances.count(row.first)&&!history.value.born_acceptances.count(row.first)){auto source=read_scheduled_birth_source(c,interactions,family,catalog,state,access,row.first);if(!source.ok)return {false,{},source.error};auto time=compare_world_moments(source.value.occurrence,now.value);if(!time.ok)return {false,{},time.error};if(time.value<=0)return broken("UnresolvedBirthDueAtStableBoundary",row.first);}return common_success();
}
Status visit_scheduled_births_due_at(const CommonStore& c,const InteractionCatalog& interactions,const FamilyCatalog& family,const BirthArrangementCatalog& catalog,const FamilyBirthState& state,const BirthArrangementAccess& access,const WorldMoment& at,ScheduledBirthDueVisitor& visitor){
 auto history=validate_birth_arrangements(c,interactions,family,catalog,state,access);if(!history.ok)return {false,{},history.error};auto valid=selected(c,catalog,access);if(!valid.ok)return valid;auto now=access.people->read_current_moment();if(!now.ok)return {false,{},now.error};if(!same(now.value,at)||!same(at,WorldMoment{at.half_day_tick,{}}))return broken("BirthDueExactCalendarBoundaryRequired");
 // Borrow one accepted row at a time. A callback must not insert/reorder the
 // accepted table. New terminal rows may be appended to the same candidate.
 const auto original_fact_count=c.state().facts.order().size();for(std::size_t index=0;index<original_fact_count;++index){const auto& id=c.state().facts.order()[index];const auto* accepted=state.replies.find(id);if(!accepted||accepted->decision!="accepted")continue;bool terminal=false;for(const auto& r:state.closures.rows())terminal=terminal||r.second.accepted_id==id;for(const auto& r:state.births.rows())terminal=terminal||r.second.accepted_id==id;if(terminal)continue;auto source=read_scheduled_birth_source(c,interactions,family,catalog,state,access,id);if(!source.ok)return {false,{},source.error};auto compare=compare_world_moments(source.value.occurrence,at);if(!compare.ok)return {false,{},compare.error};if(compare.value<0)return broken("MissedBirthDueBoundary",id);if(compare.value==0){valid=visitor.due(source.value);if(!valid.ok)return valid;}}
 return common_success();
}
}
