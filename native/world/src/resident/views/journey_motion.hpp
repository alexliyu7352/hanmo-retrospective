// B有限只读投影：只读当前人物/行旅和指定地图，D另包同源身份；不寻路、不导出世界。
#pragma once
#include "hanmo/resident/presentation_contract.hpp"
#include "resident/domains/travel/travel.hpp"
namespace hanmo::resident {
// 原场景/侧栏按raw位置路由，不把Following/InArmy替换成领队位置。
struct JourneyLocationSummary {
    StableId kind;
    std::optional<StableId> place_id,settlement_id;
};
// 地图显示点取同候选resolved位置；转为屏幕Vector2仅在Godot主线程发生。
// 单条作者边至多64个中间点；原Godot沿线浮点插值仍只在主线程表现层运行。
struct JourneyRoadPosition { StableId edge_id; Integer progress=0,length=1; std::vector<PointI> points; bool raw=false; };
struct JourneyMapPosition { StableId world_map_id; PointI observer_position; std::optional<JourneyRoadPosition> road; };
struct JourneyTerrainSummary { bool paused=false; Integer remaining_ticks=0; TravelDestination destination; };
struct JourneyRoadSummary { StableId edge_id; Integer progress=0; bool paused=false; };
struct JourneyTravelSummary { bool active=false; std::optional<JourneyTerrainSummary> terrain; std::optional<JourneyRoadSummary> road; };
// 集合每页最多64项，total_rows与next_cursor仅为当前只读快照的显示边界。
struct JourneyRoutePage {
    bool active=false;
    Integer cursor=0,total_rows=0;
    std::optional<Integer> next_cursor;
    std::vector<PointI> points;
    bool road=false,returning=false;
    std::vector<StableId> path;
};
struct JourneyCrossingRow { StableId flag_id; bool closed=false; };
struct JourneyCrossingPage {
    Integer cursor=0,total_rows=0;
    std::optional<Integer> next_cursor;
    std::vector<JourneyCrossingRow> rows;
    bool road=false;
};
struct JourneyCompanionRow { StableId actor_id; std::string display_name; Integer cost_per_tick=0; };
struct JourneyCompanionPage {
    bool party_present=false;
    Integer cursor=0,total_rows=0;
    std::optional<Integer> next_cursor;
    std::vector<JourneyCompanionRow> rows;
};
// 仅取原raw kind/place/settlement；会面早退前可调用，不读行动详细参数。
Outcome<JourneyLocationSummary> query_journey_location(const CommonRead&,const StableId& observer);
// 据点取目录地图坐标，原野取保存整数点；未知/不匹配来源明确失败，不补零坐标。
Outcome<JourneyMapPosition> query_journey_map_position(const CommonRead&,const TravelCatalog&,const TravelServices&,const StableId& observer);
// sidebar只在C确认没有pending encounter后调用；不在本函数重新选择互动或计算口粮。
Outcome<JourneyTravelSummary> query_journey_travel(const CommonRead&,const TravelState&,const StableId& observer);
// 直接分页当前点+已保存余路线，不裁剪、不平滑、不重新规划；raw非Wilderness的地形高亮拒绝。
Outcome<JourneyRoutePage> query_journey_route(const CommonRead&,const TravelState&,const StableId& observer,Integer cursor=0);
// 当前地图crossing/blocked_flag首现顺序，同名旗标只显示一次；未设置flag沿原false缺省。
Outcome<JourneyCrossingPage> query_journey_crossings(const TravelCatalog&,const TravelServices&,Integer cursor=0,Integer tick=0);
// 只读实际party原成员序、排除leader；C会面早退后才调用，不从军团成员冒充同行名单。
Outcome<JourneyCompanionPage> query_journey_companions(const CommonState&,const TravelServices&,const PresentationRead&,const StableId& observer,Integer cursor=0);
} // 返回值全部深拥有；原子发布与跨页身份一致由中央/D负责。
