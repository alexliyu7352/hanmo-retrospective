// A2 缺实现入口共用的瞬态失败出口；不扩写复制或保留计量算法。
#pragma once
#include "checkpoint_job_budget.hpp"
#include <limits>

namespace hanmo::resident {
// 保留 uint8_t 最大值 255 仅表示进程内 NOT_IMPLEMENTED；不用于持久化或线协议。
// 不修改既有公共失败枚举及其数值，也不把此保留值解释为内存预算或成功。
inline constexpr CheckpointJobFailureCode checkpoint_not_implemented_code =
    static_cast<CheckpointJobFailureCode>(
        std::numeric_limits<std::uint8_t>::max());

// 沿既有共享异常出口明确终止缺实现调用；由原公开边界返回失败，不制造空成功。
[[noreturn]] inline void throw_checkpoint_not_implemented() {
  throw checkpoint_job_detail::FailureException{
      CheckpointJobFailure::fixed(checkpoint_not_implemented_code)};
}
} // namespace hanmo::resident
