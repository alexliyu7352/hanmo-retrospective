// 原五种任期业务接唯一runtime/Office writer；实际时刻由本次world clock提供。
#pragma once
#include "resident/domains/organization/organization_runtime.hpp"
namespace hanmo::resident {
class PoliticalActorQualificationRead {
public:
  virtual ~PoliticalActorQualificationRead() = default;
  virtual Status validate_independent_office_age(const StableId &,
                                                 const WorldMoment &) const = 0;
};
// 预览只借原运行时、同次读票与资格来源；不能借只读世界取得可变组织账。
struct PoliticalOrganizationRuntimeReadContext {
  const OrganizationRuntime &runtime;
  const OrganizationAuthorityClock &clock;
  const ReadStamp &read_stamp;
  const OfficeTransitionSourceRead &transitions;
  const PoliticalActorQualificationRead *qualifications = nullptr;
};
// 写入只借当前候选的原运行时；转为只读视图不复制账本或更换来源。
struct PoliticalOrganizationRuntimeWriteContext {
  OrganizationRuntime &runtime;
  const OrganizationAuthorityClock &clock;
  const ReadStamp &read_stamp;
  const OfficeTransitionSourceRead &transitions;
  const PoliticalActorQualificationRead *qualifications = nullptr;
  // 只返回原引用；调用者仍负责保活完整候选及其拥有体。
  PoliticalOrganizationRuntimeReadContext read() const {
    return {runtime, clock, read_stamp, transitions, qualifications};
  }
};
} // namespace hanmo::resident
