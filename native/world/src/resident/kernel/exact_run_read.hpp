// run/冷续读取同一真实owner，不从兼容remaining或许可上限推导完成/恢复权。
#pragma once
#include "exact_complete_world.hpp"
#include "hanmo/resident_world_api.hpp"
namespace hanmo::resident {
Outcome<ContinuationStartedSource> read_complete_exact_continuation_start(const WorldBundle&,const CompleteExactWorldReadBindings&,const StableId& action_id,const StableId& action_kind);
Outcome<RunStop> read_complete_exact_continuation_stop(const WorldBundle&,const CompleteExactWorldReadBindings&,const ContinuationSnapshot&);
Status validate_complete_exact_continuation_source(const WorldBundle&,const CompleteExactWorldReadBindings&,const ContinuationSnapshot&);
bool complete_exact_run_kind(const CompleteExactTimeRuntime&,const StableId& action_kind) noexcept;
}
