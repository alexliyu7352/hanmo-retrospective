// 原terrain_contact的binary64同步运动求交；禁止fast-math及FMA重排改变规则量化结果。
#include "contact_model.hpp"
#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <limits>
#if defined(__FAST_MATH__)
#error "Contact geometry requires IEEE binary64 without fast-math"
#endif
#if defined(__clang__)
#pragma clang fp contract(off)
#pragma clang fp reassociate(off)
#elif defined(__GNUC__)
#pragma GCC optimize ("no-fast-math", "fp-contract=off")
#endif

namespace hanmo::resident {
namespace {
constexpr double epsilon=0.000000000001,radius=4.0;
constexpr Integer time_den=1000000000;
// 无部分成功的原领域错误，几何不设置事务或能力状态。
Status failure(const char* code){return {false,{},{{code,""},ErrorKind::rule,{},{},{},{}}};}
// 连续点逐坐标比较，与GD整数/浮点Array混合比较保持同一epsilon。
bool same(ContactPoint a,ContactPoint b){return std::abs(a.x-b.x)<epsilon&&std::abs(a.y-b.y)<epsilon;}
// 持久点只接受地图合法整数域，不把图外点夹回。
bool point(PointI p){return p.x>=0&&p.y>=0&&p.x<=100000&&p.y<=100000;}
// 临时积分交点允许有限binary64，不允许NaN或图外坐标。
bool point(ContactPoint p){return std::isfinite(p.x)&&std::isfinite(p.y)&&p.x>=0&&p.y>=0&&p.x<=100000&&p.y<=100000;}
// 整数转临时浮点仅在几何边界发生，保留64位标量语义。
ContactPoint real(PointI p){return {static_cast<double>(p.x),static_cast<double>(p.y)};}
// 切段归一化半日时刻；速度先作源端点差，再除时长，不能提前量化。
struct Segment {ContactPoint from,to;double start,end,vx,vy;Integer point_index;};
// 从真实积分段生成匀速片段，不复用另一轨迹的预算。
Segment segment(const TravelMotion& trace,std::size_t i){
    const auto& p=trace.segments[i];double start=p.start_cost/static_cast<double>(trace.budget),end=p.end_cost/static_cast<double>(trace.budget);
    return {p.from,p.to,start,end,(p.to.x-p.from.x)/(end-start),(p.to.y-p.from.y)/(end-start),p.point_index};
}
// 沿本人的段投影连续位置，先夹时刻再减起点，与原运算次序相同。
ContactPoint position(const Segment& s,double time){double elapsed=std::clamp(time,s.start,s.end)-s.start;return {s.from.x+s.vx*elapsed,s.from.y+s.vy*elapsed};}
// 求相对速度进入半径四圆的时间区间，负epsilon只用于切线舍入余量。
std::optional<std::pair<double,double>> inside(ContactPoint offset,ContactPoint velocity,double duration){
    double speed_squared=velocity.x*velocity.x+velocity.y*velocity.y;
    if(speed_squared<=epsilon){if(offset.x*offset.x+offset.y*offset.y<=radius*radius)return {{0.0,duration}};return {};}
    double closest=-(offset.x*velocity.x+offset.y*velocity.y)/speed_squared;
    double x=offset.x+velocity.x*closest,y=offset.y+velocity.y*closest;
    double remainder=radius*radius-x*x-y*y;
    if(remainder < -epsilon)return {};
    double half_span=std::sqrt(std::max(0.0,remainder)/speed_squared);
    double enter=std::max(0.0,closest-half_span),leave=std::min(duration,closest+half_span);
    if(enter>leave+epsilon)return {};
    return {{std::min(enter,duration),std::max(leave,0.0)}};
}
// 排除任何一方落在据点入口半径内的区间；定义次序不影响结果。
double outside(const Segment& a,const Segment& b,double start,double end,const std::vector<PointI>& centers){
    std::vector<std::pair<double,double>> blocked;
    for(auto center:centers)for(const auto* s:{&a,&b}){
        auto p=position(*s,start);auto interval=inside({p.x-center.x,p.y-center.y},{s->vx,s->vy},end-start);
        if(interval)blocked.emplace_back(start+interval->first,start+interval->second);
    }
    std::sort(blocked.begin(),blocked.end());double candidate=start;
    for(const auto& interval:blocked){if(interval.second<candidate)continue;if(interval.first>candidate)break;candidate=interval.second+1.0/static_cast<double>(time_den);}
    return candidate<=end?candidate:-1.0;
}
// 候选只保存同一次检测输入的借用和已经算好的整数几何，不提前复制整份行旅/版本列表。
struct AnchorGeometry { PointI point; Integer suffix_index=0; };
struct ContactCandidate {
    const TravelMotion* first=nullptr; const TravelMotion* second=nullptr;
    Integer time_num=0;
    AnchorGeometry first_anchor,second_anchor;
};
// 与公开ContactHit比较器同一有理时刻和人物平局规则，借用从不离开detect调用。
bool candidate_before(const ContactCandidate& a,const ContactCandidate& b){Integer left=a.time_num*time_den,right=b.time_num*time_den;if(left!=right)return left<right;return a.first->actor_id==b.first->actor_id?a.second->actor_id<b.second->actor_id:a.first->actor_id<b.first->actor_id;}
// 所有位置、时间与suffix运算保持原顺序；此处只延后纯深复制，不删任何几何候选。
std::optional<AnchorGeometry> anchor(const TravelMotion& source,const Segment& s,double time){
    if(source.stationary)return AnchorGeometry{source.stationary_location->point,0};
    Integer index=s.point_index;auto p=position(s,time);
    if(time>=s.end-epsilon&&index<static_cast<Integer>(source.params.points.size())&&same(s.to,real(source.params.points[static_cast<std::size_t>(index)])))++index;
    while(index<static_cast<Integer>(source.params.points.size())&&std::abs(p.x-source.params.points[static_cast<std::size_t>(index)].x)<epsilon&&std::abs(p.y-source.params.points[static_cast<std::size_t>(index)].y)<epsilon)++index;
    if(index>=static_cast<Integer>(source.params.points.size()))return {};
    return AnchorGeometry{{static_cast<Integer>(std::round(p.x)),static_cast<Integer>(std::round(p.y))},index};
}
// 只有真正选中的端点才深拥有剩余行动；公开返回值与输入没有任何可写别名。
ContactEndpoint materialize_endpoint(const TravelMotion& source,const AnchorGeometry& geometry){
    if(source.stationary)return ContactEndpoint{source.actor_id,*source.stationary_location,source};
    auto copied=source;copied.params.points.erase(copied.params.points.begin(),copied.params.points.begin()+geometry.suffix_index);
    copied.params.route_index+=geometry.suffix_index;copied.params.travel_credit=0;
    return ContactEndpoint{source.actor_id,{source.map_id,source.origin,geometry.point},std::move(copied)};
}
// 选择完成后才建立稳定ID/锚点/轨迹的完整拥有型结果，不把输入指针传给C安装入口。
ContactHit materialize(const ContactCandidate& hit){return ContactHit{hit.first->actor_id,hit.second->actor_id,hit.time_num,time_den,materialize_endpoint(*hit.first,hit.first_anchor),materialize_endpoint(*hit.second,hit.second_anchor)};}
// 真端点位移是唯一moving判据，元数据不能制造会面。
bool moves(const TravelMotion& trace){for(const auto& s:trace.segments)if(!same(s.from,s.to))return true;return false;}
// 已通过trace验证的内部求交，规范人物次序后按原双循环选择首次接触。
std::optional<ContactCandidate> intersection(const TravelMotion& first,const TravelMotion& second,const ContactEntrances& entrances){
    const auto& a=first.actor_id<second.actor_id?first:second;const auto& b=first.actor_id<second.actor_id?second:first;
    if(a.actor_id==b.actor_id||(!a.action_id.empty()&&a.action_id==b.action_id)||a.map_id!=b.map_id)return {};
    if(!a.params.encounter_id.empty()||!b.params.encounter_id.empty())return {};
    if(std::find(b.params.met_actions.begin(),b.params.met_actions.end(),a.action_id)!=b.params.met_actions.end()||std::find(a.params.met_actions.begin(),a.params.met_actions.end(),b.action_id)!=a.params.met_actions.end())return {};
    if(!moves(a)&&!moves(b))return {};
    static const std::vector<PointI> empty;auto it=entrances.find(a.map_id);const auto& centers=it==entrances.end()?empty:it->second;
    for(auto p:centers)if(!point(p))return {};
    std::optional<ContactCandidate> best;
    for(std::size_t ai=0;ai<a.segments.size();++ai){auto sa=segment(a,ai);if(sa.point_index>=static_cast<Integer>(a.params.points.size()))continue;
        for(std::size_t bi=0;bi<b.segments.size();++bi){auto sb=segment(b,bi);if(sb.point_index>=static_cast<Integer>(b.params.points.size()))continue;
            double start=std::max(sa.start,sb.start),end=std::min(sa.end,sb.end);if(start>end)continue;
            auto pa=position(sa,start),pb=position(sb,start);auto interval=inside({pa.x-pb.x,pa.y-pb.y},{sa.vx-sb.vx,sa.vy-sb.vy},end-start);if(!interval)continue;
            double time=outside(sa,sb,start+interval->first,start+interval->second,centers);if(time<0.0)continue;
            auto aa=anchor(a,sa,time),ab=anchor(b,sb,time);if(!aa||!ab)continue;
            ContactCandidate hit{&a,&b,static_cast<Integer>(std::round(time*static_cast<double>(time_den))),*aa,*ab};
            if(!best||candidate_before(hit,*best))best=hit;
        }
    }
    return best;
}
} // 无可变全局缓存；每次检测只使用自己的冻结轨迹。

// 逐原首错校验，typed字段仍核范围和连续性，不以模型类型替代领域验证。
Status validate_terrain_trace(const TravelMotion& t){
    if(t.actor_id.empty()||t.map_id.empty()||t.origin.empty())return failure("InvalidTerrainTraceIdentity");
    if(t.stationary){
        if(!t.stationary_location)return failure("InvalidStationaryTerrainTrace");
        auto expected=stationary_terrain_trace(t.actor_id,*t.stationary_location);
        if(!expected||t.action_id!=""||t.map_id!=expected->map_id||t.origin!=expected->origin||t.budget!=1||t.params.points.size()!=1||t.segments.size()!=1)return failure("InvalidStationaryTerrainTrace");
        const auto& parameters=t.params;
        if(!parameters.plan_id.empty()||!parameters.map_id.empty()||!parameters.origin.empty()||!parameters.payer_wallet_id.empty()||parameters.mode!="foot"||parameters.route_index!=1||parameters.budget!=1||parameters.travel_credit!=0||parameters.money_per_tick!=0||parameters.fatigue_per_tick!=0||parameters.paused||!parameters.encounter_id.empty()||!parameters.met_actions.empty()||!std::holds_alternative<StableId>(parameters.destination)||!std::get<StableId>(parameters.destination).empty()||t.moving||t.end_credit!=0||t.reached_points!=0) return failure("InvalidStationaryTerrainTrace");
        const auto& s=t.segments.front();auto p=real(t.stationary_location->point);
        if(real(t.params.points.front()).x!=p.x||real(t.params.points.front()).y!=p.y||s.from.x!=p.x||s.from.y!=p.y||s.to.x!=p.x||s.to.y!=p.y||s.start_cost!=0||s.end_cost!=1||s.point_index!=0)return failure("InvalidStationaryTerrainTrace");
        return {true,{},{}};
    }
    if(t.action_id.empty())return failure("InvalidTerrainTraceIdentity");
    if(t.budget<=0||t.budget>4294967294LL)return failure("InvalidTerrainTraceBudget");
    if(t.params.points.empty())return failure("InvalidTerrainTracePoints");
    for(auto p:t.params.points)if(!point(p))return failure("InvalidTerrainTracePoints");
    if(t.params.route_index<1)return failure("InvalidTerrainTracePointIndex");
    if(t.params.travel_credit<0||t.params.travel_credit>2147483647LL)return failure("InvalidTerrainTraceBudget");
    if(t.segments.empty())return failure("InvalidTerrainTraceSegments");
    double previous_cost=0.0;std::optional<ContactPoint> previous_point;Integer previous_index=-1;
    for(const auto& s:t.segments){
        if(!point(s.from)||!point(s.to))return failure("InvalidTerrainTraceSegments");
        if(!std::isfinite(s.start_cost)||!std::isfinite(s.end_cost)||s.start_cost!=previous_cost||s.end_cost<=s.start_cost||s.end_cost>t.budget)return failure("InvalidTerrainTraceTime");
        if(previous_point&&!same(s.from,*previous_point))return failure("DiscontinuousTerrainTrace");
        if(s.point_index<previous_index||s.point_index<0||s.point_index>static_cast<Integer>(t.params.points.size()))return failure("InvalidTerrainTracePointIndex");
        if(s.point_index==static_cast<Integer>(t.params.points.size())&&(!same(s.from,s.to)||!same(s.to,real(t.params.points.back()))))return failure("InvalidTerrainTraceFinishedTail");
        previous_cost=s.end_cost;previous_point=s.to;previous_index=s.point_index;
    }
    if(previous_cost!=t.budget)return failure("IncompleteTerrainTrace");
    return {true,{},{}};
}
// 无行动停驻覆盖半日，不产生可兑现旅行预算。
std::optional<TravelMotion> stationary_terrain_trace(const StableId& actor,const WildernessLocation& location){
    if(actor.empty()||location.map_id.empty()||location.origin.empty()||!point(location.point))return {};
    TravelMotion t;t.actor_id=actor;t.map_id=location.map_id;t.origin=location.origin;t.stationary=true;t.stationary_location=location;t.params.points={location.point};auto p=real(location.point);t.segments.push_back({p,p,0.0,1.0,0});return t;
}
// 固定分母且范围不超过十亿，交叉乘积在int64内；按原人物元组打破平局。
bool contact_before(const ContactHit& a,const ContactHit& b){Integer l=a.time_num*b.time_den,r=b.time_num*a.time_den;if(l!=r)return l<r;return a.a==b.a?a.b<b.b:a.a<b.a;}
// 公共双轨迹入口对损坏输入返回无接触，不将其当成合法移动。
std::optional<ContactHit> terrain_intersection(const TravelMotion& a,const TravelMotion& b,const ContactEntrances& entrances){if(!validate_terrain_trace(a).ok||!validate_terrain_trace(b).ok)return {};auto hit=intersection(a,b,entrances);if(!hit)return {};return materialize(*hit);}
// 新域只接收原 basis 已采样的局部单段；共同窗和最终量化仍归原精确时间 caller。
Outcome<std::optional<LocalTerrainContact>> terrain_local_interval_intersection(
    const TravelMotion &first, const TravelMotion &second, bool instant_only,
    const ContactEntrances &entrances) {
  for (const auto *trace : {&first, &second}) {
    auto valid = validate_terrain_trace(*trace);
    if (!valid.ok)
      return {false, {}, std::move(valid.error)};
    if (trace->budget != 1 || trace->segments.size() != 1 ||
        trace->segments.front().start_cost != 0.0 ||
        trace->segments.front().end_cost != 1.0)
      return {false, {}, failure("InvalidLocalTerrainTrace").error};
    // 零宽窗两端必须确实采自同一刻，不把有位移的输入冒作瞬时接触。
    const auto &local = trace->segments.front();
    if (instant_only &&
        (local.from.x != local.to.x || local.from.y != local.to.y))
      return {false, {}, failure("InvalidLocalTerrainInstantTrace").error};
  }
  const auto &a = first.actor_id < second.actor_id ? first : second;
  const auto &b = first.actor_id < second.actor_id ? second : first;
  if (a.actor_id == b.actor_id ||
      (!a.action_id.empty() && a.action_id == b.action_id) ||
      a.map_id != b.map_id || !a.params.encounter_id.empty() ||
      !b.params.encounter_id.empty())
    return {true, {}, {}};
  if (std::find(b.params.met_actions.begin(), b.params.met_actions.end(),
                a.action_id) != b.params.met_actions.end() ||
      std::find(a.params.met_actions.begin(), a.params.met_actions.end(),
                b.action_id) != a.params.met_actions.end())
    return {true, {}, {}};
  // 整体 Running/Held 资格归原 caller；局部静止可能是有效
  // padding，仍须核原圆与入口。
  static const std::vector<PointI> empty;
  const auto found = entrances.find(a.map_id);
  const auto &centers = found == entrances.end() ? empty : found->second;
  for (const auto center : centers)
    if (!point(center))
      return {false, {}, failure("InvalidTerrainContactEntrance").error};
  const auto sa = segment(a, 0), sb = segment(b, 0);
  if (sa.point_index >= static_cast<Integer>(a.params.points.size()) ||
      sb.point_index >= static_cast<Integer>(b.params.points.size()))
    return {true, {}, {}};
  // 局部时长固定为 1，速度就是有界端点差；不除以可能极短的世界时长。
  const auto pa = position(sa, 0.0), pb = position(sb, 0.0);
  const auto interval =
      inside({pa.x - pb.x, pa.y - pb.y}, {sa.vx - sb.vx, sa.vy - sb.vy},
             instant_only ? 0.0 : 1.0);
  if (!interval)
    return {true, {}, {}};
  const auto time = outside(sa, sb, interval->first, interval->second, centers);
  if (time < 0.0)
    return {true, {}, {}};
  const auto aa = anchor(a, sa, time), ab = anchor(b, sb, time);
  if (!aa || !ab)
    return {true, {}, {}};
  // 原未量化参数生成各自锚点和余程；只把此 binary64 交给 caller 作精确映回。
  return {true,
          LocalTerrainContact{time, materialize_endpoint(a, *aa),
                              materialize_endpoint(b, *ab)},
          {}};
}
// 全批先统计有效身份，重复者整组排除；单人每半日最多参与一场。
std::vector<ContactHit> detect_terrain_contacts(const std::vector<TravelMotion>& traces,const ContactEntrances& entrances,Integer* candidates,Integer* exact){
    if(candidates)*candidates=0;
    if(exact)*exact=0;
    std::vector<const TravelMotion*> valid;std::unordered_map<StableId,Integer> people,actions;
    for(const auto& t:traces)if(validate_terrain_trace(t).ok){valid.push_back(&t);++people[t.actor_id];if(!t.action_id.empty())++actions[t.action_id];}
    std::vector<ContactCandidate> hits;
    for(std::size_t i=0;i<valid.size();++i){const auto& a=*valid[i];if(people[a.actor_id]>1||actions[a.action_id]>1)continue;
        for(std::size_t j=i+1;j<valid.size();++j){const auto& b=*valid[j];if(people[b.actor_id]>1||actions[b.action_id]>1)continue;if(candidates)++*candidates;
            // 原时间epsilon允许极短段边界外接触；不能用固定半径空间包围盒提前剔除。
            if(a.map_id!=b.map_id)continue;
            if(exact)++*exact;
            auto hit=intersection(a,b,entrances);if(hit)hits.push_back(std::move(*hit));
        }
    }
    std::sort(hits.begin(),hits.end(),candidate_before);std::unordered_set<StableId> occupied;std::vector<ContactHit> result;
    for(const auto& hit:hits){if(occupied.count(hit.first->actor_id)||occupied.count(hit.second->actor_id))continue;occupied.insert(hit.first->actor_id);occupied.insert(hit.second->actor_id);result.push_back(materialize(hit));}return result;
}
} // 同时触及军事者仍由上层按原普通/军事分流，纯几何不删军人。
