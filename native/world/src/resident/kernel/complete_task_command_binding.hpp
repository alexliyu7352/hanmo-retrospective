#pragma once
#include "hanmo/resident/command_contract.hpp"
namespace hanmo::resident {
struct WorldBundle;
// Only after the real Common result/history seal, before any full cold read or
// source pruning. This binds existing Task owner references; it allocates no
// business result and grants no new Director command capability.
Status bind_complete_task_command_result(WorldBundle&,const CommitResult&);
}
