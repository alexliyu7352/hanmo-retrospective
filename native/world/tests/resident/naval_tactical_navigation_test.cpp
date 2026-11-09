// 转角/水流的有限只读报价门；没有World、随机流、真实出兵或战果注入。
#include "resident/domains/military/naval_tactical_navigation.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
int checks=0;
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
template<class T>T take(Outcome<T> value){if(!value.ok)throw std::runtime_error(value.error.original.code);return std::move(value.value);}
const HexPath* path(const std::vector<HexPath>& rows,HexPoint point){for(const auto& row:rows)if(row.position==point)return &row;return nullptr;}
BattlefieldDefinition board(){BattlefieldDefinition out;out.id="river";for(Integer q=0;q<6;++q)for(Integer r=0;r<4;++r)out.cells.push_back({q,r,"water"});out.spawns={{0,{0,0}},{1,{5,3}}};out.exits=out.spawns;return out;}
}
int main(){try{
 auto water=board();NavalTacticalNavigationPolicy policy;const auto straight=take(naval_tactical_navigation_routes(water,policy,{0,0},0,3,{},{}));check(path(straight,{3,0})&&path(straight,{3,0})->cost==3,"straight heading spends original water movement only");
 auto final=take(naval_tactical_navigation_routes(water,policy,{0,0},0,3,{}, {},3));check(!path(final,{3,0}),"final180 heading cannot be obtained for free after full move");check(path(final,{0,0})&&path(final,{0,0})->cost==2,"in place180 uses two90 units");
 auto sixty=take(naval_tactical_navigation_routes(water,policy,{0,0},0,1,{}, {},1));check(path(sixty,{0,0})&&path(sixty,{0,0})->cost==1,"sixty-degree turn rounds up once");
 policy.currents={{{1,0},3}};auto upstream=take(naval_tactical_navigation_routes(water,policy,{0,0},0,2,{}, {},0));check(path(upstream,{1,0})&&path(upstream,{1,0})->cost==2,"entering exact opposing current adds one");check(!path(upstream,{2,0}),"upstream price consumes real movement budget");policy.currents[0].heading=0;auto downstream=take(naval_tactical_navigation_routes(water,policy,{0,0},0,2,{}, {},0));check(path(downstream,{2,0})&&path(downstream,{2,0})->cost==2,"downstream does not invent a speed bonus");
 policy.currents.clear();auto blocked=take(naval_tactical_navigation_routes(water,policy,{0,0},0,3,{{1,0}},{}));check(!path(blocked,{1,0}),"occupied ship cell remains blocked");auto stopped=take(naval_tactical_navigation_routes(water,policy,{0,0},0,2,{},{{1,0}}));check(path(stopped,{1,0})&&!path(stopped,{2,0}),"entering control zone stops further motion");
 auto bad=policy;bad.currents={{{1,0},0},{{1,0},1}};check(!validate_naval_tactical_navigation(bad,&water).ok,"duplicate current source rejects");bad=policy;bad.profile="future";check(!validate_naval_tactical_navigation(bad,&water).ok,"future profile rejects");bad=policy;bad.currents={{{20,20},0}};check(!validate_naval_tactical_navigation(bad,&water).ok,"off-board flow cannot create water");check(!naval_tactical_navigation_routes(water,policy,{0,0},0,25,{},{}).ok,"finite original budget bound rejects");
 std::cout<<"NAVAL_TACTICAL_NAVIGATION="<<checks<<" FAILURES=0\n";return 0;
 }catch(const std::exception& error){std::cerr<<"NAVAL_TACTICAL_NAVIGATION_FAIL checks="<<checks<<" "<<error.what()<<'\n';return 1;}}
