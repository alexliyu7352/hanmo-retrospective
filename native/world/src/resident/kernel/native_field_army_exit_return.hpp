// 真实整军到齐源的同候选原 S1 接续；不重算战场/世界时间或替代卸装。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct WorldBundle;
class CompleteExactWorldBindingOwner;
class NativeFieldGroupWorld;
struct ExactPhaseWriteContext;
struct SourceRef;
Status adopt_native_field_army_exit_ready_in_candidate(
    WorldBundle &, CompleteExactWorldBindingOwner &, NativeFieldGroupWorld &,
    const SourceRef &actual_ready, ExactPhaseWriteContext &);
} // namespace hanmo::resident
