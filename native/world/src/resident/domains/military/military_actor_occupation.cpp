// 精确Common事实顺序消费既有名册和战斗来源；不维护第二份人物状态或军籍。
#include "military_actor_occupation.hpp"
#include "military_roster_history.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <type_traits>
#include "military_exact_sources.hpp"
#include "military_active_army_read.hpp"
#include "military_return_source.hpp"
#include "military_formation_source.hpp"
#include "naval_casualties.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include <charconv>
#include "naval_battle_state.hpp"
#include "military_commander.hpp"
#include "military_siege_occupation.hpp"
#include <algorithm>
#include <map>
#include <limits>
#include <set>
namespace hanmo::resident {
namespace {
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& row:rows)if(row.id==id)return &row;return nullptr;}
bool has(const std::vector<StableId>& rows,const StableId& id){return std::find(rows.begin(),rows.end(),id)!=rows.end();}
template<class T>Outcome<T> fail(const StableId& code,const StableId& id={}){return {false,{},common_failure(code,id).error};}
// 边界来自真实登记顺序，不把分配号当完成顺序，也不把同tick全部归为过去。
struct Boundary {
 const CommonStore& common;Integer tick,observed_limit;bool actual_prefix=false;Outcome<std::size_t> end;
 Outcome<std::size_t> position(const StableId& id)const{
  const auto* head=common.state().facts.find(id);
  if(!head)return fail<std::size_t>("MilitaryOccupationSourceMismatch",id);
  return read_actual_common_fact_position(common,*head);
 }
 Boundary(const CommonStore& c,Integer t,const StableId& id,Integer limit,const CommonActualFactPrefix* prefix=nullptr):common(c),tick(t),observed_limit(limit),actual_prefix(prefix!=nullptr),end(prefix?read_actual_common_fact_prefix(c,*prefix):(id.empty()?Outcome<std::size_t>{true,c.state().facts.order().size(),{}}:position(id))){}
 Outcome<bool> before(const StableId& id,Integer at,const StableId& kind,const StableId& domain)const{
  if(!end.ok)return {false,{},end.error};
  const auto* header=common.state().facts.find(id);auto p=position(id);
  if(at<0||at>observed_limit||!header||header->id!=id||header->kind!=kind||header->domain!=domain||!p.ok)return fail<bool>("MilitaryOccupationSourceMismatch",id);
  // Integer-only sources retain their legal/nonfuture bound. In an actual
  // prefix, only Common append position establishes occurrence membership.
  if(actual_prefix)return {true,p.value<end.value,{}};
  if((at<tick&&p.value>=end.value)||(at>tick&&p.value<end.value))return fail<bool>("MilitaryOccupationBoundaryMismatch",id);
  return {true,at<=tick&&p.value<end.value,{}};
 }
};
// Both original occupation and precise quiet history reduce the same native
// membership changes. This helper owns no saved roster or independent rules.
const StableId& member_id(const StableId& id){return id;}
const StableId& member_id(const StableId* id){return *id;}
template<class People>void replace_original_members(People& people,const std::vector<StableId>& original){
 if constexpr(std::is_same_v<typename People::value_type,const StableId*>){people.clear();people.reserve(original.size());for(const auto& id:original)people.push_back(&id);}
 else people=original;
}
template<class People>bool apply_original_member_change(const StableId& army,People& people,const MilitaryIntegrationFact* integration,const std::vector<MilitaryActorChange>* changes){
 bool relevant=false;
 if(integration){if(integration->recipient_army_id==army){replace_original_members(people,integration->recipient_members_after);relevant=true;}for(const auto& source:integration->source_armies)if(source.id==army){people.clear();relevant=true;}}
 else if(changes)for(const auto& row:*changes)if(row.army_id==army){relevant=true;if(row.status=="dead"||row.status=="captured"||row.status=="deserted")people.erase(std::remove_if(people.begin(),people.end(),[&](const auto& id){return member_id(id)==row.actor_id;}),people.end());}
 return relevant;
}
// 编军存在由已验证接敌来源保证；后续同tick整编和伤亡仍按实际头顺序应用。
Outcome<CheckpointVector<const StableId*>> member_refs_before(const CommonStore& c,const MilitaryState& m,const ArmyRecord& army,Integer tick,const StableId& boundary,Integer observed_limit,const MilitaryExactState* exact=nullptr,const WorldMoment* exact_now=nullptr,MilitaryRosterHistoryTrace* trace=nullptr,ExactArithmeticBudget budget={},const WorldMoment* exact_boundary=nullptr,const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr,const CommonActualFactPrefix* actual_prefix=nullptr,const MilitaryFieldHistoryRead* field=nullptr){
 const bool exact_order=exact_now!=nullptr;
 auto observed=exact_now?Outcome<WorldMoment>{true,*exact_now,{}}:normalize_world_moment(observed_limit,{},budget);if(!observed.ok)return {false,{},observed.error};
 const MilitaryCasualtyHistoryContext casualty_history{c,observed.value,formations,returns,actual_prefix?MilitaryCasualtyHistoryOrder::actual_common_prefix:MilitaryCasualtyHistoryOrder::legacy_serial};
 auto formation=read_original_military_formation(c.state(),m,army,observed.value,exact_order?formations:nullptr,budget);if(!formation.ok)return {false,{},formation.error};const auto& formed=formation.value;const auto* terms=formed.original_terms;
 // Schema 1 retains only the action-start head, not a completion occurrence.
 // Do not turn its normalized tick or Army allocation serial into exact proof.
 if(actual_prefix&&formed.completed_source.source_schema!=2)return fail<CheckpointVector<const StableId*>>("MilitaryActualFormationCompletionRequired",army.id);
 if(formed.completed_at.half_day_tick>tick)return fail<CheckpointVector<const StableId*>>("MilitaryOccupationFormationRequired",army.id);
 if(exact_boundary){auto prior=compare_world_moments(formed.completed_at,*exact_boundary,budget);if(!prior.ok)return {false,{},prior.error};if(prior.value>0)return fail<CheckpointVector<const StableId*>>("MilitaryOccupationFormationBoundaryMismatch",army.id);}
 Boundary at(c,tick,boundary,observed_limit,actual_prefix);if(actual_prefix&&!at.end.ok)return {false,{},at.end.error};std::size_t formation_order=0;CheckpointVector<const StableId*> people;replace_original_members(people,terms->member_actor_ids);
 struct Change {std::size_t order;const MilitaryIntegrationFact* integration=nullptr;const MilitaryCasualtyFact* casualty=nullptr;const DynamicMilitaryCasualtyFact2* exact_casualty=nullptr;};CheckpointVector<Change> changes;
 for(const auto& f:m.integration_facts){auto prior=at.before(f.id,f.tick,"military_integration","military");if(!prior.ok)return {false,{},prior.error};if(prior.value)changes.push_back({movement_detail::take(at.position(f.id)),&f,nullptr});}
 for(const auto& f:m.casualty_facts){auto prior=at.before(f.id,f.tick,"military_casualties","military");if(!prior.ok)return {false,{},prior.error};if(field){auto actual=read_selected_military_field_casualty_moment(c,m,f,field);if(!actual.ok)return {false,{},actual.error};if(actual.value){if(!exact_boundary)return fail<CheckpointVector<const StableId*>>("MilitaryFieldHistoryActualCutRequired",f.id);auto order=compare_world_moments(*actual.value,*exact_boundary,budget);if(!order.ok)return {false,{},order.error};if((prior.value&&order.value>0)||(!prior.value&&order.value<0))return fail<CheckpointVector<const StableId*>>("MilitaryExactHistoryBoundaryMomentMismatch",f.id);}}if(prior.value)changes.push_back({movement_detail::take(at.position(f.id)),nullptr,&f});}
 if(exact)for(const auto& entry:exact->dynamic_casualty_facts.rows()){const auto& f=entry.second;auto source_memory=CheckpointMemoryCharge(0);const SourceRef casualty_source{"military","military_casualties",f.id};auto actual=read_military_source_moment(m,*exact,casualty_source,2,budget,&casualty_history);if(!actual.ok)return {false,{},actual.error};if(!exact_now)return fail<CheckpointVector<const StableId*>>("MilitaryExactHistoryMomentRequired",f.id);auto within=compare_world_moments(actual.value,*exact_now,budget);if(!within.ok)return {false,{},within.error};if(within.value>0)return fail<CheckpointVector<const StableId*>>("MilitaryExactHistoryFutureSource",f.id);auto prior=at.before(f.id,actual.value.half_day_tick,"military_casualties","military");if(!prior.ok)return {false,{},prior.error};if(exact_boundary&&(actual_prefix||prior.value)){auto cutoff=compare_world_moments(actual.value,*exact_boundary,budget);if(!cutoff.ok)return {false,{},cutoff.error};if((prior.value&&cutoff.value>0)||(actual_prefix&&!prior.value&&cutoff.value<0))return fail<CheckpointVector<const StableId*>>("MilitaryExactHistoryBoundaryMomentMismatch",f.id);}if(prior.value)changes.push_back({movement_detail::take(at.position(f.id)),nullptr,nullptr,&f});}
 auto record=[&](const SourceRef& ref,Integer schema,const StableId& serial_id)->Status{if(!trace)return common_success();const auto pos=serial_id.rfind(':');Integer serial=0;if(pos==StableId::npos)return common_failure("MilitaryCompositionOriginalSerialRequired",serial_id);const auto text=std::string_view(serial_id).substr(pos+1);const auto parsed=std::from_chars(text.data(),text.data()+text.size(),serial);if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size()||serial<1||(text.size()>1&&text.front()=='0'))return common_failure("MilitaryCompositionOriginalSerialRequired",serial_id);trace->original_changes.push_back({ref,schema,serial});trace->composition_revision=std::max(trace->composition_revision,serial);return common_success();};
 if(exact_order){if(!actual_prefix&&!boundary.empty()){const auto pos=boundary.rfind(':');Integer cutoff=0;if(pos==StableId::npos)return fail<CheckpointVector<const StableId*>>("MilitaryOccupationFormationBoundaryMismatch",army.id);const auto tail=std::string_view(boundary).substr(pos+1);const auto parsed=std::from_chars(tail.data(),tail.data()+tail.size(),cutoff);if(parsed.ec!=std::errc{}||parsed.ptr!=tail.data()+tail.size()||cutoff<1||(tail.size()>1&&tail.front()=='0')||formed.original_completion_serial>=cutoff)return fail<CheckpointVector<const StableId*>>("MilitaryOccupationFormationBoundaryMismatch",army.id);}const auto& ref=formed.completed_source.source_ref;auto source=at.before(ref.id,formed.completed_at.half_day_tick,ref.kind,ref.domain);if(!source.ok)return {false,{},source.error};if(!source.value)return fail<CheckpointVector<const StableId*>>("MilitaryOccupationFormationRequired",army.id);if(actual_prefix){auto position=at.position(ref.id);if(!position.ok)return {false,{},position.error};formation_order=position.value;}}
 const auto& serial_id=formed.completed_source.source_schema==2?formed.completed_source.source_ref.id:army.id;
 auto recorded=record(formed.completed_source.source_ref,formed.completed_source.source_schema,serial_id);if(!recorded.ok)return {false,{},recorded.error};

 std::sort(changes.begin(),changes.end(),[](const auto& a,const auto& b){return a.order<b.order;});
 for(const auto& change:changes){const auto* rows=change.integration?nullptr:(change.casualty?&change.casualty->actor_changes:&change.exact_casualty->actor_changes);if(apply_original_member_change(army.id,people,change.integration,rows)){const auto& id=change.integration?change.integration->id:(change.casualty?change.casualty->id:change.exact_casualty->id);if(actual_prefix&&change.order<=formation_order)return fail<CheckpointVector<const StableId*>>("MilitaryActualMemberPredecessorMismatch",id);if(trace){auto result=record({"military",change.integration?"military_integration":"military_casualties",id},change.exact_casualty?2:1,id);if(!result.ok)return {false,{},result.error};}}}
 return {true,std::move(people),{}};
}
Outcome<std::vector<StableId>> members_before(const CommonStore& c,const MilitaryState& m,const ArmyRecord& army,Integer tick,const StableId& boundary,Integer observed_limit,const MilitaryExactState* exact=nullptr,const WorldMoment* exact_now=nullptr,MilitaryRosterHistoryTrace* trace=nullptr,ExactArithmeticBudget budget={},const WorldMoment* exact_boundary=nullptr,const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr){
 auto refs=member_refs_before(c,m,army,tick,boundary,observed_limit,exact,exact_now,trace,budget,exact_boundary,formations,returns);if(!refs.ok)return {false,{},refs.error};
 std::vector<StableId> out;out.reserve(refs.value.size());for(const auto* id:refs.value)out.push_back(*id);return {true,std::move(out),{}};
}
// 旧围城没有逐次进退事实；这里只筛出可能有关的原具名人员，不将该集合宣称为历史现值。
bool possible_army_member(const MilitaryState& m,const ArmyRecord& army,const StableId& actor,Integer tick){
 const auto* formed=find(m.action_facts,army.formation_source);const auto* terms=formed?std::get_if<ArmyFormationTerms>(&formed->terms):nullptr;
 if(formed&&formed->completed_tick<=tick&&terms&&has(terms->member_actor_ids,actor))return true;
 for(const auto& f:m.integration_facts)if(f.tick<=tick&&f.recipient_army_id==army.id&&has(f.recipient_members_after,actor))return true;
 return false;
}
// 显式陆战军官死亡与同次最终收据证明自动闭战；旧人工关闭仍没有可借的时间戳。
bool death_closed(const CommonStore& c,const MilitaryState& m,const MilitaryBattleRecord& battle){
 if(battle.phase!="closed"||battle.controller_actor_id.empty()||battle.round_receipt_ids.empty()||battle.round_receipt_ids.back()!=battle.settlement_receipt_id)return false;
 const auto* source=find(m.casualty_facts,battle.settlement_receipt_id);const auto* header=source?c.state().facts.find(source->id):nullptr;const auto* actor=c.actor(battle.controller_actor_id);if(!source||source->battle_id!=battle.id||source->tick!=battle.completed_tick||!header||header->kind!="military_casualties"||header->domain!="military"||!actor||actor->alive||actor->available||actor->hp!=0)return false;
 const BattleUnit* unit=nullptr;for(const auto& row:battle.units)if(row.commander_actor_id==actor->id&&military_commander_loss(row)==2){if(unit)return false;unit=&row;}if(!unit)return false;Integer count=0;for(const auto& change:source->actor_changes)if(change.actor_id==actor->id&&change.status=="dead"&&change.army_id==unit->army_id)++count;return count==1;
}
// Military8原确认的实际头可精确判定退出；此前旧无头状态继续走保守旧门。
Outcome<std::optional<bool>> manual_release_before(const MilitaryState& m,const MilitaryBattleRecord& b,const Boundary& at){const MilitaryBattleReleaseFact* source=nullptr;for(const auto& f:m.recovery.battle_releases)if(f.battle_id==b.id){if(source)return fail<std::optional<bool>>("MilitaryBattleReleaseRepeated",b.id);source=&f;}if(!source)return {true,std::nullopt,{}};if(b.phase!="closed"||source->actor_id!=b.controller_actor_id||source->settlement_receipt_id!=b.settlement_receipt_id||source->tick<b.completed_tick)return fail<std::optional<bool>>("MilitaryBattleReleaseSourceMismatch",source->id);auto before=at.before(source->id,source->tick,"military_battle_released","military");if(!before.ok)return {false,{},before.error};return {true,before.value,{}};}
// 真正归营须在军事会战已经关闭后才可启办；该永久原回执给出保守结束上界，不猜关闭时刻。
Outcome<bool> returned_after_battle(const CommonStore& c,const MilitaryState& m,const MilitaryBattleRecord& battle,const std::vector<StableId>& armies,const Boundary& at){
 if(armies.empty()||battle.phase!="closed")return {true,false,{}};const auto* settled=find(m.casualty_facts,battle.settlement_receipt_id);if(!settled)return fail<bool>("MilitaryOccupationSettlementRequired",battle.id);auto settlement=at.before(settled->id,settled->tick,"military_casualties","military");if(!settlement.ok)return settlement;if(!settlement.value)return {true,false,{}};
 for(const auto& army_id:armies){const auto* army=find(m.armies,army_id);const MilitaryReturnFact* receipt=nullptr;for(const auto& f:m.return_facts)if(f.army_id==army_id){if(receipt)return fail<bool>("MilitaryOccupationReturnAmbiguous",army_id);receipt=&f;}if(!receipt)return {true,false,{}};auto prior=at.before(receipt->id,receipt->tick,"military_return","military");if(!prior.ok)return prior;if(!prior.value)return {true,false,{}};
  const MilitaryLogisticsFact* action=nullptr;for(const auto& f:m.logistics_facts)if(f.result&&f.result->receipt_id==receipt->id){if(action)return fail<bool>("MilitaryOccupationReturnAmbiguous",receipt->id);action=&f;}
  if(!army||army->status!="closed"||!action||action->command!="army_return"||action->phase!="completed"||!action->success||!action->result->ok.value_or(false)||action->terms.army_id!=army_id||action->result->army_id!=army_id||action->start_tick<settled->tick||action->completed_tick!=receipt->tick||receipt->tick!=action->start_tick+1||receipt->unit_ids!=army->unit_ids)return fail<bool>("MilitaryOccupationReturnSourceRequired",receipt->id);
  auto started=at.before(action->id,action->start_tick,"military_action","military");if(!started.ok)return started;if(!started.value||movement_detail::take(at.position(settled->id))>=movement_detail::take(at.position(action->id))||movement_detail::take(at.position(action->id))>=movement_detail::take(at.position(receipt->id)))return fail<bool>("MilitaryOccupationReturnOrderMismatch",receipt->id);
  for(const auto& unit:battle.units)if(unit.army_id==army_id&&!has(receipt->unit_ids,unit.unit_id))return fail<bool>("MilitaryOccupationReturnSourceRequired",receipt->id);
 }
 return {true,true,{}};
}
// 仅一次读取已验证源；结果的来源集合稳定排序，不改变原表和引用。
Outcome<MilitaryActorOccupation> read(const CommonStore& c,const MilitaryState& m,const NavalState* naval,const StableId& actor,Integer tick,const StableId& boundary,bool current,Integer observed_limit){
 if(actor.empty()||!c.actor(actor))return fail<MilitaryActorOccupation>("UnknownActor",actor);
 if(tick<0||tick>observed_limit||(!current&&(boundary.empty()||!c.state().facts.find(boundary)||!has(c.state().facts.order(),boundary)))||(current&&tick!=observed_limit))return fail<MilitaryActorOccupation>("MilitaryOccupationBoundaryRequired",boundary);
 if(!naval)for(const auto& row:c.state().facts.rows())if(row.second.domain=="military_naval")return fail<MilitaryActorOccupation>("MilitaryOccupationNavalHistoryRequired",actor);
 Boundary at(c,tick,boundary,observed_limit);MilitaryActorOccupation out;out.actor_id=actor;out.observed_tick=tick;out.boundary_fact_id=boundary;
 bool recorded_custody=false;
 for(const auto& f:m.casualty_facts){auto prior=at.before(f.id,f.tick,"military_casualties","military");if(!prior.ok)return {false,{},prior.error};for(const auto& change:f.actor_changes)if(change.actor_id==actor&&change.status=="captured"){recorded_custody=true;if(prior.value)out.custody_source_ids.push_back(f.id);}}
 // 本代只有原俘获及移交，没有解俘命令；返营/整编不解除羁押。无源的初始羁押不能作空集。
 bool custody=false;for(const auto& army:m.armies)custody=custody||has(army.prisoner_ids,actor);for(const auto& camp:m.garrisons)custody=custody||has(camp.prisoner_ids,actor);
 if(custody&&!recorded_custody)return fail<MilitaryActorOccupation>("MilitaryOccupationCustodySourceRequired",actor);
 for(const auto& battle:m.battles){
  const auto* contact=find(m.contact_facts,battle.source_id);if(!contact)return fail<MilitaryActorOccupation>("MilitaryOccupationContactRequired",battle.id);
  auto started=at.before(contact->id,contact->tick,"military_contact","military");if(!started.ok)return {false,{},started.error};if(!started.value)continue;
  bool involved=has(battle.participant_actor_ids,actor);std::vector<StableId> involved_armies;
  for(const auto& id:battle.army_ids){const auto* army=find(m.armies,id);if(!army)return fail<MilitaryActorOccupation>("MilitaryOccupationArmyRequired",id);auto people=members_before(c,m,*army,contact->tick,contact->id,observed_limit);if(!people.ok)return {false,{},people.error};if(has(people.value,actor)){involved=true;involved_armies.push_back(id);}}
  if(!involved)continue;
  auto released=manual_release_before(m,battle,at);if(!released.ok)return {false,{},released.error};if(released.value){if(!*released.value)out.battle_source_ids.push_back(contact->id);continue;}
  auto returned=returned_after_battle(c,m,battle,involved_armies,at);if(!returned.ok)return {false,{},returned.error};if(returned.value)continue;
  if(current){if(battle.phase!="closed")out.battle_source_ids.push_back(contact->id);else if(!death_closed(c,m,battle)&&!battle.controller_actor_id.empty()&&!(battle.result&&has({"authority_lost","responsible_actor_lost","truce","cancelled","timeout"},battle.result->reason)))out.incomplete_source_ids.push_back(battle.id);continue;}
  if(battle.phase=="closed"){
   const auto* settled=find(m.casualty_facts,battle.settlement_receipt_id);if(!settled)return fail<MilitaryActorOccupation>("MilitaryOccupationSettlementRequired",battle.id);
   auto ended=at.before(settled->id,settled->tick,"military_casualties","military");if(!ended.ok)return {false,{},ended.error};
   if(ended.value){const bool automatic=death_closed(c,m,battle)||battle.controller_actor_id.empty()||(battle.result&&has({"authority_lost","responsible_actor_lost","truce","cancelled","timeout"},battle.result->reason));if(automatic)continue;return fail<MilitaryActorOccupation>("MilitaryOccupationHistoryIncomplete",battle.id);}
  }
  out.battle_source_ids.push_back(contact->id);
 }
 if(naval)for(const auto& battle:naval->battle.battles){
  const auto* contact=find(naval->battle.contacts,battle.contact_source_id);if(!contact)return fail<MilitaryActorOccupation>("MilitaryOccupationContactRequired",battle.frame.id);
  auto started=at.before(contact->id,contact->tick,"naval_contact","military_naval");if(!started.ok)return {false,{},started.error};if(!started.value)continue;
  bool involved=false;for(const auto* fleet_id:{&contact->first_fleet_id,&contact->second_fleet_id}){const auto* fleet=find(naval->fleets,*fleet_id);const auto* army=fleet?find(m.armies,fleet->army_id):nullptr;if(!army)return fail<MilitaryActorOccupation>("MilitaryOccupationArmyRequired",*fleet_id);auto people=members_before(c,m,*army,contact->tick,contact->id,observed_limit);if(!people.ok)return {false,{},people.error};involved=involved||has(people.value,actor);}
  if(!involved)continue;bool ended=false;for(const auto& f:naval->battle.continuations)if(f.battle_id==battle.frame.id&&f.after.phase=="closed"){auto prior=at.before(f.id,f.tick,"naval_continue","military_naval");if(!prior.ok)return {false,{},prior.error};ended=ended||prior.value;}
  if(!ended)out.battle_source_ids.push_back(contact->id);
 }
 for(const auto& siege:m.sieges){if(siege.started_tick>tick)continue;
  // 新工程生命周期具完整进退来源；只从当时军籍筛人物，旧无账分支保持原保守拒绝。
  auto tracked=observed_limit==c.tick()?read_military_siege_occupation(c.state(),m.siege_lifecycle,siege,tick,boundary):read_military_siege_occupation(c.state(),m.siege_lifecycle,siege,tick,boundary,DynamicStepInput{c.tick(),observed_limit});
  if(!tracked.ok)return {false,{},tracked.error};if(tracked.value){for(const auto& bound:tracked.value->army_source_ids){const auto* army=find(m.armies,bound.first);if(!army)return fail<MilitaryActorOccupation>("MilitaryOccupationArmyRequired",bound.first);auto members=members_before(c,m,*army,tick,boundary,observed_limit);if(!members.ok)return {false,{},members.error};if(has(members.value,actor))out.siege_source_ids.push_back(bound.second);}continue;}

  const MilitarySiegeReleaseFact* release=nullptr;for(const auto& source:m.recovery.siege_releases)if(source.siege_id==siege.id){if(release)return fail<MilitaryActorOccupation>("MilitarySiegeReleaseRepeated",siege.id);release=&source;}
  if(release){if(siege.phase!="closed"||!siege.result||release->result_tick!=siege.result->tick||release->result_kind!=siege.result->kind||release->winner_side!=siege.result->winner_side||release->terms!=siege.result->terms||release->reason!=siege.result->reason||release->tick<release->result_tick)return fail<MilitaryActorOccupation>("MilitarySiegeReleaseSourceMismatch",release->id);auto ended=at.before(release->id,release->tick,"military_siege_released","military");if(!ended.ok)return {false,{},ended.error};if(ended.value)continue;}
for(const auto& row:siege.armies){const auto* army=find(m.armies,row.first);if(!army)return fail<MilitaryActorOccupation>("MilitaryOccupationArmyRequired",row.first);
   if(current){if(possible_army_member(m,*army,actor,tick))out.incomplete_source_ids.push_back(siege.id);bool withdrawn_result=false;if(siege.phase=="result"&&siege.result&&siege.result->kind=="withdrawn")for(const auto& event:siege.last_events)withdrawn_result=withdrawn_result||(event.kind=="withdrawal_request"&&event.army_ids&&has(*event.army_ids,army->id));if(siege.phase!="closed"&&has(army->member_actor_ids,actor)&&(!has(siege.withdrawn_army_ids,army->id)||withdrawn_result))out.siege_source_ids.push_back(siege.id);}
   else if(possible_army_member(m,*army,actor,tick))return fail<MilitaryActorOccupation>("MilitaryOccupationHistoryIncomplete",siege.id);
  }}
 for(auto* ids:{&out.custody_source_ids,&out.battle_source_ids,&out.siege_source_ids,&out.incomplete_source_ids}){std::sort(ids->begin(),ids->end());ids->erase(std::unique(ids->begin(),ids->end()),ids->end());}
 out.history_complete=out.incomplete_source_ids.empty();return {true,std::move(out),{}};
}
}
// 接触读借同一原成员重放，不另存成员快照或复写一套军籍算法。
Outcome<std::vector<StableId>> military_army_actor_members_before(const CommonStore& c,const MilitaryState& m,const StableId& army_id,Integer tick,const StableId& before){const auto* army=find(m.armies,army_id);if(!army||tick<0||tick>c.tick()||before.empty()||!c.state().facts.find(before))return fail<std::vector<StableId>>("MilitaryOccupationBoundaryRequired",before);return members_before(c,m,*army,tick,before,c.tick());}
// 围城确认仅在原军史已验同一步中借成员前界，未提供任意future查询。
Outcome<std::vector<StableId>> military_army_actor_members_before(const CommonStore& c,const MilitaryState& m,const StableId& army,Integer tick,const StableId& before,const DynamicStepInput& phase){
 if(phase.from_tick<0||phase.from_tick!=c.tick()||phase.from_tick==std::numeric_limits<Integer>::max()||phase.to_tick!=phase.from_tick+1||tick<0||tick>phase.to_tick)return fail<std::vector<StableId>>("MilitaryOccupationPhaseClockMismatch",before);
 const auto* head=c.state().facts.find(before);const auto* formed=find(m.armies,army);if(!formed||before.rfind("military_release:",0)!=0||!head||head->kind!="military_siege_released"||head->domain!="military")return fail<std::vector<StableId>>("MilitaryOccupationPhaseBoundaryRequired",before);
 return members_before(c,m,*formed,tick,before,phase.to_tick);
}
Outcome<MilitaryActorOccupation> query_military_actor_occupation(const CommonStore& c,const MilitaryState& m,const NavalState* n,const StableId& actor){return read(c,m,n,actor,c.tick(),{},true,c.tick());}
// 本半日候选当前值包含已发生的to来源；身份仍是原候选，观察时刻明确为to而非伪装stable。
Outcome<MilitaryActorOccupation> query_military_actor_occupation(const CommonStore& c,const MilitaryState& m,const NavalState* n,const StableId& actor,const DynamicStepInput& phase){
 if(phase.from_tick<0||phase.from_tick!=c.tick()||phase.from_tick==std::numeric_limits<Integer>::max()||phase.to_tick!=phase.from_tick+1)return fail<MilitaryActorOccupation>("MilitaryOccupationPhaseClockMismatch",actor);
 return read(c,m,n,actor,phase.to_tick,{},true,phase.to_tick);
}
Outcome<MilitaryActorOccupation> query_military_actor_occupation_before(const CommonStore& c,const MilitaryState& m,const NavalState* n,const StableId& actor,Integer tick,const StableId& before){return read(c,m,n,actor,tick,before,false,c.tick());}
// 同一严格候选可重核旧credit；未来军史只观察到to，调用方须核每个真实credit的payload及tick。
Outcome<MilitaryActorOccupation> query_military_actor_occupation_before(const CommonStore& c,const MilitaryState& m,const NavalState* n,const StableId& actor,Integer tick,const StableId& before,const DynamicStepInput& phase){
 if(phase.from_tick<0||phase.from_tick!=c.tick()||phase.from_tick==std::numeric_limits<Integer>::max()||phase.to_tick!=phase.from_tick+1||tick<0||tick>phase.to_tick)return fail<MilitaryActorOccupation>("MilitaryOccupationPhaseClockMismatch",before);
 const auto* head=c.state().facts.find(before);if(before.rfind("recovery:",0)!=0||!head||head->id!=before||head->kind!="personal_recovery_credit"||head->domain!="personal_injury_recovery")return fail<MilitaryActorOccupation>("MilitaryOccupationPhaseBoundaryRequired",before);
 return read(c,m,n,actor,tick,before,false,phase.to_tick);
}
Outcome<MilitaryArmyMembersHistoryRead> read_military_army_members_exact(const CommonStore& common,const MilitaryState& military,const MilitaryExactState& exact,const StableId& army_id,const WorldMoment& now,const std::optional<SourceRef>& before,ExactArithmeticBudget budget,const MilitaryFormationSourceRead* formations,const MilitaryReturnSourceRead* returns){
 auto encoded=encode_world_moment(now,budget);if(!encoded.ok)return {false,{},encoded.error};if(now.half_day_tick<common.tick()||now.half_day_tick-common.tick()>1)return fail<MilitaryArmyMembersHistoryRead>("MilitaryExactHistoryPhaseClockMismatch",army_id);
 const auto* army=find(military.armies,army_id);if(!army)return fail<MilitaryArmyMembersHistoryRead>("MilitaryArmySourceRequired",army_id);if(before){const auto* header=common.state().facts.find(before->id);if(!header||header->kind!=before->kind||header->domain!=before->domain)return fail<MilitaryArmyMembersHistoryRead>("MilitaryOccupationBoundaryRequired",before->id);}
 MilitaryArmyMembersHistoryRead out;auto result=members_before(common,military,*army,now.half_day_tick,before?before->id:StableId{},now.half_day_tick,&exact,&now,&out.trace,budget,nullptr,formations,returns);if(!result.ok)return {false,{},result.error};out.named_actor_ids=std::move(result.value);return {true,std::move(out),{}};
}

