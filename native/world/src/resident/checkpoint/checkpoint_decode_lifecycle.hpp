// 解码前只核原 job 生命周期；不登记类型默认内存，也不构造或重算领域值。
#pragma once
#include "checkpoint_capacity.hpp"
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>
namespace hanmo::resident {
// 保留调用处的类型/数量接口，避免改动真实构造顺序；计量特化已整层退役。
template <class T>
Status charge_checkpoint_defaults(CodecDecodeBudget &budget,
                                  std::uint64_t = 1) {
  return budget.charge_candidate(0);
}
// 原 archive 仍负责首错和当前 job；此接头不授予 codec 或业务来源权限。
template <class T, class Archive>
Status prepare_checkpoint_default_values(Archive &archive,
                                         std::uint64_t count = 1) {
  return archive.template prepare_default_values<T>(count);
}
} // namespace hanmo::resident
