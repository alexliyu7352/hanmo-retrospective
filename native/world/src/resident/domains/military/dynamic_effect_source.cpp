#include "dynamic_effect_source.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include <algorithm>
#include <functional>
#include <string_view>
#include <set>
#include <tuple>
namespace hanmo::resident {using namespace movement_detail;namespace {
using Key=std::tuple<std::string_view,std::string_view,Integer>;
void head(const CommonStore& common,const SourceRef& ref){const auto* h=common.state().facts.find(ref.id);if(!h||h->id!=ref.id||h->domain!=ref.domain||h->kind!=ref.kind)fail("DynamicMilitaryActualEffectHeadRequired",ref.id);}
const DynamicBattleEffectGroup2& containing(const MilitaryExactState& exact,const SourceRef& ref,std::size_t& index){const DynamicBattleEffectGroup2* found=nullptr;for(const auto& row:exact.effect_groups.rows())for(std::size_t n=0;n<row.second.due_effect_refs.size();++n)if(same_source(row.second.due_effect_refs[n],ref)){if(found||row.first!=row.second.id)fail("DynamicMilitaryEffectInTwoGroups",ref.id);found=&row.second;index=n;}if(!found)fail("DynamicMilitaryActualEffectPayloadRequired",ref.id);return *found;}
const DynamicBattleEffectGroup2& original_unit_effect_group(
 const CommonStore& common,const MilitaryExactState& exact,const SourceRef& ref,
 const StableId& session,Integer generation,const StableId& unit,const WorldMoment& now,
 ExactArithmeticBudget b){
 const auto cut=take(read_dynamic_military_fact_position(common,ref));
 const auto* group=exact.effect_groups.find(ref.id);
 const auto* current=find_dynamic_military_session_current_row(exact,session,generation);
 if(!group||group->id!=ref.id||group->session_ref!=session||!current||current->session_ref!=session||current->generation!=generation||
    take(read_dynamic_military_effect_group_generation(exact,*group))!=generation||
    take(compare_world_moments(group->effective_at,now,b))>0)
  fail("DynamicMilitaryPointChangeSourceRequired",ref.id);
 const auto& initial_ref=current->initialization_source_ref;
 const auto* initial=exact.session_state_sources.find(initial_ref.id);
 const auto* entry=initial?std::get_if<DynamicSessionInitialized>(&initial->payload):nullptr;
 if(initial_ref.domain!="military"||initial_ref.kind!="dynamic_session_state"||
    !initial||initial->id!=initial_ref.id||initial->session_ref!=session||
    initial->generation!=generation||!entry||
    take(read_dynamic_military_fact_position(common,initial_ref))>=cut||
    take(compare_world_moments(initial->effective_at,group->effective_at,b))>0)
  fail("DynamicMilitaryPointInitializationRequired",unit);
 std::size_t membership=0;
 for(const auto& row:entry->unit_entry_rows)if(row.unit_ref==unit)++membership;
 if(membership!=1)fail("DynamicMilitaryPointOriginalUnitRequired",unit);
 return *group;
}
}
Outcome<std::size_t> read_dynamic_military_fact_position(const CommonStore& common,
 const SourceRef& ref){return guard<std::size_t>([&]{
 if(ref.domain!="military"||(ref.kind!="dynamic_effect"&&ref.kind!="dynamic_effect_group"&&
    ref.kind!="dynamic_session_state"&&ref.kind!="local_motion_edge_completed"&&
    ref.kind!="local_motion_contact"&&ref.kind!="tactical_intent_accepted"&&
    ref.kind!="tactical_formation_created"&&ref.kind!="tactical_mission_adopted"))fail("DynamicMilitaryActualSourceKindRequired",ref.id);
 // Domain owns the finite kind gate; the sole Common owner authenticates the
 // actual append position. No ID suffix or second generic ordering scanner.
 return take(read_actual_common_fact_position(common,ref));
});}
Outcome<Integer> read_dynamic_military_effect_group_generation(const MilitaryExactState& exact,
 const DynamicBattleEffectGroup2& group){return guard<Integer>([&]{
 if(exact.effect_groups.find(group.id)!=&group||group.once_keys.empty()||
    group.once_keys.size()!=group.due_effect_refs.size())
  fail("DynamicMilitaryRandomEffectActionCoverageRequired",group.id);
 Integer generation=0;
 for(const auto& key:group.once_keys){
  const auto* action=exact.actions.find(key.action_ref);
  if(!action||action->id!=key.action_ref||action->session_ref!=group.session_ref||
     key.session_ref!=group.session_ref||action->generation<=0||
     key.effect_ordinal<0||key.effect_ordinal>1||
     (generation&&generation!=action->generation))
   fail("DynamicMilitaryRandomEffectGenerationMismatch",group.id);
  generation=action->generation;
 }
 return generation;
});}
Outcome<const DynamicTacticPointChange2*> read_dynamic_military_tactic_point_change(
 const CommonStore& common,const MilitaryExactState& exact,const SourceRef& ref,Integer schema,
 const StableId& session,Integer generation,const StableId& unit,const WorldMoment& now,
 ExactArithmeticBudget b){return guard<const DynamicTacticPointChange2*>([&]{
 if(schema!=2||ref.domain!="military"||ref.kind!="dynamic_effect_group"||unit.empty())
  fail("DynamicMilitaryPointChangeSourceRequired",ref.id);
 const auto& group=original_unit_effect_group(common,exact,ref,session,generation,unit,now,b);
 const DynamicTacticPointChange2* found=nullptr;
 for(const auto& value:group.posture_effects)
  if(const auto* change=std::get_if<DynamicTacticPointChange2>(&value);change&&change->unit_ref==unit){
   if(found)fail("DynamicMilitaryDuplicatePointChange",unit);found=change;
  }
 if(!found)fail("DynamicMilitaryPointChangeRequired",unit);
 const auto expected=take(clamp_dynamic_military_bounded_delta(found->before_points,found->delta,found->cap));
 if(found->after_points!=expected)fail("DynamicMilitaryPointClampMismatch",unit);
 return found;
});}
Outcome<const DynamicLandStatusChange*> read_dynamic_military_retained_status_source(
 const CommonStore& common,const MilitaryExactState& exact,const SourceRef& retained,Integer schema,
 const StableId& session,Integer generation,const StableId& unit,DynamicMilitaryStatusKind kind,
 const WorldMoment& now,ExactArithmeticBudget b){return guard<const DynamicLandStatusChange*>([&]{
 if(schema!=2||retained.domain!="military"||retained.kind!="dynamic_effect"||unit.empty()||
    kind<DynamicMilitaryStatusKind::inspired||kind>DynamicMilitaryStatusKind::exposed)
  fail("DynamicMilitaryRetainedStatusSourceRequired",retained.id);
 const auto activation_cut=take(read_dynamic_military_fact_position(common,retained));
 std::size_t ordinal=0;const auto& actual=containing(exact,retained,ordinal);
 const SourceRef group_ref{"military","dynamic_effect_group",actual.id};
 const auto& group=original_unit_effect_group(common,exact,group_ref,session,generation,unit,now,b);
 // The existing producer appends the before-effects group cut, then its real
 // selected effect heads. Neither a future prediction nor another group's row
 // can supply this retained status.
 if(take(read_dynamic_military_fact_position(common,group_ref))>=activation_cut||
    ordinal>=group.once_keys.size())fail("DynamicMilitaryStatusActualActivationOrderMismatch",retained.id);
 const DynamicLandStatusChange* found=nullptr;
 for(const auto& change:group.status_changes)
  if(change.unit_ref==unit&&change.kind==kind&&same_source(change.actual_activation_ref,retained)&&
     same_source(change.after.retained_activation_ref,retained)){
   if(found)fail("DynamicMilitaryDuplicateRetainedStatusSource",retained.id);
   found=&change;
  }
 if(!found||found->after.kind!=kind||!same_moment(found->after.valid_from,group.effective_at,b)||
    take(compare_world_moments(found->after.valid_from,found->after.expires_at,b))>=0)
  fail("DynamicMilitaryRetainedStatusPayloadMismatch",retained.id);
 // Duration, target eligibility, refresh priority and the latest-current
 // coverage remain the original professional effect/session read's job. This
 // borrowed read proves the exact saved source; it never reactivates a status.
 return found;
});}
Outcome<DynamicMilitaryPostureOriginalSources> read_dynamic_military_posture_original_sources(
 const CommonStore& common,const MilitaryExactState& exact,const StableId& session,Integer generation,
 const StableId& unit,const DynamicMilitaryPosture& posture,const WorldMoment& now,
 ExactArithmeticBudget b){return guard<DynamicMilitaryPostureOriginalSources>([&]{
 const auto* activation=dynamic_military_posture_activation(posture);
 if(!activation||activation->domain!="military"||activation->kind!="dynamic_effect")
  fail("DynamicMilitaryOriginalPostureActivationRequired",unit);
 const auto activation_cut=take(read_dynamic_military_fact_position(common,*activation));
 std::size_t effect_index=0;const auto& owner=containing(exact,*activation,effect_index);
 auto posture_owner_source_memory=CheckpointMemoryCharge(0);
 const SourceRef owner_ref{"military","dynamic_effect_group",owner.id};
 const auto& group=original_unit_effect_group(common,exact,owner_ref,session,generation,unit,now,b);
 const auto group_cut=take(read_dynamic_military_fact_position(common,owner_ref));
 if(group_cut>=activation_cut||effect_index>=group.once_keys.size())
  fail("DynamicMilitaryPostureActualActivationOrderMismatch",activation->id);
 const auto& once=group.once_keys[effect_index];const auto* action=exact.actions.find(once.action_ref);
 if(!action||once.effect_ordinal!=0||action->unit_ref!=unit)
  fail("DynamicMilitaryPostureOriginalActionMismatch",activation->id);
 DynamicMilitaryPostureOriginalSources out;
 for(const auto& effect:group.posture_effects)
  if(const auto* applied=std::get_if<DynamicLandActivationResult>(&effect);
     applied&&same_source(applied->effect_ref,*activation)){
   if(out.actual_activation||applied->unit_ref!=unit||applied->action_ref!=action->id||
      applied->action_instance_ordinal!=action->action_instance_ordinal||applied->effect_ordinal!=0||
      applied->failure!=DynamicLandActivationFailure::none||!applied->installed_posture)
    fail("DynamicMilitaryOriginalPostureActivationMismatch",activation->id);
   out.actual_activation=applied;
  }
 if(!out.actual_activation||!dynamic_military_posture_activation_values_equal(
     *out.actual_activation->installed_posture,posture,b))
  fail("DynamicMilitaryOriginalPosturePayloadMismatch",activation->id);
 std::visit([&](const auto& value){using T=std::decay_t<decltype(value)>;
  if constexpr(!std::is_same_v<T,DynamicNoPosture>){
   const auto& from=[&]()->const WorldMoment&{if constexpr(std::is_same_v<T,DynamicSupportPosture>)return value.token.valid_from;else return value.valid_from;}();
   const auto& until=[&]()->const WorldMoment&{if constexpr(std::is_same_v<T,DynamicSupportPosture>)return value.token.expires_at;else return value.expires_at;}();
   if(!same_moment(from,group.effective_at,b)||take(compare_world_moments(from,until,b))>=0)
    fail("DynamicMilitaryPostureOriginalWindowMismatch",activation->id);
  }
 },posture);
 const auto* support=std::get_if<DynamicSupportPosture>(&posture);
 if(!support)return out;
 const auto& token=support->token;
 const auto* initial_support=std::get_if<DynamicSupportPosture>(&*out.actual_activation->installed_posture);
 if(!initial_support||initial_support->token.consumed_by_effect_ref||token.supporter_unit_ref!=unit||
    token.target_unit_ref.empty()||token.target_unit_ref==unit)
  fail("DynamicMilitaryOriginalSupportTokenMismatch",activation->id);
 // Reverse coverage is about this retained activation only, not a second
 // balance or a replay of all actions. A zero-member share still consumes it.
 for(const auto& row:exact.effect_groups.rows())for(const auto& effect:row.second.posture_effects)
  if(const auto* use=std::get_if<DynamicSupportConsumption2>(&effect);
     use&&same_source(use->activation_ref,*activation)){
   if(out.actual_support_consumption||row.first!=row.second.id||
      use->supporter_unit_ref!=unit||use->target_unit_ref!=token.target_unit_ref||
      use->actual_shared_members<0||!token.consumed_by_effect_ref||
      !same_source(*token.consumed_by_effect_ref,use->consumed_by_effect_ref))
    fail("DynamicMilitarySupportConsumptionOriginalMismatch",activation->id);
   auto posture_consumed_source_memory=CheckpointMemoryCharge(0);
   const SourceRef consumed_group_ref{"military","dynamic_effect_group",row.first};
   const auto& consumed=original_unit_effect_group(common,exact,consumed_group_ref,
       session,generation,unit,now,b);
   if(take(read_dynamic_military_fact_position(common,consumed_group_ref))<group_cut||
      take(compare_world_moments(consumed.effective_at,token.valid_from,b))<0||
      take(compare_world_moments(consumed.effective_at,token.expires_at,b))>=0||
      use->consumed_by_effect_ref.domain!="military"||use->consumed_by_effect_ref.kind!="dynamic_effect")
    fail("DynamicMilitarySupportConsumptionWindowMismatch",activation->id);
   std::size_t consumed_index=0;
   if(&containing(exact,use->consumed_by_effect_ref,consumed_index)!=&consumed||
      take(read_dynamic_military_fact_position(common,use->consumed_by_effect_ref))<=
        take(read_dynamic_military_fact_position(common,consumed_group_ref)))
    fail("DynamicMilitarySupportConsumptionEffectMismatch",activation->id);
   const DynamicBattleStrikeResult2* strike=nullptr;
   for(const auto& value:consumed.strike_results)
    if(same_source(value.original_result.effect_ref,use->consumed_by_effect_ref)){
     if(strike)fail("DynamicMilitaryDuplicateSupportStrike",activation->id);strike=&value;
    }
   if(!strike||!strike->consumed_support_activation_ref||
      !same_source(*strike->consumed_support_activation_ref,*activation)||
      !strike->supporter_unit_ref||*strike->supporter_unit_ref!=unit||
      strike->original_result.target_unit_ref!=token.target_unit_ref||
      strike->actual_support_count!=use->actual_shared_members||
      std::uint64_t(use->actual_shared_members)!=strike->support_member_refs.size())
    fail("DynamicMilitarySupportOriginalStrikeAllocationMismatch",activation->id);
   const auto& strike_key=consumed.once_keys[consumed_index];
   const auto* strike_action=exact.actions.find(strike_key.action_ref);
   if(!strike_action||strike->original_result.action_ref!=strike_action->id||
      strike->original_result.attacker_unit_ref!=strike_action->unit_ref||
      strike->original_result.action_instance_ordinal!=strike_action->action_instance_ordinal||
      strike->original_result.effect_ordinal!=strike_key.effect_ordinal)
    fail("DynamicMilitarySupportOriginalStrikeActionMismatch",activation->id);
   for(const auto& member_id:strike->support_member_refs){
    if(member_id.empty()||std::count(strike->support_member_refs.begin(),strike->support_member_refs.end(),member_id)!=1||
       std::count_if(consumed.member_allocations.begin(),consumed.member_allocations.end(),[&](const auto& member){
        return member.support_share&&member.member_id==member_id&&
          same_source(member.effect_ref,use->consumed_by_effect_ref);
       })!=1)fail("DynamicMilitarySupportOriginalMemberCoverageMismatch",activation->id);
   }
   std::size_t actual_members=0;
   for(const auto& member:consumed.member_allocations)
    if(member.support_share&&same_source(member.effect_ref,use->consumed_by_effect_ref)){
     if(member.unit_ref!=unit||member.direct_target_unit_ref!=token.target_unit_ref||
        std::count(strike->support_member_refs.begin(),strike->support_member_refs.end(),member.member_id)!=1)
      fail("DynamicMilitarySupportOriginalMemberMismatch",activation->id);
     ++actual_members;
    }
   if(actual_members!=strike->support_member_refs.size())
    fail("DynamicMilitarySupportOriginalMemberCoverageMismatch",activation->id);
   out.actual_support_consumption=use;
  }
 if(bool(token.consumed_by_effect_ref)!=bool(out.actual_support_consumption))
  fail("DynamicMilitarySupportActualConsumptionRequired",activation->id);
 return out;
});}
CheckpointMemoryCharge DynamicMilitaryActualEffectRead::reserve_default(){


    return CheckpointMemoryCharge(0);
}
CheckpointMemoryCharge DynamicMilitaryActualEffectRead::reserve(const DynamicMilitaryActualEffectReadBuildView& value){


    return CheckpointMemoryCharge(0);
}
CheckpointMemoryCharge DynamicMilitaryActualEffectRead::reserve(const DynamicMilitaryActualEffectRead& value){


    return CheckpointMemoryCharge(0);
}
DynamicMilitaryActualEffectRead::DynamicMilitaryActualEffectRead()
    :copied_fields(reserve_default()){}
DynamicMilitaryActualEffectRead::DynamicMilitaryActualEffectRead(const DynamicMilitaryActualEffectReadBuildView& value)
    :copied_fields(reserve(value)),
     source{{StableId(value.source.source_ref.domain),StableId(value.source.source_ref.kind),StableId(value.source.source_ref.id)},value.source.source_schema},
     action_ref(value.action_ref),effective_at(value.effective_at),use(value.use){}
DynamicMilitaryActualEffectRead::DynamicMilitaryActualEffectRead(const DynamicMilitaryActualEffectRead& value)
    :copied_fields(reserve(value)),source(value.source),action_ref(value.action_ref),
     effective_at(value.effective_at),use(value.use){}
Outcome<DynamicMilitaryActualEffectRead> read_dynamic_military_effect_source(const CommonStore& common,const MilitaryExactState& exact,const DynamicMilitaryActionCatalog2& catalog,const SourceRef& source,Integer source_schema,ExactArithmeticBudget b){return guard<DynamicMilitaryActualEffectRead>([&]{
 const auto& ref=source;if(source_schema!=2||ref.domain!="military"||ref.kind!="dynamic_effect")fail("DynamicMilitaryActualEffect2Required",ref.id);head(common,ref);std::size_t index=0;const auto& group=containing(exact,ref,index);{


 auto effect_group_source_memory=CheckpointMemoryCharge(0);
 const SourceRef effect_group_source{"military","dynamic_effect_group",group.id};
 head(common,effect_group_source);
}
 if(group.due_effect_refs.size()!=group.once_keys.size())fail("DynamicMilitaryEffectOnceCoverage",group.id);const auto& key=group.once_keys[index];const auto* action=exact.actions.find(key.action_ref);const auto* timing=action?catalog.timings.find(action->timing_definition_ref):nullptr;
 if(!action||action->id!=key.action_ref||action->session_ref!=group.session_ref||key.session_ref!=group.session_ref||!timing||timing->version!=action->timing_definition_version||key.effect_ordinal<0||key.effect_ordinal>1||take(compare_world_moments(action->accepted_at,group.effective_at,b))>=0)fail("DynamicMilitaryEffectActionBindingRequired",ref.id);
 const DynamicLandActivationResult* activation=nullptr;const DynamicLandStrikeResult* strike=nullptr;
 for(const auto& value:group.posture_effects)if(const auto* row=std::get_if<DynamicLandActivationResult>(&value);row&&same_source(row->effect_ref,ref)){if(activation)fail("DuplicateDynamicMilitaryActivation",ref.id);activation=row;}
 for(const auto& row:group.strike_results)if(same_source(row.original_result.effect_ref,ref)){if(strike)fail("DuplicateDynamicMilitaryStrike",ref.id);strike=&row.original_result;}
 if(activation&&(activation->action_ref!=action->id||activation->unit_ref!=action->unit_ref||activation->action_instance_ordinal!=action->action_instance_ordinal||activation->effect_ordinal!=key.effect_ordinal))fail("DynamicMilitaryActivationBindingMismatch",ref.id);
 if(strike&&(strike->action_ref!=action->id||strike->attacker_unit_ref!=action->unit_ref||strike->action_instance_ordinal!=action->action_instance_ordinal||strike->effect_ordinal!=key.effect_ordinal))fail("DynamicMilitaryStrikeBindingMismatch",ref.id);
 DynamicMilitaryActualEffectRead out(DynamicMilitaryActualEffectReadBuildView{{{source.domain,source.kind,source.id},source_schema},action->id,group.effective_at,DynamicTacticalEffectUse::activation});
 if(key.effect_ordinal==1){const auto* release=std::get_if<DynamicTacticalReleased>(&action->effect_state);if(timing->commit_kind!=DynamicMilitaryCommitKind::projectile_release||!timing->flight_duration||!release||activation||!strike||!same_moment(release->impact_due_at,group.effective_at,b)||!same_moment(take(add_world_duration(release->released_at,*timing->flight_duration,b)),group.effective_at,b)||take(compare_world_moments(release->released_at,group.effective_at,b))>=0)fail("DynamicMilitaryActualReleaseBeforeImpactRequired",ref.id);head(common,release->release_source_ref);std::size_t release_index=0;const auto& release_group=containing(exact,release->release_source_ref,release_index);if(release_group.once_keys.size()!=release_group.due_effect_refs.size()||release_group.once_keys[release_index].action_ref!=action->id||release_group.once_keys[release_index].effect_ordinal!=0||!same_moment(release_group.effective_at,release->released_at,b))fail("DynamicMilitaryActualReleaseBeforeImpactRequired",ref.id);out.use=DynamicTacticalEffectUse::projectile_impact;
 }else{if(!activation)fail("DynamicMilitaryActualActivationRequired",ref.id);if(timing->commit_kind==DynamicMilitaryCommitKind::projectile_release){if(strike||!timing->flight_duration)fail("DynamicMilitaryReleaseCannotApplyStrike",ref.id);out.use=DynamicTacticalEffectUse::projectile_release;}else if(timing->commit_kind!=DynamicMilitaryCommitKind::at_effect)fail("DynamicMilitaryCommitKindRequired",ref.id);}
 return out;
});}
Status validate_dynamic_military_effect_action_coverage(const CommonStore& common,const MilitaryExactState& exact,const DynamicMilitaryActionCatalog2& catalog,ExactArithmeticBudget b){return status([&]{
 std::set<Key,std::less<Key>,CheckpointAllocator<Key>> once;
 for(const auto& row:exact.effect_groups.rows()){const auto& group=row.second;if(group.due_effect_refs.size()!=group.once_keys.size())fail("DynamicMilitaryEffectOnceCoverage",row.first);for(std::size_t n=0;n<group.due_effect_refs.size();++n){const auto& k=group.once_keys[n];if(!once.emplace(k.session_ref,k.action_ref,k.effect_ordinal).second)fail("DynamicMilitaryEffectAlreadyApplied",k.action_ref);const auto source=take(read_dynamic_military_effect_source(common,exact,catalog,group.due_effect_refs[n],2,b));const auto& action=*exact.actions.find(source.action_ref);if(source.use==DynamicTacticalEffectUse::activation){const auto* applied=std::get_if<DynamicTacticalApplied>(&action.effect_state);if(!applied||!same_source(applied->effect_source_ref,source.source.source_ref)||!same_moment(applied->effective_at,source.effective_at,b))fail("DynamicMilitaryActionEffectAdoptionRequired",source.action_ref);}else{const auto* released=std::get_if<DynamicTacticalReleased>(&action.effect_state);if(!released||(source.use==DynamicTacticalEffectUse::projectile_release?(!same_source(released->release_source_ref,source.source.source_ref)||!same_moment(released->released_at,source.effective_at,b)):(!released->impact_source_ref||!same_source(*released->impact_source_ref,source.source.source_ref))))fail("DynamicMilitaryActionEffectAdoptionRequired",source.action_ref);}}}
 for(const auto& row:exact.actions.rows()){const auto& action=row.second;if(const auto* applied=std::get_if<DynamicTacticalApplied>(&action.effect_state))take(read_dynamic_military_effect_source(common,exact,catalog,applied->effect_source_ref,2,b));if(const auto* released=std::get_if<DynamicTacticalReleased>(&action.effect_state)){take(read_dynamic_military_effect_source(common,exact,catalog,released->release_source_ref,2,b));if(released->impact_source_ref)take(read_dynamic_military_effect_source(common,exact,catalog,*released->impact_source_ref,2,b));}}
});}
}
