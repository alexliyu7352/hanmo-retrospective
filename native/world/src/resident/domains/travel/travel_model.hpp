// 行旅领域唯一权威载荷；共同人物、钱包、时钟与行动头仍由CommonRead/Write拥有。
#pragma once
#include "../../model/common_model.hpp"
#include <unordered_map>

namespace hanmo::resident {
// 目的地保原字符串/整数坐标二选一，不在热路径转JSON或以城市替换原野目标。
using TravelDestination=std::variant<StableId,PointI>;
// 活动行旅的完整typed参数；points始终是原事实的剩余后缀。
struct TerrainTravelParams {
    StableId plan_id,map_id,origin,payer_wallet_id;
    StableId mode="foot";
    Integer route_index=1,budget=1,travel_credit=0,money_per_tick=0,fatigue_per_tick=0;
    std::vector<PointI> points;
    TravelDestination destination;
    bool paused=false;
    StableId encounter_id;
    bool has_encounter_id=true,has_met_actions=true;
    std::vector<StableId> met_actions;
    std::vector<StableId> field_order; // 冷边界字段插入序，仅含明确schema键，不保存任意值。
};
// 作者道路行动保留原路径、方向和可选键存在性；与地形行动互斥且不另建世界。
struct RoadTravelParams {
    // begin_action原命令留下的字段仅保来源；改道后不能把它们当作当前路径真值。
    std::optional<TravelDestination> destination;
    std::optional<std::vector<StableId>> via;
    std::vector<StableId> path;
    Integer edge_index=0;
    StableId payer_wallet_id;
    bool returning=false,paused=false,reverse_first=false;
    bool has_terrain=false,has_paused=true,has_reverse_first=true,has_encounter_id=false,has_met_actions=false;
    StableId encounter_id;
    std::vector<StableId> met_actions,field_order;
};
// 道路报价区分首次启程和改道；只有原改道报价带returning字段。
struct RoadTravelQuote { std::vector<StableId> path; std::optional<bool> returning; };
// 只读半日道路轨迹保存端点转边后的参数快照，不持世界引用。
struct RoadTravelMotion {
    StableId actor_id,action_id,edge_id,origin;
    Integer start=0,end=0;
    bool moving=false;
    RoadTravelParams params;
};
struct RoadContactEndpoint { StableId actor_id; OnRouteLocation anchor; RoadTravelMotion trace; };
struct RoadContactEndpoints { RoadContactEndpoint first,second; };
// 救助只记录真实起点；物资损失经共享recipe，不能在此另持库存。
struct RescueParams { StableId origin; };
// 永久计划事实由B持typed载荷，ID和全局插入次序由A登记一次。
struct TerrainJourneyFact {
    StableId id,actor_id,action_id,map_id,mode;
    Integer tick=0,budget=1,starting_load=0;
    PointI origin;
    TravelDestination destination;
    std::vector<PointI> points;
    std::vector<StableId> field_order;
};
// NPC有限计划保原索引、休整时刻与在途标记，不运行独立AI时钟。
struct TravelItinerary { Integer index=0,ready_tick=0; bool travelling=false; std::vector<StableId> field_order; };
// 领域记录使用ID索引加显式原序；共同action_order/fact_order仍只由A维护。
struct TravelState {
    OrderedTable<TerrainTravelParams> actions;
    OrderedTable<RoadTravelParams> road_actions;
    OrderedTable<RescueParams> rescues;
    OrderedTable<TerrainJourneyFact> journey_facts;
    OrderedTable<TravelItinerary> itineraries;
};
// 行旅报价保持原规则返回值，不隐含未来半日预演或提前扣费。
struct TravelQuote {
    Integer duration=0,cost=0,budget=1,starting_load=0,money_per_tick=0,fatigue_per_tick=0;
    StableId map_id,origin,mode,payer_wallet_id;
    std::vector<PointI> points;
    TravelDestination destination;
    std::optional<RoadTravelQuote> road;
};
// 领域命令交中央合成唯一union；非法类型由边界decoder拒绝。
struct TravelCommand {
    TravelDestination destination;
    std::vector<StableId> via;
    bool has_destination=true,has_via=false;
    std::vector<StableId> field_order{}; // 仅destination/via两键的原输入序，空序采用存在键的固定顺序。
};
struct PauseTravelCommand {}; struct ResumeTravelCommand {}; struct TravelWaitCommand {};
struct RerouteCommand { TravelDestination destination; std::vector<StableId> via; };
struct ReturnTravelCommand {}; struct RescueCommand {}; struct MoveLocalCommand { StableId place_id; };
using TravelCommandPayload=std::variant<TravelCommand,PauseTravelCommand,ResumeTravelCommand,TravelWaitCommand,RerouteCommand,ReturnTravelCommand,RescueCommand,MoveLocalCommand>;
// 准备结果不持候选引用；apply时只操作当前Txn并由中央承担唯一提交边界。
struct PreparedTravelOp { StableId actor_id; TravelCommandPayload command; std::optional<TravelQuote> quote; };
// 军事行军票据只包含本次真实移动整数前缀，由C消费条件规则。
struct TerrainExecution {
    StableId actor_id,action_id,plan_id,map_id;
    Integer action_start_tick=0,source_tick=0,route_index_before=0,route_index_after=0,travel_credit_before=0,travel_credit_after=0;
    PointI from_point,to_point;
    Location result_location;
};
struct TravelStepResult { bool completed=false; std::optional<TerrainExecution> execution; };
struct TravelScheduleProposal { StableId actor_id,destination; Integer next_index=0; };
} // 领域状态不保存CommonRead引用或跨事务资源借用。
