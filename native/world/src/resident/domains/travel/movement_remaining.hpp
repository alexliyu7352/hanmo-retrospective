#pragma once
#include "movement_exact.hpp"
#include "hanmo/resident_world_api.hpp"
namespace hanmo::resident {
struct MovementConditionalRemaining { MovementExecutionRef execution; StableId prefix_ref,unit_ref; HalfDayDuration conditional_remaining; SourceRef actual_basis_source; };
Outcome<RunStop> read_movement_continuation_stop(const MovementExactState&,const StableId& prefix_id,ExactArithmeticBudget budget={});
// 固定当前输入的纯服务余量；不推预计世界到达时刻，不推进原执行。
Outcome<MovementConditionalRemaining> read_movement_conditional_remaining(const MovementExactState&,const TravelCatalog&,const TravelServices&,const StableId& prefix_id,ExactArithmeticBudget budget={});
}
