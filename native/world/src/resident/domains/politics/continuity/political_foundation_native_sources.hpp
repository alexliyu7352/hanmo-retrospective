// 同一候选的实际成立本人来源图。P10捕获与获知全验分层，不产生另一持久根。
#pragma once
#include "political_foundation_disclosure.hpp"
namespace hanmo::resident {
// 第一阶段只实际持有原上下文及两个P10来源。不得在构造时反入未成的获知图。
// 外层retaining
// owner必须保活真实World/body/control/clock/锁目录；此类不注册能力。
class NativeFoundationPersonalCaptureSources final {
  FoundationPersonalSourceContext personal_;
  NativeFoundationIntentResultRead intent_;
  NativeFoundationSupportResultRead support_;

public:
  explicit NativeFoundationPersonalCaptureSources(
      FoundationPersonalSourceContext);
  NativeFoundationPersonalCaptureSources(
      const NativeFoundationPersonalCaptureSources &) = delete;
  NativeFoundationPersonalCaptureSources &
  operator=(const NativeFoundationPersonalCaptureSources &) = delete;
  NativeFoundationPersonalCaptureSources(
      NativeFoundationPersonalCaptureSources &&) = delete;
  NativeFoundationPersonalCaptureSources &
  operator=(NativeFoundationPersonalCaptureSources &&) = delete;
  const FoundationPersonalSourceContext &personal() const { return personal_; }
  const NativeFoundationIntentResultRead &intent_results() const {
    return intent_;
  }
  const NativeFoundationSupportResultRead &support_results() const {
    return support_;
  }
};
struct FoundationPersonalKnowledgeInputs {
  const SocialState &social;
  const CommunicationLinksState &communications;
  const CommunicationSourceRead &communication_sources;
  const ExactResultTimeRead &actual_results;
  const InteractionState &interaction;
  const InteractionCatalog &interaction_catalog;
  const ReadStampAuthority &authority;
  const DomainReadVersionSource &versions;
};
// 第二阶段借第一阶段和已经真实捕获的P10快照，实际持有原Report/披露来源。
// 外层必须保两阶段及上述跨域输入同候选、同job寿命；禁止复制/移动悬挂内部引用。
class NativeFoundationPersonalKnowledgeSources final {
  const NativeFoundationPersonalCaptureSources &capture_;
  FoundationPersonalKnowledgeInputs inputs_;
  ExactResultReadStamp result_stamp_;
  FoundationSupportContext support_;
  NativeFoundationPersonalReportRead reports_;
  FoundationIntentDisclosureContext disclosure_context_;
  NativeFoundationIntentDisclosureRead disclosure_;

public:
  NativeFoundationPersonalKnowledgeSources(
      const NativeFoundationPersonalCaptureSources &,
      FoundationPersonalKnowledgeInputs);
  NativeFoundationPersonalKnowledgeSources(
      const NativeFoundationPersonalKnowledgeSources &) = delete;
  NativeFoundationPersonalKnowledgeSources &
  operator=(const NativeFoundationPersonalKnowledgeSources &) = delete;
  NativeFoundationPersonalKnowledgeSources(
      NativeFoundationPersonalKnowledgeSources &&) = delete;
  NativeFoundationPersonalKnowledgeSources &
  operator=(NativeFoundationPersonalKnowledgeSources &&) = delete;
  const FoundationSupportContext &support() const { return support_; }
  const NativeFoundationPersonalReportRead &reports() const { return reports_; }
  const NativeFoundationIntentDisclosureRead &disclosure() const {
    return disclosure_;
  }
  // 稳定World出口仍必须调用此full门，不能把前阶段的source-only捕获当全验。
  // 这只覆盖已有意向/本人回应，不冒整个M-CONTINUITY或成立结果已完成。
  Status validate_after_capture() const;
};
} // namespace hanmo::resident
