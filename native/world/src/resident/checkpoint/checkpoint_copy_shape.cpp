#include "checkpoint_copy_shape.hpp"
#include <stdexcept>
namespace hanmo::resident {
std::string checkpoint_fresh_text_copy(std::string_view value) {
  return std::string(value.empty() ? "" : value.data(), value.size());
}
std::string checkpoint_fresh_string_copy(const std::string &value) {
  return checkpoint_fresh_text_copy(value);
}

// own先于父引用声明，所有typed字段已由record销毁后才归还普通字段预留。
struct CheckpointCopyLease::State {
  CheckpointMemoryCharge own;
  std::shared_ptr<const State> left, right;
  explicit State(CheckpointMemoryCharge value) : own(std::move(value)) {}
  State(std::shared_ptr<const State> a, std::shared_ptr<const State> b)
      : left(std::move(a)), right(std::move(b)) {}
};
CheckpointCopyLease
CheckpointCopyLease::reserve() {
  CheckpointMemoryCharge reserved(0);
  CheckpointCopyLease result;
  result.state_ = checkpoint_make_shared<State>(std::move(reserved));
  return result;
}
CheckpointCopyLease
CheckpointCopyLease::hold_both(const CheckpointCopyLease &a,
                               const CheckpointCopyLease &b) {
  if (!a.state_)
    return b;
  if (!b.state_ || a.state_ == b.state_)
    return a;
  CheckpointCopyLease result;
  result.state_ = checkpoint_make_shared<State>(a.state_, b.state_);
  return result;
}
} // namespace hanmo::resident
