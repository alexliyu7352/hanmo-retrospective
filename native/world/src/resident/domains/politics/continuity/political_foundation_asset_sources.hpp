// 原成立采用真实S5投入的窄消费；预约、历史采用和已履行分别核，不创造资源。
#pragma once
#include "political_foundation_terms.hpp"
#include "resident/kernel/world_timeline.hpp"
#include <span>
namespace hanmo::resident {
struct FoundationAssetReadContext;

// 同候选已经真实预约后的决定前门；引用按原Terms.asset_contribution_refs顺序。
// 只核原投入一腿，不替代本人支持、场所/载体、供养、旧责或成立公权。
Status validate_foundation_current_asset_preparations(
    const FoundationAssetReadContext &, const MomentOwnerSource &actual_intent,
    Integer terms_revision, const FoundationTerms &,
    std::span<const SourceRef> actual_preparations);
// 冷读实际决定的前序来源：调用者先反读原决定payload/M，再读真实采用。
// 不要求旧钱货今日仍被预约，不用今日Owner重判原捐助，也不授再次支用权。
Status validate_foundation_historical_asset_preparations(
    const FoundationAssetReadContext &, const MomentOwnerSource &actual_intent,
    Integer terms_revision, const FoundationTerms &,
    std::span<const SourceRef> actual_preparations,
    const MomentOwnerSource &actual_decision, const WorldMoment &decided_at);
// 当前原登记末段只认同一实际准备的真实履行，顺序决定<实际初始安装<履行<登记。
// 此口使用原S5热执行capture，不能用于冷RegistrationResult历史反读。
// 调用者必须先反读实际installation payload/M及同决定/组织绑定，再传入本口；
// 仍须原S6真实body/Office/member和完整RegistrationResult逐项封口。
Status validate_foundation_current_registered_asset_fulfillments(
    const FoundationAssetReadContext &, const MomentOwnerSource &actual_intent,
    Integer terms_revision, const FoundationTerms &,
    std::span<const SourceRef> actual_preparations,
    std::span<const SourceRef> actual_fulfillments,
    const StableId &actual_organization,
    const MomentOwnerSource &actual_decision, const WorldMoment &decided_at,
    const MomentOwnerSource &actual_installation,
    const WorldMoment &installed_at,
    const MomentOwnerSource &actual_registration,
    const WorldMoment &registered_at);
// 冷RegistrationResult只读S5原持久捕获的真实执行证据；不要求热capture
// 仍存在，也不能用于当前候选确认本次已经履行或获得再次支用许可。
Status validate_foundation_historical_registered_asset_fulfillments(
    const FoundationAssetReadContext &, const MomentOwnerSource &actual_intent,
    Integer terms_revision, const FoundationTerms &,
    std::span<const SourceRef> actual_preparations,
    std::span<const SourceRef> actual_fulfillments,
    const StableId &actual_organization,
    const MomentOwnerSource &actual_decision, const WorldMoment &decided_at,
    const MomentOwnerSource &actual_installation,
    const WorldMoment &installed_at,
    const MomentOwnerSource &actual_registration,
    const WorldMoment &registered_at);
} // namespace hanmo::resident
