// 人员分类/看管链按共同事实先后核证；同 tick 不可用当前看管人倒推旧战果。
#include "naval_casualties.hpp"
#include "naval_commander.hpp"
#include "naval_contact_history.hpp"
#include "occupation_time.hpp"
#include "battle_codec.hpp"
#include "resident/storage/common_codec.hpp"
#include <algorithm>
#include <charconv>
#include <map>
#include <set>
namespace hanmo::resident {
namespace {
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& row:rows)if(row.id==id)return &row;return nullptr;}
bool has(const std::vector<StableId>& rows,const StableId& id){return std::find(rows.begin(),rows.end(),id)!=rows.end();}
template<class T>Outcome<T> fail(const StableId& code,const StableId& path={}){return {false,{},common_failure(code,path).error};}
bool allocated(const StableId& id,const StableId& prefix){if(id.rfind(prefix,0)!=0)return false;Integer n=0;const auto suffix=id.substr(prefix.size());auto p=std::from_chars(suffix.data(),suffix.data()+suffix.size(),n);return p.ec==std::errc{}&&p.ptr==suffix.data()+suffix.size()&&n>0&&std::to_string(n)==suffix;}
MilitaryRosterReceipt row(const MilitaryUnitRecord& unit){MilitaryRosterReceipt out;out.unit_id=unit.id;out.fit=unit.fit;out.wounded=unit.wounded;out.dead=unit.dead;out.captured=unit.captured;out.deserted=unit.deserted;out.member_ids=unit.member_ids;out.morale=unit.morale;out.captors=unit.captors;return out;}
MilitaryRosterReceipt row(const BattleUnit& unit){MilitaryRosterReceipt out;out.unit_id=unit.unit_id;out.fit=unit.fit;out.wounded=unit.wounded;out.dead=unit.dead;out.captured=unit.captured;out.deserted=unit.deserted;out.member_ids=unit.member_ids;out.morale=unit.morale;return out;}
void erase(std::vector<StableId>& rows,const StableId& id){rows.erase(std::remove(rows.begin(),rows.end(),id),rows.end());}
// 最近原伤亡收据之后的接管/整编仅改看管责任，人员原籍和分类保持原账。
Outcome<std::vector<std::pair<StableId,StableId>>> prior_captors(const CommonStore& common,const MilitaryState& military,const NavalState& naval,const StableId& unit_id,const StableId& before,const NavalOriginalSourceContext* original){
 std::vector<std::pair<StableId,StableId>> keepers;
 for(const auto& id:common.state().facts.order()){
  if(id==before)return {true,std::move(keepers),{}};
  if(original){const auto* head=common.state().facts.find(id);if(!head)return fail<decltype(keepers)>("NavalCustodySourceRequired",id);const auto* old=find(military.casualty_facts,id);const auto* precise=original->exact.dynamic_casualty_facts.find(id);
   if(old||precise||head->kind=="military_casualties"){if(head->domain!="military"||head->kind!="military_casualties"||bool(old)==bool(precise))return fail<decltype(keepers)>("MilitaryCasualtyPayloadCoverage",id);auto now=original->authority.office.clock.read_current_moment();if(!now.ok)return {false,{},now.error};
    if(old){if(old->tick<0||old->tick>now.value.half_day_tick)return fail<decltype(keepers)>("NavalCasualtyHistoryClockMismatch",id);for(const auto& unit:old->units)if(unit.unit_id==unit_id)keepers=unit.captors;}
    else{const MilitaryCasualtyHistoryContext context{common,now.value,&original->formations,&original->returns,MilitaryCasualtyHistoryOrder::actual_common_prefix};auto source=read_military_casualty_source(military,original->exact,{head->domain,head->kind,head->id},2,original->authority.office.budget,&context);if(!source.ok)return {false,{},source.error};auto checked=validate_military_source_in_actual_window(common,SourceRefBuildView{head->domain,head->kind,head->id},source.value.occurred_at,now.value,original->window,original->authority.office.budget);if(!checked.ok)return {false,{},checked.error};for(const auto& unit:source.value.roster_receipts)if(unit.unit_id==unit_id)keepers=unit.captors;}
    continue;
   }
  }else if(const auto* receipt=find(military.casualty_facts,id)){for(const auto& unit:receipt->units)if(unit.unit_id==unit_id)keepers=unit.captors;continue;}
  auto move=[&](const StableId& person,const StableId& from,const StableId& to)->bool{for(auto& keeper:keepers)if(keeper.first==person){if(keeper.second!=from)return false;keeper.second=to;}return true;};
  if(const auto* occupation=find(military.occupation_facts,id);occupation&&occupation->custody_transfers){if(original&&occupation->exact_time){auto now=original->authority.office.clock.read_current_moment();if(!now.ok)return {false,{},now.error};const SourceRef ref{"military","military_occupation",id};auto occurred=read_military_occupation_actual_moment(common,military,original->authority.office.catalog,ref,2,now.value,original->authority.office.budget);if(!occurred.ok)return {false,{},occurred.error};auto checked=validate_military_source_in_actual_window(common,ref,occurred.value,now.value,original->window,original->authority.office.budget);if(!checked.ok)return {false,{},checked.error};}for(const auto& transfer:*occupation->custody_transfers)if(!move(transfer.person_id,transfer.from_army_id,transfer.to_army_id))return fail<decltype(keepers)>("NavalCustodyPredecessorMismatch",id);}
  if(const auto* integration=find(military.integration_facts,id))for(const auto& transfer:integration->custody)if(!move(transfer.member_id,transfer.from_army_id,transfer.to_army_id))return fail<decltype(keepers)>("NavalCustodyPredecessorMismatch",id);
  if(const auto* capture=find(naval.battle.captures,id)){const auto* from=find(naval.fleets,capture->from_fleet_id);const auto* to=find(naval.fleets,capture->to_fleet_id);if(!from||!to)return fail<decltype(keepers)>("NavalCaptureFleetMismatch",id);for(const auto& person:capture->original_prisoner_ids)if(!move(person,from->army_id,to->army_id))return fail<decltype(keepers)>("NavalCustodyPredecessorMismatch",id);}
 }
 return fail<decltype(keepers)>("NavalCasualtySourceMissing",before);
}
}
// 同一轮的局部标记只能单向变化，部将名单不重复算作新兵，也不产生第二份死亡来源。
Outcome<std::vector<MilitaryActorChange>> naval_round_actor_changes(const CommonRead& common,const NavalTacticalFrame& before,const NavalTacticalFrame& after){
 std::vector<MilitaryActorChange> result;std::set<StableId> actors;
 for(const auto& prior:before.units){const BattleUnit* next=nullptr;for(const auto& u:after.units)if(u.unit_id==prior.unit_id)next=&u;
  if(!next||next->army_id!=prior.army_id||next->commander_actor_id!=prior.commander_actor_id||!valid_naval_commander_loss(prior)||!valid_naval_commander_loss(*next)||naval_commander_loss(*next)<naval_commander_loss(prior))return fail<decltype(result)>("NavalCommanderLossSourceMismatch",prior.unit_id);
  auto add=[&](const StableId& person,const StableId& status)->Status{if(!common.actor(person)||!actors.insert(person).second)return common_failure("NavalCasualtyActorMismatch",person);MilitaryActorChange change;change.actor_id=person;change.army_id=prior.army_id;change.status=status;result.push_back(std::move(change));return common_success();};
  for(const auto& person:prior.member_ids)if(common.actor(person)){StableId status;if(has(next->dead,person)&&!has(prior.dead,person))status="dead";else if(has(next->wounded,person)&&!has(prior.wounded,person))status="wounded";if(!status.empty()){auto checked=add(person,status);if(!checked.ok)return {false,{},checked.error};}}
  if(naval_commander_loss(*next)>naval_commander_loss(prior)&&!has(prior.member_ids,prior.commander_actor_id)){auto checked=add(prior.commander_actor_id,naval_commander_loss(*next)==2?"dead":"wounded");if(!checked.ok)return {false,{},checked.error};}
 }
 return {true,std::move(result),{}};
}
// 仅从结算后的唯一军事单位提取收据；传入的是原既有单位 ID，不能生成缺失人员。
Outcome<MilitaryCasualtyFact> make_naval_casualty_receipt(const MilitaryState& military,const StableId& id,const StableId& source,Integer tick,const std::vector<StableId>& units,const std::vector<MilitaryActorChange>& changes){
 if(!allocated(id,"military:")||(!allocated(source,"naval_round:")&&!allocated(source,"naval_capture:"))||tick<0)return fail<MilitaryCasualtyFact>("InvalidNavalCasualtySource",source);
 MilitaryCasualtyFact out;out.id=id;out.battle_id=source;out.tick=tick;out.actor_changes=changes;std::set<StableId> seen;for(const auto& name:units){const auto* unit=find(military.military_units,name);if(!unit||!seen.insert(name).second)return fail<MilitaryCasualtyFact>("NavalCasualtyUnitMismatch",name);std::set<StableId> members(unit->member_ids.begin(),unit->member_ids.end()),classified;bool valid=members.size()==unit->member_ids.size()&&!members.count("");for(const auto* group:{&unit->fit,&unit->wounded,&unit->dead,&unit->captured,&unit->deserted})for(const auto& person:*group)if(!classified.insert(person).second)valid=false;std::set<StableId> captured;for(const auto& keeper:unit->captors)if(keeper.second.empty()||!has(unit->captured,keeper.first)||!captured.insert(keeper.first).second)valid=false;if(!valid||classified!=members||captured.size()!=unit->captured.size()||unit->morale<0||unit->morale>100)return fail<MilitaryCasualtyFact>("NavalCasualtyRosterMismatch",name);out.units.push_back(row(*unit));}std::set<StableId> actors;for(const auto& change:changes)if(change.actor_id.empty()||!actors.insert(change.actor_id).second||(change.status!="dead"&&change.status!="wounded"&&change.status!="captured"))return fail<MilitaryCasualtyFact>("NavalCasualtyActorMismatch",change.actor_id);if(out.units.empty())return fail<MilitaryCasualtyFact>("NavalCasualtyUnitMismatch",source);return {true,std::move(out),{}};
}
// Naval 来源、Common 头、原军旅收据保持严格先后，人口不能只因 ID 前缀便接受伪战损。
static Status casualty_receipt(const CommonStore& common,const MilitaryState& military,const NavalState& naval,const StableId& id,Integer limit,const NavalOriginalSourceContext* original){
 const auto* receipt=find(military.casualty_facts,id);if(original&&original->exact.dynamic_casualty_facts.find(id))return common_failure("MilitarySourcePayloadDuplicated",id);auto bad=[&](){return common_failure("NavalCasualtySourceMismatch",id);};if(!receipt||receipt->tick<0||receipt->tick>limit||!allocated(id,"military:"))return bad();
 std::map<StableId,std::size_t> positions;for(std::size_t n=0;n<common.state().facts.order().size();++n)positions.emplace(common.state().facts.order()[n],n);
 auto header=[&](const StableId& key,const StableId& kind,const StableId& domain){const auto* h=common.state().facts.find(key);return h&&h->kind==kind&&h->domain==domain&&positions.count(key);};
 if(!header(id,"military_casualties","military"))return bad();for(const auto& change:receipt->actor_changes)if(change.status=="dead"){const auto* actor=common.actor(change.actor_id);if(!actor||actor->alive||actor->available||actor->hp!=0||!actor->action_id.empty())return bad();}for(const auto& change:receipt->actor_changes)for(const auto& prior:military.casualty_facts)if(prior.id!=id&&positions.count(prior.id))for(const auto& old:prior.actor_changes)if(old.actor_id==change.actor_id&&old.status=="dead"&&(positions.at(prior.id)<positions.at(id)||change.status=="dead"))return bad();const auto* round=find(naval.battle.rounds,receipt->battle_id);const auto* capture=find(naval.battle.captures,receipt->battle_id);if(bool(round)==bool(capture))return bad();
 const auto* underlying=capture?find(naval.battle.rounds,capture->round_source_id):round;if(!underlying)return bad();const NavalBattleRecord* battle=nullptr;for(const auto& candidate:naval.battle.battles)if(candidate.frame.id==underlying->battle_id)battle=&candidate;const auto* contact=battle?find(naval.battle.contacts,battle->contact_source_id):nullptr;
 if(!contact||contact->battle_id!=battle->frame.id||!header(contact->id,"naval_contact","military_naval")||!header(underlying->id,"naval_round","military_naval")||positions[contact->id]>=positions[underlying->id])return bad();
 if(!header(receipt->battle_id,round?"naval_round":"naval_capture","military_naval")||positions[receipt->battle_id]>=positions[id]||(capture&&positions[underlying->id]>=positions[capture->id]))return bad();
 if((round?round->casualty_source_id:capture->casualty_source_id)!=id||receipt->tick!=(round?round->tick:capture->tick))return bad();
 MilitaryCasualtyFact expected;expected.id=id;expected.battle_id=receipt->battle_id;expected.tick=receipt->tick;
 if(round){
  for(const auto& after:round->after.units){const BattleUnit* before=nullptr;for(const auto& unit:round->before.units)if(unit.unit_id==after.unit_id)before=&unit;if(!before||after.captured!=before->captured||after.deserted!=before->deserted||after.member_ids!=before->member_ids)return bad();auto r=row(after);auto prior=prior_captors(common,military,naval,after.unit_id,round->id,original);if(!prior.ok)return {false,{},prior.error};r.captors=std::move(prior.value);expected.units.push_back(std::move(r));

  }
  auto changes=naval_round_actor_changes(common,round->before,round->after);if(!changes.ok)return {false,{},changes.error};expected.actor_changes=std::move(changes.value);
 }else{
  const NavalBattleBinding* binding=nullptr;for(const auto& b:underlying->after.bindings)if(b.vessel_id==capture->vessel_id)binding=&b;const BattleUnit* unit=nullptr;if(binding)for(const auto& u:underlying->after.units)if(u.unit_id==binding->unit_id)unit=&u;const auto* losing=find(naval.fleets,capture->from_fleet_id);const auto* winning=find(naval.fleets,capture->to_fleet_id);if(!unit||!losing||!winning||unit->army_id!=losing->army_id)return bad();auto r=row(*unit);auto prior=prior_captors(common,military,naval,unit->unit_id,capture->id,original);if(!prior.ok)return {false,{},prior.error};r.captors=std::move(prior.value);
  for(const auto& person:capture->prisoner_ids){if(has(r.fit,person)||has(r.wounded,person)){erase(r.fit,person);erase(r.wounded,person);r.captured.push_back(person);r.captors.emplace_back(person,winning->army_id);}if(common.actor(person)){MilitaryActorChange change;change.actor_id=person;change.army_id=losing->army_id;change.captor_army_id=winning->army_id;change.status="captured";expected.actor_changes.push_back(std::move(change));}}
  for(const auto& person:capture->original_prisoner_ids){for(auto& keeper:r.captors)if(keeper.first==person){if(keeper.second!=losing->army_id)return bad();keeper.second=winning->army_id;}if(common.actor(person)){MilitaryActorChange change;change.actor_id=person;change.army_id=losing->army_id;change.captor_army_id=winning->army_id;change.status="captured";expected.actor_changes.push_back(std::move(change));}}
  r.morale=0;expected.units.push_back(std::move(r));
 }
 try{if(!compare_boundary(battle_codec::encode(expected),battle_codec::encode(*receipt)).values_equal)return bad();}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}return common_success();
}
Status validate_naval_casualty_receipt(const CommonStore& c,const MilitaryState& m,const NavalState& n,const StableId& id,Integer limit){return casualty_receipt(c,m,n,id,limit,nullptr);}
Status validate_naval_casualty_receipt(const CommonStore& c,const MilitaryState& m,const NavalState& n,const StableId& id,Integer limit,const NavalOriginalSourceContext& original){auto bound=validate_naval_original_source_context(original,c,m,original.authority.office.catalog,original.authority.office.access);if(!bound.ok)return bound;const auto* receipt=find(m.casualty_facts,id);if(!receipt)return common_failure("NavalCasualtySourceMismatch",id);for(const auto& change:receipt->actor_changes){auto deaths=change.status=="dead"?read_naval_receipt_other_deaths_source(original,change.actor_id,id):read_naval_receipt_prior_death_source(original,change.actor_id,id);if(!deaths.ok)return {false,{},deaths.error};if(deaths.value)return common_failure("NavalCasualtySourceMismatch",id);}return casualty_receipt(c,m,n,id,limit,&original);}
// 本域来源必须是唯一同人首次死亡与同场原舰队，不能由当前 alive=false 或任意水点生成。
Outcome<std::optional<NavalRemainsLocationSource>> naval_remains_location(const CommonRead& common,const MilitaryState& military,const NavalBattleState& naval,const StableId& actor_id){
 const auto* actor=common.actor(actor_id);if(!actor)return fail<std::optional<NavalRemainsLocationSource>>("UnknownActor",actor_id);if(actor->alive)return {true,std::nullopt,{}};std::optional<NavalRemainsLocationSource> out;
 for(const auto& receipt:military.casualty_facts)for(const auto& change:receipt.actor_changes)if(change.actor_id==actor_id&&change.status=="dead"){
  const auto* round=find(naval.rounds,receipt.battle_id);if(!round)continue;const NavalBattleRecord* battle=nullptr;for(const auto& b:naval.battles)if(b.frame.id==round->battle_id)battle=&b;const auto* contact=battle?find(naval.contacts,battle->contact_source_id):nullptr;const NavalBattleBinding* binding=nullptr;
  for(const auto& unit:round->before.units)if(unit.army_id==change.army_id&&(unit.commander_actor_id==actor_id||has(unit.member_ids,actor_id)))for(const auto& link:round->before.bindings)if(link.unit_id==unit.unit_id)binding=&link;
  if(out||!contact||!binding||receipt.id!=round->casualty_source_id||receipt.tick!=round->tick||receipt.tick<0||receipt.tick>common.tick()||actor->available||actor->hp!=0||!actor->action_id.empty())return fail<std::optional<NavalRemainsLocationSource>>("NavalRemainsSourceMismatch",actor_id);
  const auto* anchor=binding->fleet_id==contact->first_fleet_id?&contact->first_anchor:binding->fleet_id==contact->second_fleet_id?&contact->second_anchor:nullptr;const auto* at=std::get_if<WildernessLocation>(&actor->location);if(!anchor||!at||at->map_id!=anchor->map_id||at->origin!=anchor->origin||at->point.x!=anchor->point.x||at->point.y!=anchor->point.y)return fail<std::optional<NavalRemainsLocationSource>>("NavalRemainsAnchorMismatch",actor_id);
  out=NavalRemainsLocationSource{actor_id,binding->fleet_id,change.army_id,round->id,receipt.id,*anchor};
 }
 return {true,std::move(out),{}};
}
// 公共冷读同时核实际 Common 头和具名收据，完整战术重放仍由原 World validator 负责。
Outcome<std::optional<NavalRemainsLocationSource>> query_naval_remains_location(const CommonStore& common,const MilitaryState& military,const NavalState& naval,const StableId& actor_id){auto source=naval_remains_location(common,military,naval.battle,actor_id);if(!source.ok||!source.value)return source;auto checked=validate_naval_casualty_receipt(common,military,naval,source.value->casualty_id,common.tick());if(!checked.ok)return {false,{},checked.error};return source;}

}
