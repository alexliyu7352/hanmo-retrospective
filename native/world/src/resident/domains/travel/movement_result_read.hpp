#pragma once
#include "movement_exact.hpp"
namespace hanmo::resident {
// 只核既有 Group/contact 延后到达的来源局部形状；胜方、目录终点与
// terminal→下一 Opening→arrival 的全因果由 Field 原 reader 另行强验。
// arrival 必须借本 movement.arrivals 原行；不得以临时副本替代同根身份。
bool is_field_group_contact_arrival(const MovementExactState &,
                                    const MovementArrivalSource &,
                                    ExactArithmeticBudget budget = {});
// schema=1；到达只证明位姿，绝不冒军令履约或Action终结。
Outcome<WorldMoment>
read_movement_source_moment(const MovementExactState &, const SourceRef &,
                            Integer schema, ExactArithmeticBudget budget = {});
Outcome<std::optional<SourceRef>>
adopt_exact_personal_contact_arrival_in_candidate(
    CommonWrite &, MovementExactState &, const StableId &contact_id,
    const StableId &prefix_id, ExactArithmeticBudget budget = {});
Outcome<MovementArrivalSource>
read_verified_movement_arrival(const MovementExactState &, const SourceRef &,
                               Integer schema,
                               ExactArithmeticBudget budget = {});
} // namespace hanmo::resident