Outcome<MilitaryArmyMembersHistoryRead> read_military_army_members_before_exact(const CommonStore& common,const MilitaryState& military,const MilitaryExactState& exact,const StableId& army_id,const SourceRef& before,const WorldMoment& at,const WorldMoment& now,ExactArithmeticBudget budget,const MilitaryFormationSourceRead* formations,const MilitaryReturnSourceRead* returns){
 auto encoded=encode_world_moment(now,budget);if(!encoded.ok)return {false,{},encoded.error};encoded=encode_world_moment(at,budget);if(!encoded.ok)return {false,{},encoded.error};auto order=compare_world_moments(at,now,budget);if(!order.ok)return {false,{},order.error};if(order.value>0||now.half_day_tick<common.tick()||now.half_day_tick-common.tick()>1)return fail<MilitaryArmyMembersHistoryRead>("MilitaryExactHistoryPhaseClockMismatch",army_id);
 const auto* army=find(military.armies,army_id);const auto* header=common.state().facts.find(before.id);if(!army||!header||header->kind!=before.kind||header->domain!=before.domain)return fail<MilitaryArmyMembersHistoryRead>("MilitaryOccupationBoundaryRequired",before.id);
 MilitaryArmyMembersHistoryRead out;auto result=members_before(common,military,*army,at.half_day_tick,before.id,now.half_day_tick,&exact,&now,&out.trace,budget,&at,formations,returns);if(!result.ok)return {false,{},result.error};out.named_actor_ids=std::move(result.value);return {true,std::move(out),{}};
}

