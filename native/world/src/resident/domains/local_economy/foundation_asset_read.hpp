// B1 原资材门分为已实际预约和已实际履行；两者不可用一个成功 bool 互换。
#pragma once
#include "foundation_asset_sources.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/storage/common_store.hpp"

namespace hanmo::resident {
struct FoundationSupportContext;
struct FoundationIntentSource;
// 共同根、完整内容身份与时钟必须与原 S6 personal context 为同一候选。
// 返回值仅借这些真实根，政治不得把指针存入持久结果或另造空 provider。
struct FoundationAssetReadContext {
  const CommonStore &common;
  const FoundationAssetSourcesState &state;
  const CatalogIdentity &identity;
  const CurrentMomentRead &clock;
  const FoundationSupportContext &foundation;
};
struct FoundationAssetConsentRead {
  const FoundationAssetOffer *offer = nullptr;
  const FoundationAssetConsent *consent = nullptr;
  const FoundationIntentSource *intent = nullptr;
};
struct PreparedFoundationAssetRead {
  const FoundationAssetOffer *offer = nullptr;
  const FoundationAssetConsent *consent = nullptr;
  const FoundationAssetPreparation *preparation = nullptr;
  const FoundationIntentSource *intent = nullptr;
};
struct FulfilledFoundationAssetRead {
  PreparedFoundationAssetRead prepared;
  const FoundationAssetFulfillment *fulfillment = nullptr;
};
// 同 job 首次源根的完整上下文门；不授本人表达或任何资源权限。
Status
validate_foundation_asset_source_context(const FoundationAssetReadContext &);
// 只读已实际发生的原 offer；不凭条款存在给当前用途/预约/成立许可。
Outcome<const FoundationAssetOffer *>
read_foundation_asset_offer(const FoundationAssetReadContext &,
                            const SourceRef &, Integer schema);
// 同版本人财产同意的真实来源，不等于已预约或已履行。
Outcome<FoundationAssetConsentRead>
read_foundation_asset_consent(const FoundationAssetReadContext &,
                              const SourceRef &, Integer schema);
// 成立采用前的当前门：核同版本人同意、当前有效窗口、准确 Common kind3
// 预约及尚未履行。只限同一未发布候选，FoundingDecision 须 pin 真 preparation。
Outcome<PreparedFoundationAssetRead> read_prepared_foundation_asset_commitment(
    const FoundationAssetReadContext &, const MomentOwnerSource &actual_intent,
    Integer terms_revision, const SourceRef &original_offer,
    const SourceRef &actual_preparation);
// 历史采用 cut 只读实际 preparation/同意/意向链，不检查今日资源是否仍
// 预约，也不授予再次使用。原 S6 决定的历史 reader 只可消费这个具名口。
Outcome<PreparedFoundationAssetRead>
read_foundation_asset_preparation_source(const FoundationAssetReadContext &,
                                         const SourceRef &, Integer schema);
// 历史履行 cut 仍逐原冷保留的真实钱/批前后帧核全部原语，不要求今日
// 资源仍在或热 capture 活着，不给当前再次转入/发起支付权限。
Outcome<FulfilledFoundationAssetRead>
read_foundation_asset_fulfillment_source(const FoundationAssetReadContext &,
                                         const SourceRef &, Integer schema);
// 原登记最终门：沿真实 preparation→fulfillment 反读原当时 Owner/钱货
// 执行结果与受领组织；此当前门再核同 candidate 原热 capture，冷历史
// 只能走上面具名历史 cut，不能把无热捕获误当本次已经实际转入。
Outcome<FulfilledFoundationAssetRead>
read_fulfilled_foundation_asset_commitment(
    const FoundationAssetReadContext &, const MomentOwnerSource &actual_intent,
    Integer terms_revision, const SourceRef &original_offer,
    const SourceRef &actual_fulfillment,
    const StableId &registered_organization);
} // namespace hanmo::resident
