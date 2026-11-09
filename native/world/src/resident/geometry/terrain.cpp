// 逐运算迁移world_terrain.gd；不重排浮点积分，也不让编译器合并乘加。
#include "terrain.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#if defined(__FAST_MATH__)
#error "Terrain geometry requires IEEE binary64 without fast-math"
#endif
#if defined(__clang__)
#pragma clang fp contract(off)
#pragma clang fp reassociate(off)
#elif defined(__GNUC__)
#pragma GCC optimize ("no-fast-math", "fp-contract=off")
#endif

namespace hanmo::resident::terrain {
namespace {
static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits==53,
              "Terrain geometry requires IEEE binary64");
constexpr const char* surfaces[]={"plain","road","forest","mountain","water","ford","bridge","pass"};
constexpr Integer default_costs[3][8]={
    {10,8,18,0,0,26,8,18},
    {16,12,32,0,0,42,12,26},
    {0,0,0,0,8,8,8,0}
};

// 旧地形错误未携带path；类型化结果保留空路径而不伪造调用方字段位置。
template<class T> Outcome<T> fail(const std::string& code) {
    Outcome<T> result;
    result.error.original={code,{}};
    return result;
}

// 原模式枚举的全部合法值；恶意强制转换不能成为数组下标。
bool valid_mode(Mode mode) {
    return mode==Mode::foot || mode==Mode::army || mode==Mode::boat;
}

// 成本表固定顺序只用于查值与最小值，不依赖std::map遍历顺序选路。
int surface_index(const std::string& surface) {
    for(int i=0;i<8;++i) if(surface==surfaces[i]) return i;
    return -1;
}

// UTF-8目录字符串按Unicode码点计长度，避免把中文短旗标误按字节拒绝。
std::size_t character_count(const std::string& text) {
    std::size_t result=0;
    for(unsigned char value:text) if((value&0xc0)!=0x80) ++result;
    return result;
}

// 限定ID只允许旧正则中的ASCII字符；短非限定旗标仍保持原宽松合同。
bool blocked_flag_valid(const std::string& flag) {
    if(flag.empty()) return false;
    const auto separator=flag.find("::");
    const auto length=character_count(flag);
    if(separator==std::string::npos) return length<=80;
    if(length>114 || separator<1 || separator>32) return false;
    const auto local=separator+2;
    if(local>=flag.size() || flag.size()-local>80) return false;
    if(flag[0]<'a' || flag[0]>'z' || flag[local]<'a' || flag[local]>'z') return false;
    for(std::size_t i=1;i<separator;++i) {
        const auto c=flag[i];
        if(!((c>='a' && c<='z') || (c>='0' && c<='9') || c=='_' || c=='-')) return false;
    }
    for(std::size_t i=local+1;i<flag.size();++i) {
        const auto c=flag[i];
        if(!((c>='a' && c<='z') || (c>='0' && c<='9') || c=='_' || c=='-' || c=='.')) return false;
    }
    return true;
}

// 每次公开查询先核地图，再核模式；Flags的键和值类型已由C++边界保证。
Status context(const TerrainMap& map,Mode mode) {
    const auto result=validate_map(map);
    if(!result.ok) return result;
    if(!valid_mode(mode)) return fail<Done>("InvalidTravelMode");
    return {true,{},{}};
}

// 半开地图范围不接受负点或width/height，不夹取也不改变权威端点。
bool inside(const TerrainMap& map,PointI point) {
    return point.x>=0 && point.x<map.width && point.y>=0 && point.y<map.height;
}

// 按模式与地形取显式覆盖；所有调用均在地图与模式验证后发生。
Integer configured_rate(const TerrainMap& map,Mode mode,const std::string& terrain) {
    const auto mode_costs=map.costs.find(mode);
    if(mode_costs!=map.costs.end()) {
        const auto value=mode_costs->second.find(terrain);
        if(value!=mode_costs->second.end()) return value->second;
    }
    return default_costs[static_cast<int>(mode)][surface_index(terrain)];
}

// 水陆能力先于成本覆盖；缺省blocked_flag仍按旧规则查询空字符串旗标。
Integer cell_rate(const TerrainMap& map,Integer index,Mode mode,const Flags& flags) {
    const auto& cell=map.cells[static_cast<std::size_t>(index)];
    const auto flag=flags.find(cell.blocked_flag.value_or(""));
    if(flag!=flags.end() && flag->second) return 0;
    auto terrain=cell.terrain;
    if(mode==Mode::boat) {
        if(terrain!="water") return 0;
    } else {
        if(terrain=="water" && cell.crossing!=std::optional<std::string>{"ford"} && cell.crossing!=std::optional<std::string>{"bridge"}) return 0;
        terrain=cell.crossing.value_or(terrain);
    }
    return configured_rate(map,mode,terrain);
}

// Godot roundf/roundi在此非负地图坐标上为最近整数、半值远离零。
Integer rounded(double value) {
    return static_cast<Integer>(std::round(value));
}

// 恰在格界时检查四周并取最大费率；任何接触阻断格都使该点不可通过。
Integer rate_at(const TerrainMap& map,double x,double y,Mode mode,const Flags& flags) {
    const double size=static_cast<double>(map.cell_size);
    const double column_position=x/size,row_position=y/size;
    Integer column=static_cast<Integer>(std::floor(column_position));
    Integer row=static_cast<Integer>(std::floor(row_position));
    if(std::abs(column_position-std::round(column_position))<EPSILON) column=rounded(column_position);
    if(std::abs(row_position-std::round(row_position))<EPSILON) row=rounded(row_position);
    Integer columns[2]={column,0},rows[2]={row,0};
    int column_count=1,row_count=1;
    if(column>0 && std::abs(column_position-static_cast<double>(column))<EPSILON) columns[column_count++]=column-1;
    if(row>0 && std::abs(row_position-static_cast<double>(row))<EPSILON) rows[row_count++]=row-1;
    const Integer width=map.width/map.cell_size,height=map.height/map.cell_size;
    Integer result=0;
    for(int r=0;r<row_count;++r) for(int c=0;c<column_count;++c) {
        const auto cy=rows[r],cx=columns[c];
        if(cx<0 || cy<0 || cx>=width || cy>=height) return 0;
        const auto rate=cell_rate(map,cy*width+cx,mode,flags);
        if(rate==0) return 0;
        result=std::max(result,rate);
    }
    return result;
}

// 同源积分以所有格界切分；重复交点仍先核阻断，再跳过极短重复间隔。
Outcome<SegmentProfile> segment(const TerrainMap& map,PointI from,PointI to,Mode mode,const Flags& flags,bool include_profile) {
    if(!inside(map,from) || rate_at(map,static_cast<double>(from.x),static_cast<double>(from.y),mode,flags)==0) return fail<SegmentProfile>("InvalidOrigin");
    if(!inside(map,to) || rate_at(map,static_cast<double>(to.x),static_cast<double>(to.y),mode,flags)==0) return fail<SegmentProfile>("InvalidDestination");
    const double dx=static_cast<double>(to.x-from.x),dy=static_cast<double>(to.y-from.y);
    const double length=std::sqrt(dx*dx+dy*dy);
    if(length==0.0) return {true,{},{}};
    std::vector<double> cuts={0.0,1.0};
    const Integer starts[2]={from.x,from.y},ends[2]={to.x,to.y};
    for(int axis=0;axis<2;++axis) {
        const double difference=static_cast<double>(ends[axis]-starts[axis]);
        if(difference==0.0) continue;
        const auto first=static_cast<Integer>(std::floor(static_cast<double>(std::min(starts[axis],ends[axis]))/static_cast<double>(map.cell_size)))+1;
        const auto last=static_cast<Integer>(std::floor(static_cast<double>(std::max(starts[axis],ends[axis]))/static_cast<double>(map.cell_size)));
        for(Integer boundary=first;boundary<=last;++boundary) {
            const double portion=static_cast<double>(boundary*map.cell_size-starts[axis])/difference;
            if(portion>EPSILON && portion<1.0-EPSILON) cuts.push_back(portion);
        }
    }
    std::sort(cuts.begin(),cuts.end());
    double total=0.0;
    std::vector<SegmentSpan> spans;
    for(std::size_t index=0;index<cuts.size();++index) {
        const double at=cuts[index];
        if(rate_at(map,static_cast<double>(from.x)+dx*at,static_cast<double>(from.y)+dy*at,mode,flags)==0) return fail<SegmentProfile>("SegmentBlocked");
        if(index==0 || at-cuts[index-1]<EPSILON) continue;
        const double middle=(at+cuts[index-1])/2.0;
        const auto rate=rate_at(map,static_cast<double>(from.x)+dx*middle,static_cast<double>(from.y)+dy*middle,mode,flags);
        if(rate==0) return fail<SegmentProfile>("SegmentBlocked");
        const double before=total;
        total+=length*(at-cuts[index-1])*static_cast<double>(rate);
        if(include_profile) {
            const PointD end={static_cast<double>(from.x)+dx*at,static_cast<double>(from.y)+dy*at};
            if(!spans.empty() && spans.back().rate==rate) {
                spans.back().to=end;
                spans.back().end_cost=total;
            } else {
                const PointD start=spans.empty()?PointD{static_cast<double>(from.x),static_cast<double>(from.y)}:spans.back().to;
                spans.push_back({start,end,before,total,rate});
            }
        }
    }
    if(total>static_cast<double>(MAX_COST)) return fail<SegmentProfile>("RouteTooLong");
    SegmentProfile result;
    result.cost=static_cast<Integer>(std::ceil(total-EPSILON));
    if(include_profile) {
        result.raw_cost=std::min(total,static_cast<double>(result.cost));
        result.spans=std::move(spans);
        if(!result.spans.empty()) result.spans.back().end_cost=result.raw_cost;
    }
    return {true,std::move(result),{}};
}

// 格心使用整数除法，奇数cell_size不会偷偷变成半整数节点。
PointI node_point(const TerrainMap& map,Integer node,PointI from,PointI to) {
    const auto count=static_cast<Integer>(map.cells.size());
    if(node==count) return from;
    if(node==count+1) return to;
    const Integer width=map.width/map.cell_size;
    return {(node%width)*map.cell_size+map.cell_size/2,(node/width)*map.cell_size+map.cell_size/2};
}

// 周边节点依次按dy、dx枚举，起点另包括自己的格心。
std::vector<Integer> neighbors(const TerrainMap& map,PointI point,bool include_self) {
    const Integer width=map.width/map.cell_size,height=map.height/map.cell_size;
    const Integer cx=point.x/map.cell_size,cy=point.y/map.cell_size;
    std::vector<Integer> result;
    for(Integer dy=-1;dy<=1;++dy) for(Integer dx=-1;dx<=1;++dx) {
        if(!include_self && dx==0 && dy==0) continue;
        if(cx+dx>=0 && cx+dx<width && cy+dy>=0 && cy+dy<height) result.push_back((cy+dy)*width+cx+dx);
    }
    return result;
}

// 终点邻格接精确目标，保持原实际端点而不是吸附格心。
bool near_cell(const TerrainMap& map,PointI a,PointI b) {
    return std::abs(a.x/map.cell_size-b.x/map.cell_size)<=1 && std::abs(a.y/map.cell_size-b.y/map.cell_size)<=1;
}

// 最小正费率忽略通行能力和障碍，与原启发式完全同源。
Integer minimum_rate(const TerrainMap& map,Mode mode) {
    Integer result=10000;
    for(const auto* surface:surfaces) {
        const auto rate=configured_rate(map,mode,surface);
        if(rate>0) result=std::min(result,rate);
    }
    return result;
}

// 精确整数坐标相等，不经浮点近似或位置别名判断。
bool same_point(PointI a,PointI b) { return a.x==b.x && a.y==b.y; }

// 只移除同向共线点，叉积与点积均在有效地图的int64范围内。
std::vector<PointI> simplify(const std::vector<PointI>& points) {
    std::vector<PointI> result;
    for(const auto point:points) {
        while(result.size()>=2) {
            const auto a=result[result.size()-2],b=result.back();
            const Integer dx=b.x-a.x,dy=b.y-a.y,ex=point.x-b.x,ey=point.y-b.y;
            if(dx*ey!=dy*ex || dx*ex+dy*ey<0) break;
            result.pop_back();
        }
        result.push_back(point);
    }
    return result;
}

// 最终简化折线重新逐段计价，避免保留被移除中点造成的额外向上取整。
Outcome<Route> path_result(const TerrainMap& map,std::vector<PointI> points,Mode mode,const Flags& flags) {
    Route result;
    result.points=std::move(points);
    for(std::size_t index=1;index<result.points.size();++index) {
        const auto part=segment(map,result.points[index-1],result.points[index],mode,flags,false);
        if(!part.ok) return {false,{},part.error};
        result.segment_costs.push_back(part.value.cost);
        result.cost+=part.value.cost;
        if(result.cost>MAX_COST) return fail<Route>("RouteTooLong");
    }
    return {true,std::move(result),{}};
}

// 堆条目保留已发现距离快照；过期条目出堆时由当前distances剔除。
struct Candidate { Integer node=0,cost=0,estimate=0; };

// 同成本路线按estimate、cost、node决胜，与旧最小堆比较顺序一致。
bool before(const Candidate& a,const Candidate& b) {
    if(a.estimate!=b.estimate) return a.estimate<b.estimate;
    if(a.cost!=b.cost) return a.cost<b.cost;
    return a.node<b.node;
}

// 原堆上浮流程不为完全相等条目增加稳定序号或改变节点顺序。
void push(std::vector<Candidate>& heap,Candidate item) {
    heap.push_back(item);
    auto index=heap.size()-1;
    while(index>0) {
        const auto parent=(index-1)/2;
        if(!before(heap[index],heap[parent])) break;
        std::swap(heap[index],heap[parent]);
        index=parent;
    }
}

// 调用方保证堆非空；下沉时相等左右子项保留左项优先。
Candidate pop(std::vector<Candidate>& heap) {
    const auto result=heap.front(),last=heap.back();
    heap.pop_back();
    if(heap.empty()) return result;
    heap.front()=last;
    std::size_t index=0;
    while(index*2+1<heap.size()) {
        auto child=index*2+1;
        if(child+1<heap.size() && before(heap[child+1],heap[child])) ++child;
        if(!before(heap[child],heap[index])) break;
        std::swap(heap[child],heap[index]);
        index=child;
    }
    return result;
}
} // 全部辅助结构仅在本次纯查询内有效。

