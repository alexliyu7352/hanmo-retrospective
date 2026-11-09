// 接管只读报价按实船、原兵员和原Common容器逐项核证，失败没有任何资源副作用。
#include "naval_capture.hpp"
#include "naval_manifest_history.hpp"
#include "military_field_command_source.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
namespace {
// 唯一账中缺引用即拒绝，不能为俘获临时造一名船长或一只口袋。
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& row:rows)if(row.id==id)return &row;return nullptr;}
const NavalBattleRecord* battle(const NavalBattleState& s,const StableId& id){for(const auto& b:s.battles)if(b.frame.id==id)return &b;return nullptr;}
const BattleUnit* unit(const NavalTacticalFrame& f,const StableId& id){for(const auto& u:f.units)if(u.unit_id==id)return &u;return nullptr;}
const NavalBattleBinding* binding(const NavalTacticalFrame& f,const StableId& id){for(const auto& b:f.bindings)if(b.vessel_id==id)return &b;return nullptr;}
bool has(const std::vector<StableId>& rows,const StableId& id){return std::find(rows.begin(),rows.end(),id)!=rows.end();}
bool same_owner(const Owner& a,const Owner& b){return !a.extra&&!b.extra&&a.kind==b.kind&&a.id==b.id;}
template<class T>Outcome<T> fail(const StableId& code,const StableId& id={}){return {false,{},common_failure(code,id).error};}
// 原行程在接敌锚点被整场占用时允许战斗命令，但不能借同址绕过别的行动。
bool held_travel(const CommonRead& common,const NavalContactFact& contact,const ActorCoreView& actor,const StableId& leader){
 if(actor.action_id.empty())return true;const auto* action=common.action(actor.action_id);const auto* motion=contact.first_motion.actor_id==leader?&contact.first_motion:contact.second_motion.actor_id==leader?&contact.second_motion:nullptr;
 return motion&&!motion->stationary&&motion->action_id==actor.action_id&&action&&action->kind=="travel"&&action->actor_id==leader&&has(action->participants,actor.id);
}
std::vector<StableId> sorted(std::vector<StableId> ids){std::sort(ids.begin(),ids.end());return ids;}
}
// 持久claim须先通过完整冷验证；此处再核当前权源/部队/船货，不能把client的胜利声明当claim。
Outcome<NavalCaptureQuote> preview_naval_capture(const CommonRead& common,const MilitaryState& military,const MilitaryCatalog& army_catalog,const NavalAssetState& assets,const NavalAssetCatalog& ships,const NavalBattleState& battles,const StableId& actor_id,const NavalCaptureCommand& command){
 if(!naval_request_key_valid(command.request_key))return fail<NavalCaptureQuote>("InvalidNavalRequestKey");const auto* current=battle(battles,command.battle_id);const auto* round=find(battles.rounds,command.round_source_id);const auto* ship=find(assets.vessels,command.vessel_id);
 if(!current||!round||round->battle_id!=command.battle_id||round->round!=current->frame.round||current->frame.phase!="settlement"||current->round_source_ids.empty()||current->round_source_ids.back()!=round->id||!ship)return fail<NavalCaptureQuote>("NavalCaptureRoundRequired");
 const auto* link=binding(round->after,ship->id);const NavalCaptureClaim* claim=nullptr;for(const auto& c:round->claims)if(link&&c.unit_id==link->unit_id&&c.captor_unit_id==command.captor_unit_id){if(claim)return fail<NavalCaptureQuote>("AmbiguousNavalCaptureClaim");claim=&c;}
 if(!claim||(claim->kind!="boarding_win"&&claim->kind!="surrender")||claim->round!=round->round)return fail<NavalCaptureQuote>("NavalBoardingOrSurrenderRequired");
 for(const auto& f:battles.declines)if(f.round_source_id==round->id&&f.vessel_id==ship->id)return fail<NavalCaptureQuote>("NavalClaimAlreadyDeclined",f.id);
 for(const auto& f:battles.captures)if((f.round_source_id==round->id&&(f.vessel_id==ship->id||f.captor_unit_id==command.captor_unit_id))||(f.actor_id==actor_id&&f.request_key==command.request_key))return fail<NavalCaptureQuote>("NavalCaptureAlreadyRecorded",f.id);
 const auto* taker=unit(round->after,claim->captor_unit_id);const auto* defeated=unit(round->after,claim->unit_id);const auto* winner=taker?find(military.armies,taker->army_id):nullptr;const auto* loser=defeated?find(military.armies,defeated->army_id):nullptr;const auto* actor=common.actor(actor_id);
 if(!taker||!defeated||!winner||!loser||winner->status!="active"||loser->status!="active"||winner->commander_actor_id!=actor_id||!actor||!actor->alive||!actor->available||taker->fit.empty()||taker->morale<=0||taker->withdrawn||taker->side==defeated->side||claim->captor_army_id!=winner->id)return fail<NavalCaptureQuote>("NavalCaptureAuthorityRequired");
 // 真正完成整军接任才继受本军战果处置；仅签受或Unit接任不构成Army权源。
 StableId authority_source;
 if(const auto* field=current_military_field_army_assignment(military,army_catalog,winner->id,actor_id)){if(field->tick<0||field->tick>common.tick()||field->target.fleet_id.empty())return fail<NavalCaptureQuote>("NavalCaptureAuthorityRequired");authority_source=field->id;}
 else{const auto* campaign=find(military.campaigns,winner->order_id);const auto* office=campaign?find(military.offices,campaign->office_id):nullptr;const auto* definition=office?find(army_catalog.offices,office->id):nullptr;
  if(!office||!definition||!office->enabled||office->holder_actor_id!=actor_id||definition->organization_id!=winner->organization_id||(definition->command_scope!="army"&&winner->unit_ids.size()!=1))return fail<NavalCaptureQuote>("NavalCaptureAuthorityRequired");
  const auto* appointment=find(military.action_facts,office->appointment_id);const auto* terms=appointment?std::get_if<OfficeTerms>(&appointment->terms):nullptr;if(!appointment||appointment->command!="office_accept"||appointment->phase!="completed"||!appointment->success||appointment->actor_id!=actor_id||appointment->completed_tick>common.tick()||!terms||terms->office_id!=office->id||terms->actor_id!=actor_id||terms->organization_id!=winner->organization_id)return fail<NavalCaptureQuote>("NavalCaptureAppointmentRequired");authority_source=appointment->id;
 }
 const NavalFleetRecord* winning=nullptr;for(const auto& f:assets.fleets)if(f.status=="active"&&f.army_id==winner->id){if(winning)return fail<NavalCaptureQuote>("NavalArmyAssignedTwice");winning=&f;}const auto* losing=find(assets.fleets,link->fleet_id);
 if(const auto* field=current_military_field_army_assignment(military,army_catalog,winner->id,actor_id))if(!winning||field->target.fleet_id!=winning->id)return fail<NavalCaptureQuote>("NavalCaptureFleetMismatch");
 if(!winning||!losing||losing->status!="active"||ship->fleet_id!=losing->id||losing->army_id!=loser->id||losing->id==winning->id)return fail<NavalCaptureQuote>("NavalCaptureFleetMismatch");
 const auto* contact=find(battles.contacts,current->contact_source_id);if(!contact)return fail<NavalCaptureQuote>("NavalContactSourceRequired");if(!held_travel(common,*contact,*actor,winner->commander_actor_id))return fail<NavalCaptureQuote>("NavalPersonOccupied",actor_id);const auto* anchor=contact->first_fleet_id==winning->id?&contact->first_anchor:contact->second_fleet_id==winning->id?&contact->second_anchor:nullptr;
 const auto* actual=std::get_if<WildernessLocation>(&actor->location);if(!anchor||!actual||anchor->map_id!=actual->map_id||anchor->point.x!=actual->point.x||anchor->point.y!=actual->point.y)return fail<NavalCaptureQuote>("NavalCaptureMustBePresent");
 const auto* defeated_anchor=contact->first_fleet_id==losing->id?&contact->first_anchor:contact->second_fleet_id==losing->id?&contact->second_anchor:nullptr;const auto* enemy_leader=common.actor(loser->commander_actor_id);const auto* enemy_at=enemy_leader?std::get_if<WildernessLocation>(&enemy_leader->location):nullptr;if(!defeated_anchor||!enemy_at||enemy_at->map_id!=defeated_anchor->map_id||enemy_at->point.x!=defeated_anchor->point.x||enemy_at->point.y!=defeated_anchor->point.y)return fail<NavalCaptureQuote>("NavalCaptureTargetMoved");
 // 当前真实名册必须与同轮已结算兵损一致，坏来源不得复活死者或多造可俘获者。
 const auto* defeated_unit=find(military.military_units,defeated->unit_id);auto expected=defeated->fit;expected.insert(expected.end(),defeated->wounded.begin(),defeated->wounded.end());if(!defeated_unit||defeated_unit->army_id!=loser->id||defeated_unit->fit!=defeated->fit||defeated_unit->wounded!=defeated->wounded||sorted(expected)!=sorted(claim->survivor_ids))return fail<NavalCaptureQuote>("NavalCaptureRosterChanged");
 const auto* taker_unit=find(military.military_units,taker->unit_id);if(!taker_unit||taker_unit->army_id!=winner->id||taker_unit->fit!=taker->fit||taker_unit->wounded!=taker->wounded)return fail<NavalCaptureQuote>("NavalCaptureRosterChanged");
 NavalCaptureQuote quote;quote.authority_source=authority_source;quote.round_source_id=round->id;quote.vessel_id=ship->id;quote.from_fleet_id=losing->id;quote.to_fleet_id=winning->id;quote.captor_unit_id=taker->unit_id;quote.from_owner=ship->owner;quote.to_owner.kind=Owner::Kind::organization;quote.to_owner.id=winner->organization_id;quote.anchor=*anchor;quote.prisoner_ids=expected;quote.assignments=command.assignments;
 const NavalVesselAssignment* old=nullptr;for(const auto& a:losing->assignments)if(a.vessel_id==ship->id)old=&a;if(!old)return fail<NavalCaptureQuote>("NavalCaptureFleetMismatch");
 auto add_person=[&](const StableId& id){const auto* p=common.actor(id);if(p&&p->alive&&!has(quote.prisoner_ids,id))quote.prisoner_ids.push_back(id);};add_person(defeated->commander_actor_id);for(const auto& id:old->passenger_ids)add_person(id);quote.original_prisoner_ids=old->prisoner_ids;
 if(has(quote.prisoner_ids,loser->commander_actor_id)&&losing->assignments.size()!=1)return fail<NavalCaptureQuote>("NavalCommandVesselMustBeLast",ship->id);
 auto observed=observe_naval_combat_manifest(common,military,assets,battles,*winning);if(!observed.ok)return {false,{},observed.error};auto observation=std::move(observed.value);std::set<StableId> added;
 // 私人行囊保持私产；仅船舱当前Owner及同主无预留货物随原船产权recipe转移。
 const auto* hold=common.inventory(ship->cargo_inventory_id);if(!hold||!same_owner(hold->owner,ship->owner))return fail<NavalCaptureQuote>("NavalCargoSourceMismatch",ship->id);
 for(const auto& lot:hold->lots)if(!lot.reserved_by.empty())return fail<NavalCaptureQuote>("NavalCapturedCargoReserved",ship->id);
 for(const auto& lot:hold->lots)quote.cargo.push_back({hold->id,lot.id,lot.item_id,lot.quantity,lot.owner,same_owner(lot.owner,ship->owner)?quote.to_owner:lot.owner,{}});
 quote.resources.operations.push_back(SetInventoryOwner{hold->id,quote.to_owner});for(const auto& lot:hold->lots)if(same_owner(lot.owner,ship->owner))quote.resources.operations.push_back(SetLotOwner{hold->id,lot.id,quote.to_owner,false});quote.resources.operations.push_back(AdjustLots{hold->id,{},true});
 auto hold_weight=common.inventory_weight(hold->id);if(!hold_weight.ok)return {false,{},hold_weight.error};Integer captured_weight=hold_weight.value;
 for(const auto& id:old->extra_inventory_ids){const auto* extra=common.inventory(id);if(id!=loser->inventory_id||!extra||extra->owner.kind!=Owner::Kind::organization||extra->owner.id!=loser->organization_id)return fail<NavalCaptureQuote>("NavalCaptureCargoSourceMismatch",id);auto weight=common.inventory_weight(id);if(!weight.ok)return {false,{},weight.error};if(captured_weight>hold->capacity||weight.value>hold->capacity-captured_weight)return fail<NavalCaptureQuote>("NavalCapturedHoldOverCapacity",ship->id);captured_weight+=weight.value;
  for(const auto& lot:extra->lots){if(!lot.reserved_by.empty())return fail<NavalCaptureQuote>("NavalCapturedCargoReserved",lot.id);if(!same_owner(lot.owner,extra->owner))return fail<NavalCaptureQuote>("NavalCapturedExtraCargoProtected",lot.id);quote.resources.operations.push_back(MoveIntegrationLot{id,hold->id,lot.id,lot.quantity,lot.owner,quote.to_owner});quote.cargo.push_back({id,lot.id,lot.item_id,lot.quantity,lot.owner,quote.to_owner,{}});}
  quote.emptied_inventory_ids.push_back(id);quote.resources.expected_versions.push_back(common.read_version({ResourceKey::Kind::inventory,id}));
 }
 observation.inventories.push_back({hold->id,quote.to_owner,captured_weight});
 for(const auto* ids:{&quote.prisoner_ids,&quote.original_prisoner_ids})for(const auto& id:*ids){if(!added.insert(id).second||has(observation.prisoner_ids,id)||has(observation.army_member_ids,id))return fail<NavalCaptureQuote>("NavalCaptiveAlreadyAssigned",id);observation.prisoner_ids.push_back(id);if(const auto* p=common.actor(id)){if(!held_travel(common,*contact,*p,loser->commander_actor_id))return fail<NavalCaptureQuote>("NavalPersonOccupied",id);if(id==loser->commander_actor_id&&!p->action_id.empty()&&!has(quote.interrupted_action_ids,p->action_id))quote.interrupted_action_ids.push_back(p->action_id);observation.people.push_back({id,p->inventory_id,p->alive,false,0,{}});const auto* bag=common.inventory(p->inventory_id);auto weight=common.inventory_weight(p->inventory_id);if(!bag||!weight.ok)return fail<NavalCaptureQuote>("NavalInventorySourceRequired",id);observation.inventories.push_back({bag->id,bag->owner,weight.value});}}
 // 接管只移动实际登船的本部及其原随行载荷；不能借零时长接管把远处友军换船。
 for(const auto& previous:winning->assignments){const NavalVesselAssignment* changed=nullptr;for(const auto& proposed:command.assignments)if(proposed.unit_id==previous.unit_id){if(changed)return fail<NavalCaptureQuote>("NavalCaptureManifestSourceMismatch",proposed.unit_id);changed=&proposed;}
  const auto expected_vessel=previous.unit_id==taker->unit_id?ship->id:previous.vessel_id;
  if(!changed||changed->vessel_id!=expected_vessel||changed->passenger_ids!=previous.passenger_ids||changed->extra_inventory_ids!=previous.extra_inventory_ids)return fail<NavalCaptureQuote>("NavalCaptureMustUseBoardingCrew",previous.unit_id);
  auto prisoners=previous.prisoner_ids;if(previous.unit_id==taker->unit_id){prisoners.insert(prisoners.end(),quote.prisoner_ids.begin(),quote.prisoner_ids.end());prisoners.insert(prisoners.end(),quote.original_prisoner_ids.begin(),quote.original_prisoner_ids.end());}
  if(sorted(changed->prisoner_ids)!=sorted(prisoners))return fail<NavalCaptureQuote>("NavalCaptivesMustStayOnBoardedShip",previous.unit_id);
 }
 // 只容许原己方船或本次实际夺取船；移动部队不等于把它的旧船和舱货瞬移一起搬走。
 std::set<StableId> permitted{ship->id},selected;for(const auto& a:winning->assignments)permitted.insert(a.vessel_id);for(const auto& a:command.assignments)if(!permitted.count(a.vessel_id)||!selected.insert(a.vessel_id).second)return fail<NavalCaptureQuote>("NavalCaptureManifestSourceMismatch",a.vessel_id);if(!selected.count(ship->id))return fail<NavalCaptureQuote>("NavalCapturedVesselMustBeCrewed");for(const auto& a:winning->assignments)if(!selected.count(a.vessel_id))quote.released_vessel_ids.push_back(a.vessel_id);
 auto projected=assets;projected.fleets.erase(std::remove_if(projected.fleets.begin(),projected.fleets.end(),[](const auto& f){return f.status=="captured";}),projected.fleets.end());for(auto& v:projected.vessels)if(permitted.count(v.id)){v.fleet_id.clear();v.berth_id=observation.berth_id;v.afloat_location.reset();if(v.id==ship->id)v.owner=quote.to_owner;}
 projected.fleets.erase(std::remove_if(projected.fleets.begin(),projected.fleets.end(),[&](const auto& f){return f.id==winning->id;}),projected.fleets.end());for(auto& f:projected.fleets)if(f.id==losing->id)f.assignments.erase(std::remove_if(f.assignments.begin(),f.assignments.end(),[&](const auto& a){return a.vessel_id==ship->id;}),f.assignments.end());projected.fleets.erase(std::remove_if(projected.fleets.begin(),projected.fleets.end(),[](const auto& f){return f.assignments.empty();}),projected.fleets.end());
 auto loads=preview_naval_formation(projected,ships,observation,{winner->id,command.assignments},true);if(!loads.ok)return {false,{},loads.error};quote.loads=std::move(loads.value.loads);
 for(const auto& inv:observation.inventories)quote.resources.expected_versions.push_back(common.read_version({ResourceKey::Kind::inventory,inv.id}));for(const auto& person:observation.people)quote.resources.expected_versions.push_back(common.read_version({ResourceKey::Kind::actor,person.id}));return {true,std::move(quote),{}};
}
}
