// 原 MoveLocal 与同候选 S1 来源接头；不创建第二次捕获或替代位置权威。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct WorldBundle;
struct CompleteExactWorldReadBindings;
// 未选 exact Travel 时仍调用原位置 writer；已选则必须有真实 capture 和原空间头。
Status apply_native_local_move_in_candidate(
    WorldBundle &, const StableId &actor, const StableId &place,
    const CompleteExactWorldReadBindings *actual = nullptr);
// 反覆盖布局之后沿原同世界历史校验；不是当前空表或 Common 投影的替代校验。
Status validate_native_local_move_world_sources(const WorldBundle &);
} // namespace hanmo::resident
