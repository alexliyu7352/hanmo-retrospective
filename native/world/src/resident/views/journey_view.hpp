// 有限深拥有显示DTO；只携带主线程展示数据，不能作为世界规则写入来源。
#pragma once
#include "hanmo/resident_world_api.hpp"
#include <functional>

namespace hanmo::resident {
// 文本/位置只供展示，地图投影Vector2由Godot主线程计算，绝不反写权威坐标。
struct JourneyViewRow { StableId id,kind; std::string text; std::optional<PointI> map_position; };
struct JourneyViewPage {
    WorldIdentity identity;
    StableId actor_id;
    Integer cursor=0,total_rows=0;
    std::optional<Integer> next_cursor;
    std::vector<JourneyViewRow> rows;
};
// 按页调用受控读取器，绝不先制作全世界数组再截64项；失败不返回半页。
using JourneyRowReader=std::function<Outcome<JourneyViewRow>(Integer)>;
Outcome<std::shared_ptr<const JourneyViewPage>> make_journey_page(const WorldIdentity&,const StableId&,Integer cursor,Integer total_rows,const JourneyRowReader&);
} // 返回的页不持有world/domain引用，后续换world仍可安全销毁。
