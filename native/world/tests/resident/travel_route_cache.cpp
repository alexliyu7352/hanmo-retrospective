// 有界纯缓存只消除重复A*；原旗标、模式、地图身份及结果深拥有均必须保全。
#include "resident/geometry/route_cache.hpp"
#include <iostream>
using namespace hanmo::resident;
using namespace hanmo::resident::terrain;
namespace {
int checks=0,failures=0;
// 错误保留具体检查标签，不用耗时阈值代替缓存语义证明。
void check(bool value,const char* label){++checks;if(!value){++failures;std::cerr<<"FAIL "<<label<<'\n';}}
// 明确的小地图仍交生产map validator一次，缓存仅接prepared输入。
TerrainMap map(){TerrainMap result;result.id="map.test";result.width=100;result.height=100;result.cell_size=20;result.cells.resize(25);result.cells[12].blocked_flag="gate.closed";return result;}
}
// 不执行未来半日或资源变更；命中统计及原route值都直接核对。
int main(){auto m=map();check(validate_map(m).ok,"fixture valid");RouteCache cache;auto first=cache.plan(m,{10,50},{90,50},Mode::foot,{});check(first.ok&&first.value.cost==800,"real first plan");auto second=cache.plan(m,{10,50},{90,50},Mode::foot,{});check(second.ok&&second.value.cost==first.value.cost&&cache.stats().hits==1&&cache.stats().misses==1,"repeat reuses exact route");second.value.points.front().x=999;check(cache.plan(m,{10,50},{90,50},Mode::foot,{}).value.points.front().x==10,"result is deep owned");
    auto closed=cache.plan(m,{10,50},{90,50},Mode::foot,{{"gate.closed",true}});auto direct=plan_prepared(m,{10,50},{90,50},Mode::foot,{{"gate.closed",true}});check(closed.ok==direct.ok&&closed.value.cost==direct.value.cost&&closed.value.cost>800,"changed blocking flag recomputes actual detour");auto opened=cache.plan(m,{10,50},{90,50},Mode::foot,{{"gate.closed",false},{"other",true}});auto before=cache.stats();auto reordered=cache.plan(m,{10,50},{90,50},Mode::foot,{{"other",true},{"gate.closed",false}});check(opened.ok&&reordered.value.cost==opened.value.cost&&cache.stats().hits==before.hits+1,"flag insertion order does not miss identical inputs");
    auto army=cache.plan(m,{10,50},{90,50},Mode::army,{});check(army.ok&&army.value.cost==1280,"mode gets distinct cost");auto copy=m;copy.costs[Mode::foot]["plain"]=20;auto different=cache.plan(copy,{10,50},{90,50},Mode::foot,{});check(different.ok&&different.value.cost==1600,"same named map object cannot cross identity");auto invalid=cache.plan(m,{-1,50},{90,50},Mode::foot,{});check(!invalid.ok&&invalid.error.original.code=="InvalidOrigin","failed plan keeps original error");
    RouteCache bounded(2,4);bounded.plan(m,{10,10},{30,10},Mode::foot,{});bounded.plan(m,{10,30},{30,30},Mode::foot,{});bounded.plan(m,{10,70},{30,70},Mode::foot,{});check(bounded.stats().entries==2&&bounded.stats().points==4,"entry and point bounds enforced");auto misses=bounded.stats().misses;bounded.plan(m,{10,10},{30,10},Mode::foot,{});check(bounded.stats().misses==misses+1,"oldest route evicted without rule changes");RouteCache bypass(0);bypass.plan(m,{10,10},{30,10},Mode::foot,{});bypass.plan(m,{10,10},{30,10},Mode::foot,{});check(bypass.stats().entries==0&&bypass.stats().misses==2,"zero bound safely bypasses");Flags large;for(int i=0;i<257;++i)large.emplace("flag."+std::to_string(i),false);RouteCache key_bound;auto uncached=key_bound.plan(m,{10,10},{30,10},Mode::foot,large);check(uncached.ok&&uncached.value.cost==200&&key_bound.stats().entries==0,"oversize flag key bypass preserves result");std::cout<<"ROUTE_CACHE_CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;}
