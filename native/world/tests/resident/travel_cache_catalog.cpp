// 纯目录缓存复制/移动和占用边界；不推进世界，不重跑接触数值矩阵。
#include "resident/domains/travel/travel.hpp"
#include <iostream>
using namespace hanmo::resident;
using namespace hanmo::resident::terrain;
namespace {
int checks=0,failures=0;
// 每项核对生命周期或硬上限，耗时不作为正确性条件。
void check(bool ok,const char* label){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<label<<'\n';}}
// 小地图仍使用生产验证；之后每个目录在第一次查询前完成所有改动。
TravelCatalog catalog(){TravelCatalog c;TerrainMap m;m.id="map.test";m.width=100;m.height=100;m.cell_size=20;m.cells.resize(25);c.world_map_id=m.id;c.maps.insert(c.world_map_id,std::move(m));return c;}
// 经目录持有的缓存与地图借用调用唯一prepared几何实现。
Outcome<Route> route(const TravelCatalog& c){return c.route_cache->plan(*c.maps.find(c.world_map_id),{10,50},{90,50},Mode::foot,{});}
// 返回路线必须深拥有，目录和缓存的析构不能影响调用者的路线。
Route detached_route(){auto temporary=catalog();return route(temporary).value;}
}
// 目录更新用复制/重新准备后的独立实例；不在缓存生效后原地修改其地图。
int main(){
    auto original=catalog();check(validate_map(*original.maps.find(original.world_map_id)).ok,"fixture validated");check(route(original).ok,"original cache warmed");
    TravelCatalog copied(original);check(copied.route_cache!=original.route_cache&&copied.route_cache->stats().entries==0,"copy construction starts independent empty cache");
    check(copied.maps.find(copied.world_map_id)!=original.maps.find(original.world_map_id),"copied map has independent lifetime");
    copied.maps.find(copied.world_map_id)->costs[Mode::foot]["plain"]=20;
    check(validate_map(*copied.maps.find(copied.world_map_id)).ok,"replacement map validated before use");
    check(route(copied).value.cost==1600&&route(original).value.cost==800,"changed copied catalog never borrows old geometry");

    std::weak_ptr<RouteCache> obsolete=original.route_cache;original=copied;
    check(obsolete.expired(),"copy assignment releases old cache");
    check(original.route_cache!=copied.route_cache&&original.route_cache->stats().entries==0,"copy assignment resets cache even when source is warm");
    check(route(original).value.cost==1600,"copy assignment uses replacement geometry");
    auto self_cache=original.route_cache;original=original;check(original.route_cache==self_cache&&route(original).value.cost==1600,"self assignment retains valid cache and map");self_cache.reset();

    auto* moved_map=copied.maps.find(copied.world_map_id);auto moved_cache=copied.route_cache;auto old_hits=moved_cache->stats().hits;
    TravelCatalog moved(std::move(copied));check(moved.maps.find(moved.world_map_id)==moved_map&&moved.route_cache==moved_cache,"move construction transfers stable map storage and cache");
    check(route(moved).value.cost==1600&&moved_cache->stats().hits==old_hits+1,"moved cache key remains valid");
    auto target=catalog();route(target);std::weak_ptr<RouteCache> target_cache=target.route_cache;old_hits=moved_cache->stats().hits;target=std::move(moved);
    check(target_cache.expired()&&target.maps.find(target.world_map_id)==moved_map,"move assignment releases obsolete target and preserves map borrow");
    check(route(target).value.cost==1600&&moved_cache->stats().hits==old_hits+1,"move assignment preserves exact cached geometry");
    auto detached=detached_route();check(detached.cost==800&&detached.points.front().x==10&&detached.points.back().x==90,"returned route survives catalog destruction");

    const auto& map=*target.maps.find(target.world_map_id);RouteCache default_bound;
    for(Integer i=0;i<257;++i)default_bound.plan(map,{i%90,10},{i%90+1,20+i/90},Mode::foot,{});
    check(default_bound.stats().entries==256&&default_bound.stats().points==512,"default cache evicts at 256 entries");
    auto misses=default_bound.stats().misses;default_bound.plan(map,{0,10},{1,20},Mode::foot,{});
    check(default_bound.stats().misses==misses+1&&default_bound.stats().entries==256,"full cache keeps hard bound while replacing oldest key");
    RouteCache point_bound(256,3);point_bound.plan(map,{0,10},{1,20},Mode::foot,{});point_bound.plan(map,{1,10},{2,20},Mode::foot,{});
    check(point_bound.stats().entries==1&&point_bound.stats().points==2,"point budget evicts independently from entry bound");
    RouteCache oversize(256,1);auto direct=oversize.plan(map,{0,10},{1,20},Mode::foot,{});
    check(direct.ok&&oversize.stats().entries==0&&oversize.stats().points==0,"one oversized route bypasses retention without changing result");
    RouteCache failures_cache;failures_cache.plan(map,{-1,10},{1,20},Mode::foot,{});auto invalid=failures_cache.plan(map,{-1,10},{1,20},Mode::foot,{});
    check(!invalid.ok&&invalid.error.original.code=="InvalidOrigin"&&failures_cache.stats().misses==2&&failures_cache.stats().entries==0,"failed plans are never cached");

    RouteCache key_bound;Flags exact_flags{{StableId(16384-map.id.size(),'x'),false}};
    auto exact=key_bound.plan(map,{0,10},{1,20},Mode::foot,exact_flags);check(exact.ok&&key_bound.stats().entries==1,"exact 16KiB text key can be retained");
    Flags too_large{{StableId(16385-map.id.size(),'x'),false}};auto before=key_bound.stats();auto bypass=key_bound.plan(map,{0,10},{1,20},Mode::foot,too_large);
    check(bypass.ok&&bypass.value.cost==exact.value.cost&&key_bound.stats().entries==before.entries&&key_bound.stats().misses==before.misses,"oversized key bypasses without growing retained memory");
    Flags count_bound;for(int i=0;i<257;++i)count_bound.emplace(std::to_string(i),false);before=key_bound.stats();auto too_many=key_bound.plan(map,{0,10},{1,20},Mode::foot,count_bound);
    check(too_many.ok&&too_many.value.cost==exact.value.cost&&key_bound.stats().entries==before.entries&&key_bound.stats().misses==before.misses,"more than 256 flags bypasses retained key storage");
    std::cout<<"TRAVEL_CACHE_CATALOG_CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;
}