// A captured empty/current prefix needs no invented boundary source. The
// original reducer still owns formation, integration and casualty membership.
Outcome<CheckpointVector<const StableId*>> read_military_army_member_refs_at_actual_prefix(const CommonStore& common,const MilitaryState& military,const MilitaryExactState& exact,const StableId& army_id,const CommonActualFactPrefix& prefix,const WorldMoment& at,const WorldMoment& now,ExactArithmeticBudget budget,const MilitaryFormationSourceRead* formations,const MilitaryReturnSourceRead* returns){
 using R=CheckpointVector<const StableId*>;
 auto encoded=encode_world_moment(now,budget);if(!encoded.ok)return {false,{},encoded.error};encoded=encode_world_moment(at,budget);if(!encoded.ok)return {false,{},encoded.error};auto order=compare_world_moments(at,now,budget);if(!order.ok)return {false,{},order.error};if(order.value>0||now.half_day_tick<common.tick()||now.half_day_tick-common.tick()>1)return fail<R>("MilitaryExactHistoryPhaseClockMismatch",army_id);
 auto cut=read_actual_common_fact_prefix(common,prefix);if(!cut.ok)return {false,{},cut.error};
 const auto* army=find(military.armies,army_id);if(!army)return fail<R>("MilitaryArmySourceRequired",army_id);
 return member_refs_before(common,military,*army,at.half_day_tick,{},now.half_day_tick,&exact,&now,nullptr,budget,&at,formations,returns,&prefix);
}
Outcome<CheckpointVector<const StableId*>> read_military_army_member_refs_at_selected_actual_prefix(const CommonStore& common,const MilitaryState& military,const CatalogIdentity& identity,const MilitaryExactState* exact,const StableId& army_id,const CommonActualFactPrefix& prefix,const WorldMoment& at,const WorldMoment& now,ExactArithmeticBudget budget,const MilitaryFormationSourceRead* formations,const MilitaryReturnSourceRead* returns,const MilitaryFieldHistoryRead* field){
 using R=CheckpointVector<const StableId*>;
 auto selected=validate_selected_military_roster_root(common,identity,exact);if(!selected.ok)return {false,{},selected.error};selected=validate_selected_military_field_history(common,military,identity,field);if(!selected.ok)return {false,{},selected.error};
 auto encoded=encode_world_moment(now,budget);if(!encoded.ok)return {false,{},encoded.error};encoded=encode_world_moment(at,budget);if(!encoded.ok)return {false,{},encoded.error};auto order=compare_world_moments(at,now,budget);if(!order.ok)return {false,{},order.error};if(order.value>0||now.half_day_tick<common.tick()||now.half_day_tick-common.tick()>1)return fail<R>("MilitaryExactHistoryPhaseClockMismatch",army_id);
 auto cut=read_actual_common_fact_prefix(common,prefix);if(!cut.ok)return {false,{},cut.error};
 const auto* army=find(military.armies,army_id);if(!army)return fail<R>("MilitaryArmySourceRequired",army_id);
 return member_refs_before(common,military,*army,at.half_day_tick,{},now.half_day_tick,exact,&now,nullptr,budget,&at,formations,returns,&prefix,field);
}
Outcome<MilitaryArmyMembersHistoryRead> read_military_army_members_at_selected_actual_prefix(const CommonStore& common,const MilitaryState& military,const CatalogIdentity& identity,const MilitaryExactState* exact,const StableId& army_id,const CommonActualFactPrefix& prefix,const WorldMoment& at,const WorldMoment& now,ExactArithmeticBudget budget,const MilitaryFormationSourceRead* formations,const MilitaryReturnSourceRead* returns,const MilitaryFieldHistoryRead* field){
 using R=MilitaryArmyMembersHistoryRead;
 auto selected=validate_selected_military_roster_root(common,identity,exact);if(!selected.ok)return {false,{},selected.error};selected=validate_selected_military_field_history(common,military,identity,field);if(!selected.ok)return {false,{},selected.error};
 auto encoded=encode_world_moment(now,budget);if(!encoded.ok)return {false,{},encoded.error};encoded=encode_world_moment(at,budget);if(!encoded.ok)return {false,{},encoded.error};auto order=compare_world_moments(at,now,budget);if(!order.ok)return {false,{},order.error};if(order.value>0||now.half_day_tick<common.tick()||now.half_day_tick-common.tick()>1)return fail<R>("MilitaryExactHistoryPhaseClockMismatch",army_id);
 auto cut=read_actual_common_fact_prefix(common,prefix);if(!cut.ok)return {false,{},cut.error};
 const auto* army=find(military.armies,army_id);if(!army)return fail<R>("MilitaryArmySourceRequired",army_id);
 MilitaryArmyMembersHistoryRead out;auto rows=member_refs_before(common,military,*army,at.half_day_tick,{},now.half_day_tick,exact,&now,&out.trace,budget,&at,formations,returns,&prefix,field);if(!rows.ok)return {false,{},rows.error};for(const auto* id:rows.value)out.named_actor_ids.push_back(*id);return {true,std::move(out),{}};
}