// 验证顺序与旧入口一致：尺寸、格数、逐格地形/设施/旗标、成本覆盖。
Status validate_map(const TerrainMap& map) {
    if(map.width<1 || map.width>100000 || map.height<1 || map.height>100000 || map.cell_size<1 || map.cell_size>100000) return fail<Done>("InvalidMapDimensions");
    if(map.width%map.cell_size!=0 || map.height%map.cell_size!=0 || map.cell_size<2) return fail<Done>("InvalidMapDimensions");
    const Integer count=(map.width/map.cell_size)*(map.height/map.cell_size);
    if(count>65536 || map.cells.size()!=static_cast<std::size_t>(count)) return fail<Done>("InvalidMapCells");
    for(const auto& cell:map.cells) {
        const auto surface=surface_index(cell.terrain);
        if(surface<0 || surface>=5) return fail<Done>("InvalidTerrain");
        if(cell.crossing) {
            if(*cell.crossing!="ford" && *cell.crossing!="bridge" && *cell.crossing!="pass") return fail<Done>("InvalidCrossing");
            if((*cell.crossing=="pass" && cell.terrain!="mountain") || (*cell.crossing!="pass" && cell.terrain!="water")) return fail<Done>("InvalidCrossingTerrain");
        }
        if(cell.blocked_flag && !blocked_flag_valid(*cell.blocked_flag)) return fail<Done>("InvalidBlockedFlag");
    }
    for(const auto& mode_costs:map.costs) {
        if(!valid_mode(mode_costs.first)) return fail<Done>("InvalidTravelCosts");
        for(const auto& entry:mode_costs.second) {
            if(surface_index(entry.first)<0 || entry.second<0 || entry.second>10000) return fail<Done>("InvalidTravelCosts");
        }
    }
    return {true,{},{}};
}

