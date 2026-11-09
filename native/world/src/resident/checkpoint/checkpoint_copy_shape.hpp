// 真实复制的普通字符串构造与共享 job 生命周期；无逐型计划、容量或计量模式。
#pragma once
#include "hanmo/resident/checkpoint_memory.hpp"
#include <limits>
#include <string>
#include <string_view>
namespace hanmo::resident {
// Borrowed text inputs for one fresh ordinary string-vector target. The
// tracked input view shell is not charged again as if it were that target.
struct CheckpointStringVectorBuildView {
  const CheckpointVector<std::string_view> &values;
};


// 可抛typed移动/交换期间共享原两份普通字段租约；不额外收费同一payload。
// 失败后私有候选仍可安全析构；成功后每个record只留对应原payload租约。
class CheckpointCopyLease {
  struct State;
  std::shared_ptr<const State> state_;

public:
  CheckpointCopyLease() noexcept = default;
  static CheckpointCopyLease reserve();
  static CheckpointCopyLease hold_both(const CheckpointCopyLease &,
                                       const CheckpointCopyLease &);

};
// 普通 fresh 字符串构造；真实长度与读档剩余字节门由调用处保留。
std::string checkpoint_fresh_text_copy(std::string_view);
std::string checkpoint_fresh_string_copy(const std::string &);
} // namespace hanmo::resident