Outcome<CheckpointVector<const StableId*>> read_military_army_member_refs_before_exact(const CommonStore& common,const MilitaryState& military,const MilitaryExactState& exact,const StableId& army_id,const SourceRef& before,const WorldMoment& at,const WorldMoment& now,ExactArithmeticBudget budget,const MilitaryFormationSourceRead* formations,const MilitaryReturnSourceRead* returns){
 auto encoded=encode_world_moment(now,budget);if(!encoded.ok)return {false,{},encoded.error};encoded=encode_world_moment(at,budget);if(!encoded.ok)return {false,{},encoded.error};auto order=compare_world_moments(at,now,budget);if(!order.ok)return {false,{},order.error};if(order.value>0||now.half_day_tick<common.tick()||now.half_day_tick-common.tick()>1)return fail<CheckpointVector<const StableId*>>("MilitaryExactHistoryPhaseClockMismatch",army_id);
 const auto* army=find(military.armies,army_id);const auto* header=common.state().facts.find(before.id);if(!army||!header||header->kind!=before.kind||header->domain!=before.domain)return fail<CheckpointVector<const StableId*>>("MilitaryOccupationBoundaryRequired",before.id);
 return member_refs_before(common,military,*army,at.half_day_tick,before.id,now.half_day_tick,&exact,&now,nullptr,budget,&at,formations,returns);
}

}