// 所有无效输入统一使驻留查询返回false，不改变调用者输入。
bool point_valid(const TerrainMap& map,PointI point,Mode mode,const Flags& flags) {
    if(!context(map,mode).ok) return false;
    return point_valid_prepared(map,point,mode,flags);
}

// 已验证只读目录免于逐次扫描；非法模式仍先于任何数组索引被拒绝。
bool point_valid_prepared(const TerrainMap& map,PointI point,Mode mode,const Flags& flags) {
    if(!valid_mode(mode) || !inside(map,point)) return false;
    return rate_at(map,static_cast<double>(point.x),static_cast<double>(point.y),mode,flags)>0;
}

// 成本与轨迹共用内部积分，省略轨迹时不改变任何运算顺序。
Outcome<SegmentCost> segment_cost(const TerrainMap& map,PointI from,PointI to,Mode mode,const Flags& flags) {
    const auto valid=context(map,mode);
    if(!valid.ok) return {false,{},valid.error};
    return segment_cost_prepared(map,from,to,mode,flags);
}

// 已验证目录的成本入口保留端点与每个格界的完整检查，不重新扫描整图。
Outcome<SegmentCost> segment_cost_prepared(const TerrainMap& map,PointI from,PointI to,Mode mode,const Flags& flags) {
    if(!valid_mode(mode)) return fail<SegmentCost>("InvalidTravelMode");
    const auto result=segment(map,from,to,mode,flags,false);
    if(!result.ok) return {false,{},result.error};
    return {true,{result.value.cost},{}};
}

