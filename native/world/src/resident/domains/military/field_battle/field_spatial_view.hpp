// S1 连续会战位置值；此头不依赖 TravelServices，供原空间读口优先选择。
#pragma once
#include "field_time.hpp"
namespace hanmo::resident {
// S1 的在战读义：当前点来自唯一编队，绝不返回冻结的 OnRoute 冒可移动位置。
struct FieldBattleSpatialView {
  StableId battle_id, army_id, formation_id, field_hash;
  Integer side = 0;
  FieldBattleTime now;
  PointI point_dm;
};
} // namespace hanmo::resident
