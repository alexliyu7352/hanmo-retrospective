// 当前地图可见人物只读页；位置索引由共同候选派生，不进入存档或形成第二位置真值。
#pragma once
#include "hanmo/resident/checkpoint_memory.hpp"
#include "journey_motion.hpp"
#include "resident/storage/common_store.hpp"
#include <map>
#include <tuple>
namespace hanmo::resident {
// 精确位置键区分同城不同场所、同路不同进度和同图不同整数点，不定义新的观察半径。
struct MapPresenceLocationKey {
  int kind = 0;
  StableId primary, secondary;
  Integer x = 0, y = 0;
  bool operator<(const MapPresenceLocationKey &other) const {
    return std::tie(kind, primary, secondary, x, y) <
           std::tie(other.kind, other.primary, other.secondary, other.x,
                    other.y);
  }
  bool operator==(const MapPresenceLocationKey &other) const {
    return std::tie(kind, primary, secondary, x, y) ==
           std::tie(other.kind, other.primary, other.secondary, other.x,
                    other.y);
  }
};
// 索引只存同位置的原ID序；查询游标不会扫描远方人物或透露远方人口总数。
struct MapPresenceIndex {
  WorldIdentity identity;
  CheckpointMap<MapPresenceLocationKey, std::vector<StableId>> actors;
};
struct MapPresenceCursor {
  WorldIdentity identity;
  StableId observer_actor_id;
  MapPresenceLocationKey location;
  Integer offset = 0;
};
struct MapPresenceActor {
  StableId actor_id;
  std::string display_name;
  Integer observed_tick = 0;
};
struct MapPresencePage {
  WorldIdentity identity;
  StableId observer_actor_id;
  MapPresenceLocationKey location;
  JourneyMapPosition position;
  Integer cursor = 0, scanned = 0;
  std::optional<MapPresenceCursor> next_cursor;
  std::vector<MapPresenceActor> rows;
};
inline constexpr Integer map_presence_page_limit = 64,
                         map_presence_scan_limit = 256;
// 只在候选创建/发布时派生；还原存档后按原CommonStore位置重新建立。
MapPresenceIndex build_map_presence_index(const CommonStore &);
// 仅当前控制人可查询；同页只读获准人物姓名，不返回行动计划、钱货或远方坐标。
Outcome<MapPresencePage>
query_map_presence(const CommonStore &, const TravelCatalog &,
                   const TravelServices &, const PresentationRead &,
                   const MapPresenceIndex &, const StableId &observer,
                   const std::optional<MapPresenceCursor> &cursor = {});
// 唯一边界编码仅包含typed投影；原生和Godot沿同一行旅位置形状显示道路进度。
BoundaryValue encode_map_presence(const MapPresencePage &);
Outcome<MapPresenceCursor> decode_map_presence_cursor(const BoundaryValue &);
} // namespace hanmo::resident
