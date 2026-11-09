// 已准备且不再修改的目录专用纯路线缓存；不缓存人物、费用、权限、行动或未来世界状态。
#pragma once
#include "terrain.hpp"
#include <deque>
#include <mutex>
#include "resident/checkpoint/checkpoint_retention.hpp"
namespace hanmo::resident::terrain {
// 仅诊断计算命中与有界占用，计数不进入权威世界或检查点。
struct RouteCacheStats { std::size_t hits=0,misses=0,entries=0,points=0; };
class RouteCache {
public:
    // 每个缓存只能借用其存活期内不变的地图；TravelCatalog复制时另建缓存，不能继承旧地图指针。
    explicit RouteCache(std::size_t max_entries=256,std::size_t max_points=32768):max_entries_(max_entries),max_points_(max_points){}
    // 只复用原A*成功的纯几何结果；失败、报价和共享资源守卫均不缓存。
    Outcome<Route> plan(const TerrainMap&,PointI from,PointI to,Mode,const Flags&);
    // 有限诊断副本，不暴露内部路线的可写别名。
    RouteCacheStats stats() const;
    // 在原互斥锁内封存派生缓存，再核实际 allocator owner。
    void seal_checkpoint_retention(CheckpointRetentionCensus&) const;
private:
    struct Key { const TerrainMap* map=nullptr;StableId map_id;PointI from,to;Mode mode=Mode::foot;std::vector<std::pair<StableId,bool>> flags; };
    struct Entry { Key key;Route route; };
    static bool same_key(const Key&,const Key&);
    std::size_t max_entries_,max_points_,hits_=0,misses_=0,points_=0;
    mutable std::mutex mutex_;
    std::deque<Entry,CheckpointAllocator<Entry>> entries_;
    mutable bool checkpoint_sealed_=false;
};
} // mutex仅保护可重建缓存；不存在第二个世界提交点。
