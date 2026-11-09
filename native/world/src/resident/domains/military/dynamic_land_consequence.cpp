#include "dynamic_land_consequence.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "dynamic_session.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include <algorithm>
#include <set>
#include <functional>
#include <string_view>
namespace hanmo::resident {using namespace movement_detail;namespace {
CheckpointMemoryCharge reserve_land_consequence(const DynamicMilitaryLandConsequenceBuildView& input){
 return CheckpointMemoryCharge(0);
}
CheckpointMemoryCharge reserve_land_application(const MomentOwnerSourceBuildView& battle,const MomentOwnerSourceBuildView& casualty){
 return CheckpointMemoryCharge(0);
}
std::vector<StableId> copy_consequence_ids(const CheckpointStringVectorBuildView& input){
 // The output owner's preceding lease covers this exact fresh final buffer.
 // No insertion grows it or relocates an existing owned string.
 std::vector<StableId> out;out.reserve(input.values.size());
 for(const auto id:input.values)out.emplace_back(id.data(),id.size());
 return out;
}
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& row:rows)if(row.id==id)return &row;return nullptr;}
template<class T>T* find(std::vector<T>& rows,const StableId& id){for(auto& row:rows)if(row.id==id)return &row;return nullptr;}
bool has(const std::vector<StableId>& rows,const StableId& id){return std::find(rows.begin(),rows.end(),id)!=rows.end();}
void header(const CommonStore& common,const SourceRef& ref){const auto* h=common.state().facts.find(ref.id);if(!h||h->id!=ref.id||h->domain!=ref.domain||h->kind!=ref.kind)fail("DynamicMilitaryActualSourceHeadRequired",ref.id);}
void sorted_ids(const std::vector<StableId>& ids,const char* code){const StableId* last=nullptr;for(const auto& id:ids){if(id.empty()||(last&&id<=*last))fail(code,id);last=&id;}}
bool same_roster(const MilitaryUnitRecord& a,const MilitaryUnitRecord& b){return a.id==b.id&&a.army_id==b.army_id&&a.commander_actor_id==b.commander_actor_id&&a.troop_type_id==b.troop_type_id&&a.member_ids==b.member_ids&&a.fit==b.fit&&a.wounded==b.wounded&&a.dead==b.dead&&a.captured==b.captured&&a.deserted==b.deserted&&a.captors==b.captors&&a.morale==b.morale;}
bool same_receipt(const MilitaryRosterReceipt& a,const MilitaryRosterReceipt& b){return a.unit_id==b.unit_id&&a.fit==b.fit&&a.wounded==b.wounded&&a.dead==b.dead&&a.captured==b.captured&&a.deserted==b.deserted&&a.member_ids==b.member_ids&&a.morale==b.morale&&a.captors==b.captors;}
bool same_change(const MilitaryActorChange& a,const MilitaryActorChange& b){return a.actor_id==b.actor_id&&a.status==b.status&&a.captor_army_id==b.captor_army_id&&a.garrison_id==b.garrison_id&&a.army_id==b.army_id;}
void partition(const MilitaryUnitRecord& unit){using PartitionIds=std::set<std::string_view,std::less<std::string_view>,CheckpointAllocator<std::string_view>>;PartitionIds actual,expected(unit.member_ids.begin(),unit.member_ids.end());if(expected.size()!=unit.member_ids.size())fail("DynamicMilitaryOriginalRosterDuplicate",unit.id);for(const auto* group:{&unit.fit,&unit.wounded,&unit.dead,&unit.captured,&unit.deserted})for(const auto& id:*group)if(id.empty()||!actual.insert(id).second)fail("DynamicMilitaryOriginalRosterDuplicate",id);if(actual!=expected||unit.morale<0||unit.morale>100)fail("DynamicMilitaryOriginalRosterPartition",unit.id);}
DynamicMilitaryLandConsequenceRead read_group(const CommonStore& common,const MilitaryState& military,const MilitaryExactState& exact,const DynamicMilitaryLandConsequenceContext& c,const SourceRef& ref){
 const auto now=take(c.clock.read_current_moment());auto group=take(c.effects.read_actual_land_consequence(ref));
 if(group.source_schema!=2||!same_source(group.effect_group_source,ref)||ref.domain!="military"||ref.kind!="dynamic_effect_group"||group.session_ref.empty()||group.generation<1||group.original_casualty_rule_ref.empty()||!source_valid(group.actual_contact_source)||take(compare_world_moments(group.started_at,group.effective_at,c.budget))>0||take(compare_world_moments(group.effective_at,now,c.budget))>0)fail("DynamicMilitaryLandEffectGroupRequired",ref.id);
 header(common,ref);header(common,group.actual_contact_source);sorted_ids(group.unit_ids,"DynamicMilitaryEffectUnitOrder");sorted_ids(group.army_ids,"DynamicMilitaryEffectArmyOrder");if(group.unit_ids.empty()||group.army_ids.empty()||group.strike_snapshot.size()!=group.unit_ids.size())fail("DynamicMilitaryEffectRosterCoverage",ref.id);
 const auto* session=find_dynamic_military_session_current_row(exact,group.session_ref,group.generation);if(!session||!same_moment(session->opened_at,group.started_at,c.budget))fail("DynamicMilitaryEffectSessionRequired",ref.id);
 for(const auto& old:military.battles)if(old.id==group.session_ref)fail("DuplicateDynamicMilitarySessionCurrent",group.session_ref);
 for(std::size_t n=0;n<group.unit_ids.size();++n){if(group.strike_snapshot[n].unit_ref!=group.unit_ids[n]||std::none_of(session->unit_tactical_states.begin(),session->unit_tactical_states.end(),[&](const auto& u){return u.unit_ref==group.unit_ids[n];}))fail("DynamicMilitaryEffectRosterCoverage",group.unit_ids[n]);}
 take(validate_dynamic_land_member_allocations(group.strike_snapshot,group.complete_strikes,group.actual_supports,group.casualty_rule,group.merged));return group;
}
struct Expected {std::vector<MilitaryUnitRecord> before;DynamicMilitaryCasualtyFact2 casualty;};
Expected expected(const CommonStore& common,const MilitaryState& military,const MilitaryExactState& exact,const DynamicMilitaryLandConsequenceContext& c,const DynamicMilitaryLandConsequenceRead& group){
 auto history=take(read_military_rosters_before_exact(common,military,c.catalog,exact,group.unit_ids,group.effect_group_source,group.effective_at,take(c.clock.read_current_moment()),c.population,c.population_catalog,c.budget,c.formations,c.returns,c.periods));
 Expected out;out.before=std::move(history.units);out.casualty.session_ref=group.session_ref;out.casualty.effect_group_ref=group.effect_group_source.id;out.casualty.effective_at=group.effective_at;out.casualty.original_casualty_rule_ref=group.original_casualty_rule_ref;
 {
 using ConsequenceIds=std::set<std::string_view,std::less<std::string_view>,CheckpointAllocator<std::string_view>>;
 ConsequenceIds all_members,army_ids;
 for(const auto& id:group.unit_ids){const auto* before=find(out.before,id);if(!before)fail("DynamicMilitaryOriginalRosterRequired",id);partition(*before);army_ids.insert(before->army_id);const auto input=std::find_if(group.strike_snapshot.begin(),group.strike_snapshot.end(),[&](const auto& x){return x.unit_ref==id;});
  if(input==group.strike_snapshot.end()||input->original_fit_ids!=before->fit)fail("DynamicMilitaryStrikeRosterSourceMismatch",id);
  // Posture morale is already part of the group's original strike snapshot;
  // its source owner validates that finite A-layer delta before this sink.
  MilitaryRosterReceipt row;row.unit_id=id;row.fit=before->fit;row.wounded=before->wounded;row.dead=before->dead;row.captured=before->captured;row.deserted=before->deserted;row.member_ids=before->member_ids;row.morale=input->original_morale;row.captors=before->captors;
  for(const auto& member:before->member_ids)if(!all_members.insert(member).second)fail("DynamicMilitaryMemberInTwoUnits",member);
  for(const auto& allocation:group.merged.member_allocations)if(allocation.unit_ref==id){if(!has(row.fit,allocation.member_id)||(allocation.resulting_status!="wounded"&&allocation.resulting_status!="dead"))fail("DynamicMilitaryActualFitAllocationRequired",allocation.member_id);row.fit.erase(std::remove(row.fit.begin(),row.fit.end(),allocation.member_id),row.fit.end());(allocation.resulting_status=="dead"?row.dead:row.wounded).push_back(allocation.member_id);
   if(common.actor(allocation.member_id)){MilitaryActorChange change;change.actor_id=allocation.member_id;change.status=allocation.resulting_status;change.army_id=before->army_id;out.casualty.actor_changes.push_back(std::move(change));out.casualty.member_effect_links.push_back({allocation.member_id,Integer(out.casualty.actor_changes.size()-1),allocation.resulting_status,allocation.effect_ref});}
  }
  for(const auto& morale:group.merged.morale_changes)if(morale.unit_ref==id){if(morale.before_morale!=row.morale)fail("DynamicMilitaryMoraleSnapshotMismatch",id);row.morale=morale.after_morale;}
  for(const auto& morale:group.original_consequence_morale)if(morale.unit_ref==id){if(morale.before_morale!=row.morale||morale.after_morale<0||morale.after_morale>100||morale.delta!=morale.after_morale-morale.before_morale)fail("DynamicMilitaryOriginalConsequenceMoraleMismatch",id);row.morale=morale.after_morale;}
  std::sort(row.wounded.begin(),row.wounded.end());std::sort(row.dead.begin(),row.dead.end());out.casualty.roster_receipts.push_back(std::move(row));
 }
 // F commander exposure uses the same original before roster and actual group.
 // Enlisted commanders already have exactly one E member consequence; a separate
 // named commander is never inserted into any soldier classification.
 for(const auto& loss:group.commander_consequences){const auto* before=find(out.before,loss.unit_ref);if(!before||before->army_id!=loss.army_ref||before->commander_actor_id!=loss.actor_ref||loss.before_loss<0||loss.before_loss>1||loss.after_loss<=loss.before_loss||loss.after_loss>2||!same_source(loss.actual_effect_group,group.effect_group_source)||!common.actor(loss.actor_ref)||loss.actual_enlisted_member!=has(before->member_ids,loss.actor_ref))fail("DynamicMilitaryCommanderOriginalSourceMismatch",loss.actor_ref);
  const auto status=loss.after_loss==2?StableId("dead"):StableId("wounded");const auto actual=std::find_if(out.casualty.actor_changes.begin(),out.casualty.actor_changes.end(),[&](const auto& a){return a.actor_id==loss.actor_ref;});
  if(loss.actual_enlisted_member){if(actual==out.casualty.actor_changes.end()||actual->status!=status||actual->army_id!=loss.army_ref)fail("DynamicMilitaryEnlistedCommanderAllocationMismatch",loss.actor_ref);continue;}
  if(all_members.count(loss.actor_ref)||actual!=out.casualty.actor_changes.end())fail("DynamicMilitaryCommanderInOtherRoster",loss.actor_ref);
  MilitaryActorChange change;change.actor_id=loss.actor_ref;change.status=status;change.army_id=loss.army_ref;out.casualty.actor_changes.push_back(std::move(change));out.casualty.member_effect_links.push_back({loss.actor_ref,Integer(out.casualty.actor_changes.size()-1),status,group.effect_group_source});
 }
 // F keeps the actual E wound effect even when that same survivor is then
 // captured. The final roster has one classification; named consequences have
 // one ordinal per actual purpose, consumed in this original causal order.
 ConsequenceIds disposed;
 for(const auto& change:group.dispositions.member_dispositions){const auto* before=find(out.before,change.unit_ref);auto final=std::find_if(out.casualty.roster_receipts.begin(),out.casualty.roster_receipts.end(),[&](const auto& x){return x.unit_id==change.unit_ref;});if(!before||final==out.casualty.roster_receipts.end()||!disposed.insert(change.member_ref).second||!same_source(change.actual_effect_group,group.effect_group_source)||(change.before_status!="fit"&&change.before_status!="wounded")||(change.after_status!="captured"&&change.after_status!="deserted"))fail("DynamicMilitaryDispositionSourceMismatch",change.member_ref);
  auto& from=change.before_status=="fit"?final->fit:final->wounded;const auto actual=std::find(from.begin(),from.end(),change.member_ref);if(actual==from.end())fail("DynamicMilitaryDispositionBeforeMismatch",change.member_ref);from.erase(actual);
  if(change.after_status=="captured"){const auto* keeper=find(out.before,change.captor_unit_ref);const auto* own_army=find(military.armies,before->army_id);const auto* keeper_army=keeper?find(military.armies,keeper->army_id):nullptr;if(!keeper||keeper->army_id!=change.captor_army_ref||keeper->army_id==before->army_id||!own_army||!keeper_army||own_army->organization_id==keeper_army->organization_id||std::any_of(final->captors.begin(),final->captors.end(),[&](const auto& x){return x.first==change.member_ref;}))fail("MilitaryCaptorRequired",change.member_ref);final->captured.push_back(change.member_ref);final->captors.emplace_back(change.member_ref,change.captor_army_ref);}
  else{if(!change.captor_unit_ref.empty()||!change.captor_army_ref.empty())fail("UnexpectedMilitaryCaptor",change.member_ref);final->deserted.push_back(change.member_ref);}
  if(common.actor(change.member_ref)){MilitaryActorChange actual;actual.actor_id=change.member_ref;actual.status=change.after_status;actual.army_id=before->army_id;actual.captor_army_id=change.captor_army_ref;out.casualty.actor_changes.push_back(std::move(actual));out.casualty.member_effect_links.push_back({change.member_ref,Integer(out.casualty.actor_changes.size()-1),change.after_status,group.effect_group_source});}
 }
 for(auto& row:out.casualty.roster_receipts){std::sort(row.captured.begin(),row.captured.end());std::sort(row.deserted.begin(),row.deserted.end());}
 if(army_ids.size()!=group.army_ids.size()||!std::equal(army_ids.begin(),army_ids.end(),group.army_ids.begin(),[](std::string_view have,const StableId& wanted){return have==std::string_view(wanted);}))fail("DynamicMilitaryEffectArmyCoverage",group.effect_group_source.id);
 }
 return out;
}
void compare_saved(const DynamicMilitaryCasualtyFact2& saved,const DynamicMilitaryCasualtyFact2& value,ExactArithmeticBudget budget){if(saved.session_ref!=value.session_ref||saved.effect_group_ref!=value.effect_group_ref||saved.original_casualty_rule_ref!=value.original_casualty_rule_ref||!same_moment(saved.effective_at,value.effective_at,budget)||saved.roster_receipts.size()!=value.roster_receipts.size()||saved.actor_changes.size()!=value.actor_changes.size()||saved.member_effect_links.size()!=value.member_effect_links.size())fail("DynamicMilitaryCasualtySourceMismatch",saved.id);for(std::size_t n=0;n<saved.roster_receipts.size();++n)if(!same_receipt(saved.roster_receipts[n],value.roster_receipts[n]))fail("DynamicMilitaryCasualtyRosterMismatch",saved.id);for(std::size_t n=0;n<saved.actor_changes.size();++n){const auto& a=saved.member_effect_links[n];const auto& b=value.member_effect_links[n];if(!same_change(saved.actor_changes[n],value.actor_changes[n])||a.actor_id!=b.actor_id||a.effect_index!=b.effect_index||a.purpose!=b.purpose||!same_source(a.effect_source,b.effect_source))fail("DynamicMilitaryCasualtyActorSourceMismatch",saved.id);}}
const DynamicMilitaryBattleFact2& battle_for(const MilitaryExactState& exact,const DynamicMilitaryCasualtyFact2& casualty){const DynamicMilitaryBattleFact2* out=nullptr;for(const auto& row:exact.dynamic_battle_facts.rows())if(row.second.session_ref==casualty.session_ref&&row.second.effect_group_ref==casualty.effect_group_ref){if(out)fail("DynamicMilitaryDuplicateEffectResult",casualty.id);out=&row.second;}if(!out)fail("DynamicMilitaryBattleSourceRequired",casualty.id);return *out;}
DynamicMilitaryLandConsequenceApplication result(const MilitaryState& military,const MilitaryExactState& exact,const DynamicMilitaryCasualtyFact2& casualty,bool repeated,ExactArithmeticBudget b,const MilitaryCasualtyHistoryContext& history){
 const auto& battle=battle_for(exact,casualty);
 const auto* battle_head=history.common.state().facts.find(battle.id);
 const auto* casualty_head=history.common.state().facts.find(casualty.id);
 if(!battle_head||battle_head->id!=battle.id||battle_head->domain!="military"||battle_head->kind!="military_battle"||
    !casualty_head||casualty_head->id!=casualty.id||casualty_head->domain!="military"||casualty_head->kind!="military_casualties")
  fail("DynamicMilitaryActualSourceHeadRequired",casualty.id);
 DynamicMilitaryLandConsequenceApplication out(
  {{battle_head->domain,battle_head->kind,battle_head->id},2},
  {{casualty_head->domain,casualty_head->kind,casualty_head->id},2},repeated);
 out.actual_named_consequences.reserve(casualty.actor_changes.size());
 for(std::size_t n=0;n<casualty.actor_changes.size();++n)
  out.actual_named_consequences.push_back(take(read_military_actor_consequence(military,exact,out.casualty_source.source_ref,2,Integer(n),b,&history)));
 return out;
}
}
DynamicMilitaryLandConsequenceApplication::DynamicMilitaryLandConsequenceApplication(
 const MomentOwnerSourceBuildView& battle,const MomentOwnerSourceBuildView& casualty,bool repeated)
 :copied_fields(reserve_land_application(battle,casualty)),
 battle_source{{StableId(battle.source_ref.domain),StableId(battle.source_ref.kind),StableId(battle.source_ref.id)},battle.source_schema},
 casualty_source{{StableId(casualty.source_ref.domain),StableId(casualty.source_ref.kind),StableId(casualty.source_ref.id)},casualty.source_schema},
 already_applied(repeated){
 static_assert(std::is_nothrow_move_constructible_v<MomentOwnerSource>);
 static_assert(std::is_nothrow_move_constructible_v<CheckpointVector<MilitaryActorConsequence>>);
}
DynamicMilitaryLandConsequenceRead::DynamicMilitaryLandConsequenceRead(const DynamicMilitaryLandConsequenceBuildView& in)
 :copied_fields(reserve_land_consequence(in)),
 effect_group_source(in.effect_group_source),source_schema(in.source_schema),
 session_ref(checkpoint_fresh_string_copy(in.session_ref)),generation(in.generation),
 effective_at(in.effective_at),started_at(in.started_at),
 actual_contact_source(in.actual_contact_source),
 original_casualty_rule_ref(checkpoint_fresh_string_copy(in.original_casualty_rule_ref)),
 army_ids(copy_consequence_ids(in.army_ids)),unit_ids(copy_consequence_ids(in.unit_ids)),
 strike_snapshot(in.strike_snapshot),complete_strikes(in.complete_strikes),
 actual_supports(in.actual_supports),casualty_rule(in.casualty_rule),merged(in.merged),
 commander_consequences(in.commander_consequences),
 original_consequence_morale(in.original_consequence_morale),
 command_shock_causes(in.command_shock_causes),dispositions(in.dispositions){}
