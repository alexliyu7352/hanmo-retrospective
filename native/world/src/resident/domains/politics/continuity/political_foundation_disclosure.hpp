// 原作者披露真实意向；实际决策者只向原founder回复自己的同版决定。
// 两者都仍走原S3接触/发信/送达，不替任何人作决定或直接写获知。
#pragma once
#include "political_foundation_report.hpp"
#include "resident/domains/interaction/report_disclosure.hpp"

namespace hanmo::resident {
struct InteractionState;
struct InteractionCatalog;
struct FoundationIntentDisclosureContext {
  const FoundationPersonalSourceContext &personal;
  const SocialState &social;
  const ExactResultTimeRead &results;
  const ExactResultReadStamp &result_stamp;
  const InteractionState &interaction;
  const InteractionCatalog &interaction_catalog;
  const ReadStampAuthority &authority;
  const DomainReadVersionSource &versions;
  // 回应分支必须借同一实际full支持来源；空值只允许原意向披露，非完整能力。
  const FoundationSupportContext *support = nullptr;
};

// 此候选依赖待审 S3 原 virtual 的 caller-owned charge 参数；未接 shared ABI。
// 外层 retaining scope 保同一 World/clock/body/control/报告与租约，不单独注册。
class NativeFoundationIntentDisclosureRead final
    : public ReportDisclosureOriginalRead {
  const FoundationIntentDisclosureContext &context_;

public:
  explicit NativeFoundationIntentDisclosureRead(
      const FoundationIntentDisclosureContext &context)
      : context_(context) {}
  Outcome<std::optional<MomentOwnerSource>> read_report_disclosure_permission(
      const Report &, const SubjectRef &sender, const SubjectRef &receiver,
      const WorldMoment &, const ReadStamp &,
      CheckpointMemoryCharge &returned_source_memory) const override;
  Status validate_report_disclosure_permission(
      const Report &, const SubjectRef &sender, const SubjectRef &receiver,
      const MomentOwnerSource &, const WorldMoment &,
      const SourceRef &actual_disclosure) const override;
};
} // namespace hanmo::resident
