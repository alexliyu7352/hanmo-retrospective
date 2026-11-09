// 缓存key包含全部可改变几何的输入；继续调用唯一精确prepared A*，不复制另一条路线算法。
#include "route_cache.hpp"
#include <algorithm>
namespace hanmo::resident::terrain {
// 指针只作同一不可变目录内的地图身份；任何地图副本都不能借同名地图命中旧结果。
bool RouteCache::same_key(const Key& a,const Key& b) {
    return a.map==b.map&&a.map_id==b.map_id&&a.from.x==b.from.x&&a.from.y==b.from.y&&a.to.x==b.to.x&&a.to.y==b.to.y&&a.mode==b.mode&&a.flags==b.flags;
}
// 旗标先规范序，仅影响cache key，不改变底层查询按当前bool取值的行为。
Outcome<Route> RouteCache::plan(const TerrainMap& map,PointI from,PointI to,Mode mode,const Flags& flags) {
    // 有限目录flag键仍设独立字节界；异常庞大输入只旁路cache，不改变原几何结果。
    constexpr std::size_t key_bytes_limit=16384;
    std::size_t key_bytes=map.id.size();
    if(flags.size()>256||key_bytes>key_bytes_limit) return plan_prepared(map,from,to,mode,flags);
    for(const auto& flag:flags) {if(flag.first.size()>key_bytes_limit-key_bytes)return plan_prepared(map,from,to,mode,flags);key_bytes+=flag.first.size();}
    Key key;key.map=&map;key.map_id=map.id;key.from=from;key.to=to;key.mode=mode;
    key.flags.assign(flags.begin(),flags.end());std::sort(key.flags.begin(),key.flags.end());
    std::lock_guard<std::mutex> guard(mutex_);
    for(const auto& entry:entries_) if(same_key(entry.key,key)) {++hits_;return {true,entry.route,{}};}
    ++misses_;
    auto result=plan_prepared(map,from,to,mode,flags);
    if(checkpoint_sealed_||!result.ok||max_entries_==0||result.value.points.size()>max_points_) return result;
    while(!entries_.empty()&&(entries_.size()>=max_entries_||points_+result.value.points.size()>max_points_)) {
        points_-=entries_.front().route.points.size();entries_.pop_front();
    }
    entries_.push_back({std::move(key),result.value});points_+=result.value.points.size();
    return result;
}
// 在原互斥锁内封存缓存；真实 deque allocator owner 仍执行同 job 约束。
void RouteCache::seal_checkpoint_retention(CheckpointRetentionCensus& census) const {
    std::lock_guard<std::mutex> guard(mutex_);checkpoint_sealed_=true;
    census.owner(entries_.get_allocator().allocation_owner());
}
RouteCacheStats RouteCache::stats() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return {hits_,misses_,entries_.size(),points_};
}
} // 命中仍返回深拥有Route，后续begin/step不持有缓存内向量引用。