Status validate_dynamic_military_land_consequence(const CommonStore& common,const MilitaryState& military,const MilitaryExactState& exact,const DynamicMilitaryLandConsequenceContext& c,const SourceRef& ref){return status([&]{if(ref.domain!="military"||ref.kind!="military_casualties")fail("DynamicMilitaryCasualtySourceRequired",ref.id);const auto* saved=exact.dynamic_casualty_facts.find(ref.id);if(!saved||saved->id!=ref.id)fail("DynamicMilitaryCasualtySourceRequired",ref.id);header(common,ref);auto group=read_group(common,military,exact,c,{"military","dynamic_effect_group",saved->effect_group_ref});const auto value=expected(common,military,exact,c,group);compare_saved(*saved,value.casualty,c.budget);const auto& battle=battle_for(exact,*saved);header(common,{"military","military_battle",battle.id});if(battle.final||battle.result||battle.winner_side||battle.army_ids!=group.army_ids||battle.unit_ids!=group.unit_ids||!same_source(battle.source_ref,group.effect_group_source)||!same_source(battle.contact_source_ref,group.actual_contact_source)||!same_moment(battle.started_at,group.started_at,c.budget)||!same_moment(battle.effective_at,group.effective_at,c.budget))fail("DynamicMilitaryBattleEffectSourceMismatch",battle.id);});}
Outcome<DynamicMilitaryLandConsequenceApplication> apply_dynamic_military_land_consequences_in_candidate(CommonStore& common,MilitaryState& military,MilitaryExactState& exact,const DynamicMilitaryLandConsequenceContext& c,const SourceRef& ref){return guard<DynamicMilitaryLandConsequenceApplication>([&]{
 const MilitaryCasualtyHistoryContext history{common,take(c.clock.read_current_moment()),c.formations,c.returns};
 if(ref.domain!="military"||ref.kind!="dynamic_effect_group"||ref.id.empty())fail("DynamicMilitaryLandEffectGroupRequired",ref.id);
 const DynamicMilitaryCasualtyFact2* previous=nullptr;for(const auto& row:exact.dynamic_casualty_facts.rows())if(row.second.effect_group_ref==ref.id){if(previous)fail("DynamicMilitaryDuplicateEffectResult",ref.id);previous=&row.second;}if(previous){take(validate_dynamic_military_land_consequence(common,military,exact,c,{"military","military_casualties",previous->id}));return result(military,exact,*previous,true,c.budget,history);}
 auto group=read_group(common,military,exact,c,ref);if(!same_moment(group.effective_at,take(c.clock.read_current_moment()),c.budget))fail("DynamicMilitaryConsequencePhaseMismatch",ref.id);auto value=expected(common,military,exact,c,group);
 for(const auto& original:value.before){const auto* current=find(military.military_units,original.id);const auto* army=find(military.armies,original.army_id);if(!current||!same_roster(*current,original)||!army||army->status!="active"||!has(army->unit_ids,original.id))fail("DynamicMilitaryConsequenceOriginalCurrentMismatch",original.id);}
 DynamicMilitaryBattleFact2 battle;battle.id=take(common.next_id("military"));battle.session_ref=group.session_ref;battle.started_at=group.started_at;battle.effective_at=group.effective_at;battle.effect_group_ref=ref.id;battle.army_ids=group.army_ids;battle.unit_ids=group.unit_ids;battle.source_ref=ref;battle.contact_source_ref=group.actual_contact_source;
 value.casualty.id=take(common.next_id("military"));const auto casualty_id=value.casualty.id;take(common.append_fact({battle.id,"military_battle","military"}));take(common.append_fact({casualty_id,"military_casualties","military"}));const auto battle_id=battle.id;if(!exact.dynamic_battle_facts.insert(battle_id,std::move(battle))||!exact.dynamic_casualty_facts.insert(casualty_id,std::move(value.casualty)))fail("DynamicMilitaryDuplicateEffectResult",ref.id);
 const auto& casualty=*exact.dynamic_casualty_facts.find(casualty_id);for(const auto& row:casualty.roster_receipts){auto* unit=find(military.military_units,row.unit_id);unit->fit=row.fit;unit->wounded=row.wounded;unit->dead=row.dead;unit->captured=row.captured;unit->deserted=row.deserted;unit->member_ids=row.member_ids;unit->morale=row.morale;unit->captors=row.captors;}
 for(const auto& change:group.dispositions.member_dispositions)if(change.after_status=="captured"){auto* keeper=find(military.armies,change.captor_army_ref);if(!keeper||keeper->status!="active"||has(keeper->prisoner_ids,change.member_ref))fail("MilitaryCaptorRequired",change.member_ref);keeper->prisoner_ids.push_back(change.member_ref);}
 for(const auto& change:casualty.actor_changes)if(change.status=="dead"||change.status=="captured"||change.status=="deserted"){auto* army=find(military.armies,change.army_id);if(!army)fail("DynamicMilitaryCasualtyArmyRequired",change.army_id);army->member_actor_ids.erase(std::remove(army->member_actor_ids.begin(),army->member_actor_ids.end(),change.actor_id),army->member_actor_ids.end());}
 take(validate_dynamic_military_land_consequence(common,military,exact,c,{"military","military_casualties",casualty_id}));return result(military,exact,casualty,false,c.budget,history);
});}
}
