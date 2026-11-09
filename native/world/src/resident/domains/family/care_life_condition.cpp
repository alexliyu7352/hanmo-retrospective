#include "care_life_condition.hpp"
#include "resident/domains/actor_foundation/authored_origin.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={},ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,id,kind).error};}
template<class T>T take(Outcome<T> v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
bool same(const SourceRef& a,const SourceRef& b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
SourceRef source(const DeathRef& d){return {d.domain,d.kind,d.id};}
std::size_t index(const CommonStore& c,const SourceRef& ref){const auto* h=c.state().facts.find(ref.id);const auto& order=c.state().facts.order();const auto at=std::find(order.begin(),order.end(),ref.id);if(!h||h->id!=ref.id||h->domain!=ref.domain||h->kind!=ref.kind||at==order.end())fail("CareLifeOriginalFactRequired",ref.id);return static_cast<std::size_t>(at-order.begin());}
void sources(const CareGoalSourceContext& g,const CharacterConditionSources& body){
 if(&g.common!=&body.common||&g.deaths!=body.deaths||&g.life!=body.life_sources||(g.rules.selected_care_version()!=5&&g.rules.selected_care_version()!=6)||!g.coverage||&g.coverage->handovers.acceptance.goals!=&g)fail("CareLifeSameOriginalSourcesRequired",{},ErrorKind::unsupported_capability);
 take(read_original_care_participant_purpose(body.catalog));if(take(compare_world_moments(take(g.clock.read_current_moment()),body.actual_now)))fail("CareLifeSameActualMomentRequired");
}
const CareCoverageSatisfied& satisfied(const CareCoverageSources& x,const StableId& binding){const CareCoverageSatisfied* found=nullptr;for(const auto& pair:x.tables.satisfied.rows())if(pair.second.binding_ref==binding){if(found||pair.first!=pair.second.fact_id)fail("CareLifeSatisfactionNotUnique",binding);found=&pair.second;}if(!found)fail("CareLifeSatisfactionMissing",binding);return *found;}
bool active_at_death(const CareCoverageSources& x,const CareBinding& b,const SourceRef& death){
 const auto& g=x.handovers.acceptance.goals;const auto& sat=satisfied(x,b.binding_id);
 if(index(g.common,sat.handover_result_ref)>=index(g.common,death))return false;
 const auto prior=take(read_care_requirement_before_original_fact(g,sat.requirement_id,death));const auto* completed=prior.last_transition?std::get_if<CareHandoverCompletedEvent>(&prior.last_transition->body):nullptr;
 return prior.state==StateGoalState::satisfied&&completed&&completed->binding_id==b.binding_id;
}
const CareTemporalEvent* outcome(const CareGoalSourceContext& g,const StableId& requirement,const DeathRef& death,bool holder,const StableId& binding={}){
 const CareTemporalEvent* found=nullptr;
 for(const auto& pair:g.goals.temporal_events.rows()){
  bool match=false;if(holder){const auto* lost=std::get_if<CareCoverageLostEvent>(&pair.second.body);const auto* why=lost?std::get_if<CareHolderDeath>(&lost->cause):nullptr;match=lost&&why&&lost->requirement_id==requirement&&lost->old_binding_ref==binding&&same(source(why->death_ref),source(death));}
  else{const auto* closed=std::get_if<CareRequirementClosedEvent>(&pair.second.body);const auto* why=closed?std::get_if<CareSubjectDeath>(&closed->cause):nullptr;match=closed&&why&&closed->requirement_id==requirement&&same(source(why->death_ref),source(death));}
  if(match){if(found||pair.first!=pair.second.event_id)fail("CareLifeOutcomeNotUnique",requirement);found=&pair.second;}
 }
 return found;
}
const ConditionReceiptV2& death_receipt(const CharacterConditionSources& body,const StableId& death){const ConditionReceiptV2* found=nullptr;for(const auto& pair:body.state.character_condition_extensions.applied_receipts.rows())if(pair.second.life_transition_ref&&*pair.second.life_transition_ref==death){if(found||pair.first!=pair.second.result_id)fail("CareLifeReceiptNotUnique",death);found=&pair.second;}if(!found)fail("CareLifeActualReceiptRequired",death);return *found;}
const CapacityTransitionV2& care_change(const CharacterConditionSources& body,const ConditionReceiptV2& receipt){
 const auto* purpose=take(read_original_care_participant_purpose(body.catalog));const CapacityTransitionV2* found=nullptr;
 for(const auto& id:receipt.capacity_transition_refs){const auto* row=body.state.character_condition_extensions.capacity_transitions.find(id);if(!row)fail("CareLifeCapacityOutputMissing",id);if(!actor_foundation_rule_equal(row->purpose,purpose->purpose_ref))continue;
  auto actual=take(read_care_participant_capacity_change(body,character_capacity_transition_source(id)));if(found||actual.original_receipt!=&receipt||!actual.priority_holder_death_id||!receipt.life_transition_ref||*actual.priority_holder_death_id!=*receipt.life_transition_ref)fail("CareLifeCapacityOutputMismatch",id);found=actual.transition;
 }
 if(!found)fail("CareLifeActualCapacityOutputRequired",receipt.result_id);return *found;
}
void checked_outcome(const CareCoverageSources& x,const CareTemporalEvent& event,const StableId& requirement,const DeathRef& death){
 const auto& g=x.handovers.acceptance.goals;const auto* actual=take(read_care_goal_transition_source(g,event.original_transition_ref));if(actual!=&event)fail("CareLifeOriginalOutcomeMismatch",requirement);
 const auto original=take(read_verified_death4(g.common,g.deaths,g.life,death,take(g.clock.read_current_moment())));
 if(original.effective_at.kind!=ActorFoundationAt::Kind::exact_moment||take(compare_world_moments(event.occurred_at,original.effective_at.exact_moment))||index(g.common,source(death))>=index(g.common,event.original_transition_ref))fail("CareLifeOutcomeCausalityMismatch",requirement);
 const auto from=index(g.common,source(death)),until=index(g.common,event.original_transition_ref);for(const auto& pair:g.deaths.deaths.rows()){const SourceRef ref{"family_life","family_death",pair.first};const auto n=index(g.common,ref);if(n>from&&n<until)fail("CareLifeOutcomeDelayedPastOriginalLife",requirement);}
}
}
bool care_life_condition_consumer_selected(const ActorFoundationCatalog& catalog)noexcept{
 for(const auto& module:catalog.identity.module_versions)if(module.first=="care_requirements"&&(module.second=="5"||module.second=="6"))return true;return false;
}
bool NativeCareLifeConditionWrite::bound_to(const CommonStore& common,const AuthoredActorFoundationState& foundation,const FamilyDeathState& deaths)const noexcept{
 return &binding_.common==&common&&&binding_.body.common==&common&&&binding_.body.state==&foundation&&binding_.body.deaths==&deaths&&&binding_.goal_sources.common==&common&&&binding_.goal_sources.deaths==&deaths;
}
Status NativeCareLifeConditionWrite::consume_actual_life_condition(const BoundActorConditionWrite& token){
 const auto result=apply_care_life_condition_in_candidate(binding_.common,binding_.goals,binding_.coverage,binding_.goal_sources,binding_.body,token,binding_.affairs,binding_.policies,binding_.goal_read,binding_.tasks);return result.ok?common_success():Status{false,{},result.error};
}
Outcome<CareLifeConditionApplication> apply_care_life_condition_in_candidate(CommonStore& common,CareGoalWriteTables goals,CareCoverageWriteTables tables,const CareGoalSourceContext& g,const CharacterConditionSources& body,const BoundActorConditionWrite& token,AffairsState& affairs,const StateNeedPolicyRead& policies,const StateGoalRead& goal_read,StateTaskConsumer& tasks){try{
 sources(g,body);const auto& x=*g.coverage;
 if(&common!=&g.common||&goals.requirements!=&g.goals.requirements||&goals.transitions!=&g.goals.transitions||&goals.temporal_events!=&g.goals.temporal_events||&tables.bindings!=&x.tables.bindings||&tables.satisfied!=&x.tables.satisfied||&tables.lost!=&x.tables.lost)fail("CareLifeSameCandidateRequired");
 const auto* receipt=take(read_actual_actor_condition_write(common,body.state,token));take(read_character_condition_receipt_provenance(body,token.receipt_ref));
 if(!receipt->life_transition_ref)return {true,{},{}};care_change(body,*receipt);
 const DeathRef death{"family_life","family_death",*receipt->life_transition_ref};const auto actual=take(read_verified_death4(common,g.deaths,g.life,death,body.actual_now));if(actual.actor_id!=receipt->actor_id)fail("CareLifeReceiptActorMismatch",receipt->result_id);
 bool fresh=false;
 for(const auto& pair:goals.requirements.rows())if(pair.second.subject_actor_id==receipt->actor_id){const auto prior=take(read_care_requirement_before_original_fact(g,pair.first,source(death)));if((prior.state==StateGoalState::unsatisfied||prior.state==StateGoalState::satisfied)&&!outcome(g,pair.first,death,false))fresh=true;}
 for(const auto& pair:tables.bindings.rows())if(pair.second.carer_actor_id==receipt->actor_id&&active_at_death(x,pair.second,source(death))&&!outcome(g,satisfied(x,pair.first).requirement_id,death,true,pair.first))fresh=true;
 // A missing original reaction cannot be repaired by a later capture sweep.
 // Replaying already present outcomes remains valid and side-effect free.
 if(fresh&&common.actor_condition_execution_drafts().size()!=static_cast<std::size_t>(token.capture_sequence))fail("CareLifeImmediateOriginalTokenRequired",receipt->result_id);
 const auto before_count=common.state().facts.order().size();
 // An actual subject death closes its original target. A holder death affects
 // only bindings genuinely active at its source cut, never a later handover.
 for(const auto& pair:goals.requirements.rows())if(pair.second.subject_actor_id==receipt->actor_id){
  const auto prior=take(read_care_requirement_before_original_fact(g,pair.first,source(death)));
  if(prior.state==StateGoalState::unsatisfied||prior.state==StateGoalState::satisfied)take(close_care_coverage_after_subject_death_in_candidate(common,goals,tables,g,pair.first,death));
 }
 for(const auto& pair:tables.bindings.rows())if(pair.second.carer_actor_id==receipt->actor_id&&active_at_death(x,pair.second,source(death)))take(lose_care_coverage_after_holder_death_in_candidate(common,goals,tables,g,pair.first,death));
 CareLifeConditionApplication result;
 // Reconcile after the real group of Care state changes. The persisted outcome
 // owns the once key; no extra processed-source table or transient ID array.
 for(const auto& pair:goals.temporal_events.rows()){
  const auto* lost=std::get_if<CareCoverageLostEvent>(&pair.second.body);const auto* holder=lost?std::get_if<CareHolderDeath>(&lost->cause):nullptr;
  const auto* closed=std::get_if<CareRequirementClosedEvent>(&pair.second.body);const auto* subject=closed?std::get_if<CareSubjectDeath>(&closed->cause):nullptr;
  if(!(holder&&same(source(holder->death_ref),source(death)))&&!(subject&&same(source(subject->death_ref),source(death))))continue;
  const auto& requirement=holder?lost->requirement_id:closed->requirement_id;const bool old=index(common,pair.second.original_transition_ref)<before_count;
  take(reconcile_applied_care_goal_in_candidate(common,goals,g,{requirement,pair.second.original_transition_ref,old},affairs,policies,goal_read,tasks));
  auto& count=old?result.already_applied_transitions:result.actual_goal_transitions;if(count==std::numeric_limits<Integer>::max())fail("CareLifeOutcomeCountOverflow");++count;
 }
 return {true,result,{}};
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Status validate_care_life_condition_coverage(const CareCoverageSources& x,const CharacterConditionSources& body){try{
 const auto& g=x.handovers.acceptance.goals;sources(g,body);take(validate_care_requirement_goal_sources(g));take(validate_condition_purpose_capacity_coverage(body));
 for(const auto& pair:g.deaths.deaths.rows()){
  const auto& row=pair.second;if(!row.precise||pair.first!=row.precise->death_id)fail("CareLifeActualDeath4Required",pair.first);const DeathRef death{"family_life","family_death",pair.first};take(read_verified_death4(g.common,g.deaths,g.life,death,body.actual_now));care_change(body,death_receipt(body,pair.first));
  for(const auto& goal:g.goals.requirements.rows())if(goal.second.subject_actor_id==row.actor_id){const auto prior=take(read_care_requirement_before_original_fact(g,goal.first,source(death)));if(prior.state!=StateGoalState::unsatisfied&&prior.state!=StateGoalState::satisfied)continue;const auto* result=outcome(g,goal.first,death,false);if(!result)fail("CareSubjectDeathOutcomeMissing",goal.first);checked_outcome(x,*result,goal.first,death);}
  for(const auto& binding:x.tables.bindings.rows())if(binding.second.carer_actor_id==row.actor_id&&active_at_death(x,binding.second,source(death))){const auto& sat=satisfied(x,binding.first);const auto* result=outcome(g,sat.requirement_id,death,true,binding.first);if(!result)fail("CareHolderDeathOutcomeMissing",binding.first);checked_outcome(x,*result,sat.requirement_id,death);}
 }
 // A Life4 receipt cannot be reclassified as generic RequiredCapacityLost.
 for(const auto& pair:g.goals.temporal_events.rows())if(const auto* lost=std::get_if<CareCoverageLostEvent>(&pair.second.body))if(const auto* why=std::get_if<CareRequiredCapacityLost>(&lost->cause)){
  const auto* receipt=take(read_character_condition_receipt_provenance(body,why->condition_receipt_ref));if(receipt->life_transition_ref)fail("CareHolderDeathHasPriority",pair.first);
 }
 return common_success();
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
}
