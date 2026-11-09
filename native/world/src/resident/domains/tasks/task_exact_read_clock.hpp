// Ephemeral actual S0 phase read. Never serialized and never a second current/elapsed owner.
#pragma once
#include "resident/kernel/world_timeline.hpp"
namespace hanmo::resident {
class CommonStore;struct TaskState;struct WorldBundle;class CharacterConditionWorldRead;struct TaskExactTerminalSource;
struct TaskExactReadClock {const ActionElapsedStore*elapsed=nullptr;const TimeScaleRef*scale=nullptr;WorldMoment at;
 // Borrowed from the actual outer owning lease; never an ambient/global read.
 const WorldBundle* world=nullptr;const CharacterConditionWorldRead* original_body=nullptr;
};
Outcome<std::optional<TaskExactReadClock>> capture_task_exact_read_clock(const WorldBundle&,const CharacterConditionWorldRead* original_body=nullptr);
// Null means this Action has no original death terminal. A present row is fully
// proved through the same Task/Family4 owner and actual S0 release binding.
Outcome<const TaskExactTerminalSource*> read_task_exact_death_clock(const CommonStore&,const TaskState&,const TaskExactReadClock&,const StableId&action);
Status validate_task_exact_active_clock(const CommonStore&,const TaskState&,const TaskExactReadClock&,const StableId&action,Integer original_start_tick,Integer original_duration);
Status validate_task_exact_completion_clock(const CommonStore&,const TaskState&,const TaskExactReadClock&,const StableId&action,Integer original_start_tick,Integer original_duration,Integer original_end_tick);
Status validate_task_exact_historical_result_clock(const CommonStore&,const TaskState&,const TaskExactReadClock&,const StableId&action,const StableId&original_result,Integer original_end_tick);
}
