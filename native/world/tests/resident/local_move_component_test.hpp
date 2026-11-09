// 原真实 bank 的有限 local-move 组件检查；零行不冒充 writer 已执行。
#pragma once
#include "hanmo/resident_world_api.hpp"
namespace hanmo::resident {
struct WorldBundle;
struct LocalMoveComponentCoverage {
 Integer selection_cases=0, actual_source_rows=0, codec_cases=0, negative_cases=0;
};
void check_local_move_components(const WorldBundle&,LocalMoveComponentCoverage&);
} // namespace hanmo::resident
