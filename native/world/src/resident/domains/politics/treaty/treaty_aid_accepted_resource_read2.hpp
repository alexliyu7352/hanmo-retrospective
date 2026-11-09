#pragma once
#include "treaty_aid_public_asset_history.hpp"
#include "treaty_aid_recipient_acceptance.hpp"
#include "treaty_resource_activation.hpp"
namespace hanmo::resident {
// 最终激活可能晚于提案/签署且期间发生真实产权变化，必须使用本次接受原边界，
// 不能拿签署或预算批准时的产权替代。
class TreatyAidAcceptedPublicAssetHistoryRead
    : public TreatyAidPublicAssetHistoryRead {
public:
  virtual Outcome<Owner>
  read_owner_before_aid_acceptance(const TreatyResourceSourceLinks2 &,
                                   const TreatyAidAcceptedResourceSource2 &,
                                   const ResourceKey &) const = 0;
};
struct TreatyAidAcceptedResourceContext2 {
  const TreatyResourceActivationContext &activations;
  const TreatyAidRecipientAcceptanceContext &recipients;
  const TreatyAidAcceptedPublicAssetHistoryRead &public_assets;
};
struct TreatyAidAcceptedResourceView2 {
  const TreatyAidAcceptedResourceSource2 *source = nullptr;
  const TreatyActivationSource *activation = nullptr;
  TreatyAidRecipientAcceptanceView recipient;
  // 真实历史签署及本人同意原源，不制造 Grant 快照。
  std::vector<MomentOwnerSource> authority_sources;
};
Outcome<TreatyAidAcceptedResourceView2>
read_treaty_aid_accepted_resources2(const TreatyAidAcceptedResourceContext2 &,
                                    const MomentOwnerSource &);
Status visit_treaty_aid_accepted_original_prefix2(
    const TreatyAidAcceptedResourceContext2 &, const MomentOwnerSource &,
    CommonActualFactPrefixSink &);
class NativeTreatyAidAcceptedResourceResultRead2 final
    : public ExactResultOwnerRead {
  const TreatyAidAcceptedResourceContext2 &x_;

public:
  explicit NativeTreatyAidAcceptedResourceResultRead2(
      const TreatyAidAcceptedResourceContext2 &x)
      : x_(x) {}
  Outcome<ExactOwnerFactRead>
  read_selected_fact(const FactRecord &) const override;
  Outcome<WorldMoment>
  read_selected_source_moment(const MomentOwnerSource &) const override;
};
Status validate_treaty_aid_accepted_resources2(
    const TreatyAidAcceptedResourceContext2 &);
// 外层激活须在发布前、下游 Schedule/hold 前逐原 Aid 条款调用；任一失败
// 撤销整个候选，不能发布只完成部分条款的中间态。
Outcome<MomentOwnerSource> accept_treaty_aid_resources_in_candidate2(
    CommonStore &, OrderedTable<TreatyAidAcceptedResourceSource2> &,
    const TreatyAidAcceptedResourceContext2 &,
    const MomentOwnerSource &activation, const StableId &term,
    const MomentOwnerSource &actual_recipient_consent);
} // namespace hanmo::resident
