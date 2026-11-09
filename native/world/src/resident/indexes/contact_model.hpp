// 行动前接触快照与真实整数锚点；C只消费本phase批次并通过B截断行旅。
#pragma once
#include "../domains/travel/travel_model.hpp"
namespace hanmo::resident {
// 积分格界点保binary64；整数持久位置只在接触锚点显式量化。
struct ContactPoint { double x=0.0,y=0.0; };
struct ContactSegment { ContactPoint from,to; double start_cost=0.0,end_cost=0.0; Integer point_index=0; };
// 由原积分实际分支标记成本，不从静止端点或credit反猜已耗。
enum class TerrainCostPieceKind { moving_cost,rounding_padding,unused_budget_tail };
struct TravelMotion {
    StableId actor_id,action_id,map_id,origin;
    TerrainTravelParams params;
    std::vector<ContactSegment> segments;
    Integer budget=1,end_credit=0,reached_points=0;
    PointI end;
    bool moving=false,stationary=false;
    std::optional<WildernessLocation> stationary_location;
    std::vector<ReadVersion> read_versions;
    std::vector<TerrainCostPieceKind> cost_piece_kinds;
    Integer actual_consumed_total=0,original_point_count=0;
};
// 每个锚点保留自己的剩余折线，不把两个人吸附到共同中点。
struct ContactEndpoint { StableId actor_id; WildernessLocation anchor; TravelMotion trace; };
struct ContactHit { StableId a,b; Integer time_num=0,time_den=1000000000; ContactEndpoint first,second;std::optional<RoadContactEndpoints> road{}; };
struct ContactBatch {
    Integer source_tick=0,phase_token=0;
    std::vector<ContactHit> ordered_hits;
    std::vector<TravelMotion> motions;
    std::vector<ReadVersion> read_versions;
    Integer candidate_pairs=0,exact_pairs=0;
};
using ContactEntrances=std::unordered_map<StableId,std::vector<PointI>>;
// 只读纯几何入口；无效trace不参加候选，重复身份整组拒绝，选择按最早时刻稳定排序。
Status validate_terrain_trace(const TravelMotion&);
std::optional<TravelMotion> stationary_terrain_trace(const StableId&,const WildernessLocation&);
std::optional<ContactHit> terrain_intersection(const TravelMotion&,const TravelMotion&,const ContactEntrances& = {});
// R §5.6新域的单对实际段交集；仅局部[0,1]位移，时间尚未量化。
// 输入为各自原basis采样的单段拥有值，零宽交集只核当刻原半径/入口谓词。
struct LocalTerrainContact {double local_parameter=0.0;ContactEndpoint first,second;};
Outcome<std::optional<LocalTerrainContact>> terrain_local_interval_intersection(const TravelMotion&,const TravelMotion&,bool instant_only,const ContactEntrances& = {});
std::vector<ContactHit> detect_terrain_contacts(const std::vector<TravelMotion>&,const ContactEntrances& = {},Integer* candidates=nullptr,Integer* exact=nullptr);
bool contact_before(const ContactHit&,const ContactHit&);
} // 检测不安装encounter，不改变资源或时钟。
