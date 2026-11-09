#include "dynamic_session_random_history.hpp"
#include "dynamic_session.hpp"
#include "dynamic_effect_source.hpp"
#include "resident/kernel/session_rng_initialization.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include <algorithm>
namespace hanmo::resident {using namespace movement_detail;
namespace {
SourceRef session_source(const StableId& id){return {"military","dynamic_session_state",id};}
SourceRef group_source(const StableId& id){return {"military","dynamic_effect_group",id};}
void append_original_slots(DynamicMilitarySessionRandom& current,
 const std::vector<DynamicMilitaryRandomSlot>& slots){
 auto after=current;
 if(!slots.empty()){
  const auto& last=slots.back();after.current_state=last.after_state;
  after.draw_ordinal=last.after_draw_ordinal;after.last_commit_source_ref=last.effect_ref;
 }
 // The one existing implementation verifies every original PCG range call,
 // stream identity, exact ordinal adjacency and the complete final state.
 take(validate_dynamic_military_random_chain(current,slots,after));
 current=std::move(after);
}
DynamicMilitarySessionRandom fold(const CommonStore& common,const MilitaryExactState& exact,
 const StableId& session,Integer generation,std::size_t exclusive_end,
 const WorldMoment& now,ExactArithmeticBudget b){
 if(session.empty()||generation<=0)fail("DynamicMilitaryRandomSessionRequired",session);
 take(encode_world_moment(now,b));
 const auto* actual=find_dynamic_military_session_current_row(exact,session,generation);
 if(!actual||actual->session_ref!=session||actual->generation!=generation)
  fail("DynamicMilitaryRandomSessionRequired",session);
 const auto& order=common.state().facts.order();
 if(exclusive_end>order.size())fail("DynamicMilitaryRandomOriginalHeadOrderMismatch",session);
 // Reverse coverage: a retained row cannot disappear merely because its head
 // was removed from Common. No deep row/vector is copied to make this index.
 for(const auto& row:exact.session_state_sources.rows())if(row.second.session_ref==session){
  if(row.first!=row.second.id||row.second.generation!=generation)
   fail("DynamicMilitaryRandomSessionSourceMismatch",row.first);
  (void)take(read_dynamic_military_fact_position(common,session_source(row.first)));
 }
 for(const auto& row:exact.effect_groups.rows())if(row.second.session_ref==session){
  if(row.first!=row.second.id)fail("DynamicMilitaryRandomEffectSourceMismatch",row.first);
  (void)take(read_dynamic_military_fact_position(common,group_source(row.first)));
 }
 std::optional<DynamicMilitarySessionRandom> current;
 const WorldMoment* previous_at=nullptr;
 auto at=[&](const WorldMoment& value,const StableId& id){
  take(encode_world_moment(value,b));
  if(take(compare_world_moments(value,now,b))>0||
     (previous_at&&take(compare_world_moments(value,*previous_at,b))<0))
   fail("DynamicMilitaryRandomOriginalTimeOrderMismatch",id);
  previous_at=&value;
 };
 for(std::size_t i=0;i<exclusive_end;++i){
  const auto* header=common.state().facts.find(order[i]);
  if(!header||header->id!=order[i])fail("DynamicMilitaryRandomOriginalHeadOrderMismatch",order[i]);
  if(header->domain!="military")continue;
  if(header->kind=="dynamic_session_state"){
   const auto* source=exact.session_state_sources.find(header->id);
   if(!source||source->id!=header->id)fail("DynamicMilitaryRandomOriginalPayloadRequired",header->id);
   if(source->session_ref!=session)continue;
   if(source->generation!=generation)fail("DynamicMilitaryRandomSessionSourceMismatch",source->id);
   at(source->effective_at,source->id);
   if(const auto* init=std::get_if<DynamicSessionInitialized>(&source->payload)){
    const auto ref=session_source(source->id);
    if(current||!same_source(actual->initialization_source_ref,ref)||
       !same_source(init->initial_rng_source_ref,ref)||
       init->definition_lock_ref!=actual->definition_lock_ref||
       !same_moment(source->effective_at,actual->opened_at,b))
     fail("DynamicMilitaryRandomInitializationSourceMismatch",source->id);
    const auto original=take(derive_original_session_rng_initialization(common.state(),
     session,generation,init->entry_or_contact_source_ref,ref));
    if(init->initial_rng_seed!=original.seed||init->initial_rng_state!=original.state||
       init->initial_draw_ordinal!=original.draw_ordinal)
     fail("DynamicMilitaryRandomInitializationSourceMismatch",source->id);
    current=DynamicMilitarySessionRandom{{session,generation,ref},original.algorithm_version,
     original.seed,original.state,original.draw_ordinal,ref};
   }else{
    if(!current)fail("DynamicMilitaryRandomRetainedInitializationRequired",source->id);
    if(const auto* decision=std::get_if<DynamicSessionDecisionCommitted>(&source->payload)){
     for(const auto& slot:decision->rng_slots)
      if(slot.stage!=DynamicMilitaryRandomStage::original_ai_decision||
         !same_source(slot.effect_ref,session_source(source->id)))
       fail("DynamicMilitaryDecisionRandomSourceMismatch",source->id);
     append_original_slots(*current,decision->rng_slots);
    }
   }
  }else if(header->kind=="dynamic_effect_group"){
   const auto* group=exact.effect_groups.find(header->id);
   if(!group||group->id!=header->id)fail("DynamicMilitaryRandomOriginalPayloadRequired",header->id);
   if(group->session_ref!=session)continue;
   if(!current)fail("DynamicMilitaryRandomRetainedInitializationRequired",group->id);
   if(take(read_dynamic_military_effect_group_generation(exact,*group))!=generation)
    fail("DynamicMilitaryRandomEffectGenerationMismatch",group->id);
   at(group->effective_at,group->id);
   for(const auto& slot:group->rng_slots){
    if(slot.stage==DynamicMilitaryRandomStage::original_ai_decision||
       (!same_source(slot.effect_ref,group_source(group->id))&&
        std::none_of(group->due_effect_refs.begin(),group->due_effect_refs.end(),
         [&](const auto& ref){return same_source(ref,slot.effect_ref);})))
     fail("DynamicMilitaryRandomEffectSlotOwnerMismatch",group->id);
   }
   append_original_slots(*current,group->rng_slots);
  }
 }
 if(!current)fail("DynamicMilitaryRandomRetainedInitializationRequired",session);
 return std::move(*current);
}
}
Outcome<DynamicMilitarySessionRandom> read_dynamic_military_random_before_source(
 const CommonStore& common,const MilitaryExactState& exact,const SourceRef& ref,
 const WorldMoment& now,ExactArithmeticBudget b){return guard<DynamicMilitarySessionRandom>([&]{
 const auto position=take(read_dynamic_military_fact_position(common,ref));
 if(ref.domain!="military")fail("DynamicMilitaryRandomCommitSourceRequired",ref.id);
 const StableId* session=nullptr;const WorldMoment* at=nullptr;Integer generation=0;
 if(ref.kind=="dynamic_session_state"){
  const auto* source=exact.session_state_sources.find(ref.id);
  if(!source||source->id!=ref.id||!std::holds_alternative<DynamicSessionDecisionCommitted>(source->payload))
   fail("DynamicMilitaryRandomCommitSourceRequired",ref.id);
  session=&source->session_ref;generation=source->generation;at=&source->effective_at;
 }else if(ref.kind=="dynamic_effect_group"){
  const auto* group=exact.effect_groups.find(ref.id);
  if(!group||group->id!=ref.id)fail("DynamicMilitaryRandomCommitSourceRequired",ref.id);
  session=&group->session_ref;generation=take(read_dynamic_military_effect_group_generation(exact,*group));at=&group->effective_at;
 }else fail("DynamicMilitaryRandomCommitSourceRequired",ref.id);
 if(take(compare_world_moments(*at,now,b))>0)fail("DynamicMilitaryFutureRandomCommit",ref.id);
 // Using the real boundary M also rejects a prior append that claims a later M.
 return fold(common,exact,*session,generation,position,*at,b);
});}
Status validate_retained_dynamic_military_random_history(const CommonStore& common,
 const MilitaryExactState& exact,const DynamicMilitarySessionCurrent2& current,
 const WorldMoment& now,ExactArithmeticBudget b){return status([&]{
 if(find_dynamic_military_session_current_row(exact,current.session_ref,current.generation)!=&current)
  fail("DynamicMilitaryRandomBorrowedCurrentRequired",current.session_ref);
 const auto expected=fold(common,exact,current.session_ref,current.generation,
  common.state().facts.order().size(),now,b);
 take(validate_dynamic_military_random_chain(expected,{},current.session_rng));
});}
}
