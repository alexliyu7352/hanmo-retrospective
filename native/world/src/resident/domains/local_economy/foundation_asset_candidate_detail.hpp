// 同一 B1 候选的唯一私有 allocator/追加正文；各 writer 不另复制预算规则。
#pragma once
#include "foundation_asset_candidate.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <stdexcept>
namespace hanmo::resident::foundation_asset_candidate_detail {
struct Failure {
  RuleError error;
};
[[noreturn]] inline void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
inline int compare(const WorldMoment &a, const WorldMoment &b) {
  return take(compare_world_moments(a, b));
}
// 新字段只能属于这个私有候选和当前 job；失败不恢复半搬前像，整候选弃。
inline void candidate(const FoundationAssetWriteContext &x) {
  take(validate_foundation_asset_source_context(x.read));
  const auto memory = current_checkpoint_memory();
  const auto &owner = x.owned_fields.resource();
  if (&x.common != &x.read.common || &x.state != &x.read.state || !memory ||
      !owner || !memory->shares_job(*owner))
    fail("FoundationAssetActualCandidateRequired");
  const auto same = [&](const auto &rows) {
    const auto resource = rows.get_allocator().resource();
    return resource && memory->shares_job(*resource);
  };
  if (!same(x.state.offers) || !same(x.state.consents) ||
      !same(x.state.preparations) || !same(x.state.fulfillments))
    fail("FoundationAssetActualCandidateAllocatorRequired");
}
// 所有普通文本都先按原 pinned fresh-range 布局预留，再构造唯一最终字段。
inline StableId fresh(std::string_view value) {


  StableId result(value.data(), value.size());
  return result;
}
template <class T> void append(CheckpointVector<T> &rows, T &&row) {
  if (rows.size() == rows.max_size())
    fail("FoundationAssetSourceCapacityExceeded");
  CheckpointVector<T> next(rows.get_allocator());
  next.reserve(rows.size() + 1);
  for (auto &old : rows)
    next.emplace_back(std::move(old));
  next.emplace_back(std::move(row));
  rows.swap(next);
}
} // namespace hanmo::resident::foundation_asset_candidate_detail
