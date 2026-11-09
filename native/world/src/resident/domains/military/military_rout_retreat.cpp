// 有限图上反向最短路只确定原出口方向；这一轮仍复用原24点内可达集及控制区停步规则。
#include "military_rout_retreat.hpp"
#include "military_commander.hpp"
#include "resident/model/common_model.hpp"
#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
namespace H=military_hex;
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& r:rows)if(r.id==id)return &r;return nullptr;}
template<class T>Outcome<T> failure(const StableId& code,const StableId& id={}){return {false,{},common_failure(code,id).error};}
bool present(const BattleUnit& u){return !u.withdrawn&&!u.fit.empty();}
bool controls(const BattleUnit& u){return present(u)&&u.morale>0&&military_commander_loss(u)!=2;}
// 所有边仍调用原movement_cost；路径长远超单轮时也不扩大本轮移动预算。
std::map<StableId,Integer> distances(const BattlefieldDefinition& board,const TroopDefinition& troop,Integer side,const std::set<StableId>& blocked){
 std::map<StableId,Integer> result;std::set<std::pair<Integer,StableId>> queue;std::map<StableId,HexPoint> cells;
 for(const auto& c:board.cells)cells.emplace(H::key({c.q,c.r}),HexPoint{c.q,c.r});
 for(const auto& e:board.exits)if(e.side==side&&H::movement_cost(board,e.position,troop)>=0&&!blocked.count(H::key(e.position))){const auto key=H::key(e.position);result[key]=0;queue.emplace(0,key);}
 while(!queue.empty()){
  const auto next=*queue.begin();queue.erase(queue.begin());if(result.at(next.second)!=next.first)continue;const auto destination=cells.at(next.second);const Integer step=H::movement_cost(board,destination,troop);
  for(const auto p:H::neighbors(destination)){
   const auto key=H::key(p);if(blocked.count(key)||H::movement_cost(board,p,troop)<0)continue;
   const Integer cost=next.first+step;const auto previous=result.find(key);if(previous!=result.end()&&previous->second<=cost)continue;
   if(previous!=result.end())queue.erase({previous->second,key});result[key]=cost;queue.emplace(cost,key);
  }
 }
 return result;
}
}
Status validate_military_rout_rule(const MilitaryRoutRetreatRule& rule,const BattlefieldDefinition& board){
 if(rule.profile!="two_blocked_rounds_v1"||rule.blocked_rounds_before_capture!=2)return common_failure("MilitaryRoutProfileMismatch");auto geometry=H::validate(board);if(!geometry.ok)return geometry;
 bool exits[2]={false,false};for(const auto& exit:board.exits)if(exit.side>=0&&exit.side<=1)exits[exit.side]=true;
 if(!exits[0]||!exits[1])return common_failure("MilitaryRoutBothExitsRequired",board.id);return common_success();
}
Outcome<MilitaryRoutRetreatQuote> quote_military_rout_retreat(const MilitaryBattleRecord& battle,const MilitaryBattleCatalog& catalog,const MilitaryRoutRetreatRule& rule,const StableId& id){
 using R=MilitaryRoutRetreatQuote;const auto* board=find(catalog.battlefields,battle.battlefield_id);const BattleUnit* unit=nullptr;
 for(const auto& u:battle.units)if(u.unit_id==id){if(unit)return failure<R>("DuplicateMilitaryUnit",id);unit=&u;}
 if(!board||!unit)return failure<R>("MilitaryRoutSourceRequired",id);auto valid=validate_military_rout_rule(rule,*board);if(!valid.ok)return {false,{},valid.error};
 const auto* troop=find(catalog.troop_types,unit->troop_type_id);if(!troop||troop->movement<0||troop->movement>16||battle.phase!="planning"||battle.round<1||battle.round>9999||!present(*unit)||unit->morale!=0||unit->side<0||unit->side>1||unit->heading<0||unit->heading>5||H::movement_cost(*board,unit->position,*troop)<0)return failure<R>("MilitaryRoutUnitRequired",id);
 R quote;quote.battle_id=battle.id;quote.unit_id=id;quote.round=battle.round;quote.origin=quote.destination=unit->position;quote.heading=unit->heading;quote.path={unit->position};
 std::vector<HexPoint> occupied,zones;std::set<StableId> blocked,seen;
 for(const auto& other:battle.units){if(other.unit_id.empty()||!seen.insert(other.unit_id).second)return failure<R>("DuplicateMilitaryUnit",other.unit_id);if(other.unit_id==id||!present(other))continue;const auto key=H::key(other.position);if(!blocked.insert(key).second||other.position==unit->position)return failure<R>("MilitaryUnitsOverlap",id);occupied.push_back(other.position);if(other.side!=unit->side&&controls(other)){auto near=H::neighbors(other.position);zones.insert(zones.end(),near.begin(),near.end());if(H::distance(unit->position,other.position)==1)quote.adjacent_captor_unit_ids.push_back(other.unit_id);}}
 std::sort(quote.adjacent_captor_unit_ids.begin(),quote.adjacent_captor_unit_ids.end());
 const auto remaining=distances(*board,*troop,unit->side,blocked);const auto start=remaining.find(H::key(unit->position));if(start==remaining.end())return {true,std::move(quote),{}};quote.exit_reachable=true;quote.at_exit=start->second==0;if(quote.at_exit)return {true,std::move(quote),{}};
 Integer budget=troop->movement;for(const auto& status:unit->statuses)if((status.first=="suppressed"||status.first=="hindered")&&status.second>=battle.round){budget=std::max<Integer>(0,budget-1);break;}
 auto available=H::reachable(*board,unit->position,*troop,budget,occupied,zones);if(!available.ok)return {false,{},available.error};
 const HexPath* best=nullptr;std::tuple<Integer,Integer,StableId> score{start->second,0,H::key(unit->position)};
 for(const auto& candidate:available.value){const auto rest=remaining.find(H::key(candidate.position));if(rest==remaining.end()||rest->second>=start->second)continue;auto selected=std::make_tuple(rest->second,candidate.cost,H::key(candidate.position));if(!best||selected<score){best=&candidate;score=std::move(selected);}}
 if(best){quote.destination=best->position;quote.path=best->path;quote.cost=best->cost;quote.heading=H::direction(unit->position,best->position);quote.can_progress=true;quote.at_exit=std::get<0>(score)==0;}
 return {true,std::move(quote),{}};
}
}
