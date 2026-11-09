#pragma once
#include "hanmo/resident/command_contract.hpp"
namespace hanmo::resident {
struct WorldBundle;
// Only the actual committed original action-start tail calls this. The before
// world remains borrowed until the whole unpublished candidate is accepted.
Status bind_complete_growth_command_start(const WorldBundle* actual_before,
    WorldBundle&,const StableId& actor,Integer request_seq,const CommitResult&);
// Re-enter only for this same actual result after original commit-tail writers.
// The Growth owner verifies any already bound release matches every identity field.
Status bind_complete_growth_release_result(const WorldBundle* actual_before,
    WorldBundle&,const StableId& actor,Integer request_seq,const CommitResult&);
// The actual original result/history have been appended, and any Director6
// adoption exists. Each command is finalized before a later same-M command.
Status bind_complete_growth_command_result(const WorldBundle* actual_before,
    WorldBundle&,const StableId& actor,Integer request_seq,const CommitResult&);
}
