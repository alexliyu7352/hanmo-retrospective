#pragma once
#include "task_exact_execution.hpp"
#include "work_execution/work_exact_runtime.hpp"
namespace hanmo::resident {
class CharacterConditionWorldRead;class CompleteExactWorldBindingOwner;
// 唯一 tasks ActionTime owner，固定F与原监督V按具名kind派发。
// factory仅含实际原持者函数指针；每次读重新取当前phase lease。
Outcome<std::shared_ptr<const ActionTimeOriginalSourceRead>> make_complete_task_action_time_reader(const WorldBundle&,TaskWorkWorldFactory,const CharacterConditionWorldRead* original_body=nullptr);
// Same complete owner, without re-entering its public World factory. The caller
// owns this reader inside that immovable owner and destroys it first. Each
// supervised read still acquires a genuine actual-M inner Task/Work lease.
Outcome<std::shared_ptr<const ActionTimeOriginalSourceRead>> make_complete_task_action_time_reader_in_owner(
 const WorldBundle&,const CompleteExactWorldBindingOwner&,const CharacterConditionWorldRead* original_body=nullptr);
}