namespace hanmo::resident {
namespace {
struct ActiveArmyEvent {
 MilitaryHistorySource source;WorldMoment at;
 // Only selected actual-prefix collection uses these occurrence fields.
 bool genuine_moment=true;Integer legacy_tick=0;std::size_t common_position=0;
 const ArmyRecord* formation=nullptr;
 const ArmyFormationTerms* formation_terms=nullptr;
 const MilitaryIntegrationFact* integration=nullptr;
 const std::vector<MilitaryActorChange>* actor_changes=nullptr;
 const MilitaryReturnFact* returned=nullptr;
 const DynamicMilitaryReturnFact2* exact_returned=nullptr;
 const NavalCaptureFact* captured=nullptr;
 StableId captured_army;
};
Integer active_army_serial(const CommonStore& common,const StableId& id){
 const auto pos=id.rfind(':');Integer value=0;
 if(pos==StableId::npos)movement_detail::fail("MilitaryActiveArmyOriginalSerialRequired",id);
 const auto suffix=id.substr(pos+1);const auto parsed=std::from_chars(suffix.data(),suffix.data()+suffix.size(),value);
 if(parsed.ec!=std::errc{}||parsed.ptr!=suffix.data()+suffix.size()||value<1||value>=common.state().next_instance_id||std::to_string(value)!=suffix)movement_detail::fail("MilitaryActiveArmyOriginalSerialRequired",id);
 return value;
}
std::vector<ActiveArmyEvent> active_army_events(const CommonStore& common,const MilitaryState& military,const MilitaryExactState* exact,const NavalState* naval,const WorldMoment& now,ExactArithmeticBudget budget,const MilitaryFormationSourceRead* formations,const MilitaryReturnSourceRead* returns,const CommonActualFactPrefix* actual_prefix=nullptr,const WorldMoment* actual_cut=nullptr,const NavalOriginalSourceContext* naval_sources=nullptr,const MilitaryFieldHistoryRead* field=nullptr){
 using namespace movement_detail;
 const MilitaryCasualtyHistoryContext casualty_history{common,now,formations,returns,actual_prefix?MilitaryCasualtyHistoryOrder::actual_common_prefix:MilitaryCasualtyHistoryOrder::legacy_serial};
 if(bool(actual_prefix)!=bool(actual_cut))movement_detail::fail("MilitaryActualArmyCutRequired");
 const auto prefix_end=actual_prefix?take(read_actual_common_fact_prefix(common,*actual_prefix)):std::size_t{};
 if(actual_cut){take(encode_world_moment(*actual_cut,budget));if(take(compare_world_moments(*actual_cut,now,budget))>0)movement_detail::fail("MilitaryActiveArmyFutureCut");}
 take(encode_world_moment(now,budget));auto low=take(normalize_world_moment(common.tick(),{},budget));
 if(take(compare_world_moments(now,low,budget))<0)movement_detail::fail("MilitaryActiveArmyClockMismatch");
 if(common.tick()!=INT64_MAX&&take(compare_world_moments(now,take(normalize_world_moment(common.tick()+1,{},budget)),budget))>0)movement_detail::fail("MilitaryActiveArmyClockMismatch");
 if(!naval)for(const auto& row:common.state().facts.rows())if(row.second.domain=="military_naval")movement_detail::fail("MilitaryActiveArmyNavalHistoryRequired",row.first);
 if(returns&&!returns->bound_to(common,military))movement_detail::fail("CrossCandidateMilitaryReturnSources");
 std::vector<ActiveArmyEvent> events;std::set<StableId> ids,armies,return_applications,return_receipts;std::set<Integer> serials;
 auto append=[&](ActiveArmyEvent event,const StableId& serial_id){const auto& ref=event.source.source_ref;const auto* head=common.state().facts.find(ref.id);
  if(!head||head->id!=ref.id||head->kind!=ref.kind||head->domain!=ref.domain||!ids.insert(ref.id).second)movement_detail::fail("MilitaryActiveArmySourceMismatch",ref.id);
  event.source.original_serial=active_army_serial(common,serial_id);if(!serials.insert(event.source.original_serial).second)movement_detail::fail("MilitaryActiveArmySerialRepeated",ref.id);
  if(actual_prefix){
   event.common_position=take(read_actual_common_fact_position(common,*head));
   if(event.genuine_moment){
    take(encode_world_moment(event.at,budget));if(take(compare_world_moments(event.at,now,budget))>0)movement_detail::fail("MilitaryActiveArmyFutureSource",ref.id);
    const auto order=take(compare_world_moments(event.at,*actual_cut,budget));const bool included=event.common_position<prefix_end;
    if((included&&order>0)||(!included&&order<0))movement_detail::fail("MilitaryActiveArmyActualBoundaryMomentMismatch",ref.id);
   }else if(event.legacy_tick<0||event.legacy_tick>now.half_day_tick)movement_detail::fail("MilitaryActiveArmyFutureSource",ref.id);
  }else{take(encode_world_moment(event.at,budget));if(take(compare_world_moments(event.at,now,budget))>0)movement_detail::fail("MilitaryActiveArmyFutureSource",ref.id);}
  events.push_back(std::move(event));};
 auto legacy_moment=[&](ActiveArmyEvent& event,Integer tick){if(actual_prefix){event.genuine_moment=false;event.legacy_tick=tick;}else event.at=take(normalize_world_moment(tick,{},budget));};
 for(const auto& army:military.armies){
  const auto formation=take(read_original_military_formation(common.state(),military,army,now,formations,budget));const auto* terms=formation.original_terms;
  if(!terms||!armies.insert(army.id).second)movement_detail::fail("MilitaryActiveArmyFormationRequired",army.id);
  if(actual_prefix&&formation.completed_source.source_schema!=2)movement_detail::fail("MilitaryActualFormationCompletionRequired",army.id);
  std::set<StableId> people;for(const auto& actor:terms->member_actor_ids)if(!common.actor(actor)||!people.insert(actor).second)movement_detail::fail("MilitaryActiveArmyFormationMemberMismatch",actor);
  ActiveArmyEvent event;event.source={formation.completed_source.source_ref,formation.completed_source.source_schema,0};event.at=formation.completed_at;event.formation=&army;event.formation_terms=terms;append(std::move(event),formation.completed_source.source_schema==2?formation.completed_source.source_ref.id:army.id);
 }
 for(const auto& integrated:military.integration_facts){
  if(!armies.count(integrated.recipient_army_id)||integrated.source_armies.empty())movement_detail::fail("MilitaryActiveArmyIntegrationRequired",integrated.id);
  std::set<StableId> from;for(const auto& source:integrated.source_armies)if(source.id==integrated.recipient_army_id||!armies.count(source.id)||!from.insert(source.id).second)movement_detail::fail("MilitaryActiveArmyIntegrationRequired",integrated.id);
  ActiveArmyEvent event;event.source={{"military","military_integration",integrated.id},1,0};legacy_moment(event,integrated.tick);event.integration=&integrated;append(std::move(event),integrated.id);
 }
 for(const auto& casualty:military.casualty_facts){
  ActiveArmyEvent event;event.source={{"military","military_casualties",casualty.id},1,0};const auto field_moment=field?take(read_selected_military_field_casualty_moment(common,military,casualty,field)):std::optional<WorldMoment>{};if(field_moment)event.at=*field_moment;else{const auto original_moment=exact?take(read_military_source_moment(military,*exact,event.source.source_ref,1,budget)):take(read_legacy_military_casualty_source_moment(military,event.source.source_ref,budget));if(actual_prefix)legacy_moment(event,casualty.tick);else event.at=original_moment;}event.actor_changes=&casualty.actor_changes;append(std::move(event),casualty.id);
 }
 if(exact)for(const auto& row:exact->dynamic_casualty_facts.rows()){
  const auto& casualty=row.second;if(row.first!=casualty.id)movement_detail::fail("MilitaryActiveArmySourceMismatch",row.first);
  ActiveArmyEvent event;event.source={{"military","military_casualties",casualty.id},2,0};event.at=take(read_military_source_moment(military,*exact,event.source.source_ref,2,budget,&casualty_history));event.actor_changes=&casualty.actor_changes;append(std::move(event),casualty.id);
 }
 for(const auto& returned:military.return_facts){
  const auto* army=find(military.armies,returned.army_id);const MilitaryLogisticsFact* action=nullptr;
  for(const auto& candidate:military.logistics_facts)if(candidate.result&&candidate.result->receipt_id==returned.id){if(action)movement_detail::fail("MilitaryActiveArmyReturnAmbiguous",returned.id);action=&candidate;}
  const auto* head=action?common.state().facts.find(action->id):nullptr;
  if(!army||!action||!head||head->domain!="military"||head->kind!="military_action"||action->command!="army_return"||action->phase!="completed"||!action->success||!action->result->ok.value_or(false)||action->terms.army_id!=army->id||action->result->army_id!=army->id||action->terms.garrison_id!=returned.garrison_id||action->terms.source_garrison_id!=returned.source_garrison_id||action->actor_id!=returned.actor_id||action->result->order_id!=returned.order_id||army->order_id!=returned.order_id||action->completed_tick!=returned.tick||action->start_tick==INT64_MAX||returned.tick!=action->start_tick+1||(actual_prefix?take(read_actual_common_fact_position(common,SourceRefBuildView{"military","military_action",action->id}))>=take(read_actual_common_fact_position(common,SourceRefBuildView{"military","military_return",returned.id})):active_army_serial(common,action->id)>=active_army_serial(common,returned.id)))movement_detail::fail("MilitaryActiveArmyReturnRequired",returned.id);
  ActiveArmyEvent event;event.source={{"military","military_return",returned.id},1,0};legacy_moment(event,returned.tick);event.returned=&returned;append(std::move(event),returned.id);
 }
 if(returns)for(const auto& army:military.armies){
  const auto returned=take(returns->read_exact_army_return(army.id));if(!returned)continue;
  if(!returned->original||returned->original->army_id!=army.id||returned->actual_return.source_schema!=2||returned->actual_application.source_schema!=2||returned->actual_return.source_ref.domain!="military"||returned->actual_return.source_ref.kind!="military_return"||returned->actual_return.source_ref.id!=returned->original->id||!same_source(returned->actual_application.source_ref,returned->original->completion_source_ref)||!return_applications.insert(returned->actual_application.source_ref.id).second)movement_detail::fail("MilitaryActiveArmyReturnRequired",army.id);
  if(!return_receipts.insert(returned->actual_return.source_ref.id).second)movement_detail::fail("MilitaryActiveArmyReturnRequired",army.id);
  if(actual_prefix){const auto receipt=take(read_actual_common_fact_position(common,returned->actual_return.source_ref));const auto application=take(read_actual_common_fact_position(common,returned->actual_application.source_ref));if(receipt>=application||(receipt<prefix_end)!=(application<prefix_end))movement_detail::fail("MilitaryActiveArmyReturnApplicationCutMismatch",army.id);}
  ActiveArmyEvent event;event.source={actual_prefix?returned->actual_application.source_ref:returned->actual_return.source_ref,2,0};event.at=returned->original->completed_at;event.exact_returned=returned->original;const auto& serial_id=actual_prefix?returned->actual_application.source_ref.id:returned->original->id;append(std::move(event),serial_id);
 }
 for(const auto& row:common.state().facts.rows())if(row.second.domain=="military"&&row.second.kind=="military_logistics_completion_applied"&&!return_applications.count(row.first))movement_detail::fail("MilitaryActiveArmyReturnHistoryRequired",row.first);
 if(naval)for(const auto& captured:naval->battle.captures){
  const auto* fleet=find(naval->fleets,captured.from_fleet_id);if(!fleet||!armies.count(fleet->army_id))movement_detail::fail("MilitaryActiveArmyCaptureRequired",captured.id);
  if(actual_prefix){if(!naval_sources)movement_detail::fail("MilitaryActiveArmyNavalOriginalSourcesRequired",captured.id);take(validate_naval_casualty_receipt(common,military,*naval,captured.casualty_source_id,now.half_day_tick,*naval_sources));}
  else take(validate_naval_casualty_receipt(common,military,*naval,captured.casualty_source_id,now.half_day_tick));
  ActiveArmyEvent event;event.source={{"military_naval","naval_capture",captured.id},1,0};legacy_moment(event,captured.tick);event.captured=&captured;event.captured_army=fleet->army_id;append(std::move(event),captured.id);
 }
 // Missing retained payloads cannot silently produce an empty Army history.
 for(const auto& row:common.state().facts.rows()){const auto& f=row.second;const bool required=(f.domain=="military"&&(f.kind=="military_action_completion_applied"||f.kind=="military_integration"||f.kind=="military_casualties"||f.kind=="military_return"))||(f.domain=="military_naval"&&f.kind=="naval_capture");if(required&&!ids.count(f.id)&&!(actual_prefix&&f.domain=="military"&&f.kind=="military_return"&&return_receipts.count(f.id)))movement_detail::fail("MilitaryActiveArmyHistoryIncomplete",f.id);}
 if(actual_prefix){
  std::sort(events.begin(),events.end(),[](const auto& a,const auto& b){return a.common_position<b.common_position;});
  const WorldMoment* previous=nullptr;
  for(const auto& event:events)if(event.genuine_moment){
   if(previous&&take(compare_world_moments(*previous,event.at,budget))>0)movement_detail::fail("MilitaryActiveArmyCausalOrderMismatch",event.source.source_ref.id);
   previous=&event.at;
  }
 }
 else{std::sort(events.begin(),events.end(),[&](const auto& a,const auto& b){const auto order=take(compare_world_moments(a.at,b.at,budget));return order?order<0:a.source.original_serial<b.source.original_serial;});
  Integer serial=0;for(const auto& event:events){if(event.source.original_serial<=serial)movement_detail::fail("MilitaryActiveArmyCausalOrderMismatch",event.source.source_ref.id);serial=event.source.original_serial;}}
 return events;
}
struct ActiveArmyFold {std::vector<StableId> people;StableId status="active";};
void apply_active_army_event(std::map<StableId,ActiveArmyFold>& state,const ActiveArmyEvent& event){
 if(event.formation){if(!state.emplace(event.formation->id,ActiveArmyFold{event.formation_terms->member_actor_ids,"active"}).second)movement_detail::fail("MilitaryActiveArmyFormationRepeated",event.formation->id);return;}
 if(event.integration){auto recipient=state.find(event.integration->recipient_army_id);if(recipient==state.end()||recipient->second.status!="active"||recipient->second.people!=event.integration->recipient_members_before)movement_detail::fail("MilitaryActiveArmyIntegrationPredecessorMismatch",event.source.source_ref.id);for(const auto& source:event.integration->source_armies){auto old=state.find(source.id);if(old==state.end()||old->second.status!="active")movement_detail::fail("MilitaryActiveArmyIntegrationPredecessorMismatch",source.id);old->second.status="closed";}}
 if(event.actor_changes)for(const auto& actor:*event.actor_changes)if(!state.count(actor.army_id))movement_detail::fail("MilitaryActiveArmyCasualtyPredecessorMissing",actor.army_id);
 for(auto& army:state)apply_original_member_change(army.first,army.second.people,event.integration,event.actor_changes);
 if(event.returned){auto army=state.find(event.returned->army_id);if(army==state.end()||army->second.status!="active")movement_detail::fail("MilitaryActiveArmyReturnPredecessorMismatch",event.returned->id);army->second.status="closed";}
 if(event.exact_returned){auto army=state.find(event.exact_returned->army_id);if(army==state.end()||army->second.status!="active")movement_detail::fail("MilitaryActiveArmyReturnPredecessorMismatch",event.exact_returned->id);army->second.status="closed";}
 if(event.captured){auto army=state.find(event.captured_army);if(army==state.end())movement_detail::fail("MilitaryActiveArmyCapturePredecessorMismatch",event.captured->id);for(const auto& actor:event.captured->prisoner_ids)army->second.people.erase(std::remove(army->second.people.begin(),army->second.people.end(),actor),army->second.people.end());}
 if(event.captured&&event.captured->from_after_assignments.empty()){auto army=state.find(event.captured_army);if(army==state.end()||army->second.status!="active")movement_detail::fail("MilitaryActiveArmyCapturePredecessorMismatch",event.captured->id);army->second.status="defeated";}
}
struct ActorActiveArmyEvent {
 const ActiveArmyEvent* original=nullptr;
 std::vector<StableId> before,after;
};
std::vector<ActorActiveArmyEvent> actor_active_army_events(const std::vector<ActiveArmyEvent>& events,const MilitaryState& military,const StableId& actor,bool require_current=true){
 std::map<StableId,ActiveArmyFold> full;std::vector<ActorActiveArmyEvent> relevant;
 auto active=[&]{std::vector<StableId> ids;for(const auto& army:full)if(army.second.status=="active"&&has(army.second.people,actor))ids.push_back(army.first);return ids;};
 for(const auto& event:events){auto before=active();apply_active_army_event(full,event);auto after=active();if(before!=after)relevant.push_back({&event,std::move(before),std::move(after)});}
 // The projected head never relaxes validation of the full original world chain.
 if(require_current)for(const auto& army:military.armies){auto replay=full.find(army.id);if(replay==full.end()||replay->second.people!=army.member_actor_ids||replay->second.status!=army.status)movement_detail::fail("MilitaryActiveArmyUntrackedCurrentMutation",army.id);}
 return relevant;
}
// 显式目录选型只选择原历史根，完整Army重放与原冻结头排序不变。
MilitaryActorActiveArmyRead active_army_read_selected_impl(const CommonStore& common,const MilitaryState& military,const MilitaryExactState* exact,const NavalState* naval,const StableId& actor,const WorldMoment& at,const std::vector<SourceRef>* head,const WorldMoment& now,ExactArithmeticBudget budget,const MilitaryFormationSourceRead* formations,const MilitaryReturnSourceRead* returns,const MilitarySelectedHistoryRead* selected=nullptr){
 using namespace movement_detail;if(selected){take(validate_selected_military_roster_root(common,selected->identity,exact));take(validate_selected_military_field_history(common,military,selected->identity,selected->field));if(selected->exact!=exact)movement_detail::fail("MilitaryActiveArmySelectedRootMismatch");}if(!common.actor(actor))movement_detail::fail("UnknownActor",actor);take(encode_world_moment(at,budget));if(take(compare_world_moments(at,now,budget))>0)movement_detail::fail("MilitaryActiveArmyFutureCut",actor);
 const auto events=active_army_events(common,military,exact,naval,now,budget,formations,returns,nullptr,nullptr,nullptr,selected?selected->field:nullptr);const auto relevant=actor_active_army_events(events,military,actor);Integer cutoff=0;std::set<StableId> frozen;
 if(head)for(const auto& ref:*head){const ActiveArmyEvent* original=nullptr;for(const auto& item:relevant)if(same_source(ref,item.original->source.source_ref)){original=item.original;break;}if(!original||!frozen.insert(ref.id).second||take(compare_world_moments(original->at,at,budget))>0)movement_detail::fail("MilitaryActiveArmyFrozenSourceMismatch",ref.id);cutoff=std::max(cutoff,original->source.original_serial);}
 MilitaryActorActiveArmyRead out;out.actor_id=actor;out.observed_at=at;
 for(const auto& item:relevant){const auto& event=*item.original;const auto order=take(compare_world_moments(event.at,at,budget));const bool included=order<0||(order==0&&(!head||event.source.original_serial<=cutoff));if(included){if(head&&!frozen.count(event.source.source_ref.id))movement_detail::fail("MilitaryActiveArmyFrozenHeadIncomplete",event.source.source_ref.id);if(out.active_army_ids!=item.before)movement_detail::fail("MilitaryActorArmyProjectedChainMismatch",actor);out.active_army_ids=item.after;out.original_sources.push_back(event.source);}else if(head&&frozen.count(event.source.source_ref.id))movement_detail::fail("MilitaryActiveArmyFrozenSourceMismatch",event.source.source_ref.id);}
 if(head&&head->size()!=out.original_sources.size())movement_detail::fail("MilitaryActiveArmyFrozenHeadMismatch",actor);
 if(head)for(std::size_t i=0;i<head->size();++i)if(!same_source((*head)[i],out.original_sources[i].source_ref))movement_detail::fail("MilitaryActiveArmyFrozenHeadOrderMismatch",actor);
 return out;
}
// 旧引用入口仍走同一重放/冻结头算法，不从空表推断新选型。
MilitaryActorActiveArmyRead active_army_read(const CommonStore& common,const MilitaryState& military,const MilitaryExactState& exact,const NavalState* naval,const StableId& actor,const WorldMoment& at,const std::vector<SourceRef>* head,const WorldMoment& now,ExactArithmeticBudget budget,const MilitaryFormationSourceRead* formations,const MilitaryReturnSourceRead* returns){
 return active_army_read_selected_impl(common,military,&exact,naval,actor,at,head,now,budget,formations,returns);
}
}
Outcome<MilitaryActorActiveArmyRead> read_actual_actor_active_armies(const CommonStore& c,const MilitaryState& m,const MilitaryExactState& exact,const NavalState* n,const StableId& actor,const WorldMoment& now,ExactArithmeticBudget budget,const MilitaryFormationSourceRead* formations,const MilitaryReturnSourceRead* returns){return movement_detail::guard<MilitaryActorActiveArmyRead>([&]{return active_army_read(c,m,exact,n,actor,now,nullptr,now,budget,formations,returns);});}
Outcome<MilitaryActorActiveArmyRead> read_actor_active_armies_at_source_head(const CommonStore& c,const MilitaryState& m,const MilitaryExactState& exact,const NavalState* n,const StableId& actor,const WorldMoment& at,const std::vector<SourceRef>& head,const WorldMoment& now,ExactArithmeticBudget budget,const MilitaryFormationSourceRead* formations,const MilitaryReturnSourceRead* returns){return movement_detail::guard<MilitaryActorActiveArmyRead>([&]{return active_army_read(c,m,exact,n,actor,at,&head,now,budget,formations,returns);});}
Outcome<MilitaryActorActiveArmyRead> read_actor_active_armies_at_actual_prefix(const CommonStore& c,const MilitaryState& m,const MilitaryExactState& exact,const NavalState* naval,const StableId& actor,const CommonActualFactPrefix& prefix,const WorldMoment& at,const WorldMoment& now,ExactArithmeticBudget budget,const MilitaryFormationSourceRead* formations,const MilitaryReturnSourceRead* returns,const NavalOriginalSourceContext* naval_sources){return movement_detail::guard<MilitaryActorActiveArmyRead>([&]{
 using namespace movement_detail;if(!c.actor(actor))movement_detail::fail("UnknownActor",actor);const auto end=take(read_actual_common_fact_prefix(c,prefix));
 // Source and every genuine-M boundary are checked before any projection or
 // positive result. Full predecessor replay is independent of today's state.
 const auto events=active_army_events(c,m,&exact,naval,now,budget,formations,returns,&prefix,&at,naval_sources);
 const auto relevant=actor_active_army_events(events,m,actor,false);
 MilitaryActorActiveArmyRead out;out.actor_id=actor;out.observed_at=at;
 for(const auto& item:relevant){const auto& event=*item.original;if(event.common_position>=end)continue;if(out.active_army_ids!=item.before)movement_detail::fail("MilitaryActorArmyProjectedChainMismatch",actor);out.active_army_ids=item.after;out.original_sources.push_back(event.source);}
 return out;
});}Outcome<MilitaryActorActiveArmyRead> read_actor_active_armies_at_selected_actual_prefix(const CommonStore& c,const MilitaryState& m,const CatalogIdentity& identity,const MilitaryExactState* exact,const NavalState* naval,const StableId& actor,const CommonActualFactPrefix& prefix,const WorldMoment& at,const WorldMoment& now,ExactArithmeticBudget budget,const MilitaryFormationSourceRead* formations,const MilitaryReturnSourceRead* returns,const NavalOriginalSourceContext* naval_sources,const MilitaryFieldHistoryRead* field){return movement_detail::guard<MilitaryActorActiveArmyRead>([&]{
 using namespace movement_detail;take(validate_selected_military_roster_root(c,identity,exact));take(validate_selected_military_field_history(c,m,identity,field));if(!c.actor(actor))movement_detail::fail("UnknownActor",actor);const auto end=take(read_actual_common_fact_prefix(c,prefix));
 // Source and every genuine-M boundary are checked before any projection or
 // positive result. Full predecessor replay is independent of today's state.
 const auto events=active_army_events(c,m,exact,naval,now,budget,formations,returns,&prefix,&at,naval_sources,field);
 const auto relevant=actor_active_army_events(events,m,actor,false);
 MilitaryActorActiveArmyRead out;out.actor_id=actor;out.observed_at=at;
 for(const auto& item:relevant){const auto& event=*item.original;if(event.common_position>=end)continue;if(out.active_army_ids!=item.before)movement_detail::fail("MilitaryActorArmyProjectedChainMismatch",actor);out.active_army_ids=item.after;out.original_sources.push_back(event.source);}
 return out;
});}
Outcome<MilitaryHistorySource> read_military_active_army_source(const CommonStore& c,const MilitaryState& m,const MilitaryExactState& exact,const NavalState* n,const SourceRef& source,const WorldMoment& now,ExactArithmeticBudget budget,const MilitaryFormationSourceRead* formations,const MilitaryReturnSourceRead* returns){return movement_detail::guard<MilitaryHistorySource>([&]{const auto events=active_army_events(c,m,&exact,n,now,budget,formations,returns);for(const auto& event:events)if(movement_detail::same_source(source,event.source.source_ref))return event.source;movement_detail::fail("UnsupportedMilitaryActiveArmySource",source.id);});}
Outcome<std::vector<MilitaryActorActiveArmyHeadChange>> read_actor_active_army_head_changes(const CommonStore& c,const MilitaryState& m,const MilitaryExactState& exact,const NavalState* n,const StableId& actor,const WorldMoment& from_at,const std::vector<SourceRef>& from_head,const WorldMoment& to_at,const std::vector<SourceRef>& to_head,const WorldMoment& now,ExactArithmeticBudget b,const MilitaryFormationSourceRead* formations,const MilitaryReturnSourceRead* returns){return movement_detail::guard<std::vector<MilitaryActorActiveArmyHeadChange>>([&]{
 using namespace movement_detail;if(take(compare_world_moments(from_at,to_at,b))>0)movement_detail::fail("MilitaryActiveArmyHeadIntervalReversed",actor);
 const auto from=active_army_read(c,m,exact,n,actor,from_at,&from_head,now,b,formations,returns);const auto to=active_army_read(c,m,exact,n,actor,to_at,&to_head,now,b,formations,returns);
 if(from.original_sources.size()>to.original_sources.size())movement_detail::fail("MilitaryActiveArmyHeadCausalOrderReversed",actor);
 for(std::size_t i=0;i<from.original_sources.size();++i)if(!same_source(from.original_sources[i].source_ref,to.original_sources[i].source_ref))movement_detail::fail("MilitaryActiveArmyHeadNotPrefix",actor);
 const auto events=active_army_events(c,m,&exact,n,now,b,formations,returns);const auto relevant=actor_active_army_events(events,m,actor);std::vector<MilitaryActorActiveArmyHeadChange> out;
 for(std::size_t i=from.original_sources.size();i<to.original_sources.size();++i){const auto& item=relevant[i];const auto& event=*item.original;if(!same_source(event.source.source_ref,to.original_sources[i].source_ref))movement_detail::fail("MilitaryActiveArmyHeadNotPrefix",actor);if(take(compare_world_moments(event.at,from_at,b))<0||take(compare_world_moments(event.at,to_at,b))>0)movement_detail::fail("MilitaryActiveArmyHeadMomentMismatch",event.source.source_ref.id);out.push_back({event.source,event.at,item.before,item.after});}
 if(out.empty()?from.active_army_ids!=to.active_army_ids:(out.front().before_active_army_ids!=from.active_army_ids||out.back().after_active_army_ids!=to.active_army_ids))movement_detail::fail("MilitaryActiveArmyHeadDeltaMismatch",actor);
 return out;
});}
// 目录选中的Field或旧动态根均先核身份；不以空exact根冒原军队历史。
Outcome<MilitaryActorActiveArmyRead> read_actual_actor_active_armies_selected(const CommonStore& c,const MilitaryState& m,const MilitarySelectedHistoryRead& selected,const NavalState* n,const StableId& actor,const WorldMoment& now,ExactArithmeticBudget budget,const MilitaryFormationSourceRead* formations,const MilitaryReturnSourceRead* returns){return movement_detail::guard<MilitaryActorActiveArmyRead>([&]{return active_army_read_selected_impl(c,m,selected.exact,n,actor,now,nullptr,now,budget,formations,returns,&selected);});}
// 保原FSL source-head的M/serial切口，不能用当前Common prefix替代历史头。
Outcome<MilitaryActorActiveArmyRead> read_actor_active_armies_at_selected_source_head(const CommonStore& c,const MilitaryState& m,const MilitarySelectedHistoryRead& selected,const NavalState* n,const StableId& actor,const WorldMoment& at,const std::vector<SourceRef>& head,const WorldMoment& now,ExactArithmeticBudget budget,const MilitaryFormationSourceRead* formations,const MilitaryReturnSourceRead* returns){return movement_detail::guard<MilitaryActorActiveArmyRead>([&]{return active_army_read_selected_impl(c,m,selected.exact,n,actor,at,&head,now,budget,formations,returns,&selected);});}
// 单源也验证全链与Field实际应用M，未知/跨根源保持拒绝。
Outcome<MilitaryHistorySource> read_selected_military_active_army_source(const CommonStore& c,const MilitaryState& m,const MilitarySelectedHistoryRead& selected,const NavalState* n,const SourceRef& source,const WorldMoment& now,ExactArithmeticBudget budget,const MilitaryFormationSourceRead* formations,const MilitaryReturnSourceRead* returns){return movement_detail::guard<MilitaryHistorySource>([&]{using namespace movement_detail;take(validate_selected_military_roster_root(c,selected.identity,selected.exact));take(validate_selected_military_field_history(c,m,selected.identity,selected.field));const auto events=active_army_events(c,m,selected.exact,n,now,budget,formations,returns,nullptr,nullptr,nullptr,selected.field);for(const auto& event:events)if(same_source(source,event.source.source_ref))return event.source;fail("UnsupportedMilitaryActiveArmySource",source.id);});}
// 原两头之间每次真实变更逐条保留，同M join/leave不合并或丢弃。
Outcome<std::vector<MilitaryActorActiveArmyHeadChange>> read_actor_active_army_head_changes_selected(const CommonStore& c,const MilitaryState& m,const MilitarySelectedHistoryRead& selected,const NavalState* n,const StableId& actor,const WorldMoment& from_at,const std::vector<SourceRef>& from_head,const WorldMoment& to_at,const std::vector<SourceRef>& to_head,const WorldMoment& now,ExactArithmeticBudget b,const MilitaryFormationSourceRead* formations,const MilitaryReturnSourceRead* returns){return movement_detail::guard<std::vector<MilitaryActorActiveArmyHeadChange>>([&]{
 using namespace movement_detail;if(take(compare_world_moments(from_at,to_at,b))>0)movement_detail::fail("MilitaryActiveArmyHeadIntervalReversed",actor);
 const auto from=active_army_read_selected_impl(c,m,selected.exact,n,actor,from_at,&from_head,now,b,formations,returns,&selected);const auto to=active_army_read_selected_impl(c,m,selected.exact,n,actor,to_at,&to_head,now,b,formations,returns,&selected);
 if(from.original_sources.size()>to.original_sources.size())movement_detail::fail("MilitaryActiveArmyHeadCausalOrderReversed",actor);
 for(std::size_t i=0;i<from.original_sources.size();++i)if(!same_source(from.original_sources[i].source_ref,to.original_sources[i].source_ref))movement_detail::fail("MilitaryActiveArmyHeadNotPrefix",actor);
 const auto events=active_army_events(c,m,selected.exact,n,now,b,formations,returns,nullptr,nullptr,nullptr,selected.field);const auto relevant=actor_active_army_events(events,m,actor);std::vector<MilitaryActorActiveArmyHeadChange> out;
 for(std::size_t i=from.original_sources.size();i<to.original_sources.size();++i){const auto& item=relevant[i];const auto& event=*item.original;if(!same_source(event.source.source_ref,to.original_sources[i].source_ref))movement_detail::fail("MilitaryActiveArmyHeadNotPrefix",actor);if(take(compare_world_moments(event.at,from_at,b))<0||take(compare_world_moments(event.at,to_at,b))>0)movement_detail::fail("MilitaryActiveArmyHeadMomentMismatch",event.source.source_ref.id);out.push_back({event.source,event.at,item.before,item.after});}
 if(out.empty()?from.active_army_ids!=to.active_army_ids:(out.front().before_active_army_ids!=from.active_army_ids||out.back().after_active_army_ids!=to.active_army_ids))movement_detail::fail("MilitaryActiveArmyHeadDeltaMismatch",actor);
 return out;
});}


}
