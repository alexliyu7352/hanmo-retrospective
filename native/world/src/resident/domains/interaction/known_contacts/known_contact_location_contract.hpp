// 位置见闻的有限值合同；不依赖Interaction状态，使新独立段不回包旧交互模型。
#pragma once
#include "resident/model/common_model.hpp"
namespace hanmo::resident {
// observed为原亲见时刻，learned为实际获知时刻；没有当前位置或隐含追踪权限。
struct KnownContactLocation {
    StableId subject_actor_id,place_id,source_fact_id,source_kind;
    Integer observed_tick=0,learned_tick=0;
};
}
