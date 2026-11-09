// 原 Affairs JSON 和 shape 共用同一字段访问；来源冷验由原 owner 独立执行。
#pragma once
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
struct AffairsState;
Status emit_affairs_state_fields(ProjectionWriter&,const AffairsState&);
}
