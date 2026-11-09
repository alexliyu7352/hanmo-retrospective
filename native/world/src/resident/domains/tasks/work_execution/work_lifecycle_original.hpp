// 精确新源由原 Governance/Work 读口验证；未提供的新永久理由不伪装成已实现。
#pragma once
#include "work_original_read.hpp"
#include "work_reassignment_sources.hpp"
namespace hanmo::resident {
class NativeGovernanceWorkLifecycleRead final
    : public WorkLifecycleOriginalRead {
  const WorkReassignmentSourceContext &sources_;
  const WorkLifecycleOriginalRead *other_originals_ = nullptr;

public:
  // 所选 /6 的截止与替任直接读取本图原源，不要求不存在的兜底 owner。
  explicit NativeGovernanceWorkLifecycleRead(
      const WorkReassignmentSourceContext &sources,
      const WorkLifecycleOriginalRead *originals = nullptr)
      : sources_(sources), other_originals_(originals) {}
  // 已有其它 profile 仍可显式借用它自己的原读口，不改变其历史来源规则。
  NativeGovernanceWorkLifecycleRead(
      const WorkReassignmentSourceContext &sources,
      const WorkLifecycleOriginalRead &originals)
      : sources_(sources), other_originals_(&originals) {}
  Outcome<WorkStopRead>
  read_actual_work_stop(const WorkEpisode &, const MomentOwnerSource &,
                        const WorldMoment &) const override;
  Status
  validate_actual_work_leader_change(const WorkEpisode &,
                                     const WorkLeaderChange &) const override;
};
} // namespace hanmo::resident