// 轨迹返回拥有型值；同一输入重查不会复用或改写上次结果。
Outcome<SegmentProfile> segment_profile(const TerrainMap& map,PointI from,PointI to,Mode mode,const Flags& flags) {
    const auto valid=context(map,mode);
    if(!valid.ok) return {false,{},valid.error};
    return segment_profile_prepared(map,from,to,mode,flags);
}

// 已验证目录的轨迹积分；调用方必须保持map与flags在调用期间不变。
Outcome<SegmentProfile> segment_profile_prepared(const TerrainMap& map,PointI from,PointI to,Mode mode,const Flags& flags) {
    if(!valid_mode(mode)) return fail<SegmentProfile>("InvalidTravelMode");
    return segment(map,from,to,mode,flags,true);
}

// 原A*保留起点直连目标、八邻接和首次等价路径；失败时不提供半条路线。
Outcome<Route> plan(const TerrainMap& map,PointI from,PointI to,Mode mode,const Flags& flags) {
    const auto valid=context(map,mode);
    if(!valid.ok) return {false,{},valid.error};
    return plan_prepared(map,from,to,mode,flags);
}

// 已验证目录的原A*入口；免扫整图不意味着跳过模式、端点或候选边通行检查。
Outcome<Route> plan_prepared(const TerrainMap& map,PointI from,PointI to,Mode mode,const Flags& flags) {
    if(!valid_mode(mode)) return fail<Route>("InvalidTravelMode");
    if(!inside(map,from) || rate_at(map,static_cast<double>(from.x),static_cast<double>(from.y),mode,flags)==0) return fail<Route>("InvalidOrigin");
    if(!inside(map,to) || rate_at(map,static_cast<double>(to.x),static_cast<double>(to.y),mode,flags)==0) return fail<Route>("InvalidDestination");
    if(same_point(from,to)) return {true,{{from},{},0},{}};
    const auto origin=static_cast<Integer>(map.cells.size()),destination=origin+1;
    const auto count=static_cast<std::size_t>(destination+1);
    std::vector<Integer> distances(count,-1),previous(count,-1);
    std::vector<bool> visited(count,false);
    distances[static_cast<std::size_t>(origin)]=0;
    const auto minimum=minimum_rate(map,mode);
    std::vector<Candidate> heap;
    push(heap,{origin,0,0});
    while(!heap.empty()) {
        const auto current=pop(heap);
        auto id=current.node;
        const auto index=static_cast<std::size_t>(id);
        if(visited[index] || current.cost!=distances[index]) continue;
        if(id==destination) {
            std::vector<Integer> ids={id};
            while(id!=origin) {
                id=previous[static_cast<std::size_t>(id)];
                ids.push_back(id);
            }
            std::reverse(ids.begin(),ids.end());
            std::vector<PointI> points;
            for(const auto node:ids) {
                const auto point=node_point(map,node,from,to);
                if(points.empty() || !same_point(point,points.back())) points.push_back(point);
            }
            return path_result(map,simplify(points),mode,flags);
        }
        visited[index]=true;
        const auto point=node_point(map,id,from,to);
        auto adjacent=neighbors(map,point,id==origin);
        if(id==origin || near_cell(map,point,to)) adjacent.push_back(destination);
        for(const auto next:adjacent) {
            const auto next_index=static_cast<std::size_t>(next);
            if(visited[next_index]) continue;
            const auto end=node_point(map,next,from,to);
            const auto part=segment(map,point,end,mode,flags,false);
            if(!part.ok) continue;
            const Integer cost=distances[index]+part.value.cost;
            if(cost>MAX_COST || (distances[next_index]>=0 && cost>=distances[next_index])) continue;
            distances[next_index]=cost;
            previous[next_index]=id;
            const double dx=static_cast<double>(end.x-to.x),dy=static_cast<double>(end.y-to.y);
            const auto heuristic=static_cast<Integer>(std::floor(std::sqrt(dx*dx+dy*dy)*static_cast<double>(minimum)));
            push(heap,{next,cost,cost+heuristic});
        }
    }
    return fail<Route>("NoRoute");
}
// 连续域范围仍严格半开；finite是输入合法性，不用epsilon吸附位置。
bool point_valid_continuous_prepared(const TerrainMap& map,PointD point,Mode mode,const Flags& flags){return valid_mode(mode)&&std::isfinite(point.x)&&std::isfinite(point.y)&&point.x>=0&&point.y>=0&&point.x<map.width&&point.y<map.height&&rate_at(map,point.x,point.y,mode,flags)>0;}
// 原整数输入先分派原函数；新首段只改差值和防下溢长度，保原切格/积分/末段截齐。
Outcome<SegmentProfile> segment_profile_continuous_prepared(const TerrainMap& map,PointD from,PointD to,Mode mode,const Flags& flags){
    if(!valid_mode(mode))return fail<SegmentProfile>("InvalidTravelMode");
    if(!point_valid_continuous_prepared(map,from,mode,flags))return fail<SegmentProfile>("InvalidOrigin");
    if(!point_valid_continuous_prepared(map,to,mode,flags))return fail<SegmentProfile>("InvalidDestination");
    if(from.x==std::floor(from.x)&&from.y==std::floor(from.y)&&to.x==std::floor(to.x)&&to.y==std::floor(to.y))return segment_profile_prepared(map,{Integer(from.x),Integer(from.y)},{Integer(to.x),Integer(to.y)},mode,flags);
    const double dx=to.x-from.x,dy=to.y-from.y,s=std::max(std::abs(dx),std::abs(dy));if(s==0)return {true,{},{}};
    const double nx=dx/s,ny=dy/s,length=s*std::sqrt(nx*nx+ny*ny);std::vector<double> cuts={0,1};
    const double starts[2]={from.x,from.y},ends[2]={to.x,to.y};for(int axis=0;axis<2;++axis){double difference=ends[axis]-starts[axis];if(difference==0)continue;Integer first=Integer(std::floor(std::min(starts[axis],ends[axis])/double(map.cell_size)))+1,last=Integer(std::floor(std::max(starts[axis],ends[axis])/double(map.cell_size)));for(Integer boundary=first;boundary<=last;++boundary){double portion=(double(boundary*map.cell_size)-starts[axis])/difference;if(portion>EPSILON&&portion<1-EPSILON)cuts.push_back(portion);}}
    std::sort(cuts.begin(),cuts.end());double total=0;std::vector<SegmentSpan> spans;for(std::size_t i=0;i<cuts.size();++i){double at=cuts[i];if(!rate_at(map,from.x+dx*at,from.y+dy*at,mode,flags))return fail<SegmentProfile>("SegmentBlocked");if(i==0||at-cuts[i-1]<EPSILON)continue;double middle=(at+cuts[i-1])/2;auto rate=rate_at(map,from.x+dx*middle,from.y+dy*middle,mode,flags);if(!rate)return fail<SegmentProfile>("SegmentBlocked");double before=total;total+=length*(at-cuts[i-1])*double(rate);PointD end{from.x+dx*at,from.y+dy*at};if(!spans.empty()&&spans.back().rate==rate){spans.back().to=end;spans.back().end_cost=total;}else spans.push_back({spans.empty()?from:spans.back().to,end,before,total,rate});}
    if(total>double(MAX_COST))return fail<SegmentProfile>("RouteTooLong");SegmentProfile out;out.cost=std::max<Integer>(1,Integer(std::ceil(total-EPSILON)));out.raw_cost=std::min(total,double(out.cost));out.spans=std::move(spans);if(!out.spans.empty())out.spans.back().end_cost=out.raw_cost;return {true,std::move(out),{}};
}
// 精确dyadic差号与绝对值，只用于continuous起点共线判断，不代替原积分浮点顺序。
namespace {
struct SignedDyadic { bool negative=false; CanonicalFraction magnitude; };
struct GeometryFailure { RuleError error; };
template<class T> T exact_value(Outcome<T> value){if(!value.ok)throw GeometryFailure{value.error};return std::move(value.value);}
SignedDyadic exact_difference(double a,double b,ExactArithmeticBudget budget){auto x=exact_value(fraction_from_binary64(a,budget)),y=exact_value(fraction_from_binary64(b,budget));bool negative=exact_value(compare_fractions(x,y,budget))<0;return {negative,negative?exact_value(subtract_fractions(y,x,budget)):exact_value(subtract_fractions(x,y,budget))};}
SignedDyadic exact_product(const SignedDyadic& a,const SignedDyadic& b,ExactArithmeticBudget budget){auto result=exact_value(multiply_fractions(a.magnitude,b.magnitude,budget));return {exact_value(compare_fractions(result,CanonicalFraction{},budget))==0?false:a.negative!=b.negative,result};}
bool continuous_collinear_forward(PointD a,PointI b,PointI c,ExactArithmeticBudget budget){auto dx=exact_difference(double(b.x),a.x,budget),dy=exact_difference(double(b.y),a.y,budget),ex=exact_difference(double(c.x),double(b.x),budget),ey=exact_difference(double(c.y),double(b.y),budget);auto left=exact_product(dx,ey,budget),right=exact_product(dy,ex,budget);if(left.negative!=right.negative||exact_value(compare_fractions(left.magnitude,right.magnitude,budget))!=0)return false;auto first=exact_product(dx,ex,budget),second=exact_product(dy,ey,budget);if(!first.negative&&!second.negative)return true;if(first.negative&&second.negative)return false;return first.negative?exact_value(compare_fractions(second.magnitude,first.magnitude,budget))>=0:exact_value(compare_fractions(first.magnitude,second.magnitude,budget))>=0;}
}
// A*仅虚拟origin换为实际continuous点，其余节点/堆/启发/平手和旧整数边全部复用。
Outcome<ContinuousRoute> plan_continuous_prepared(const TerrainMap& map,PointD from,PointI to,Mode mode,const Flags& flags,ExactArithmeticBudget budget){try{
    if(!valid_mode(mode))return fail<ContinuousRoute>("InvalidTravelMode");if(!point_valid_continuous_prepared(map,from,mode,flags))return fail<ContinuousRoute>("InvalidOrigin");if(!point_valid_prepared(map,to,mode,flags))return fail<ContinuousRoute>("InvalidDestination");
    if(from.x==std::floor(from.x)&&from.y==std::floor(from.y)){auto legacy=plan_prepared(map,{Integer(from.x),Integer(from.y)},to,mode,flags);if(!legacy.ok)return {false,{},legacy.error};ContinuousRoute out;out.origin=from;out.points.assign(legacy.value.points.begin()+1,legacy.value.points.end());out.segment_costs=legacy.value.segment_costs;out.cost=legacy.value.cost;return {true,std::move(out),{}};}
    const Integer origin=Integer(map.cells.size()),destination=origin+1;const auto count=std::size_t(destination+1);std::vector<Integer> distances(count,-1),previous(count,-1);std::vector<bool> visited(count,false);distances[std::size_t(origin)]=0;auto minimum=minimum_rate(map,mode);std::vector<Candidate> heap;push(heap,{origin,0,0});
    while(!heap.empty()){auto current=pop(heap);auto id=current.node;auto index=std::size_t(id);if(visited[index]||current.cost!=distances[index])continue;if(id==destination){std::vector<Integer> ids{id};while(id!=origin){id=previous[std::size_t(id)];ids.push_back(id);}std::reverse(ids.begin(),ids.end());std::vector<PointI> tail;for(auto node:ids)if(node!=origin){auto point=node_point(map,node,{},to);if(tail.empty()||!same_point(point,tail.back()))tail.push_back(point);}tail=simplify(tail);while(tail.size()>=2&&continuous_collinear_forward(from,tail[0],tail[1],budget))tail.erase(tail.begin());ContinuousRoute out;out.origin=from;out.points=tail;PointD here=from;for(auto p:tail){auto cost=segment_profile_continuous_prepared(map,here,{double(p.x),double(p.y)},mode,flags);if(!cost.ok)return {false,{},cost.error};out.segment_costs.push_back(cost.value.cost);out.cost+=cost.value.cost;if(out.cost>MAX_COST)return fail<ContinuousRoute>("RouteTooLong");here={double(p.x),double(p.y)};}return {true,std::move(out),{}};}
        visited[index]=true;const bool at_origin=id==origin;PointI integer_point=at_origin?PointI{Integer(std::floor(from.x)),Integer(std::floor(from.y))}:node_point(map,id,{},to);PointD point=at_origin?from:PointD{double(integer_point.x),double(integer_point.y)};auto adjacent=neighbors(map,integer_point,at_origin);if(at_origin||near_cell(map,integer_point,to))adjacent.push_back(destination);for(auto next:adjacent){auto ni=std::size_t(next);if(visited[ni])continue;auto end=node_point(map,next,{},to);auto part=at_origin?segment_profile_continuous_prepared(map,point,{double(end.x),double(end.y)},mode,flags):segment_profile_prepared(map,integer_point,end,mode,flags);if(!part.ok)continue;Integer cost=distances[index]+part.value.cost;if(cost>MAX_COST||(distances[ni]>=0&&cost>=distances[ni]))continue;distances[ni]=cost;previous[ni]=id;double dx=double(end.x-to.x),dy=double(end.y-to.y);auto heuristic=Integer(std::floor(std::sqrt(dx*dx+dy*dy)*double(minimum)));push(heap,{next,cost,cost+heuristic});}}
    return fail<ContinuousRoute>("NoRoute");
}catch(const GeometryFailure& error){return {false,{},error.error};}}
// 连续域20轮二分保原参数次序，预算不足最小正成本时保原点，不赠最低预算。
Outcome<PointD> partial_point_continuous_prepared(const TerrainMap& map,PointD from,PointI to,Mode mode,const Flags& flags,const CanonicalFraction& budget,ExactArithmeticBudget arithmetic){try{Integer low=0,high=1000000;PointD result=from;for(int i=0;i<20;++i){Integer amount=(low+high+1)/2;double ratio=double(amount)/1000000.0;PointD candidate{from.x+(double(to.x)-from.x)*ratio,from.y+(double(to.y)-from.y)*ratio};auto prefix=segment_profile_continuous_prepared(map,from,candidate,mode,flags);bool allowed=false;if(prefix.ok){auto cost=exact_value(normalize_fraction(std::to_string(prefix.value.cost),"1",arithmetic));allowed=exact_value(compare_fractions(cost,budget,arithmetic))<=0&&segment_profile_continuous_prepared(map,candidate,{double(to.x),double(to.y)},mode,flags).ok;}if(allowed){result=candidate;low=amount;}else high=amount-1;if(low>=high)break;}return {true,result,{}};}catch(const GeometryFailure& error){return {false,{},error.error};}}
} // 本模块不包含任何世界写入、预演或GDScript回调。
