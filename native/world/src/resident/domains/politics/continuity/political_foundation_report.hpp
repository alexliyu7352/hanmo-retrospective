// 成立意向的原 P10 投影与 S3 正文地址；不直接写 Report、获知或送达。
#pragma once
#include "political_foundation_decisions.hpp"
#include "resident/domains/interaction/report_official_content.hpp"

namespace hanmo::resident {
// 只选实际意向头。支持/成立结果尚未接入此叶，不能把未知 kind 当普通报告。
bool native_foundation_intent_result_kind(const FactRecord &);
class NativeFoundationIntentResultRead final : public ExactResultOwnerRead {
  const FoundationPersonalSourceContext &context_;

public:
  explicit NativeFoundationIntentResultRead(
      const FoundationPersonalSourceContext &context)
      : context_(context) {}
  Outcome<ExactOwnerFactRead>
  read_selected_fact(const FactRecord &) const override;
  Outcome<WorldMoment>
  read_selected_source_moment(const MomentOwnerSource &) const override;
};

// 本人真实答复由原P10给本人可知；founder不因source存在自动获知回应。
// 捕获只走source-only cut避免ExactResult→获知→ExactResult自证，出口仍全验。
bool native_foundation_support_result_kind(const FactRecord &);
class NativeFoundationSupportResultRead final : public ExactResultOwnerRead {
  const FoundationPersonalSourceContext &context_;

public:
  explicit NativeFoundationSupportResultRead(
      const FoundationPersonalSourceContext &context)
      : context_(context) {}
  Outcome<ExactOwnerFactRead>
  read_selected_fact(const FactRecord &) const override;
  Outcome<WorldMoment>
  read_selected_source_moment(const MomentOwnerSource &) const override;
};

// 已有真实 P10 Report 必须与原意向、原 M 和完整正文逐项同源。
// 只给原 founder 自己的直接观察，不把被邀请人预先列作知情者。
Outcome<OfficialContentRef> read_foundation_intent_report_official_content(
    const FoundationPersonalSourceContext &, const SocialState &,
    const ExactResultTimeRead &, const ExactResultReadStamp &,
    const StableId &actual_report_id);
class NativeFoundationIntentReportRead final
    : public ReportOfficialContentRead {
  const FoundationPersonalSourceContext &context_;
  const SocialState &social_;

public:
  NativeFoundationIntentReportRead(
      const FoundationPersonalSourceContext &context, const SocialState &social)
      : context_(context), social_(social) {}
  bool bound_to(const CommonStore &,
                const SocialState &) const noexcept override;
  Outcome<std::optional<OfficialContentRef>>
  read_report_official_content(const Report &, const ExactResultTimeRead &,
                               const ExactResultReadStamp &) const override;
};
// 回应仍携同一意向正文版本，但report的实际fact必须是这项本人accept/refuse。
// 完整支持reader在此必核原先真实获知，不借P10 source-only cut授披露许可。
Outcome<OfficialContentRef> read_foundation_support_report_official_content(
    const FoundationSupportContext &, const StableId &actual_report_id);
class NativeFoundationPersonalReportRead final
    : public ReportOfficialContentRead {
  const FoundationSupportContext &context_;

public:
  explicit NativeFoundationPersonalReportRead(
      const FoundationSupportContext &context)
      : context_(context) {}
  bool bound_to(const CommonStore &,
                const SocialState &) const noexcept override;
  Outcome<std::optional<OfficialContentRef>>
  read_report_official_content(const Report &, const ExactResultTimeRead &,
                               const ExactResultReadStamp &) const override;
};
} // namespace hanmo::resident
