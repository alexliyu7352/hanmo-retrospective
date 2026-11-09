// 只有实际实船才有水上机动；船损/士气不把幸存者默认为漂失、陆上逃兵或已被俘。
#include "naval_tactics.hpp"
#include "naval_commander.hpp"
#include "resident/model/common_operations.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
namespace {
namespace H=military_hex;
const std::map<StableId,Integer> water_costs{{"water",1}};
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& r:rows)if(r.id==id)return &r;return nullptr;}
const BattleUnit* unit(const NavalTacticalFrame& f,const StableId& id){for(const auto& u:f.units)if(u.unit_id==id)return &u;return nullptr;}
const NavalBattleBinding* binding(const NavalTacticalFrame& f,const StableId& id){for(const auto& b:f.bindings)if(b.unit_id==id)return &b;return nullptr;}
bool has(const std::vector<StableId>& rows,const StableId& id){return std::find(rows.begin(),rows.end(),id)!=rows.end();}
template<class T>Outcome<T> fail(const StableId& code,const StableId& id={}){return {false,{},common_failure(code,id).error};}
// 溃退/撤离仍占船；只有可工作的本场原兵员和完好实船能发起机动或接舷。
bool crew_active(const BattleUnit& u){return !u.withdrawn&&u.morale>0&&!u.fit.empty()&&naval_commander_loss(u)!=2;}
// 失能只禁止推进船体；原幸存船员仍可在实际相邻的船上防守/接舷。
bool can_sail(const NavalAssetState& s,const NavalAssetCatalog& c,const NavalTacticalFrame& f,const BattleUnit& u){const auto* b=binding(f,u.unit_id);const auto* v=b?find(s.vessels,b->vessel_id):nullptr;const auto* h=v?find(c.hulls,v->hull_id):nullptr;return h&&v->integrity>0&&crew_active(u)&&Integer(u.fit.size())>=h->minimum_crew;}
}
// 战场绑定须对应本舰队的同一真实unit，不得把别人的船当作本部移动点来源。
Status validate_naval_tactical_frame(const NavalAssetState& s,const NavalAssetCatalog& c,const BattlefieldDefinition& board,const NavalTacticalFrame& f){
 auto valid=validate_naval_assets(s,c,true);if(!valid.ok)return valid;valid=H::validate(board);if(!valid.ok)return valid;
 if(f.id.empty()||f.source_id.empty()||!has({"planning","settlement"},f.phase)||f.battlefield_id!=board.id||f.round<1||f.round>9999||f.controller_side<0||f.controller_side>1||f.units.empty()||f.units.size()>128||f.bindings.size()!=f.units.size())return common_failure("InvalidNavalTacticalFrame");
 std::set<StableId> units,vessels,positions,controlled,people;std::set<Integer> sides;std::map<StableId,Integer> army_sides;
 for(const auto& u:f.units){const auto* b=binding(f,u.unit_id);const auto* v=b?find(s.vessels,b->vessel_id):nullptr;const auto* fleet=b?find(s.fleets,b->fleet_id):nullptr;
  if(u.unit_id.empty()||!units.insert(u.unit_id).second||u.side<0||u.side>1||u.heading<0||u.heading>5||!b||!v||!fleet||fleet->status!="active"||fleet->army_id!=u.army_id||v->fleet_id!=fleet->id||!vessels.insert(v->id).second)return common_failure("NavalBattleBindingMismatch",u.unit_id);
  auto affiliation=army_sides.emplace(u.army_id,u.side);if(!affiliation.second&&affiliation.first->second!=u.side)return common_failure("NavalArmySideMismatch",u.army_id);
  if(u.commander_actor_id.empty()||u.troop_type_id.empty()||!valid_naval_commander_loss(u)||u.tactic_points!=0||u.morale<0||u.morale>100)return common_failure("InvalidNavalCrew",u.unit_id);
  std::set<StableId> members(u.member_ids.begin(),u.member_ids.end()),states;
  if(members.empty()||members.size()>4096||members.size()!=u.member_ids.size()||members.count(StableId{}))return common_failure("InvalidNavalCrew",u.unit_id);
  for(const auto* group:{&u.fit,&u.wounded,&u.dead,&u.captured,&u.deserted})for(const auto& id:*group)if(!members.count(id)||!states.insert(id).second)return common_failure("NavalCrewStateOverlap",id);
  if(members!=states)return common_failure("NavalCrewStateMissing",u.unit_id);
  auto aboard=members;aboard.insert(u.commander_actor_id);for(const auto& person:aboard)if(!people.insert(person).second)return common_failure("NavalCrewAssignedTwice",person);
  bool actual=false;for(const auto& entry:fleet->assignments)if(entry.vessel_id==v->id&&entry.unit_id==u.unit_id)actual=true;if(!actual)return common_failure("NavalBattleBindingMismatch",u.unit_id);
  if(H::movement_cost(board,u.position,water_costs)<0||(!u.withdrawn&&!positions.insert(H::key(u.position)).second))return common_failure("NavalBattlePositionOverlap",u.unit_id);sides.insert(u.side);
 }
 for(const auto& b:f.bindings)if(!units.count(b.unit_id))return common_failure("NavalBattleBindingMismatch",b.unit_id);
 if(f.controller_actor_id.empty()!=f.controlled_unit_ids.empty())return common_failure("NavalControlSetMismatch");
 for(const auto& id:f.controlled_unit_ids)if(!units.count(id)||unit(f,id)->side!=f.controller_side||!controlled.insert(id).second)return common_failure("NavalControlSetMismatch",id);
 if(sides.size()!=2)return common_failure("NavalBattleSidesRequired");return common_success();
}
// 船型直接向原六角引擎给水域成本；双方船体和失能船都继续占格。
Outcome<std::vector<HexPath>> naval_movement_options(const NavalAssetState& s,const NavalAssetCatalog& c,const BattlefieldDefinition& board,const NavalTacticalFrame& f,const StableId& id,const NavalTacticalNavigationPolicy* policy,std::optional<Integer> final_heading){
 auto valid=validate_naval_tactical_frame(s,c,board,f);if(!valid.ok)return {false,{},valid.error};const auto* u=unit(f,id);if(!u)return fail<std::vector<HexPath>>("UnknownNavalBattleUnit",id);
 if(policy){auto checked=validate_naval_tactical_navigation(*policy,&board);if(!checked.ok)return {false,{},checked.error};}
 if(!can_sail(s,c,f,*u)){if(policy&&final_heading&&*final_heading!=u->heading)return {true,{},{}};return {true,{{u->position,{u->position},0}},{}};}
 const auto* ship=find(s.vessels,binding(f,id)->vessel_id);const auto* hull=find(c.hulls,ship->hull_id);std::vector<HexPoint> occupied,stops;
 for(const auto& other:f.units)if(other.unit_id!=id&&!other.withdrawn){occupied.push_back(other.position);if(other.side!=u->side&&crew_active(other))for(auto p:H::neighbors(other.position))stops.push_back(p);}
 if(policy)return naval_tactical_navigation_routes(board,*policy,u->position,u->heading,hull->movement,occupied,stops,final_heading);
 return H::reachable(board,u->position,water_costs,hull->movement,occupied,stops);
}
// 接舷是实际相邻船之间的请求；本阶段不生成俘虏、战利品或船主变更。
Outcome<NavalTacticalQuote> preview_naval_tactical_order(const NavalAssetState& s,const NavalAssetCatalog& c,const BattlefieldDefinition& board,const NavalTacticalFrame& f,const StableId& id,const NavalTacticalOrder& order,const NavalTacticalNavigationPolicy* policy,const NavalGunneryPolicy* gunnery){
 if(f.phase!="planning")return fail<NavalTacticalQuote>("NavalBattleNotPlanning");
 auto moves=naval_movement_options(s,c,board,f,id,policy,policy?std::optional<Integer>{order.heading}:std::nullopt);if(!moves.ok)return {false,{},moves.error};const auto* u=unit(f,id);
 if(order.heading<0||order.heading>5||(!has({"hold","move","board","ram","surrender","withdraw"},order.action)&&!(gunnery&&order.action=="hull_attack")))return fail<NavalTacticalQuote>("InvalidNavalOrder",id);
 if(!crew_active(*u)&&order.action!="hold"&&order.action!="surrender")return fail<NavalTacticalQuote>("NavalCrewCannotAct",id);
 if(!can_sail(s,c,f,*u)&&(order.destination!=u->position||order.action=="withdraw"||order.action=="move"||order.action=="ram"))return fail<NavalTacticalQuote>("NavalShipCannotSail",id);
 const HexPath* path=nullptr;for(const auto& p:moves.value)if(p.position==order.destination)path=&p;if(!path)return fail<NavalTacticalQuote>("NavalDestinationUnreachable",id);
 if(order.action=="hold"&&order.destination!=u->position)return fail<NavalTacticalQuote>("InvalidNavalHold",id);
 if(has({"board","ram","surrender"},order.action)){const auto* target=unit(f,order.target_unit_id);if(!target||target->side==u->side||target->withdrawn||H::distance(order.destination,target->position)!=1)return fail<NavalTacticalQuote>("NavalBoardingRequiresAdjacentEnemy",id);}
 if(order.action=="surrender"&&(order.destination!=u->position||u->withdrawn||(u->fit.empty()&&u->wounded.empty())||!crew_active(*unit(f,order.target_unit_id))))return fail<NavalTacticalQuote>("NavalSurrenderRequiresPresentCaptor",id);
 if(order.action=="ram"&&(path->cost<1||(policy&&path->path.size()<2)))return fail<NavalTacticalQuote>("NavalRamNeedsMovement",id);
 if(!has({"board","ram","surrender","hull_attack"},order.action)&&!order.target_unit_id.empty())return fail<NavalTacticalQuote>("UnexpectedNavalTarget",id);
 if(order.action=="hull_attack"){const auto* target=unit(f,order.target_unit_id);const auto* link=binding(f,id);const auto* ship=link?find(s.vessels,link->vessel_id):nullptr;const NavalGunneryWeapon* weapon=nullptr;if(gunnery&&ship)for(const auto& w:gunnery->weapons)if(w.hull_id==ship->hull_id)weapon=&w;if(!weapon||gunnery->profile!="entry_naval_skill_v1"||weapon->minimum_range<1||weapon->maximum_range<weapon->minimum_range||weapon->maximum_range>16)return fail<NavalTacticalQuote>("NavalGunneryWeaponRequired",id);if(!target||target->withdrawn||target->side==u->side)return fail<NavalTacticalQuote>("NavalGunneryTargetUnavailable",order.target_unit_id);const auto distance=H::distance(order.destination,target->position);if(distance<weapon->minimum_range||distance>weapon->maximum_range||!H::line_of_sight(board,order.destination,target->position))return fail<NavalTacticalQuote>("NavalGunneryOutOfReach",id);}
 if(order.action=="withdraw"){bool exit=false;for(const auto& e:board.exits)if(e.side==u->side&&e.position==order.destination)exit=true;if(!exit)return fail<NavalTacticalQuote>("NavalExitRequired",id);}
 return {true,{path->path,path->cost,order.action=="board"},{}};
}
// 微步看同一个旧占用再统一落位；交换、争格和撞上失能船都停下，不能依数组顺序抢格。
Outcome<NavalManeuverResult> resolve_naval_maneuvers(const NavalAssetState& s,const NavalAssetCatalog& c,const BattlefieldDefinition& board,const NavalTacticalFrame& f,const NavalTacticalOrders& orders,const NavalTacticalNavigationPolicy* policy,const NavalGunneryPolicy* gunnery){
 auto valid=validate_naval_tactical_frame(s,c,board,f);if(!valid.ok)return {false,{},valid.error};if(orders.size()!=f.units.size())return fail<NavalManeuverResult>("IncompleteNavalOrders");
 std::map<StableId,NavalTacticalQuote> quotes;std::map<StableId,NavalTacticalOrder> plans;
 for(const auto& entry:orders){if(plans.count(entry.first))return fail<NavalManeuverResult>("DuplicateNavalOrder",entry.first);auto q=preview_naval_tactical_order(s,c,board,f,entry.first,entry.second,policy,gunnery);if(!q.ok)return {false,{},q.error};plans.emplace(entry.first,entry.second);quotes.emplace(entry.first,std::move(q.value));}
 NavalManeuverResult result;result.frame=f;std::map<StableId,std::size_t> cursors;std::set<StableId> stopped;
 for(const auto& u:f.units)cursors[u.unit_id]=1;
 for(Integer step=1;step<=24;++step){std::map<StableId,HexPoint> proposals;std::map<StableId,Integer> destinations;std::set<StableId> occupied;for(const auto& u:result.frame.units)if(!u.withdrawn)occupied.insert(H::key(u.position));
  for(const auto& entry:quotes)if(!stopped.count(entry.first)&&cursors[entry.first]<entry.second.path.size()){auto target=entry.second.path[cursors[entry.first]];proposals.emplace(entry.first,target);++destinations[H::key(target)];}
  if(proposals.empty())break;
  for(const auto& p:proposals){auto at=std::find_if(result.frame.units.begin(),result.frame.units.end(),[&](const auto& u){return u.unit_id==p.first;});if(occupied.count(H::key(p.second))||destinations[H::key(p.second)]>1){stopped.insert(p.first);result.events.push_back({"move",p.first,{},"NavalMoveBlocked",at->position});}else{at->position=p.second;++cursors[p.first];}}
 }
 // 同一移动后快照先确认所有撤离，再核全部接舷，数组次序不能决定是否逃过登船。
 for(auto& u:result.frame.units){const auto& order=plans.at(u.unit_id);u.heading=order.heading;if(order.action=="withdraw"&&u.position==order.destination){u.withdrawn=true;result.events.push_back({"withdraw",u.unit_id,{},"NavalExitReached",u.position});}}
 for(const auto& u:result.frame.units){const auto& order=plans.at(u.unit_id);if(has({"board","ram","surrender"},order.action)){const auto* target=unit(result.frame,order.target_unit_id);if(target&&!target->withdrawn&&H::distance(u.position,target->position)==1&&(order.action!="ram"||u.position!=unit(f,u.unit_id)->position)){
 auto& requests=order.action=="board"?result.boarding_requests:order.action=="ram"?result.ram_requests:result.surrender_requests;requests.emplace_back(u.unit_id,target->unit_id);result.events.push_back({order.action,u.unit_id,target->unit_id,"NavalContactRequested",u.position});
 }else result.events.push_back({order.action,u.unit_id,order.target_unit_id,"NavalContactOutOfReach",u.position});}}
 // 舰载攻击在真实同步移动后再次核射程；对手撤离不补命中，不另消耗世界半日。
 for(const auto& u:result.frame.units){const auto& order=plans.at(u.unit_id);if(order.action=="hull_attack")result.gunnery_requests.emplace_back(u.unit_id,order.target_unit_id);}
 valid=validate_naval_tactical_frame(s,c,board,result.frame);if(!valid.ok)return {false,{},valid.error};return {true,std::move(result),{}};
}
}
