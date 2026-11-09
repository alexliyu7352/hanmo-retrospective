// 验证工作集的有序去重集合沿原 job 分配器计量；不退回普通节点分配。
#pragma once
#include "hanmo/resident/checkpoint_memory.hpp"
#include <set>
namespace hanmo::resident {
template <class K, class Compare = std::less<K>>
using CheckpointSet = std::set<K, Compare, CheckpointAllocator<K>>;
}
