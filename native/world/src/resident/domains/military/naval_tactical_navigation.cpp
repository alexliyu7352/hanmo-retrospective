// 有限Dijkstra把朝向和转角余数纳入原水格搜索；同价稳定排序，不读玩家未来意图。
#include "naval_tactical_navigation.hpp"
#include "resident/model/common_model.hpp"
#include <algorithm>
#include <map>
#include <queue>
#include <functional>
#include <cstdlib>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
namespace H=military_hex;
const std::map<StableId,Integer> water{{"water",1}};
Integer turns(Integer a,Integer b){const auto d=std::abs(a-b);return std::min(d,6-d);}
Integer ceil_thirds(Integer value){return (value+2)/3;}
// 余数保存上一段已收的向上取整部分，拆成多次60度不会重收或漏收转向费。
Integer turn_price(Integer remainder,Integer steps,Integer price){return (ceil_thirds(remainder+2*steps)-ceil_thirds(remainder))*price;}
using Key=std::tuple<StableId,Integer,Integer>;
struct Route { HexPoint position;Integer heading=0,remainder=0,cost=0;std::vector<HexPoint> path; };
template<class T>Outcome<T> fail(const StableId& code){return {false,{},common_failure(code).error};}
}
Status validate_naval_tactical_navigation(const NavalTacticalNavigationPolicy& policy,const BattlefieldDefinition* board){
 if(policy.profile!="hex_helm_current_v1"||policy.quarter_turn_cost<1||policy.quarter_turn_cost>24||policy.upstream_cost<1||policy.upstream_cost>24||policy.currents.size()>1024)return common_failure("InvalidNavalTacticalNavigationPolicy");
 std::set<StableId> seen;for(const auto& flow:policy.currents){if(flow.position.q < -1024||flow.position.q>1024||flow.position.r < -1024||flow.position.r>1024||flow.heading<0||flow.heading>5||!seen.insert(H::key(flow.position)).second)return common_failure("InvalidNavalTacticalCurrent");if(board&&H::movement_cost(*board,flow.position,water)<0)return common_failure("NavalCurrentRequiresWaterCell",H::key(flow.position));}return common_success();
}
Outcome<std::vector<HexPath>> naval_tactical_navigation_routes(const BattlefieldDefinition& board,const NavalTacticalNavigationPolicy& policy,HexPoint from,Integer heading,Integer budget,const std::vector<HexPoint>& occupied,const std::vector<HexPoint>& stops,std::optional<Integer> final_heading){
 auto checked=H::validate(board);if(!checked.ok)return {false,{},checked.error};checked=validate_naval_tactical_navigation(policy,&board);if(!checked.ok)return {false,{},checked.error};
 if(heading<0||heading>5||(final_heading&&(*final_heading<0||*final_heading>5))||budget<0||budget>24||H::movement_cost(board,from,water)<0)return fail<std::vector<HexPath>>("InvalidNavalTacticalNavigationInput");
 std::set<StableId> blocked,stop;for(const auto& point:occupied){if(!H::cell(board,point))return fail<std::vector<HexPath>>("InvalidHexOccupation");if(point!=from)blocked.insert(H::key(point));}for(const auto& point:stops)stop.insert(H::key(point));
 std::map<StableId,Integer> currents;for(const auto& flow:policy.currents)currents.emplace(H::key(flow.position),flow.heading);
 const auto origin=H::key(from);const Key initial{origin,heading,0};std::map<Key,Route> routes{{initial,{from,heading,0,0,{from}}}};using Pending=std::pair<Integer,Key>;std::priority_queue<Pending,std::vector<Pending>,std::greater<Pending>> pending;pending.push({0,initial});std::set<Key> finished;
 while(!pending.empty()){
  const auto next=pending.top();pending.pop();const auto& key=next.second;if(finished.count(key)||routes.at(key).cost!=next.first)continue;finished.insert(key);const auto current=routes.at(key);
  if(current.position!=from&&stop.count(H::key(current.position)))continue;
  for(const auto& point:H::neighbors(current.position)){
   const auto cell=H::key(point);if(blocked.count(cell)||H::movement_cost(board,point,water)<0)continue;const auto direction=H::direction(current.position,point),rotation=turns(current.heading,direction);Integer cost=current.cost+1+turn_price(current.remainder,rotation,policy.quarter_turn_cost);auto flow=currents.find(cell);if(flow!=currents.end()&&direction==(flow->second+3)%6)cost+=policy.upstream_cost;
   if(cost>budget)continue;const auto remainder=(current.remainder+2*rotation)%3;const Key id{cell,direction,remainder};if(finished.count(id))continue;auto prior=routes.find(id);if(prior!=routes.end()&&prior->second.cost<=cost)continue;auto path=current.path;path.push_back(point);routes[id]={point,direction,remainder,cost,std::move(path)};pending.push({cost,id});
  }
 }
 // 同格不同到达航向分别保留至终点再比较，不能先挑便宜路径后遗漏更便宜的最终转向。
 std::map<StableId,HexPath> result;for(const auto& entry:routes){const auto& route=entry.second;Integer cost=route.cost;if(final_heading)cost+=turn_price(route.remainder,turns(route.heading,*final_heading),policy.quarter_turn_cost);if(cost>budget)continue;const auto key=H::key(route.position);auto prior=result.find(key);if(prior==result.end()||cost<prior->second.cost)result[key]={route.position,route.path,cost};}
 std::vector<HexPath> out;for(auto& entry:result)out.push_back(std::move(entry.second));return {true,std::move(out),{}};
}
}
