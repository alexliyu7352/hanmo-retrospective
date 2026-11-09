// 旧来源profile的只读足迹接缝；不增加新持久状态、Command或世界phase。
#pragma once
#include "known_contact_location_view.hpp"
namespace hanmo::resident {
struct WorldBundle;
Outcome<BoundaryValue> query_known_contact_location_world(const WorldBundle&,Integer cursor=0);
}
